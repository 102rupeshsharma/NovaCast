// SH-4 FPU instructions.

#include "sh4_harness.h"

#include <cfenv>
#include <cmath>
#include <limits>

namespace nc::cpu::test {
namespace {

using namespace as;

class Sh4FpuTest : public Sh4Test {
protected:
    Sh4FpuTest() { cpu->set_fpscr(fpscr::DN); } // single precision, round to nearest

    // Runs the program through Sh4::run(), which applies FPSCR.RM to the host.
    void run_with_rounding(int instructions) {
        prog.finish();
        prog.load(*memory);
        s().pc = prog.base();
        cpu->run(static_cast<u64>(instructions));
    }
};

TEST_F(Sh4FpuTest, SingleArithmetic) {
    set_f(1, 1.5f);
    set_f(2, 2.25f);
    set_f(3, 3.0f);
    set_f(4, -2.0f);
    set_f(5, 1.0f);
    set_f(6, 4.0f);
    set_f(7, 10.0f);
    set_f(8, 0.5f);
    prog << fadd(2, 1) << fmul(4, 3) << fdiv(6, 5) << fsub(8, 7);
    run_program();
    EXPECT_EQ(f(1), 3.75f);
    EXPECT_EQ(f(3), -6.0f);
    EXPECT_EQ(f(5), 0.25f);
    EXPECT_EQ(f(7), 9.5f);
}

TEST_F(Sh4FpuTest, Compare) {
    set_f(1, 2.0f);
    set_f(2, 1.0f);
    s().fr[3] = 0x7FC00000; // NaN
    prog << fcmp_gt(2, 1) << 0x0429 /* MOVT r4 */ << fcmp_gt(1, 2) << 0x0529 /* MOVT r5 */;
    prog << fcmp_eq(3, 3) << 0x0629 /* MOVT r6 */ << fcmp_eq(2, 2);
    run_program();
    EXPECT_EQ(r(4), 1u) << "FR1 > FR2";
    EXPECT_EQ(r(5), 0u);
    EXPECT_EQ(r(6), 0u) << "NaN is never equal";
    EXPECT_TRUE(s().sr & sr::T);
}

TEST_F(Sh4FpuTest, FloatAndTruncate) {
    r(0) = static_cast<u32>(-7);
    set_f(2, 3.9f);
    set_f(3, -3.9f);
    set_f(4, 3e9f);
    set_f(5, -3e9f);
    s().fr[6] = 0x7FC00000;
    prog << lds_fpul(0) << float_(1);
    prog << ftrc(2) << sts_fpul(8) << ftrc(3) << sts_fpul(9) << ftrc(4) << sts_fpul(10);
    prog << ftrc(5) << sts_fpul(11) << ftrc(6) << sts_fpul(12);
    run_program();
    EXPECT_EQ(f(1), -7.0f);
    EXPECT_EQ(r(8), 3u);
    EXPECT_EQ(static_cast<s32>(r(9)), -3);
    EXPECT_EQ(r(10), 0x7FFFFFFFu) << "positive overflow saturates";
    EXPECT_EQ(r(11), 0x80000000u) << "negative overflow saturates";
    EXPECT_EQ(r(12), 0x80000000u) << "NaN converts to 0x80000000";
}

TEST_F(Sh4FpuTest, RoundingModeNearest) {
    set_f(1, 1.0f);
    set_f(2, std::ldexp(0.75f, -23)); // 0.75 ulp of 1.0
    prog << fadd(2, 1);
    run_with_rounding(1);
    EXPECT_EQ(s().fr[1], 0x3F800001u);
    EXPECT_EQ(std::fegetround(), FE_TONEAREST) << "host rounding restored after run()";
}

TEST_F(Sh4FpuTest, RoundingModeToZero) {
    cpu->set_fpscr(fpscr::DN | 1); // RM = 1
    set_f(1, 1.0f);
    set_f(2, std::ldexp(0.75f, -23));
    prog << fadd(2, 1);
    run_with_rounding(1);
    EXPECT_EQ(s().fr[1], 0x3F800000u);
    EXPECT_EQ(std::fegetround(), FE_TONEAREST);
}

TEST_F(Sh4FpuTest, RoundingModeChangeInsideRunTakesEffect) {
    r(0) = fpscr::DN | 1;
    set_f(1, 1.0f);
    set_f(2, std::ldexp(0.75f, -23));
    prog << lds_fpscr(0) << fadd(2, 1);
    run_with_rounding(2);
    EXPECT_EQ(s().fr[1], 0x3F800000u);
}

TEST_F(Sh4FpuTest, DenormalsFlushWhenDnSet) {
    s().fr[1] = 0x00000001; // smallest denormal
    set_f(2, 0.0f);
    prog << fadd(2, 1);
    run_program();
    EXPECT_EQ(s().fr[1], 0u);
}

TEST_F(Sh4FpuTest, DenormalsKeptWhenDnClear) {
    cpu->set_fpscr(0);
    s().fr[1] = 0x00000001;
    set_f(2, 0.0f);
    prog << fadd(2, 1);
    run_program();
    EXPECT_EQ(s().fr[1], 0x00000001u);
}

TEST_F(Sh4FpuTest, InvalidOperationGivesSh4QuietNan) {
    set_f(1, 0.0f);
    set_f(2, 0.0f);
    prog << fdiv(2, 1);
    run_program();
    EXPECT_EQ(s().fr[1], 0x7FBFFFFFu);
}

TEST_F(Sh4FpuTest, DoublePrecision) {
    cpu->set_fpscr(fpscr::DN | fpscr::PR);
    set_d(0, 1.5);
    set_d(2, 2.25);
    set_d(4, 3.0);
    set_d(6, 0.5);
    set_d(8, 2.0);
    prog << fadd(2, 0) << fmul(6, 4) << fsqrt(8) << fcmp_gt(2, 0);
    run_program();
    EXPECT_EQ(d(0), 3.75);
    EXPECT_EQ(d(4), 1.5);
    EXPECT_EQ(d(8), std::sqrt(2.0));
    EXPECT_TRUE(s().sr & sr::T) << "DR0 (3.75) > DR2 (2.25)";
    EXPECT_EQ(s().fr[0], 0x400E0000u) << "high word of 3.75 is in the even register";
    EXPECT_EQ(s().fr[1], 0u);
}

TEST_F(Sh4FpuTest, DoubleSingleConversions) {
    cpu->set_fpscr(fpscr::DN | fpscr::PR);
    r(0) = std::bit_cast<u32>(1.25f);
    set_d(4, 0.1);
    prog << lds_fpul(0) << fcnvsd(2) << fcnvds(4) << sts_fpul(5);
    run_program();
    EXPECT_EQ(d(2), 1.25);
    EXPECT_EQ(r(5), std::bit_cast<u32>(static_cast<float>(0.1)));
}

TEST_F(Sh4FpuTest, PairedMovesWithSz) {
    cpu->set_fpscr(fpscr::DN | fpscr::SZ);
    const u32 a = kDataBase;
    memory->poke32(a & 0x1FFFFFFF, 0x11111111);
    memory->poke32((a + 4) & 0x1FFFFFFF, 0x22222222);
    r(1) = a;
    r(4) = a + 0x20;
    r(6) = a;
    prog << fmov_load(1, 2);  // DR2  <- @r1
    prog << fmov_load(1, 3);  // XD2  <- @r1
    prog << fmov(2, 5);       // XD4  <- DR2
    prog << fmov_store(3, 4); // @r4  <- XD2
    prog << fmov_postinc(6, 8); // DR8 <- @r6, r6 += 8
    run_program();
    EXPECT_EQ(s().fr[2], 0x11111111u);
    EXPECT_EQ(s().fr[3], 0x22222222u);
    EXPECT_EQ(s().xf[2], 0x11111111u);
    EXPECT_EQ(s().xf[3], 0x22222222u);
    EXPECT_EQ(s().xf[4], 0x11111111u);
    EXPECT_EQ(s().xf[5], 0x22222222u);
    EXPECT_EQ(memory->peek32((a + 0x20) & 0x1FFFFFFF), 0x11111111u);
    EXPECT_EQ(memory->peek32((a + 0x24) & 0x1FFFFFFF), 0x22222222u);
    EXPECT_EQ(r(6), a + 8);
}

TEST_F(Sh4FpuTest, PairedMoveNeedsEightByteAlignment) {
    install_trap_loop(0x100);
    cpu->set_fpscr(fpscr::DN | fpscr::SZ);
    r(1) = kDataBase + 4;
    prog << fmov_load(1, 2);
    run_to(kVbr + 0x100);
    EXPECT_EQ(cpu->read_onchip(ccn::EXPEVT, 4), exception_code::kAddressErrorRead);
}

TEST_F(Sh4FpuTest, InnerProduct) {
    for (int i = 0; i < 4; ++i) {
        set_f(i, static_cast<float>(i + 1));     // FV0 = (1,2,3,4)
        set_f(4 + i, static_cast<float>(i + 5)); // FV4 = (5,6,7,8)
    }
    prog << fipr(1, 0); // FR3 = FV0 . FV4
    run_program();
    EXPECT_EQ(f(3), 70.0f);
    EXPECT_EQ(f(0), 1.0f) << "only FR3 changes";
}

TEST_F(Sh4FpuTest, MatrixTransform) {
    // XMTRX is column-major in XF0-XF15: scale by 2 and translate by (10,20,30).
    for (int i = 0; i < 16; ++i)
        s().xf[i] = 0;
    for (int i = 0; i < 3; ++i)
        s().xf[i * 5] = std::bit_cast<u32>(2.0f);
    s().xf[12] = std::bit_cast<u32>(10.0f);
    s().xf[13] = std::bit_cast<u32>(20.0f);
    s().xf[14] = std::bit_cast<u32>(30.0f);
    s().xf[15] = std::bit_cast<u32>(1.0f);
    set_f(4, 1.0f);
    set_f(5, 2.0f);
    set_f(6, 3.0f);
    set_f(7, 1.0f);
    prog << ftrv(1); // FV4
    run_program();
    EXPECT_EQ(f(4), 12.0f);
    EXPECT_EQ(f(5), 24.0f);
    EXPECT_EQ(f(6), 36.0f);
    EXPECT_EQ(f(7), 1.0f);
}

TEST_F(Sh4FpuTest, SineCosine) {
    r(0) = 0x4000; // a quarter turn
    r(1) = 0;
    prog << lds_fpul(0) << fsca(2) << lds_fpul(1) << fsca(4);
    run_program();
    EXPECT_FLOAT_EQ(f(2), 1.0f);
    EXPECT_NEAR(f(3), 0.0f, 1e-6f);
    EXPECT_EQ(f(4), 0.0f);
    EXPECT_EQ(f(5), 1.0f);
}

TEST_F(Sh4FpuTest, SquareRootsAndMultiplyAccumulate) {
    set_f(1, 4.0f);
    set_f(2, 2.0f);
    set_f(0, 2.0f);
    set_f(3, 3.0f);
    set_f(4, 4.0f);
    prog << fsrra(1) << fsqrt(2) << fmac(3, 4); // FR4 = FR0*FR3 + FR4
    run_program();
    EXPECT_EQ(f(1), 0.5f);
    EXPECT_EQ(f(2), std::sqrt(2.0f));
    EXPECT_EQ(f(4), 10.0f);
}

TEST_F(Sh4FpuTest, SignOpsAndConstants) {
    set_f(1, -2.5f);
    set_f(2, -2.5f);
    prog << fabs_(1) << fneg(2) << fldi0(3) << fldi1(4);
    run_program();
    EXPECT_EQ(f(1), 2.5f);
    EXPECT_EQ(f(2), 2.5f);
    EXPECT_EQ(s().fr[3], 0u);
    EXPECT_EQ(f(4), 1.0f);
}

TEST_F(Sh4FpuTest, BankAndSizeToggles) {
    s().fr[1] = 0xAAAA0001;
    s().xf[1] = 0xBBBB0001;
    prog << frchg() << fschg();
    run_program();
    EXPECT_EQ(s().fr[1], 0xBBBB0001u);
    EXPECT_EQ(s().xf[1], 0xAAAA0001u);
    EXPECT_TRUE(s().fpscr & fpscr::FR);
    EXPECT_TRUE(s().fpscr & fpscr::SZ);
}

TEST_F(Sh4FpuTest, FpscrWritesAreMasked) {
    r(1) = 0xFFFFFFFF;
    prog << lds_fpscr(1) << sts_fpscr(2);
    run_program();
    EXPECT_EQ(r(2), fpscr::kWritableMask);
}

TEST_F(Sh4FpuTest, FpulTransfers) {
    s().fpul = 0x40490FDB;
    s().fr[7] = 0x3F800000;
    prog << n_(0xF00D, 3) /* FSTS FPUL,FR3 */ << n_(0xF01D, 7) /* FLDS FR7,FPUL */;
    run_program();
    EXPECT_EQ(s().fr[3], 0x40490FDBu);
    EXPECT_EQ(s().fpul, 0x3F800000u);
}

} // namespace
} // namespace nc::cpu::test
