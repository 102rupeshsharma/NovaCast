// SH-4 instruction semantics, following the Renesas SH-4 Software Manual.

#include "common/log.h"
#include "cpu/sh4.h"
#include "cpu/sh4_decode.h"

#include <array>
#include <atomic>
#include <bit>
#include <cmath>
#include <numbers>

namespace nc::cpu {

using Handler = void (*)(Sh4&, u32);

struct Interp {
    // ----- field helpers -----------------------------------------------------
    static u32 rn(u32 op) { return (op >> 8) & 0xF; }
    static u32 rm(u32 op) { return (op >> 4) & 0xF; }
    static u32 imm8(u32 op) { return op & 0xFF; }
    static u32 simm8(u32 op) { return static_cast<u32>(static_cast<s32>(static_cast<s8>(op & 0xFF))); }
    static u32 disp4(u32 op) { return op & 0xF; }

    static bool t(const Sh4& c) { return (c.s_.sr & sr::T) != 0; }
    static void set_t(Sh4& c, bool value) { c.s_.sr = value ? (c.s_.sr | sr::T) : (c.s_.sr & ~sr::T); }
    static bool flag(const Sh4& c, u32 bit) { return (c.s_.sr & bit) != 0; }
    static void set_flag(Sh4& c, u32 bit, bool value) { c.s_.sr = value ? (c.s_.sr | bit) : (c.s_.sr & ~bit); }

    static void warn_once(std::atomic<bool>& done, const char* what) {
        if (!done.exchange(true))
            LOG_WARN("sh4", "%s", what);
    }

    // ----- data transfer -----------------------------------------------------
    static void mov_imm(Sh4& c, u32 op) { c.s_.r[rn(op)] = simm8(op); }
    static void mov(Sh4& c, u32 op) { c.s_.r[rn(op)] = c.s_.r[rm(op)]; }
    static void movw_pc(Sh4& c, u32 op) {
        c.s_.r[rn(op)] = static_cast<u32>(static_cast<s16>(c.read16(c.s_.pc + 4 + imm8(op) * 2)));
    }
    static void movl_pc(Sh4& c, u32 op) { c.s_.r[rn(op)] = c.read32((c.s_.pc & ~3u) + 4 + imm8(op) * 4); }
    static void mova(Sh4& c, u32 op) { c.s_.r[0] = (c.s_.pc & ~3u) + 4 + imm8(op) * 4; }

    static u32 sx8(u8 v) { return static_cast<u32>(static_cast<s32>(static_cast<s8>(v))); }
    static u32 sx16(u16 v) { return static_cast<u32>(static_cast<s32>(static_cast<s16>(v))); }

    // MOV.x Rm,@Rn
    static void movb_store(Sh4& c, u32 op) { c.write8(c.s_.r[rn(op)], static_cast<u8>(c.s_.r[rm(op)])); }
    static void movw_store(Sh4& c, u32 op) { c.write16(c.s_.r[rn(op)], static_cast<u16>(c.s_.r[rm(op)])); }
    static void movl_store(Sh4& c, u32 op) { c.write32(c.s_.r[rn(op)], c.s_.r[rm(op)]); }
    // MOV.x @Rm,Rn
    static void movb_load(Sh4& c, u32 op) { c.s_.r[rn(op)] = sx8(c.read8(c.s_.r[rm(op)])); }
    static void movw_load(Sh4& c, u32 op) { c.s_.r[rn(op)] = sx16(c.read16(c.s_.r[rm(op)])); }
    static void movl_load(Sh4& c, u32 op) { c.s_.r[rn(op)] = c.read32(c.s_.r[rm(op)]); }
    // MOV.x Rm,@-Rn (the value stored is Rm before the decrement)
    static void movb_predec(Sh4& c, u32 op) {
        const u32 a = c.s_.r[rn(op)] - 1;
        c.write8(a, static_cast<u8>(c.s_.r[rm(op)]));
        c.s_.r[rn(op)] = a;
    }
    static void movw_predec(Sh4& c, u32 op) {
        const u32 a = c.s_.r[rn(op)] - 2;
        c.write16(a, static_cast<u16>(c.s_.r[rm(op)]));
        c.s_.r[rn(op)] = a;
    }
    static void movl_predec(Sh4& c, u32 op) {
        const u32 a = c.s_.r[rn(op)] - 4;
        c.write32(a, c.s_.r[rm(op)]);
        c.s_.r[rn(op)] = a;
    }
    // MOV.x @Rm+,Rn (when m == n the loaded value wins)
    static void movb_postinc(Sh4& c, u32 op) {
        const u32 v = sx8(c.read8(c.s_.r[rm(op)]));
        if (rn(op) != rm(op))
            c.s_.r[rm(op)] += 1;
        c.s_.r[rn(op)] = v;
    }
    static void movw_postinc(Sh4& c, u32 op) {
        const u32 v = sx16(c.read16(c.s_.r[rm(op)]));
        if (rn(op) != rm(op))
            c.s_.r[rm(op)] += 2;
        c.s_.r[rn(op)] = v;
    }
    static void movl_postinc(Sh4& c, u32 op) {
        const u32 v = c.read32(c.s_.r[rm(op)]);
        if (rn(op) != rm(op))
            c.s_.r[rm(op)] += 4;
        c.s_.r[rn(op)] = v;
    }
    // MOV.x Rm,@(R0,Rn) / MOV.x @(R0,Rm),Rn
    static void movb_store_r0(Sh4& c, u32 op) {
        c.write8(c.s_.r[rn(op)] + c.s_.r[0], static_cast<u8>(c.s_.r[rm(op)]));
    }
    static void movw_store_r0(Sh4& c, u32 op) {
        c.write16(c.s_.r[rn(op)] + c.s_.r[0], static_cast<u16>(c.s_.r[rm(op)]));
    }
    static void movl_store_r0(Sh4& c, u32 op) { c.write32(c.s_.r[rn(op)] + c.s_.r[0], c.s_.r[rm(op)]); }
    static void movb_load_r0(Sh4& c, u32 op) { c.s_.r[rn(op)] = sx8(c.read8(c.s_.r[rm(op)] + c.s_.r[0])); }
    static void movw_load_r0(Sh4& c, u32 op) { c.s_.r[rn(op)] = sx16(c.read16(c.s_.r[rm(op)] + c.s_.r[0])); }
    static void movl_load_r0(Sh4& c, u32 op) { c.s_.r[rn(op)] = c.read32(c.s_.r[rm(op)] + c.s_.r[0]); }
    // Displacement forms
    static void movb_store_disp(Sh4& c, u32 op) {
        c.write8(c.s_.r[rm(op)] + disp4(op), static_cast<u8>(c.s_.r[0]));
    }
    static void movw_store_disp(Sh4& c, u32 op) {
        c.write16(c.s_.r[rm(op)] + disp4(op) * 2, static_cast<u16>(c.s_.r[0]));
    }
    static void movl_store_disp(Sh4& c, u32 op) { c.write32(c.s_.r[rn(op)] + disp4(op) * 4, c.s_.r[rm(op)]); }
    static void movb_load_disp(Sh4& c, u32 op) { c.s_.r[0] = sx8(c.read8(c.s_.r[rm(op)] + disp4(op))); }
    static void movw_load_disp(Sh4& c, u32 op) { c.s_.r[0] = sx16(c.read16(c.s_.r[rm(op)] + disp4(op) * 2)); }
    static void movl_load_disp(Sh4& c, u32 op) { c.s_.r[rn(op)] = c.read32(c.s_.r[rm(op)] + disp4(op) * 4); }
    // GBR forms
    static void movb_store_gbr(Sh4& c, u32 op) { c.write8(c.s_.gbr + imm8(op), static_cast<u8>(c.s_.r[0])); }
    static void movw_store_gbr(Sh4& c, u32 op) {
        c.write16(c.s_.gbr + imm8(op) * 2, static_cast<u16>(c.s_.r[0]));
    }
    static void movl_store_gbr(Sh4& c, u32 op) { c.write32(c.s_.gbr + imm8(op) * 4, c.s_.r[0]); }
    static void movb_load_gbr(Sh4& c, u32 op) { c.s_.r[0] = sx8(c.read8(c.s_.gbr + imm8(op))); }
    static void movw_load_gbr(Sh4& c, u32 op) { c.s_.r[0] = sx16(c.read16(c.s_.gbr + imm8(op) * 2)); }
    static void movl_load_gbr(Sh4& c, u32 op) { c.s_.r[0] = c.read32(c.s_.gbr + imm8(op) * 4); }

    static void movt(Sh4& c, u32 op) { c.s_.r[rn(op)] = t(c) ? 1 : 0; }
    static void swapb(Sh4& c, u32 op) {
        const u32 v = c.s_.r[rm(op)];
        c.s_.r[rn(op)] = (v & 0xFFFF0000) | ((v & 0xFF) << 8) | ((v >> 8) & 0xFF);
    }
    static void swapw(Sh4& c, u32 op) {
        const u32 v = c.s_.r[rm(op)];
        c.s_.r[rn(op)] = (v << 16) | (v >> 16);
    }
    static void xtrct(Sh4& c, u32 op) {
        c.s_.r[rn(op)] = (c.s_.r[rm(op)] << 16) | (c.s_.r[rn(op)] >> 16);
    }

    // ----- arithmetic --------------------------------------------------------
    static void add(Sh4& c, u32 op) { c.s_.r[rn(op)] += c.s_.r[rm(op)]; }
    static void add_imm(Sh4& c, u32 op) { c.s_.r[rn(op)] += simm8(op); }
    static void addc(Sh4& c, u32 op) {
        const u32 a = c.s_.r[rn(op)];
        const u32 sum = a + c.s_.r[rm(op)];
        const u32 result = sum + (t(c) ? 1 : 0);
        set_t(c, a > sum || sum > result);
        c.s_.r[rn(op)] = result;
    }
    static void addv(Sh4& c, u32 op) {
        const u32 a = c.s_.r[rn(op)], b = c.s_.r[rm(op)];
        const u32 result = a + b;
        set_t(c, ((~(a ^ b) & (a ^ result)) >> 31) != 0);
        c.s_.r[rn(op)] = result;
    }
    static void sub(Sh4& c, u32 op) { c.s_.r[rn(op)] -= c.s_.r[rm(op)]; }
    static void subc(Sh4& c, u32 op) {
        const u32 a = c.s_.r[rn(op)];
        const u32 diff = a - c.s_.r[rm(op)];
        const u32 result = diff - (t(c) ? 1 : 0);
        set_t(c, a < diff || diff < result);
        c.s_.r[rn(op)] = result;
    }
    static void subv(Sh4& c, u32 op) {
        const u32 a = c.s_.r[rn(op)], b = c.s_.r[rm(op)];
        const u32 result = a - b;
        set_t(c, (((a ^ b) & (a ^ result)) >> 31) != 0);
        c.s_.r[rn(op)] = result;
    }
    static void neg(Sh4& c, u32 op) { c.s_.r[rn(op)] = 0u - c.s_.r[rm(op)]; }
    static void negc(Sh4& c, u32 op) {
        const u32 tmp = 0u - c.s_.r[rm(op)];
        const u32 result = tmp - (t(c) ? 1 : 0);
        set_t(c, tmp != 0 || tmp < result);
        c.s_.r[rn(op)] = result;
    }
    static void dt(Sh4& c, u32 op) {
        c.s_.r[rn(op)] -= 1;
        set_t(c, c.s_.r[rn(op)] == 0);
    }
    static void exts_b(Sh4& c, u32 op) { c.s_.r[rn(op)] = sx8(static_cast<u8>(c.s_.r[rm(op)])); }
    static void exts_w(Sh4& c, u32 op) { c.s_.r[rn(op)] = sx16(static_cast<u16>(c.s_.r[rm(op)])); }
    static void extu_b(Sh4& c, u32 op) { c.s_.r[rn(op)] = c.s_.r[rm(op)] & 0xFF; }
    static void extu_w(Sh4& c, u32 op) { c.s_.r[rn(op)] = c.s_.r[rm(op)] & 0xFFFF; }

    static void cmp_eq_imm(Sh4& c, u32 op) { set_t(c, c.s_.r[0] == simm8(op)); }
    static void cmp_eq(Sh4& c, u32 op) { set_t(c, c.s_.r[rn(op)] == c.s_.r[rm(op)]); }
    static void cmp_hs(Sh4& c, u32 op) { set_t(c, c.s_.r[rn(op)] >= c.s_.r[rm(op)]); }
    static void cmp_ge(Sh4& c, u32 op) {
        set_t(c, static_cast<s32>(c.s_.r[rn(op)]) >= static_cast<s32>(c.s_.r[rm(op)]));
    }
    static void cmp_hi(Sh4& c, u32 op) { set_t(c, c.s_.r[rn(op)] > c.s_.r[rm(op)]); }
    static void cmp_gt(Sh4& c, u32 op) {
        set_t(c, static_cast<s32>(c.s_.r[rn(op)]) > static_cast<s32>(c.s_.r[rm(op)]));
    }
    static void cmp_pz(Sh4& c, u32 op) { set_t(c, static_cast<s32>(c.s_.r[rn(op)]) >= 0); }
    static void cmp_pl(Sh4& c, u32 op) { set_t(c, static_cast<s32>(c.s_.r[rn(op)]) > 0); }
    static void cmp_str(Sh4& c, u32 op) {
        const u32 x = c.s_.r[rn(op)] ^ c.s_.r[rm(op)];
        set_t(c, (x & 0xFF000000) == 0 || (x & 0x00FF0000) == 0 || (x & 0x0000FF00) == 0 || (x & 0xFF) == 0);
    }

    static void div0u(Sh4& c, u32) {
        c.s_.sr &= ~(sr::M | sr::Q | sr::T);
    }
    static void div0s(Sh4& c, u32 op) {
        const bool q = (c.s_.r[rn(op)] >> 31) != 0;
        const bool m = (c.s_.r[rm(op)] >> 31) != 0;
        set_flag(c, sr::Q, q);
        set_flag(c, sr::M, m);
        set_t(c, q != m);
    }
    static void div1(Sh4& c, u32 op) {
        u32& n = c.s_.r[rn(op)];
        const u32 divisor = c.s_.r[rm(op)];
        const bool old_q = flag(c, sr::Q);
        const bool m = flag(c, sr::M);
        bool q = (n >> 31) != 0;
        n = (n << 1) | (t(c) ? 1 : 0);
        const u32 before = n;
        bool carry;
        if (old_q == m) {
            n -= divisor;
            carry = n > before; // borrow
        } else {
            n += divisor;
            carry = n < before; // carry
        }
        // The manual's 8-case table reduces to: M=0 -> Q = q ^ carry,
        // M=1 -> Q = !(q ^ carry). Old Q only chooses add or subtract above.
        q = q ^ carry ^ m;
        set_flag(c, sr::Q, q);
        set_t(c, q == m);
    }

    static void dmuls(Sh4& c, u32 op) {
        const s64 p = static_cast<s64>(static_cast<s32>(c.s_.r[rn(op)])) * static_cast<s32>(c.s_.r[rm(op)]);
        c.s_.mach = static_cast<u32>(static_cast<u64>(p) >> 32);
        c.s_.macl = static_cast<u32>(p);
    }
    static void dmulu(Sh4& c, u32 op) {
        const u64 p = static_cast<u64>(c.s_.r[rn(op)]) * c.s_.r[rm(op)];
        c.s_.mach = static_cast<u32>(p >> 32);
        c.s_.macl = static_cast<u32>(p);
    }
    static void mull(Sh4& c, u32 op) { c.s_.macl = c.s_.r[rn(op)] * c.s_.r[rm(op)]; }
    static void mulsw(Sh4& c, u32 op) {
        c.s_.macl = static_cast<u32>(static_cast<s32>(static_cast<s16>(c.s_.r[rn(op)])) *
                                     static_cast<s32>(static_cast<s16>(c.s_.r[rm(op)])));
    }
    static void muluw(Sh4& c, u32 op) { c.s_.macl = (c.s_.r[rn(op)] & 0xFFFF) * (c.s_.r[rm(op)] & 0xFFFF); }

    static void macl(Sh4& c, u32 op) {
        const u32 n = rn(op), m = rm(op);
        const u32 addr_n = c.s_.r[n];
        const u32 addr_m = (n == m) ? addr_n + 4 : c.s_.r[m];
        const s32 a = static_cast<s32>(c.read32(addr_n));
        const s32 b = static_cast<s32>(c.read32(addr_m));
        c.s_.r[n] += 4;
        c.s_.r[m] += 4;
        const s64 product = static_cast<s64>(a) * b;
        const u64 mac = (static_cast<u64>(c.s_.mach) << 32) | c.s_.macl;
        u64 result;
        if (flag(c, sr::S)) {
            // Saturating 48-bit accumulate.
            const s64 mac48 = static_cast<s64>(mac << 16) >> 16;
            s64 sum = mac48 + product;
            constexpr s64 kMax = (s64{1} << 47) - 1;
            constexpr s64 kMin = -(s64{1} << 47);
            sum = sum > kMax ? kMax : (sum < kMin ? kMin : sum);
            result = static_cast<u64>(sum);
        } else {
            result = mac + static_cast<u64>(product);
        }
        c.s_.mach = static_cast<u32>(result >> 32);
        c.s_.macl = static_cast<u32>(result);
    }
    static void macw(Sh4& c, u32 op) {
        const u32 n = rn(op), m = rm(op);
        const u32 addr_n = c.s_.r[n];
        const u32 addr_m = (n == m) ? addr_n + 2 : c.s_.r[m];
        const s16 a = static_cast<s16>(c.read16(addr_n));
        const s16 b = static_cast<s16>(c.read16(addr_m));
        c.s_.r[n] += 2;
        c.s_.r[m] += 2;
        const s64 product = static_cast<s64>(a) * b;
        if (flag(c, sr::S)) {
            // Saturating 32-bit accumulate into MACL.
            s64 sum = static_cast<s64>(static_cast<s32>(c.s_.macl)) + product;
            sum = sum > INT32_MAX ? INT32_MAX : (sum < INT32_MIN ? INT32_MIN : sum);
            c.s_.macl = static_cast<u32>(static_cast<s32>(sum));
        } else {
            const u64 mac = ((static_cast<u64>(c.s_.mach) << 32) | c.s_.macl) + static_cast<u64>(product);
            c.s_.mach = static_cast<u32>(mac >> 32);
            c.s_.macl = static_cast<u32>(mac);
        }
    }

    // ----- logic -------------------------------------------------------------
    static void and_(Sh4& c, u32 op) { c.s_.r[rn(op)] &= c.s_.r[rm(op)]; }
    static void or_(Sh4& c, u32 op) { c.s_.r[rn(op)] |= c.s_.r[rm(op)]; }
    static void xor_(Sh4& c, u32 op) { c.s_.r[rn(op)] ^= c.s_.r[rm(op)]; }
    static void not_(Sh4& c, u32 op) { c.s_.r[rn(op)] = ~c.s_.r[rm(op)]; }
    static void tst(Sh4& c, u32 op) { set_t(c, (c.s_.r[rn(op)] & c.s_.r[rm(op)]) == 0); }
    static void and_imm(Sh4& c, u32 op) { c.s_.r[0] &= imm8(op); }
    static void or_imm(Sh4& c, u32 op) { c.s_.r[0] |= imm8(op); }
    static void xor_imm(Sh4& c, u32 op) { c.s_.r[0] ^= imm8(op); }
    static void tst_imm(Sh4& c, u32 op) { set_t(c, (c.s_.r[0] & imm8(op)) == 0); }
    static void tstb_gbr(Sh4& c, u32 op) { set_t(c, (c.read8(c.s_.gbr + c.s_.r[0]) & imm8(op)) == 0); }
    static void andb_gbr(Sh4& c, u32 op) {
        const u32 a = c.s_.gbr + c.s_.r[0];
        c.write8(a, static_cast<u8>(c.read8(a) & imm8(op)));
    }
    static void orb_gbr(Sh4& c, u32 op) {
        const u32 a = c.s_.gbr + c.s_.r[0];
        c.write8(a, static_cast<u8>(c.read8(a) | imm8(op)));
    }
    static void xorb_gbr(Sh4& c, u32 op) {
        const u32 a = c.s_.gbr + c.s_.r[0];
        c.write8(a, static_cast<u8>(c.read8(a) ^ imm8(op)));
    }
    static void tas(Sh4& c, u32 op) {
        const u32 a = c.s_.r[rn(op)];
        const u8 v = c.read8(a);
        c.write8(a, static_cast<u8>(v | 0x80));
        set_t(c, v == 0);
    }

    // ----- shifts ------------------------------------------------------------
    static void shll(Sh4& c, u32 op) {
        u32& r = c.s_.r[rn(op)];
        set_t(c, (r >> 31) != 0);
        r <<= 1;
    }
    static void shlr(Sh4& c, u32 op) {
        u32& r = c.s_.r[rn(op)];
        set_t(c, (r & 1) != 0);
        r >>= 1;
    }
    static void shar(Sh4& c, u32 op) {
        u32& r = c.s_.r[rn(op)];
        set_t(c, (r & 1) != 0);
        r = static_cast<u32>(static_cast<s32>(r) >> 1);
    }
    static void rotl(Sh4& c, u32 op) {
        u32& r = c.s_.r[rn(op)];
        set_t(c, (r >> 31) != 0);
        r = std::rotl(r, 1);
    }
    static void rotr(Sh4& c, u32 op) {
        u32& r = c.s_.r[rn(op)];
        set_t(c, (r & 1) != 0);
        r = std::rotr(r, 1);
    }
    static void rotcl(Sh4& c, u32 op) {
        u32& r = c.s_.r[rn(op)];
        const bool out = (r >> 31) != 0;
        r = (r << 1) | (t(c) ? 1 : 0);
        set_t(c, out);
    }
    static void rotcr(Sh4& c, u32 op) {
        u32& r = c.s_.r[rn(op)];
        const bool out = (r & 1) != 0;
        r = (r >> 1) | (t(c) ? 0x80000000u : 0);
        set_t(c, out);
    }
    static void shll2(Sh4& c, u32 op) { c.s_.r[rn(op)] <<= 2; }
    static void shll8(Sh4& c, u32 op) { c.s_.r[rn(op)] <<= 8; }
    static void shll16(Sh4& c, u32 op) { c.s_.r[rn(op)] <<= 16; }
    static void shlr2(Sh4& c, u32 op) { c.s_.r[rn(op)] >>= 2; }
    static void shlr8(Sh4& c, u32 op) { c.s_.r[rn(op)] >>= 8; }
    static void shlr16(Sh4& c, u32 op) { c.s_.r[rn(op)] >>= 16; }
    static void shad(Sh4& c, u32 op) {
        u32& r = c.s_.r[rn(op)];
        const s32 shift = static_cast<s32>(c.s_.r[rm(op)]);
        if (shift >= 0)
            r <<= (shift & 0x1F);
        else if ((shift & 0x1F) == 0)
            r = (static_cast<s32>(r) < 0) ? 0xFFFFFFFF : 0;
        else
            r = static_cast<u32>(static_cast<s32>(r) >> ((~shift & 0x1F) + 1));
    }
    static void shld(Sh4& c, u32 op) {
        u32& r = c.s_.r[rn(op)];
        const s32 shift = static_cast<s32>(c.s_.r[rm(op)]);
        if (shift >= 0)
            r <<= (shift & 0x1F);
        else if ((shift & 0x1F) == 0)
            r = 0;
        else
            r >>= ((~shift & 0x1F) + 1);
    }

    // ----- branches ----------------------------------------------------------
    static u32 disp8_target(const Sh4& c, u32 op) { return c.s_.pc + 4 + (simm8(op) << 1); }
    static u32 disp12_target(const Sh4& c, u32 op) {
        const s32 d = static_cast<s32>((op & 0xFFF) << 20) >> 20;
        return c.s_.pc + 4 + static_cast<u32>(d * 2);
    }
    static void bt(Sh4& c, u32 op) {
        if (t(c)) {
            c.s_.pc = disp8_target(c, op);
            c.pc_written_ = true;
        }
    }
    static void bf(Sh4& c, u32 op) {
        if (!t(c)) {
            c.s_.pc = disp8_target(c, op);
            c.pc_written_ = true;
        }
    }
    static void bts(Sh4& c, u32 op) {
        if (t(c))
            c.delayed_branch(disp8_target(c, op));
    }
    static void bfs(Sh4& c, u32 op) {
        if (!t(c))
            c.delayed_branch(disp8_target(c, op));
    }
    static void bra(Sh4& c, u32 op) { c.delayed_branch(disp12_target(c, op)); }
    static void bsr(Sh4& c, u32 op) {
        const u32 target = disp12_target(c, op);
        c.s_.pr = c.s_.pc + 4;
        c.delayed_branch(target);
    }
    static void braf(Sh4& c, u32 op) { c.delayed_branch(c.s_.pc + 4 + c.s_.r[rn(op)]); }
    static void bsrf(Sh4& c, u32 op) {
        const u32 target = c.s_.pc + 4 + c.s_.r[rn(op)];
        c.s_.pr = c.s_.pc + 4;
        c.delayed_branch(target);
    }
    static void jmp(Sh4& c, u32 op) { c.delayed_branch(c.s_.r[rn(op)]); }
    static void jsr(Sh4& c, u32 op) {
        const u32 target = c.s_.r[rn(op)];
        c.s_.pr = c.s_.pc + 4;
        c.delayed_branch(target);
    }
    static void rts(Sh4& c, u32) { c.delayed_branch(c.s_.pr); }
    static void rte(Sh4& c, u32) {
        c.check_privileged();
        const u32 target = c.s_.spc;
        c.set_sr(c.s_.ssr);
        c.delayed_branch(target);
    }
    static void trapa(Sh4& c, u32 op) {
        c.tra_ = imm8(op) << 2;
        c.enter_exception(exception_code::kTrap, c.s_.pc + 2, 0x100);
        c.pc_written_ = true;
    }

    // ----- system control ----------------------------------------------------
    static void nop(Sh4&, u32) {}
    static void clrt(Sh4& c, u32) { set_t(c, false); }
    static void sett(Sh4& c, u32) { set_t(c, true); }
    static void clrs(Sh4& c, u32) { set_flag(c, sr::S, false); }
    static void sets(Sh4& c, u32) { set_flag(c, sr::S, true); }
    static void clrmac(Sh4& c, u32) { c.s_.mach = c.s_.macl = 0; }
    static void sleep(Sh4& c, u32) {
        c.check_privileged();
        c.sleeping_ = true;
    }
    static void ldtlb(Sh4& c, u32) {
        c.check_privileged();
        static std::atomic<bool> warned{false};
        warn_once(warned, "LDTLB executed: the MMU is not implemented, instruction ignored");
    }
    static void pref(Sh4& c, u32 op) {
        const u32 a = c.s_.r[rn(op)];
        if ((a & 0xFC000000) == 0xE0000000) {
            if (!c.privileged() && (c.mmucr_ & (1u << 9)))
                c.raise_address_error(a, false);
            c.flush_store_queue(a);
        }
    }
    static void cache_op(Sh4&, u32) {} // OCBI / OCBP / OCBWB: caches are not emulated
    static void movca(Sh4& c, u32 op) { c.write32(c.s_.r[rn(op)], c.s_.r[0]); }

    // STC / LDC (register forms)
    static void stc_sr(Sh4& c, u32 op) {
        c.check_privileged();
        c.s_.r[rn(op)] = c.s_.sr;
    }
    static void stc_gbr(Sh4& c, u32 op) { c.s_.r[rn(op)] = c.s_.gbr; }
    static void stc_vbr(Sh4& c, u32 op) {
        c.check_privileged();
        c.s_.r[rn(op)] = c.s_.vbr;
    }
    static void stc_ssr(Sh4& c, u32 op) {
        c.check_privileged();
        c.s_.r[rn(op)] = c.s_.ssr;
    }
    static void stc_spc(Sh4& c, u32 op) {
        c.check_privileged();
        c.s_.r[rn(op)] = c.s_.spc;
    }
    static void stc_sgr(Sh4& c, u32 op) {
        c.check_privileged();
        c.s_.r[rn(op)] = c.s_.sgr;
    }
    static void stc_dbr(Sh4& c, u32 op) {
        c.check_privileged();
        c.s_.r[rn(op)] = c.s_.dbr;
    }
    static void stc_bank(Sh4& c, u32 op) {
        c.check_privileged();
        c.s_.r[rn(op)] = c.s_.r_bank[(op >> 4) & 7];
    }
    static void ldc_sr(Sh4& c, u32 op) {
        c.check_privileged();
        c.set_sr(c.s_.r[rn(op)]);
    }
    static void ldc_gbr(Sh4& c, u32 op) { c.s_.gbr = c.s_.r[rn(op)]; }
    static void ldc_vbr(Sh4& c, u32 op) {
        c.check_privileged();
        c.s_.vbr = c.s_.r[rn(op)];
    }
    static void ldc_ssr(Sh4& c, u32 op) {
        c.check_privileged();
        c.s_.ssr = c.s_.r[rn(op)];
    }
    static void ldc_spc(Sh4& c, u32 op) {
        c.check_privileged();
        c.s_.spc = c.s_.r[rn(op)];
    }
    static void ldc_dbr(Sh4& c, u32 op) {
        c.check_privileged();
        c.s_.dbr = c.s_.r[rn(op)];
    }
    static void ldc_bank(Sh4& c, u32 op) {
        c.check_privileged();
        c.s_.r_bank[(op >> 4) & 7] = c.s_.r[rn(op)];
    }

    // STC.L / LDC.L (memory forms). The store happens before Rn changes, and
    // the load before Rm changes, so a faulting access leaves registers intact.
    static void push(Sh4& c, u32 op, u32 value) {
        const u32 a = c.s_.r[rn(op)] - 4;
        c.write32(a, value);
        c.s_.r[rn(op)] = a;
    }
    static u32 pop(Sh4& c, u32 op) {
        const u32 v = c.read32(c.s_.r[rn(op)]);
        c.s_.r[rn(op)] += 4;
        return v;
    }
    static void stcl_sr(Sh4& c, u32 op) {
        c.check_privileged();
        push(c, op, c.s_.sr);
    }
    static void stcl_gbr(Sh4& c, u32 op) { push(c, op, c.s_.gbr); }
    static void stcl_vbr(Sh4& c, u32 op) {
        c.check_privileged();
        push(c, op, c.s_.vbr);
    }
    static void stcl_ssr(Sh4& c, u32 op) {
        c.check_privileged();
        push(c, op, c.s_.ssr);
    }
    static void stcl_spc(Sh4& c, u32 op) {
        c.check_privileged();
        push(c, op, c.s_.spc);
    }
    static void stcl_sgr(Sh4& c, u32 op) {
        c.check_privileged();
        push(c, op, c.s_.sgr);
    }
    static void stcl_dbr(Sh4& c, u32 op) {
        c.check_privileged();
        push(c, op, c.s_.dbr);
    }
    static void stcl_bank(Sh4& c, u32 op) {
        c.check_privileged();
        push(c, op, c.s_.r_bank[(op >> 4) & 7]);
    }
    static void ldcl_sr(Sh4& c, u32 op) {
        c.check_privileged();
        c.set_sr(pop(c, op));
    }
    static void ldcl_gbr(Sh4& c, u32 op) { c.s_.gbr = pop(c, op); }
    static void ldcl_vbr(Sh4& c, u32 op) {
        c.check_privileged();
        c.s_.vbr = pop(c, op);
    }
    static void ldcl_ssr(Sh4& c, u32 op) {
        c.check_privileged();
        c.s_.ssr = pop(c, op);
    }
    static void ldcl_spc(Sh4& c, u32 op) {
        c.check_privileged();
        c.s_.spc = pop(c, op);
    }
    static void ldcl_dbr(Sh4& c, u32 op) {
        c.check_privileged();
        c.s_.dbr = pop(c, op);
    }
    static void ldcl_bank(Sh4& c, u32 op) {
        c.check_privileged();
        const u32 v = pop(c, op);
        c.s_.r_bank[(op >> 4) & 7] = v;
    }

    // STS / LDS
    static void sts_mach(Sh4& c, u32 op) { c.s_.r[rn(op)] = c.s_.mach; }
    static void sts_macl(Sh4& c, u32 op) { c.s_.r[rn(op)] = c.s_.macl; }
    static void sts_pr(Sh4& c, u32 op) { c.s_.r[rn(op)] = c.s_.pr; }
    static void lds_mach(Sh4& c, u32 op) { c.s_.mach = c.s_.r[rn(op)]; }
    static void lds_macl(Sh4& c, u32 op) { c.s_.macl = c.s_.r[rn(op)]; }
    static void lds_pr(Sh4& c, u32 op) { c.s_.pr = c.s_.r[rn(op)]; }
    static void stsl_mach(Sh4& c, u32 op) { push(c, op, c.s_.mach); }
    static void stsl_macl(Sh4& c, u32 op) { push(c, op, c.s_.macl); }
    static void stsl_pr(Sh4& c, u32 op) { push(c, op, c.s_.pr); }
    static void ldsl_mach(Sh4& c, u32 op) { c.s_.mach = pop(c, op); }
    static void ldsl_macl(Sh4& c, u32 op) { c.s_.macl = pop(c, op); }
    static void ldsl_pr(Sh4& c, u32 op) { c.s_.pr = pop(c, op); }

    static void sts_fpul(Sh4& c, u32 op) {
        c.check_fpu();
        c.s_.r[rn(op)] = c.s_.fpul;
    }
    static void sts_fpscr(Sh4& c, u32 op) {
        c.check_fpu();
        c.s_.r[rn(op)] = c.s_.fpscr;
    }
    static void lds_fpul(Sh4& c, u32 op) {
        c.check_fpu();
        c.s_.fpul = c.s_.r[rn(op)];
    }
    static void lds_fpscr(Sh4& c, u32 op) {
        c.check_fpu();
        c.set_fpscr(c.s_.r[rn(op)]);
    }
    static void stsl_fpul(Sh4& c, u32 op) {
        c.check_fpu();
        push(c, op, c.s_.fpul);
    }
    static void stsl_fpscr(Sh4& c, u32 op) {
        c.check_fpu();
        push(c, op, c.s_.fpscr);
    }
    static void ldsl_fpul(Sh4& c, u32 op) {
        c.check_fpu();
        c.s_.fpul = pop(c, op);
    }
    static void ldsl_fpscr(Sh4& c, u32 op) {
        c.check_fpu();
        c.set_fpscr(pop(c, op));
    }

    // ----- floating point ----------------------------------------------------
    static constexpr u32 kQuietNan32 = 0x7FBFFFFF;
    static constexpr u64 kQuietNan64 = 0x7FF7FFFFFFFFFFFFull;

    static bool pr(const Sh4& c) { return (c.s_.fpscr & fpscr::PR) != 0; }
    static bool sz(const Sh4& c) { return (c.s_.fpscr & fpscr::SZ) != 0; }
    static bool dn(const Sh4& c) { return (c.s_.fpscr & fpscr::DN) != 0; }

    static float flush(const Sh4& c, float v) {
        return (dn(c) && std::fpclassify(v) == FP_SUBNORMAL) ? std::copysign(0.0f, v) : v;
    }
    static double flush(const Sh4& c, double v) {
        return (dn(c) && std::fpclassify(v) == FP_SUBNORMAL) ? std::copysign(0.0, v) : v;
    }
    static float fr(const Sh4& c, u32 i) { return flush(c, std::bit_cast<float>(c.s_.fr[i])); }
    static void set_fr(Sh4& c, u32 i, float v) {
        c.s_.fr[i] = std::isnan(v) ? kQuietNan32 : std::bit_cast<u32>(flush(c, v));
    }
    static double dr(const Sh4& c, u32 i) {
        const u64 bits = (static_cast<u64>(c.s_.fr[i & 0xE]) << 32) | c.s_.fr[(i & 0xE) + 1];
        return flush(c, std::bit_cast<double>(bits));
    }
    static void set_dr(Sh4& c, u32 i, double v) {
        const u64 bits = std::isnan(v) ? kQuietNan64 : std::bit_cast<u64>(flush(c, v));
        c.s_.fr[i & 0xE] = static_cast<u32>(bits >> 32);
        c.s_.fr[(i & 0xE) + 1] = static_cast<u32>(bits);
    }

    // With FPSCR.SZ=1, an even register number names DRn, an odd one XDn-1.
    static u32* pair(Sh4& c, u32 reg) { return (reg & 1) ? &c.s_.xf[reg & 0xE] : &c.s_.fr[reg & 0xE]; }

    static void check_align8(Sh4& c, u32 address, bool is_write) {
        if (address & 7)
            c.raise_address_error(address, is_write);
    }
    static void load_pair(Sh4& c, u32 reg, u32 address) {
        check_align8(c, address, false);
        const u32 lo = c.read32(address);
        const u32 hi = c.read32(address + 4);
        u32* p = pair(c, reg);
        p[0] = lo;
        p[1] = hi;
    }
    static void store_pair(Sh4& c, u32 reg, u32 address) {
        check_align8(c, address, true);
        const u32* p = pair(c, reg);
        c.write32(address, p[0]);
        c.write32(address + 4, p[1]);
    }

    static void fmov(Sh4& c, u32 op) {
        c.check_fpu();
        if (sz(c)) {
            const u32* src = pair(c, rm(op));
            u32* dst = pair(c, rn(op));
            const u32 a = src[0], b = src[1];
            dst[0] = a;
            dst[1] = b;
        } else {
            c.s_.fr[rn(op)] = c.s_.fr[rm(op)];
        }
    }
    static void fmov_load(Sh4& c, u32 op) { // FMOV @Rm,FRn
        c.check_fpu();
        if (sz(c))
            load_pair(c, rn(op), c.s_.r[rm(op)]);
        else
            c.s_.fr[rn(op)] = c.read32(c.s_.r[rm(op)]);
    }
    static void fmov_store(Sh4& c, u32 op) { // FMOV FRm,@Rn
        c.check_fpu();
        if (sz(c))
            store_pair(c, rm(op), c.s_.r[rn(op)]);
        else
            c.write32(c.s_.r[rn(op)], c.s_.fr[rm(op)]);
    }
    static void fmov_postinc(Sh4& c, u32 op) { // FMOV @Rm+,FRn
        c.check_fpu();
        if (sz(c)) {
            load_pair(c, rn(op), c.s_.r[rm(op)]);
            c.s_.r[rm(op)] += 8;
        } else {
            c.s_.fr[rn(op)] = c.read32(c.s_.r[rm(op)]);
            c.s_.r[rm(op)] += 4;
        }
    }
    static void fmov_predec(Sh4& c, u32 op) { // FMOV FRm,@-Rn
        c.check_fpu();
        if (sz(c)) {
            const u32 a = c.s_.r[rn(op)] - 8;
            store_pair(c, rm(op), a);
            c.s_.r[rn(op)] = a;
        } else {
            const u32 a = c.s_.r[rn(op)] - 4;
            c.write32(a, c.s_.fr[rm(op)]);
            c.s_.r[rn(op)] = a;
        }
    }
    static void fmov_load_r0(Sh4& c, u32 op) { // FMOV @(R0,Rm),FRn
        c.check_fpu();
        const u32 a = c.s_.r[0] + c.s_.r[rm(op)];
        if (sz(c))
            load_pair(c, rn(op), a);
        else
            c.s_.fr[rn(op)] = c.read32(a);
    }
    static void fmov_store_r0(Sh4& c, u32 op) { // FMOV FRm,@(R0,Rn)
        c.check_fpu();
        const u32 a = c.s_.r[0] + c.s_.r[rn(op)];
        if (sz(c))
            store_pair(c, rm(op), a);
        else
            c.write32(a, c.s_.fr[rm(op)]);
    }

    static void fldi0(Sh4& c, u32 op) {
        c.check_fpu();
        if (!pr(c))
            c.s_.fr[rn(op)] = 0x00000000;
        else
            undefined_pr(c, "FLDI0");
    }
    static void fldi1(Sh4& c, u32 op) {
        c.check_fpu();
        if (!pr(c))
            c.s_.fr[rn(op)] = 0x3F800000;
        else
            undefined_pr(c, "FLDI1");
    }
    static void flds(Sh4& c, u32 op) {
        c.check_fpu();
        c.s_.fpul = c.s_.fr[rn(op)];
    }
    static void fsts(Sh4& c, u32 op) {
        c.check_fpu();
        c.s_.fr[rn(op)] = c.s_.fpul;
    }
    static void fabs_(Sh4& c, u32 op) {
        c.check_fpu();
        c.s_.fr[pr(c) ? rn(op) & 0xE : rn(op)] &= 0x7FFFFFFF;
    }
    static void fneg(Sh4& c, u32 op) {
        c.check_fpu();
        c.s_.fr[pr(c) ? rn(op) & 0xE : rn(op)] ^= 0x80000000;
    }

    template <typename F, typename D>
    static void binary(Sh4& c, u32 op, F single_op, D double_op) {
        c.check_fpu();
        if (pr(c))
            set_dr(c, rn(op), double_op(dr(c, rn(op)), dr(c, rm(op))));
        else
            set_fr(c, rn(op), single_op(fr(c, rn(op)), fr(c, rm(op))));
    }
    static void fadd(Sh4& c, u32 op) {
        binary(c, op, [](float a, float b) { return a + b; }, [](double a, double b) { return a + b; });
    }
    static void fsub(Sh4& c, u32 op) {
        binary(c, op, [](float a, float b) { return a - b; }, [](double a, double b) { return a - b; });
    }
    static void fmul(Sh4& c, u32 op) {
        binary(c, op, [](float a, float b) { return a * b; }, [](double a, double b) { return a * b; });
    }
    static void fdiv(Sh4& c, u32 op) {
        binary(c, op, [](float a, float b) { return a / b; }, [](double a, double b) { return a / b; });
    }
    static void fcmp_eq(Sh4& c, u32 op) {
        c.check_fpu();
        set_t(c, pr(c) ? dr(c, rn(op)) == dr(c, rm(op)) : fr(c, rn(op)) == fr(c, rm(op)));
    }
    static void fcmp_gt(Sh4& c, u32 op) {
        c.check_fpu();
        set_t(c, pr(c) ? dr(c, rn(op)) > dr(c, rm(op)) : fr(c, rn(op)) > fr(c, rm(op)));
    }
    static void fmac(Sh4& c, u32 op) {
        c.check_fpu();
        if (pr(c)) {
            undefined_pr(c, "FMAC");
            return;
        }
        const double result = static_cast<double>(fr(c, 0)) * fr(c, rm(op)) + fr(c, rn(op));
        set_fr(c, rn(op), static_cast<float>(result));
    }
    static void fsqrt(Sh4& c, u32 op) {
        c.check_fpu();
        if (pr(c))
            set_dr(c, rn(op), std::sqrt(dr(c, rn(op))));
        else
            set_fr(c, rn(op), std::sqrt(fr(c, rn(op))));
    }
    static void fsrra(Sh4& c, u32 op) {
        c.check_fpu();
        if (pr(c)) {
            undefined_pr(c, "FSRRA");
            return;
        }
        set_fr(c, rn(op), 1.0f / std::sqrt(fr(c, rn(op))));
    }
    static void float_(Sh4& c, u32 op) {
        c.check_fpu();
        const s32 v = static_cast<s32>(c.s_.fpul);
        if (pr(c))
            set_dr(c, rn(op), static_cast<double>(v));
        else
            set_fr(c, rn(op), static_cast<float>(v));
    }
    static void ftrc(Sh4& c, u32 op) {
        c.check_fpu();
        const double v = pr(c) ? dr(c, rn(op)) : static_cast<double>(fr(c, rn(op)));
        s32 result;
        if (std::isnan(v) || v <= -2147483649.0)
            result = INT32_MIN;
        else if (v >= 2147483648.0)
            result = INT32_MAX;
        else
            result = static_cast<s32>(v); // truncates toward zero
        c.s_.fpul = static_cast<u32>(result);
    }
    static void fcnvsd(Sh4& c, u32 op) {
        c.check_fpu();
        if (!pr(c)) {
            undefined_pr(c, "FCNVSD with PR=0");
            return;
        }
        set_dr(c, rn(op), static_cast<double>(flush(c, std::bit_cast<float>(c.s_.fpul))));
    }
    static void fcnvds(Sh4& c, u32 op) {
        c.check_fpu();
        if (!pr(c)) {
            undefined_pr(c, "FCNVDS with PR=0");
            return;
        }
        const float f = flush(c, static_cast<float>(dr(c, rn(op))));
        c.s_.fpul = std::isnan(f) ? kQuietNan32 : std::bit_cast<u32>(f);
    }
    static void fipr(Sh4& c, u32 op) {
        c.check_fpu();
        if (pr(c)) {
            undefined_pr(c, "FIPR");
            return;
        }
        const u32 n = (op >> 8) & 0xC, m = (op >> 6) & 0xC;
        double sum = 0;
        for (u32 i = 0; i < 4; ++i)
            sum += static_cast<double>(fr(c, n + i)) * fr(c, m + i);
        set_fr(c, n + 3, static_cast<float>(sum));
    }
    static void ftrv(Sh4& c, u32 op) {
        c.check_fpu();
        if (pr(c)) {
            undefined_pr(c, "FTRV");
            return;
        }
        const u32 n = (op >> 8) & 0xC;
        const double v[4] = {fr(c, n), fr(c, n + 1), fr(c, n + 2), fr(c, n + 3)};
        float out[4];
        for (u32 i = 0; i < 4; ++i) {
            double sum = 0;
            for (u32 j = 0; j < 4; ++j)
                sum += static_cast<double>(flush(c, std::bit_cast<float>(c.s_.xf[i + 4 * j]))) * v[j];
            out[i] = static_cast<float>(sum);
        }
        for (u32 i = 0; i < 4; ++i)
            set_fr(c, n + i, out[i]);
    }
    static void fsca(Sh4& c, u32 op) {
        c.check_fpu();
        if (pr(c)) {
            undefined_pr(c, "FSCA");
            return;
        }
        const u32 n = (op >> 8) & 0xE;
        const double angle = static_cast<double>(c.s_.fpul & 0xFFFF) * (2.0 * std::numbers::pi / 65536.0);
        set_fr(c, n, static_cast<float>(std::sin(angle)));
        set_fr(c, n + 1, static_cast<float>(std::cos(angle)));
    }
    static void frchg(Sh4& c, u32) {
        c.check_fpu();
        if (pr(c)) {
            undefined_pr(c, "FRCHG");
            return;
        }
        c.set_fpscr(c.s_.fpscr ^ fpscr::FR);
    }
    static void fschg(Sh4& c, u32) {
        c.check_fpu();
        if (pr(c)) {
            undefined_pr(c, "FSCHG");
            return;
        }
        c.set_fpscr(c.s_.fpscr ^ fpscr::SZ);
    }

    static void undefined_pr(Sh4& c, const char* what) {
        static std::atomic<bool> warned{false};
        if (!warned.exchange(true)) {
            LOG_WARN("sh4", "%s at PC=%08X is undefined for the current FPSCR.PR; executed as no-op "
                            "(further cases not logged)",
                     what, c.s_.pc);
        }
    }

    static void illegal(Sh4& c, u32) { c.raise_illegal(); }
};

// ---------------------------------------------------------------------------
// Decode table
// ---------------------------------------------------------------------------

namespace {

enum : u8 { kSlotIllegal = 1 };

struct Pattern {
    const char* bits; // 16 characters, most significant first; '0'/'1' fixed, anything else is a field
    Handler handler;
    const char* mnemonic;
    u8 flags;
};

// clang-format off
const Pattern kPatterns[] = {
    // 0000
    {"0000nnnn00000010", &Interp::stc_sr,        "STC SR,Rn", 0},
    {"0000nnnn00010010", &Interp::stc_gbr,       "STC GBR,Rn", 0},
    {"0000nnnn00100010", &Interp::stc_vbr,       "STC VBR,Rn", 0},
    {"0000nnnn00110010", &Interp::stc_ssr,       "STC SSR,Rn", 0},
    {"0000nnnn01000010", &Interp::stc_spc,       "STC SPC,Rn", 0},
    {"0000nnnn1mmm0010", &Interp::stc_bank,      "STC Rm_BANK,Rn", 0},
    {"0000nnnn00111010", &Interp::stc_sgr,       "STC SGR,Rn", 0},
    {"0000nnnn11111010", &Interp::stc_dbr,       "STC DBR,Rn", 0},
    {"0000mmmm00000011", &Interp::bsrf,          "BSRF Rm", kSlotIllegal},
    {"0000mmmm00100011", &Interp::braf,          "BRAF Rm", kSlotIllegal},
    {"0000nnnn10000011", &Interp::pref,          "PREF @Rn", 0},
    {"0000nnnn10010011", &Interp::cache_op,      "OCBI @Rn", 0},
    {"0000nnnn10100011", &Interp::cache_op,      "OCBP @Rn", 0},
    {"0000nnnn10110011", &Interp::cache_op,      "OCBWB @Rn", 0},
    {"0000nnnn11000011", &Interp::movca,         "MOVCA.L R0,@Rn", 0},
    {"0000nnnnmmmm0100", &Interp::movb_store_r0, "MOV.B Rm,@(R0,Rn)", 0},
    {"0000nnnnmmmm0101", &Interp::movw_store_r0, "MOV.W Rm,@(R0,Rn)", 0},
    {"0000nnnnmmmm0110", &Interp::movl_store_r0, "MOV.L Rm,@(R0,Rn)", 0},
    {"0000nnnnmmmm0111", &Interp::mull,          "MUL.L Rm,Rn", 0},
    {"0000000000001000", &Interp::clrt,          "CLRT", 0},
    {"0000000000011000", &Interp::sett,          "SETT", 0},
    {"0000000000101000", &Interp::clrmac,        "CLRMAC", 0},
    {"0000000000111000", &Interp::ldtlb,         "LDTLB", 0},
    {"0000000001001000", &Interp::clrs,          "CLRS", 0},
    {"0000000001011000", &Interp::sets,          "SETS", 0},
    {"0000000000001001", &Interp::nop,           "NOP", 0},
    {"0000000000011001", &Interp::div0u,         "DIV0U", 0},
    {"0000nnnn00101001", &Interp::movt,          "MOVT Rn", 0},
    {"0000nnnn00001010", &Interp::sts_mach,      "STS MACH,Rn", 0},
    {"0000nnnn00011010", &Interp::sts_macl,      "STS MACL,Rn", 0},
    {"0000nnnn00101010", &Interp::sts_pr,        "STS PR,Rn", 0},
    {"0000nnnn01011010", &Interp::sts_fpul,      "STS FPUL,Rn", 0},
    {"0000nnnn01101010", &Interp::sts_fpscr,     "STS FPSCR,Rn", 0},
    {"0000000000001011", &Interp::rts,           "RTS", kSlotIllegal},
    {"0000000000011011", &Interp::sleep,         "SLEEP", 0},
    {"0000000000101011", &Interp::rte,           "RTE", kSlotIllegal},
    {"0000nnnnmmmm1100", &Interp::movb_load_r0,  "MOV.B @(R0,Rm),Rn", 0},
    {"0000nnnnmmmm1101", &Interp::movw_load_r0,  "MOV.W @(R0,Rm),Rn", 0},
    {"0000nnnnmmmm1110", &Interp::movl_load_r0,  "MOV.L @(R0,Rm),Rn", 0},
    {"0000nnnnmmmm1111", &Interp::macl,          "MAC.L @Rm+,@Rn+", 0},
    // 0001
    {"0001nnnnmmmmdddd", &Interp::movl_store_disp, "MOV.L Rm,@(disp,Rn)", 0},
    // 0010
    {"0010nnnnmmmm0000", &Interp::movb_store,    "MOV.B Rm,@Rn", 0},
    {"0010nnnnmmmm0001", &Interp::movw_store,    "MOV.W Rm,@Rn", 0},
    {"0010nnnnmmmm0010", &Interp::movl_store,    "MOV.L Rm,@Rn", 0},
    {"0010nnnnmmmm0100", &Interp::movb_predec,   "MOV.B Rm,@-Rn", 0},
    {"0010nnnnmmmm0101", &Interp::movw_predec,   "MOV.W Rm,@-Rn", 0},
    {"0010nnnnmmmm0110", &Interp::movl_predec,   "MOV.L Rm,@-Rn", 0},
    {"0010nnnnmmmm0111", &Interp::div0s,         "DIV0S Rm,Rn", 0},
    {"0010nnnnmmmm1000", &Interp::tst,           "TST Rm,Rn", 0},
    {"0010nnnnmmmm1001", &Interp::and_,          "AND Rm,Rn", 0},
    {"0010nnnnmmmm1010", &Interp::xor_,          "XOR Rm,Rn", 0},
    {"0010nnnnmmmm1011", &Interp::or_,           "OR Rm,Rn", 0},
    {"0010nnnnmmmm1100", &Interp::cmp_str,       "CMP/STR Rm,Rn", 0},
    {"0010nnnnmmmm1101", &Interp::xtrct,         "XTRCT Rm,Rn", 0},
    {"0010nnnnmmmm1110", &Interp::muluw,         "MULU.W Rm,Rn", 0},
    {"0010nnnnmmmm1111", &Interp::mulsw,         "MULS.W Rm,Rn", 0},
    // 0011
    {"0011nnnnmmmm0000", &Interp::cmp_eq,        "CMP/EQ Rm,Rn", 0},
    {"0011nnnnmmmm0010", &Interp::cmp_hs,        "CMP/HS Rm,Rn", 0},
    {"0011nnnnmmmm0011", &Interp::cmp_ge,        "CMP/GE Rm,Rn", 0},
    {"0011nnnnmmmm0100", &Interp::div1,          "DIV1 Rm,Rn", 0},
    {"0011nnnnmmmm0101", &Interp::dmulu,         "DMULU.L Rm,Rn", 0},
    {"0011nnnnmmmm0110", &Interp::cmp_hi,        "CMP/HI Rm,Rn", 0},
    {"0011nnnnmmmm0111", &Interp::cmp_gt,        "CMP/GT Rm,Rn", 0},
    {"0011nnnnmmmm1000", &Interp::sub,           "SUB Rm,Rn", 0},
    {"0011nnnnmmmm1010", &Interp::subc,          "SUBC Rm,Rn", 0},
    {"0011nnnnmmmm1011", &Interp::subv,          "SUBV Rm,Rn", 0},
    {"0011nnnnmmmm1100", &Interp::add,           "ADD Rm,Rn", 0},
    {"0011nnnnmmmm1101", &Interp::dmuls,         "DMULS.L Rm,Rn", 0},
    {"0011nnnnmmmm1110", &Interp::addc,          "ADDC Rm,Rn", 0},
    {"0011nnnnmmmm1111", &Interp::addv,          "ADDV Rm,Rn", 0},
    // 0100
    {"0100nnnn00000000", &Interp::shll,          "SHLL Rn", 0},
    {"0100nnnn00000001", &Interp::shlr,          "SHLR Rn", 0},
    {"0100nnnn00000010", &Interp::stsl_mach,     "STS.L MACH,@-Rn", 0},
    {"0100nnnn00000011", &Interp::stcl_sr,       "STC.L SR,@-Rn", 0},
    {"0100nnnn00000100", &Interp::rotl,          "ROTL Rn", 0},
    {"0100nnnn00000101", &Interp::rotr,          "ROTR Rn", 0},
    {"0100mmmm00000110", &Interp::ldsl_mach,     "LDS.L @Rm+,MACH", 0},
    {"0100mmmm00000111", &Interp::ldcl_sr,       "LDC.L @Rm+,SR", 0},
    {"0100nnnn00001000", &Interp::shll2,         "SHLL2 Rn", 0},
    {"0100nnnn00001001", &Interp::shlr2,         "SHLR2 Rn", 0},
    {"0100mmmm00001010", &Interp::lds_mach,      "LDS Rm,MACH", 0},
    {"0100mmmm00001011", &Interp::jsr,           "JSR @Rm", kSlotIllegal},
    {"0100mmmm00001110", &Interp::ldc_sr,        "LDC Rm,SR", 0},
    {"0100nnnn00010000", &Interp::dt,            "DT Rn", 0},
    {"0100nnnn00010001", &Interp::cmp_pz,        "CMP/PZ Rn", 0},
    {"0100nnnn00010010", &Interp::stsl_macl,     "STS.L MACL,@-Rn", 0},
    {"0100nnnn00010011", &Interp::stcl_gbr,      "STC.L GBR,@-Rn", 0},
    {"0100nnnn00010101", &Interp::cmp_pl,        "CMP/PL Rn", 0},
    {"0100mmmm00010110", &Interp::ldsl_macl,     "LDS.L @Rm+,MACL", 0},
    {"0100mmmm00010111", &Interp::ldcl_gbr,      "LDC.L @Rm+,GBR", 0},
    {"0100nnnn00011000", &Interp::shll8,         "SHLL8 Rn", 0},
    {"0100nnnn00011001", &Interp::shlr8,         "SHLR8 Rn", 0},
    {"0100mmmm00011010", &Interp::lds_macl,      "LDS Rm,MACL", 0},
    {"0100nnnn00011011", &Interp::tas,           "TAS.B @Rn", 0},
    {"0100mmmm00011110", &Interp::ldc_gbr,       "LDC Rm,GBR", 0},
    {"0100nnnn00100000", &Interp::shll,          "SHAL Rn", 0},
    {"0100nnnn00100001", &Interp::shar,          "SHAR Rn", 0},
    {"0100nnnn00100010", &Interp::stsl_pr,       "STS.L PR,@-Rn", 0},
    {"0100nnnn00100011", &Interp::stcl_vbr,      "STC.L VBR,@-Rn", 0},
    {"0100nnnn00100100", &Interp::rotcl,         "ROTCL Rn", 0},
    {"0100nnnn00100101", &Interp::rotcr,         "ROTCR Rn", 0},
    {"0100mmmm00100110", &Interp::ldsl_pr,       "LDS.L @Rm+,PR", 0},
    {"0100mmmm00100111", &Interp::ldcl_vbr,      "LDC.L @Rm+,VBR", 0},
    {"0100nnnn00101000", &Interp::shll16,        "SHLL16 Rn", 0},
    {"0100nnnn00101001", &Interp::shlr16,        "SHLR16 Rn", 0},
    {"0100mmmm00101010", &Interp::lds_pr,        "LDS Rm,PR", 0},
    {"0100mmmm00101011", &Interp::jmp,           "JMP @Rm", kSlotIllegal},
    {"0100mmmm00101110", &Interp::ldc_vbr,       "LDC Rm,VBR", 0},
    {"0100nnnn00110010", &Interp::stcl_sgr,      "STC.L SGR,@-Rn", 0},
    {"0100nnnn00110011", &Interp::stcl_ssr,      "STC.L SSR,@-Rn", 0},
    {"0100mmmm00110111", &Interp::ldcl_ssr,      "LDC.L @Rm+,SSR", 0},
    {"0100mmmm00111110", &Interp::ldc_ssr,       "LDC Rm,SSR", 0},
    {"0100nnnn01000011", &Interp::stcl_spc,      "STC.L SPC,@-Rn", 0},
    {"0100mmmm01000111", &Interp::ldcl_spc,      "LDC.L @Rm+,SPC", 0},
    {"0100mmmm01001110", &Interp::ldc_spc,       "LDC Rm,SPC", 0},
    {"0100nnnn01010010", &Interp::stsl_fpul,     "STS.L FPUL,@-Rn", 0},
    {"0100mmmm01010110", &Interp::ldsl_fpul,     "LDS.L @Rm+,FPUL", 0},
    {"0100mmmm01011010", &Interp::lds_fpul,      "LDS Rm,FPUL", 0},
    {"0100nnnn01100010", &Interp::stsl_fpscr,    "STS.L FPSCR,@-Rn", 0},
    {"0100mmmm01100110", &Interp::ldsl_fpscr,    "LDS.L @Rm+,FPSCR", 0},
    {"0100mmmm01101010", &Interp::lds_fpscr,     "LDS Rm,FPSCR", 0},
    {"0100nnnn11110010", &Interp::stcl_dbr,      "STC.L DBR,@-Rn", 0},
    {"0100mmmm11110110", &Interp::ldcl_dbr,      "LDC.L @Rm+,DBR", 0},
    {"0100mmmm11111010", &Interp::ldc_dbr,       "LDC Rm,DBR", 0},
    {"0100nnnn1mmm0011", &Interp::stcl_bank,     "STC.L Rm_BANK,@-Rn", 0},
    {"0100mmmm1nnn0111", &Interp::ldcl_bank,     "LDC.L @Rm+,Rn_BANK", 0},
    {"0100mmmm1nnn1110", &Interp::ldc_bank,      "LDC Rm,Rn_BANK", 0},
    {"0100nnnnmmmm1100", &Interp::shad,          "SHAD Rm,Rn", 0},
    {"0100nnnnmmmm1101", &Interp::shld,          "SHLD Rm,Rn", 0},
    {"0100nnnnmmmm1111", &Interp::macw,          "MAC.W @Rm+,@Rn+", 0},
    // 0101
    {"0101nnnnmmmmdddd", &Interp::movl_load_disp, "MOV.L @(disp,Rm),Rn", 0},
    // 0110
    {"0110nnnnmmmm0000", &Interp::movb_load,     "MOV.B @Rm,Rn", 0},
    {"0110nnnnmmmm0001", &Interp::movw_load,     "MOV.W @Rm,Rn", 0},
    {"0110nnnnmmmm0010", &Interp::movl_load,     "MOV.L @Rm,Rn", 0},
    {"0110nnnnmmmm0011", &Interp::mov,           "MOV Rm,Rn", 0},
    {"0110nnnnmmmm0100", &Interp::movb_postinc,  "MOV.B @Rm+,Rn", 0},
    {"0110nnnnmmmm0101", &Interp::movw_postinc,  "MOV.W @Rm+,Rn", 0},
    {"0110nnnnmmmm0110", &Interp::movl_postinc,  "MOV.L @Rm+,Rn", 0},
    {"0110nnnnmmmm0111", &Interp::not_,          "NOT Rm,Rn", 0},
    {"0110nnnnmmmm1000", &Interp::swapb,         "SWAP.B Rm,Rn", 0},
    {"0110nnnnmmmm1001", &Interp::swapw,         "SWAP.W Rm,Rn", 0},
    {"0110nnnnmmmm1010", &Interp::negc,          "NEGC Rm,Rn", 0},
    {"0110nnnnmmmm1011", &Interp::neg,           "NEG Rm,Rn", 0},
    {"0110nnnnmmmm1100", &Interp::extu_b,        "EXTU.B Rm,Rn", 0},
    {"0110nnnnmmmm1101", &Interp::extu_w,        "EXTU.W Rm,Rn", 0},
    {"0110nnnnmmmm1110", &Interp::exts_b,        "EXTS.B Rm,Rn", 0},
    {"0110nnnnmmmm1111", &Interp::exts_w,        "EXTS.W Rm,Rn", 0},
    // 0111
    {"0111nnnniiiiiiii", &Interp::add_imm,       "ADD #imm,Rn", 0},
    // 1000
    {"10000000nnnndddd", &Interp::movb_store_disp, "MOV.B R0,@(disp,Rn)", 0},
    {"10000001nnnndddd", &Interp::movw_store_disp, "MOV.W R0,@(disp,Rn)", 0},
    {"10000100mmmmdddd", &Interp::movb_load_disp,  "MOV.B @(disp,Rm),R0", 0},
    {"10000101mmmmdddd", &Interp::movw_load_disp,  "MOV.W @(disp,Rm),R0", 0},
    {"10001000iiiiiiii", &Interp::cmp_eq_imm,    "CMP/EQ #imm,R0", 0},
    {"10001001dddddddd", &Interp::bt,            "BT label", kSlotIllegal},
    {"10001011dddddddd", &Interp::bf,            "BF label", kSlotIllegal},
    {"10001101dddddddd", &Interp::bts,           "BT/S label", kSlotIllegal},
    {"10001111dddddddd", &Interp::bfs,           "BF/S label", kSlotIllegal},
    // 1001 - 1011
    {"1001nnnndddddddd", &Interp::movw_pc,       "MOV.W @(disp,PC),Rn", 0},
    {"1010dddddddddddd", &Interp::bra,           "BRA label", kSlotIllegal},
    {"1011dddddddddddd", &Interp::bsr,           "BSR label", kSlotIllegal},
    // 1100
    {"11000000dddddddd", &Interp::movb_store_gbr, "MOV.B R0,@(disp,GBR)", 0},
    {"11000001dddddddd", &Interp::movw_store_gbr, "MOV.W R0,@(disp,GBR)", 0},
    {"11000010dddddddd", &Interp::movl_store_gbr, "MOV.L R0,@(disp,GBR)", 0},
    {"11000011iiiiiiii", &Interp::trapa,          "TRAPA #imm", kSlotIllegal},
    {"11000100dddddddd", &Interp::movb_load_gbr,  "MOV.B @(disp,GBR),R0", 0},
    {"11000101dddddddd", &Interp::movw_load_gbr,  "MOV.W @(disp,GBR),R0", 0},
    {"11000110dddddddd", &Interp::movl_load_gbr,  "MOV.L @(disp,GBR),R0", 0},
    {"11000111dddddddd", &Interp::mova,           "MOVA @(disp,PC),R0", 0},
    {"11001000iiiiiiii", &Interp::tst_imm,        "TST #imm,R0", 0},
    {"11001001iiiiiiii", &Interp::and_imm,        "AND #imm,R0", 0},
    {"11001010iiiiiiii", &Interp::xor_imm,        "XOR #imm,R0", 0},
    {"11001011iiiiiiii", &Interp::or_imm,         "OR #imm,R0", 0},
    {"11001100iiiiiiii", &Interp::tstb_gbr,       "TST.B #imm,@(R0,GBR)", 0},
    {"11001101iiiiiiii", &Interp::andb_gbr,       "AND.B #imm,@(R0,GBR)", 0},
    {"11001110iiiiiiii", &Interp::xorb_gbr,       "XOR.B #imm,@(R0,GBR)", 0},
    {"11001111iiiiiiii", &Interp::orb_gbr,        "OR.B #imm,@(R0,GBR)", 0},
    // 1101 - 1110
    {"1101nnnndddddddd", &Interp::movl_pc,        "MOV.L @(disp,PC),Rn", 0},
    {"1110nnnniiiiiiii", &Interp::mov_imm,        "MOV #imm,Rn", 0},
    // 1111 (FPU)
    {"1111nnnnmmmm0000", &Interp::fadd,           "FADD FRm,FRn", 0},
    {"1111nnnnmmmm0001", &Interp::fsub,           "FSUB FRm,FRn", 0},
    {"1111nnnnmmmm0010", &Interp::fmul,           "FMUL FRm,FRn", 0},
    {"1111nnnnmmmm0011", &Interp::fdiv,           "FDIV FRm,FRn", 0},
    {"1111nnnnmmmm0100", &Interp::fcmp_eq,        "FCMP/EQ FRm,FRn", 0},
    {"1111nnnnmmmm0101", &Interp::fcmp_gt,        "FCMP/GT FRm,FRn", 0},
    {"1111nnnnmmmm0110", &Interp::fmov_load_r0,   "FMOV @(R0,Rm),FRn", 0},
    {"1111nnnnmmmm0111", &Interp::fmov_store_r0,  "FMOV FRm,@(R0,Rn)", 0},
    {"1111nnnnmmmm1000", &Interp::fmov_load,      "FMOV @Rm,FRn", 0},
    {"1111nnnnmmmm1001", &Interp::fmov_postinc,   "FMOV @Rm+,FRn", 0},
    {"1111nnnnmmmm1010", &Interp::fmov_store,     "FMOV FRm,@Rn", 0},
    {"1111nnnnmmmm1011", &Interp::fmov_predec,    "FMOV FRm,@-Rn", 0},
    {"1111nnnnmmmm1100", &Interp::fmov,           "FMOV FRm,FRn", 0},
    {"1111nnnnmmmm1110", &Interp::fmac,           "FMAC FR0,FRm,FRn", 0},
    {"1111nnnn00001101", &Interp::fsts,           "FSTS FPUL,FRn", 0},
    {"1111mmmm00011101", &Interp::flds,           "FLDS FRm,FPUL", 0},
    {"1111nnnn00101101", &Interp::float_,         "FLOAT FPUL,FRn", 0},
    {"1111mmmm00111101", &Interp::ftrc,           "FTRC FRm,FPUL", 0},
    {"1111nnnn01001101", &Interp::fneg,           "FNEG FRn", 0},
    {"1111nnnn01011101", &Interp::fabs_,          "FABS FRn", 0},
    {"1111nnnn01101101", &Interp::fsqrt,          "FSQRT FRn", 0},
    {"1111nnnn01111101", &Interp::fsrra,          "FSRRA FRn", 0},
    {"1111nnnn10001101", &Interp::fldi0,          "FLDI0 FRn", 0},
    {"1111nnnn10011101", &Interp::fldi1,          "FLDI1 FRn", 0},
    {"1111nnnn10101101", &Interp::fcnvsd,         "FCNVSD FPUL,DRn", 0},
    {"1111mmmm10111101", &Interp::fcnvds,         "FCNVDS DRm,FPUL", 0},
    {"1111nnmm11101101", &Interp::fipr,           "FIPR FVm,FVn", 0},
    {"1111nnn011111101", &Interp::fsca,           "FSCA FPUL,DRn", 0},
    {"1111nn0111111101", &Interp::ftrv,           "FTRV XMTRX,FVn", 0},
    {"1111001111111101", &Interp::fschg,          "FSCHG", 0},
    {"1111101111111101", &Interp::frchg,          "FRCHG", 0},
};
// clang-format on

struct DecodeTable {
    std::array<Handler, 65536> handler{};
    std::array<u8, 65536> pattern{}; // index into kPatterns + 1, 0 = undefined
    std::array<u8, 65536> flags{};
    int overlaps = 0;
};

DecodeTable build_table() {
    static_assert(std::size(kPatterns) < 255, "pattern index must fit in u8");
    DecodeTable table;
    table.handler.fill(&Interp::illegal);
    for (u32 p = 0; p < std::size(kPatterns); ++p) {
        u32 mask = 0, value = 0;
        for (int bit = 0; bit < 16; ++bit) {
            const char ch = kPatterns[p].bits[bit];
            const u32 b = 1u << (15 - bit);
            if (ch == '0' || ch == '1') {
                mask |= b;
                if (ch == '1')
                    value |= b;
            }
        }
        for (u32 op = 0; op < 65536; ++op) {
            if ((op & mask) != value)
                continue;
            if (table.pattern[op] != 0)
                ++table.overlaps;
            table.handler[op] = kPatterns[p].handler;
            table.pattern[op] = static_cast<u8>(p + 1);
            table.flags[op] = kPatterns[p].flags;
        }
    }
    return table;
}

const DecodeTable& table() {
    static const DecodeTable instance = build_table();
    return instance;
}

// Resolve the table once at start-up so execute() is a plain array lookup.
const DecodeTable& g_table = table();

} // namespace

void Sh4::execute(u16 opcode) {
    if (in_delay_slot_ && (g_table.flags[opcode] & kSlotIllegal))
        raise_illegal();
    g_table.handler[opcode](*this, opcode);
}

const char* sh4_mnemonic(u16 opcode) {
    const u8 p = table().pattern[opcode];
    return p ? kPatterns[p - 1].mnemonic : nullptr;
}

bool sh4_is_slot_illegal(u16 opcode) {
    return (table().flags[opcode] & kSlotIllegal) != 0;
}

int sh4_decode_overlaps() {
    return table().overlaps;
}

int sh4_pattern_count() {
    return static_cast<int>(std::size(kPatterns));
}

} // namespace nc::cpu
