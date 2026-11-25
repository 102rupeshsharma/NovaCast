// SH-4 integer instructions. Expected values come from the instruction
// definitions in the SH-4 Software Manual or from host arithmetic.

#include "sh4_harness.h"

#include <random>

namespace nc::cpu::test {
namespace {

using namespace as;

constexpr bool T(u32 sr_value) {
    return (sr_value & sr::T) != 0;
}

TEST_F(Sh4Test, MovImmediateSignExtends) {
    prog << mov_i(-1, 0) << mov_i(0x7F, 1) << mov_i(-128, 2);
    run_program();
    EXPECT_EQ(r(0), 0xFFFFFFFFu);
    EXPECT_EQ(r(1), 0x7Fu);
    EXPECT_EQ(r(2), 0xFFFFFF80u);
}

TEST_F(Sh4Test, AddcPropagatesCarryAcrossWords) {
    // 64-bit add: (r1:r0) += (r3:r2)
    r(0) = 0xFFFFFFFF;
    r(1) = 0x00000001;
    r(2) = 0x00000001;
    r(3) = 0x00000002;
    prog << clrt() << addc(2, 0) << addc(3, 1);
    run_program();
    EXPECT_EQ(r(0), 0u);
    EXPECT_EQ(r(1), 4u);
    EXPECT_FALSE(T(s().sr));
}

TEST_F(Sh4Test, AddcCarryOut) {
    r(0) = 0xFFFFFFFF;
    r(1) = 0;
    prog << sett() << addc(1, 0); // 0xFFFFFFFF + 0 + 1
    run_program();
    EXPECT_EQ(r(0), 0u);
    EXPECT_TRUE(T(s().sr));
}

TEST_F(Sh4Test, AddvDetectsSignedOverflow) {
    r(0) = 0x7FFFFFFF;
    r(1) = 1;
    r(2) = 5;
    r(3) = 7;
    prog << addv(1, 0) << 0x0429 /* MOVT r4 */ << addv(3, 2);
    run_program();
    EXPECT_EQ(r(0), 0x80000000u);
    EXPECT_EQ(r(4), 1u);
    EXPECT_EQ(r(2), 12u);
    EXPECT_FALSE(T(s().sr));
}

TEST_F(Sh4Test, SubcBorrowsAcrossWords) {
    // 64-bit subtract: (r1:r0) -= (r3:r2), 0x1_00000000 - 1
    r(0) = 0;
    r(1) = 1;
    r(2) = 1;
    r(3) = 0;
    prog << clrt() << subc(2, 0) << subc(3, 1);
    run_program();
    EXPECT_EQ(r(0), 0xFFFFFFFFu);
    EXPECT_EQ(r(1), 0u);
    EXPECT_FALSE(T(s().sr));
}

TEST_F(Sh4Test, SubcBorrowOut) {
    r(0) = 0;
    r(1) = 0;
    prog << sett() << subc(1, 0); // 0 - 0 - 1
    run_program();
    EXPECT_EQ(r(0), 0xFFFFFFFFu);
    EXPECT_TRUE(T(s().sr));
}

TEST_F(Sh4Test, SubvDetectsSignedOverflow) {
    r(0) = 0x80000000;
    r(1) = 1;
    prog << subv(1, 0);
    run_program();
    EXPECT_EQ(r(0), 0x7FFFFFFFu);
    EXPECT_TRUE(T(s().sr));
}

TEST_F(Sh4Test, Negc) {
    r(1) = 1;
    r(3) = 0;
    prog << clrt() << negc(1, 0); // 0 - 1 - 0
    prog << 0x0529;               // MOVT r5
    prog << clrt() << negc(3, 2); // 0 - 0 - 0
    run_program();
    EXPECT_EQ(r(0), 0xFFFFFFFFu);
    EXPECT_EQ(r(5), 1u);
    EXPECT_EQ(r(2), 0u);
    EXPECT_FALSE(T(s().sr));
}

// The unsigned 64/32 division sequence from the SH-4 manual (DIV1 section):
// (r1:r2) / r0 -> quotient in r2. Requires r1 < r0.
TEST_F(Sh4Test, Div1Unsigned64By32MatchesHost) {
    std::mt19937 rng(1234);
    for (int trial = 0; trial < 40; ++trial) {
        const u32 divisor = (rng() | 1u) & (trial < 20 ? 0xFFFFu : 0xFFFFFFFFu);
        const u32 high = static_cast<u32>(rng() % divisor);
        const u32 low = rng();
        const u64 dividend = (static_cast<u64>(high) << 32) | low;

        cpu->reset();
        cpu->set_sr(kTestSr);
        prog = Program(kCodeBase);
        r(0) = divisor;
        r(1) = high;
        r(2) = low;
        prog << div0u();
        for (int i = 0; i < 32; ++i)
            prog << rotcl(2) << div1(0, 1);
        prog << rotcl(2);
        run_program();
        EXPECT_EQ(r(2), static_cast<u32>(dividend / divisor)) << dividend << " / " << divisor;
    }
}

// Unsigned 32/16 sequence from the manual: r1 / r0 (16-bit divisor).
TEST_F(Sh4Test, Div1Unsigned32By16MatchesHost) {
    const u32 cases[][2] = {{100000, 7}, {65535, 1}, {0x12345678, 0xFFFF}, {0, 3}, {1000, 1000}};
    for (const auto& c : cases) {
        cpu->reset();
        cpu->set_sr(kTestSr);
        prog = Program(kCodeBase);
        r(1) = c[0];
        r(0) = c[1];
        prog << shll16(0) << div0u();
        for (int i = 0; i < 16; ++i)
            prog << div1(0, 1);
        prog << rotcl(1) << extu_w(1, 1);
        run_program();
        EXPECT_EQ(r(1), c[0] / c[1]) << c[0] << " / " << c[1];
    }
}

// Signed 16/16 sequence from the manual: r1 / r0 -> r1.
TEST_F(Sh4Test, Div1Signed16By16MatchesHost) {
    const s32 cases[][2] = {{100, 7}, {-100, 7}, {100, -7}, {-100, -7}, {32767, 3}, {-32768, 2}, {5, 9}};
    for (const auto& c : cases) {
        cpu->reset();
        cpu->set_sr(kTestSr);
        prog = Program(kCodeBase);
        r(1) = static_cast<u32>(c[0]);
        r(0) = static_cast<u32>(c[1]);
        prog << shll16(0) << exts_w(1, 1) << xor_(2, 2) << mov(1, 3) << rotcl(3) << subc(2, 1) << div0s(0, 1);
        for (int i = 0; i < 16; ++i)
            prog << div1(0, 1);
        prog << exts_w(1, 1) << rotcl(1) << addc(2, 1) << exts_w(1, 1);
        run_program();
        EXPECT_EQ(static_cast<s32>(r(1)), c[0] / c[1]) << c[0] << " / " << c[1];
    }
}

TEST_F(Sh4Test, ShadArithmeticShifts) {
    r(0) = 0x80000000;
    r(1) = static_cast<u32>(-4);
    r(2) = 0x80000000;
    r(3) = static_cast<u32>(-32); // low five bits zero, negative: fill with sign
    r(4) = 1;
    r(5) = 31;
    prog << shad(1, 0) << shad(3, 2) << shad(5, 4);
    run_program();
    EXPECT_EQ(r(0), 0xF8000000u);
    EXPECT_EQ(r(2), 0xFFFFFFFFu);
    EXPECT_EQ(r(4), 0x80000000u);
}

TEST_F(Sh4Test, ShldLogicalShifts) {
    r(0) = 0x80000000;
    r(1) = static_cast<u32>(-1);
    r(2) = 0x80000000;
    r(3) = static_cast<u32>(-32);
    r(4) = 1;
    r(5) = 4;
    prog << shld(1, 0) << shld(3, 2) << shld(5, 4);
    run_program();
    EXPECT_EQ(r(0), 0x40000000u);
    EXPECT_EQ(r(2), 0u);
    EXPECT_EQ(r(4), 16u);
}

TEST_F(Sh4Test, RotateThroughCarry) {
    r(0) = 0x80000001;
    r(1) = 0x80000001;
    prog << clrt() << rotcl(0); // r0 = 0x00000002, T = 1
    prog << rotcr(1);           // r1 = 0x80000000 | (T<<31) = 0xC0000000, T = 1
    run_program();
    EXPECT_EQ(r(0), 0x00000002u);
    EXPECT_EQ(r(1), 0xC0000000u);
    EXPECT_TRUE(T(s().sr));
}

TEST_F(Sh4Test, CmpStr) {
    r(0) = 0x12345678;
    r(1) = 0xAB34CDEF; // second byte matches
    r(2) = 0x12345678;
    r(3) = 0x01020304; // no byte matches
    prog << cmp_str(1, 0) << 0x0429 /* MOVT r4 */ << cmp_str(3, 2);
    run_program();
    EXPECT_EQ(r(4), 1u);
    EXPECT_FALSE(T(s().sr));
}

TEST_F(Sh4Test, SwapAndExtract) {
    r(1) = 0x12345678;
    r(4) = 0xAABBCCDD;
    prog << swap_b(1, 2) << swap_w(1, 3) << xtrct(1, 4);
    run_program();
    EXPECT_EQ(r(2), 0x12347856u);
    EXPECT_EQ(r(3), 0x56781234u);
    EXPECT_EQ(r(4), 0x5678AABBu);
}

TEST_F(Sh4Test, Multiplies) {
    r(0) = static_cast<u32>(-2);
    r(1) = 3;
    prog << dmuls(1, 0);
    prog << 0x020A << 0x031A; // STS MACH,r2 ; STS MACL,r3
    r(4) = 0xFFFFFFFF;
    r(5) = 0xFFFFFFFF;
    prog << dmulu(5, 4) << 0x060A << 0x071A;
    r(8) = 0xFFFF;
    r(9) = 2;
    prog << muls_w(9, 8) << 0x0A1A; // r10 = MACL
    prog << mulu_w(9, 8) << 0x0B1A; // r11 = MACL
    run_program();
    EXPECT_EQ(r(2), 0xFFFFFFFFu);
    EXPECT_EQ(r(3), 0xFFFFFFFAu);
    EXPECT_EQ(r(6), 0xFFFFFFFEu);
    EXPECT_EQ(r(7), 0x00000001u);
    EXPECT_EQ(r(10), 0xFFFFFFFEu);
    EXPECT_EQ(r(11), 0x0001FFFEu);
}

TEST_F(Sh4Test, MacLAccumulatesDotProduct) {
    const u32 a = kDataBase, b = kDataBase + 0x100;
    memory->poke32(a & 0x1FFFFFFF, 2);
    memory->poke32((a + 4) & 0x1FFFFFFF, static_cast<u32>(-3));
    memory->poke32(b & 0x1FFFFFFF, 4);
    memory->poke32((b + 4) & 0x1FFFFFFF, 5);
    r(1) = a;
    r(2) = b;
    prog << 0x0028 /* CLRMAC */ << mac_l(1, 2) << mac_l(1, 2);
    run_program();
    // 2*4 + (-3)*5 = -7
    EXPECT_EQ(s().macl, static_cast<u32>(-7));
    EXPECT_EQ(s().mach, 0xFFFFFFFFu);
    EXPECT_EQ(r(1), a + 8);
    EXPECT_EQ(r(2), b + 8);
}

TEST_F(Sh4Test, MacLSaturatesTo48BitsWhenSSet) {
    const u32 a = kDataBase;
    memory->poke32(a & 0x1FFFFFFF, 0x7FFFFFFF);
    memory->poke32((a + 4) & 0x1FFFFFFF, 0x7FFFFFFF);
    r(1) = a;
    s().mach = 0x00007FFF;
    s().macl = 0xFFFFFFF0;
    prog << 0x0058 /* SETS */ << mac_l(1, 1);
    run_program();
    EXPECT_EQ(s().mach, 0x00007FFFu);
    EXPECT_EQ(s().macl, 0xFFFFFFFFu);
    EXPECT_EQ(r(1), a + 8);
}

TEST_F(Sh4Test, MacW) {
    const u32 a = kDataBase, b = kDataBase + 0x10;
    memory->poke16(a & 0x1FFFFFFF, static_cast<u16>(-3));
    memory->poke16(b & 0x1FFFFFFF, 7);
    r(1) = a;
    r(2) = b;
    prog << 0x0028 /* CLRMAC */ << mac_w(1, 2);
    run_program();
    EXPECT_EQ(s().macl, static_cast<u32>(-21));
    EXPECT_EQ(s().mach, 0xFFFFFFFFu);
    EXPECT_EQ(r(1), a + 2);
    EXPECT_EQ(r(2), b + 2);
}

TEST_F(Sh4Test, MacWSaturatesMaclWhenSSet) {
    const u32 a = kDataBase;
    memory->poke16(a & 0x1FFFFFFF, 0x7FFF);
    memory->poke16((a + 2) & 0x1FFFFFFF, 0x7FFF);
    r(1) = a;
    s().macl = 0x7FFFFF00;
    prog << 0x0058 /* SETS */ << mac_w(1, 1);
    run_program();
    EXPECT_EQ(s().macl, 0x7FFFFFFFu);
}

TEST_F(Sh4Test, TasSetsBit7AndTestsZero) {
    const u32 a = kDataBase;
    memory->poke8(a & 0x1FFFFFFF, 0);
    r(1) = a;
    prog << tas_b(1) << 0x0229 /* MOVT r2 */ << tas_b(1);
    run_program();
    EXPECT_EQ(r(2), 1u);
    EXPECT_FALSE(T(s().sr));
    EXPECT_EQ(memory->peek8(a & 0x1FFFFFFF), 0x80);
}

TEST_F(Sh4Test, DtLoop) {
    r(0) = 5;
    r(1) = 0;
    const u32 loop = prog.here();
    prog << add_i(3, 1) << dt(0);
    prog.bf(loop);
    run_program();
    EXPECT_EQ(r(0), 0u);
    EXPECT_EQ(r(1), 15u);
}

// ----- addressing modes -----------------------------------------------------

TEST_F(Sh4Test, PcRelativeLoadsAlignPc) {
    // The MOV.L sits at an address that is 2 mod 4; the literal is found
    // relative to (PC & ~3) + 4.
    prog << nop();                      // base+0
    const u32 load_at = prog.reserve(); // base+2
    prog << movw_pc(0, 2);              // base+4: reads the halfword at base+8
    const u32 end = prog.finish();      // base+6
    const u32 lit = prog.literal(0xCAFEF00D); // base+12 (aligned)
    prog.set(load_at, movl_pc((lit - ((load_at & ~3u) + 4)) / 4, 1));
    run_to(end);
    EXPECT_EQ(r(1), 0xCAFEF00Du);
    // base+8 holds the NOP of the final loop (0x0009), sign extension is visible on negatives below.
    EXPECT_EQ(r(2), 0x0009u);
}

TEST_F(Sh4Test, MovwPcSignExtends) {
    const u32 load_at = prog.reserve();
    const u32 end = prog.finish();
    const u32 lit = prog.literal(0x0000FFFE);
    prog.set(load_at, movw_pc((lit - (load_at + 4)) / 2, 3));
    run_to(end);
    EXPECT_EQ(r(3), 0xFFFFFFFEu);
}

TEST_F(Sh4Test, Mova) {
    prog << nop();
    const u32 at = prog.reserve(); // base+2
    const u32 end = prog.finish();
    const u32 lit = prog.literal(0);
    prog.set(at, mova((lit - ((at & ~3u) + 4)) / 4));
    run_to(end);
    EXPECT_EQ(r(0), lit);
}

TEST_F(Sh4Test, PostIncrementAndPreDecrement) {
    const u32 a = kDataBase;
    memory->poke32(a & 0x1FFFFFFF, 0x11111111);
    memory->poke32((a + 4) & 0x1FFFFFFF, 0x22222222);
    r(1) = a;
    r(5) = a + 0x40;
    r(6) = 0xABCD1234;
    prog << movl_postinc(1, 2) << movl_postinc(1, 3); // r2, r3 loaded; r1 += 8
    prog << movl_predec(6, 5);                        // @(a+0x3C) = r6
    run_program();
    EXPECT_EQ(r(2), 0x11111111u);
    EXPECT_EQ(r(3), 0x22222222u);
    EXPECT_EQ(r(1), a + 8);
    EXPECT_EQ(r(5), a + 0x3C);
    EXPECT_EQ(memory->peek32((a + 0x3C) & 0x1FFFFFFF), 0xABCD1234u);
}

TEST_F(Sh4Test, SameRegisterPostIncrementKeepsLoadedValue) {
    const u32 a = kDataBase;
    memory->poke32(a & 0x1FFFFFFF, 0x5A5A5A5A);
    r(1) = a;
    prog << movl_postinc(1, 1);
    run_program();
    EXPECT_EQ(r(1), 0x5A5A5A5Au);
}

TEST_F(Sh4Test, SameRegisterPreDecrementStoresOldValue) {
    const u32 a = kDataBase + 0x20;
    r(1) = a;
    prog << movl_predec(1, 1);
    run_program();
    EXPECT_EQ(r(1), a - 4);
    EXPECT_EQ(memory->peek32((a - 4) & 0x1FFFFFFF), a);
}

TEST_F(Sh4Test, ByteAndWordLoadsSignExtend) {
    const u32 a = kDataBase;
    memory->poke32(a & 0x1FFFFFFF, 0x0000F080);
    r(1) = a;
    prog << movb_load(1, 2) << movw_load(1, 3);
    run_program();
    EXPECT_EQ(r(2), 0xFFFFFF80u);
    EXPECT_EQ(r(3), 0xFFFFF080u);
}

TEST_F(Sh4Test, DisplacementR0AndGbrForms) {
    const u32 a = kDataBase;
    r(1) = a;
    r(2) = 0x13579BDF;
    r(0) = 0x10;
    r(3) = a;
    prog << movl_store_disp(2, 3, 1);  // @(12,r1) = r2
    prog << movl_load_disp(3, 1, 4);   // r4 = @(12,r1)
    prog << movl_load_r0(3, 5);        // r5 = @(r0 + r3) = @(a+0x10)
    prog << ldc_gbr(1);                // GBR = a
    prog << mov(2, 0) << movl_store_gbr(8); // @(32,GBR) = r0
    prog << mov_i(0, 0) << movl_load_gbr(8); // r0 = @(32,GBR)
    memory->poke32((a + 0x10) & 0x1FFFFFFF, 0x0F0F0F0F);
    run_program();
    EXPECT_EQ(memory->peek32((a + 12) & 0x1FFFFFFF), 0x13579BDFu);
    EXPECT_EQ(r(4), 0x13579BDFu);
    EXPECT_EQ(r(5), 0x0F0F0F0Fu);
    EXPECT_EQ(memory->peek32((a + 32) & 0x1FFFFFFF), 0x13579BDFu);
    EXPECT_EQ(r(0), 0x13579BDFu);
}

} // namespace
} // namespace nc::cpu::test
