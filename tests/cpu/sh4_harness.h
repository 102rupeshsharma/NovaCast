#pragma once

// Test support for the SH-4 interpreter: a flat memory, a tiny assembler for
// the instructions the tests use, and a fixture that runs a program until it
// reaches its final "BRA ." loop.

#include "cpu/sh4.h"

#include <gtest/gtest.h>

#include <bit>
#include <cstring>
#include <memory>
#include <vector>

namespace nc::cpu::test {

// 16 MB of RAM at physical 0x0C000000 (mirrored up to 0x0FFFFFFF).
// Every other physical address reads 0 and counts as unmapped.
class FlatMemory final : public Sh4Memory {
public:
    FlatMemory() : ram_(16 * 1024 * 1024) {}

    u8 read8(u32 a) override { return static_cast<u8>(load(a, 1)); }
    u16 read16(u32 a) override { return static_cast<u16>(load(a, 2)); }
    u32 read32(u32 a) override { return load(a, 4); }
    void write8(u32 a, u8 v) override { store(a, v, 1); }
    void write16(u32 a, u16 v) override { store(a, v, 2); }
    void write32(u32 a, u32 v) override { store(a, v, 4); }

    u32 peek32(u32 physical) { return load(physical, 4); }
    u16 peek16(u32 physical) { return static_cast<u16>(load(physical, 2)); }
    u8 peek8(u32 physical) { return static_cast<u8>(load(physical, 1)); }
    void poke32(u32 physical, u32 v) { store(physical, v, 4); }
    void poke16(u32 physical, u16 v) { store(physical, v, 2); }
    void poke8(u32 physical, u8 v) { store(physical, v, 1); }

    int unmapped = 0;

private:
    bool in_ram(u32 a) const { return (a & 0x1C000000) == 0x0C000000; }
    u32 load(u32 a, u32 size) {
        if (!in_ram(a)) {
            ++unmapped;
            return 0;
        }
        u32 v = 0;
        std::memcpy(&v, &ram_[a & 0xFFFFFF], size);
        return v;
    }
    void store(u32 a, u32 v, u32 size) {
        if (!in_ram(a)) {
            ++unmapped;
            return;
        }
        std::memcpy(&ram_[a & 0xFFFFFF], &v, size);
    }
    std::vector<u8> ram_;
};

// Opcode encoders. Names follow the SH-4 mnemonics; arguments are in
// assembler order (source first, destination last).
namespace as {
inline u16 nm(u16 base, u32 m, u32 n) { return static_cast<u16>(base | (n << 8) | (m << 4)); }
inline u16 n_(u16 base, u32 n) { return static_cast<u16>(base | (n << 8)); }
inline u16 i8(u16 base, s32 imm) { return static_cast<u16>(base | (static_cast<u32>(imm) & 0xFF)); }

inline u16 nop() { return 0x0009; }
inline u16 mov_i(s32 imm, u32 n) { return static_cast<u16>(0xE000 | (n << 8) | (static_cast<u32>(imm) & 0xFF)); }
inline u16 mov(u32 m, u32 n) { return nm(0x6003, m, n); }
inline u16 add(u32 m, u32 n) { return nm(0x300C, m, n); }
inline u16 add_i(s32 imm, u32 n) { return static_cast<u16>(0x7000 | (n << 8) | (static_cast<u32>(imm) & 0xFF)); }
inline u16 addc(u32 m, u32 n) { return nm(0x300E, m, n); }
inline u16 addv(u32 m, u32 n) { return nm(0x300F, m, n); }
inline u16 subc(u32 m, u32 n) { return nm(0x300A, m, n); }
inline u16 subv(u32 m, u32 n) { return nm(0x300B, m, n); }
inline u16 negc(u32 m, u32 n) { return nm(0x600A, m, n); }
inline u16 xor_(u32 m, u32 n) { return nm(0x200A, m, n); }
inline u16 clrt() { return 0x0008; }
inline u16 sett() { return 0x0018; }
inline u16 div0u() { return 0x0019; }
inline u16 div0s(u32 m, u32 n) { return nm(0x2007, m, n); }
inline u16 div1(u32 m, u32 n) { return nm(0x3004, m, n); }
inline u16 rotcl(u32 n) { return n_(0x4024, n); }
inline u16 rotcr(u32 n) { return n_(0x4025, n); }
inline u16 shll16(u32 n) { return n_(0x4028, n); }
inline u16 exts_w(u32 m, u32 n) { return nm(0x600F, m, n); }
inline u16 extu_w(u32 m, u32 n) { return nm(0x600D, m, n); }
inline u16 shad(u32 m, u32 n) { return nm(0x400C, m, n); }
inline u16 shld(u32 m, u32 n) { return nm(0x400D, m, n); }
inline u16 cmp_str(u32 m, u32 n) { return nm(0x200C, m, n); }
inline u16 swap_b(u32 m, u32 n) { return nm(0x6008, m, n); }
inline u16 swap_w(u32 m, u32 n) { return nm(0x6009, m, n); }
inline u16 xtrct(u32 m, u32 n) { return nm(0x200D, m, n); }
inline u16 dmuls(u32 m, u32 n) { return nm(0x300D, m, n); }
inline u16 dmulu(u32 m, u32 n) { return nm(0x3005, m, n); }
inline u16 muls_w(u32 m, u32 n) { return nm(0x200F, m, n); }
inline u16 mulu_w(u32 m, u32 n) { return nm(0x200E, m, n); }
inline u16 mac_l(u32 m, u32 n) { return nm(0x000F, m, n); }
inline u16 mac_w(u32 m, u32 n) { return nm(0x400F, m, n); }
inline u16 tas_b(u32 n) { return n_(0x401B, n); }
inline u16 dt(u32 n) { return n_(0x4010, n); }
inline u16 movl_load(u32 m, u32 n) { return nm(0x6002, m, n); }
inline u16 movl_store(u32 m, u32 n) { return nm(0x2002, m, n); }
inline u16 movl_postinc(u32 m, u32 n) { return nm(0x6006, m, n); }
inline u16 movl_predec(u32 m, u32 n) { return nm(0x2006, m, n); }
inline u16 movb_load(u32 m, u32 n) { return nm(0x6000, m, n); }
inline u16 movw_load(u32 m, u32 n) { return nm(0x6001, m, n); }
inline u16 movl_load_r0(u32 m, u32 n) { return nm(0x000E, m, n); }
inline u16 movl_store_disp(u32 m, u32 disp, u32 n) { return static_cast<u16>(nm(0x1000, m, n) | (disp & 0xF)); }
inline u16 movl_load_disp(u32 disp, u32 m, u32 n) { return static_cast<u16>(nm(0x5000, m, n) | (disp & 0xF)); }
inline u16 movl_load_gbr(u32 disp) { return static_cast<u16>(0xC600 | (disp & 0xFF)); }
inline u16 movl_store_gbr(u32 disp) { return static_cast<u16>(0xC200 | (disp & 0xFF)); }
inline u16 movl_pc(u32 disp, u32 n) { return static_cast<u16>(0xD000 | (n << 8) | (disp & 0xFF)); }
inline u16 movw_pc(u32 disp, u32 n) { return static_cast<u16>(0x9000 | (n << 8) | (disp & 0xFF)); }
inline u16 mova(u32 disp) { return static_cast<u16>(0xC700 | (disp & 0xFF)); }
inline u16 ldc_gbr(u32 m) { return n_(0x401E, m); }
inline u16 ldc_vbr(u32 m) { return n_(0x402E, m); }
inline u16 ldc_sr(u32 m) { return n_(0x400E, m); }
inline u16 stc_sr(u32 n) { return n_(0x0002, n); }
inline u16 ldc_bank(u32 m, u32 bank) { return static_cast<u16>(0x408E | (m << 8) | (bank << 4)); }
inline u16 stc_bank(u32 bank, u32 n) { return static_cast<u16>(0x0082 | (n << 8) | (bank << 4)); }
inline u16 sts_pr(u32 n) { return n_(0x002A, n); }
inline u16 lds_pr(u32 m) { return n_(0x402A, m); }
inline u16 rts() { return 0x000B; }
inline u16 rte() { return 0x002B; }
inline u16 sleep() { return 0x001B; }
inline u16 trapa(u32 imm) { return static_cast<u16>(0xC300 | (imm & 0xFF)); }
inline u16 jmp(u32 m) { return n_(0x402B, m); }
inline u16 jsr(u32 m) { return n_(0x400B, m); }
inline u16 braf(u32 m) { return n_(0x0023, m); }
inline u16 bsrf(u32 m) { return n_(0x0003, m); }
inline u16 pref(u32 n) { return n_(0x0083, n); }
// FPU
inline u16 fadd(u32 m, u32 n) { return nm(0xF000, m, n); }
inline u16 fsub(u32 m, u32 n) { return nm(0xF001, m, n); }
inline u16 fmul(u32 m, u32 n) { return nm(0xF002, m, n); }
inline u16 fdiv(u32 m, u32 n) { return nm(0xF003, m, n); }
inline u16 fcmp_eq(u32 m, u32 n) { return nm(0xF004, m, n); }
inline u16 fcmp_gt(u32 m, u32 n) { return nm(0xF005, m, n); }
inline u16 fmov(u32 m, u32 n) { return nm(0xF00C, m, n); }
inline u16 fmov_load(u32 m, u32 n) { return nm(0xF008, m, n); }
inline u16 fmov_store(u32 m, u32 n) { return nm(0xF00A, m, n); }
inline u16 fmov_postinc(u32 m, u32 n) { return nm(0xF009, m, n); }
inline u16 fmov_predec(u32 m, u32 n) { return nm(0xF00B, m, n); }
inline u16 fmac(u32 m, u32 n) { return nm(0xF00E, m, n); }
inline u16 float_(u32 n) { return n_(0xF02D, n); }
inline u16 ftrc(u32 m) { return n_(0xF03D, m); }
inline u16 fneg(u32 n) { return n_(0xF04D, n); }
inline u16 fabs_(u32 n) { return n_(0xF05D, n); }
inline u16 fsqrt(u32 n) { return n_(0xF06D, n); }
inline u16 fsrra(u32 n) { return n_(0xF07D, n); }
inline u16 fldi0(u32 n) { return n_(0xF08D, n); }
inline u16 fldi1(u32 n) { return n_(0xF09D, n); }
inline u16 fcnvsd(u32 n) { return n_(0xF0AD, n); }
inline u16 fcnvds(u32 m) { return n_(0xF0BD, m); }
inline u16 fipr(u32 vm, u32 vn) { return static_cast<u16>(0xF0ED | (vn << 10) | (vm << 8)); }
inline u16 fsca(u32 dn) { return static_cast<u16>(0xF0FD | ((dn & 0xE) << 8)); }
inline u16 ftrv(u32 vn) { return static_cast<u16>(0xF1FD | (vn << 10)); }
inline u16 frchg() { return 0xFBFD; }
inline u16 fschg() { return 0xF3FD; }
inline u16 lds_fpul(u32 m) { return n_(0x405A, m); }
inline u16 sts_fpul(u32 n) { return n_(0x005A, n); }
inline u16 lds_fpscr(u32 m) { return n_(0x406A, m); }
inline u16 sts_fpscr(u32 n) { return n_(0x006A, n); }
} // namespace as

// A program assembled in place at a virtual address.
class Program {
public:
    explicit Program(u32 base) : base_(base) {}

    Program& operator<<(u16 op) {
        code_.push_back(op);
        return *this;
    }
    u32 here() const { return base_ + static_cast<u32>(code_.size()) * 2; }
    u32 base() const { return base_; }

    // Branch encoders that compute displacements from the current position.
    Program& bra(u32 target) { return emit12(0xA000, target); }
    Program& bsr(u32 target) { return emit12(0xB000, target); }
    Program& bt(u32 target) { return emit8(0x8900, target); }
    Program& bf(u32 target) { return emit8(0x8B00, target); }
    Program& bts(u32 target) { return emit8(0x8D00, target); }
    Program& bfs(u32 target) { return emit8(0x8F00, target); }

    // Pads with NOPs until here() is a multiple of 4, then emits a 32-bit literal.
    u32 literal(u32 value) {
        while (here() & 3)
            code_.push_back(as::nop());
        const u32 at = here();
        code_.push_back(static_cast<u16>(value));
        code_.push_back(static_cast<u16>(value >> 16));
        return at;
    }

    // Ends the program with "BRA . ; NOP" and returns the loop's address.
    u32 finish() {
        const u32 at = here();
        bra(at);
        code_.push_back(as::nop());
        return at;
    }

    // Placeholder for an instruction patched later with set().
    u32 reserve() {
        const u32 at = here();
        code_.push_back(as::nop());
        return at;
    }
    void set(u32 at, u16 op) { code_[(at - base_) / 2] = op; }

    void load(FlatMemory& memory) const {
        for (std::size_t i = 0; i < code_.size(); ++i)
            memory.poke16((base_ & 0x1FFFFFFF) + static_cast<u32>(i) * 2, code_[i]);
    }

private:
    Program& emit12(u16 base, u32 target) {
        const s32 disp = (static_cast<s32>(target) - static_cast<s32>(here() + 4)) / 2;
        code_.push_back(static_cast<u16>(base | (static_cast<u32>(disp) & 0xFFF)));
        return *this;
    }
    Program& emit8(u16 base, u32 target) {
        const s32 disp = (static_cast<s32>(target) - static_cast<s32>(here() + 4)) / 2;
        code_.push_back(static_cast<u16>(base | (static_cast<u32>(disp) & 0xFF)));
        return *this;
    }

    u32 base_;
    std::vector<u16> code_;
};

class Sh4Test : public ::testing::Test {
protected:
    static constexpr u32 kCodeBase = 0x8C010000; // P1, physical 0x0C010000
    static constexpr u32 kDataBase = 0x8C100000; // P1, physical 0x0C100000
    static constexpr u32 kVbr = 0x8C020000;

    // Privileged, register bank 0, exceptions unblocked, all interrupts masked.
    static constexpr u32 kTestSr = sr::MD | sr::IMASK;

    Sh4Test() : memory(std::make_unique<FlatMemory>()), cpu(std::make_unique<Sh4>(*memory)), prog(kCodeBase) {
        cpu->set_sr(kTestSr);
        s().vbr = kVbr;
    }

    Sh4State& s() { return cpu->state(); }
    u32& r(int i) { return cpu->state().r[i]; }

    // Loads `prog`, starts at its base and steps until PC reaches `end`.
    // Fails the test if that takes more than `max_steps` steps.
    void run_to(u32 end, int max_steps = 10000) {
        prog.load(*memory);
        s().pc = prog.base();
        for (int i = 0; i < max_steps; ++i) {
            if (s().pc == end)
                return;
            cpu->step();
        }
        FAIL() << "program did not reach " << std::hex << end << ", PC=" << s().pc;
    }
    void run_program(int max_steps = 10000) {
        const u32 end = prog.finish();
        run_to(end, max_steps);
    }

    // Writes a "BRA . ; NOP" loop at an exception vector so tests can stop there.
    void install_trap_loop(u32 vector_offset) {
        Program handler(kVbr + vector_offset);
        handler.finish();
        handler.load(*memory);
    }

    float f(int i) { return std::bit_cast<float>(s().fr[i]); }
    void set_f(int i, float v) { s().fr[i] = std::bit_cast<u32>(v); }
    double d(int i) {
        return std::bit_cast<double>((static_cast<u64>(s().fr[i]) << 32) | s().fr[i + 1]);
    }
    void set_d(int i, double v) {
        const u64 bits = std::bit_cast<u64>(v);
        s().fr[i] = static_cast<u32>(bits >> 32);
        s().fr[i + 1] = static_cast<u32>(bits);
    }

    std::unique_ptr<FlatMemory> memory;
    std::unique_ptr<Sh4> cpu;
    Program prog;
};

} // namespace nc::cpu::test
