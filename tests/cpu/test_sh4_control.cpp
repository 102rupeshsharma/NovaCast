// SH-4 branches, delay slots, exceptions, interrupts, register banks and the
// on-chip CCN / store-queue / operand-cache-RAM behaviour.

#include "common/log.h"
#include "cpu/sh4_decode.h"
#include "sh4_harness.h"

#include <cstdio>
#include <string>
#include <vector>

namespace nc::cpu::test {
namespace {

using namespace as;

u32 expevt(Sh4& cpu) {
    return cpu.read_onchip(ccn::EXPEVT, 4);
}

// ----- branches and delay slots ----------------------------------------------

TEST_F(Sh4Test, ConditionalBranchesSkipAndRun) {
    r(1) = 0;
    prog << sett();
    const u32 bt_at = prog.reserve();
    prog << add_i(1, 1);          // skipped by BT
    const u32 after = prog.here();
    prog << add_i(10, 1);
    prog << clrt();
    const u32 bt2 = prog.reserve(); // BT not taken
    prog << add_i(100, 1);
    const u32 end = prog.finish();
    // Patch the branches now that the targets are known.
    prog.set(bt_at, static_cast<u16>(0x8900 | (((after - (bt_at + 4)) / 2) & 0xFF)));
    prog.set(bt2, static_cast<u16>(0x8900 | (((end - (bt2 + 4)) / 2) & 0xFF)));
    run_to(end);
    EXPECT_EQ(r(1), 110u);
}

TEST_F(Sh4Test, DelaySlotRunsForBtsTakenAndNotTaken) {
    r(1) = 0;
    r(2) = 0;
    prog << sett();
    const u32 bts1 = prog.reserve();
    prog << add_i(1, 1); // delay slot of a taken BT/S: runs
    prog << add_i(100, 1); // skipped
    const u32 target = prog.here();
    prog << clrt();
    const u32 bts2 = prog.reserve();
    prog << add_i(1, 2); // delay slot of a not-taken BT/S: runs as a normal instruction
    prog << add_i(10, 2);
    const u32 end = prog.finish();
    prog.set(bts1, static_cast<u16>(0x8D00 | (((target - (bts1 + 4)) / 2) & 0xFF)));
    prog.set(bts2, static_cast<u16>(0x8D00 | (((end - (bts2 + 4)) / 2) & 0xFF)));
    run_to(end);
    EXPECT_EQ(r(1), 1u);
    EXPECT_EQ(r(2), 11u);
}

TEST_F(Sh4Test, BraExecutesDelaySlot) {
    r(1) = 0;
    const u32 bra_at = prog.reserve();
    prog << add_i(1, 1);   // slot
    prog << add_i(50, 1);  // skipped
    const u32 end = prog.finish();
    prog.set(bra_at, static_cast<u16>(0xA000 | (((end - (bra_at + 4)) / 2) & 0xFFF)));
    run_to(end);
    EXPECT_EQ(r(1), 1u);
}

TEST_F(Sh4Test, BsrAndRtsCallAndReturn) {
    r(1) = 0;
    const u32 bsr_at = prog.reserve();
    prog << add_i(1, 1);     // slot of BSR
    prog << add_i(10, 1);    // runs after return
    const u32 end = prog.finish();
    const u32 sub = prog.here();
    prog << sts_pr(5);
    prog << rts();
    prog << add_i(100, 1);   // slot of RTS
    prog.set(bsr_at, static_cast<u16>(0xB000 | (((sub - (bsr_at + 4)) / 2) & 0xFFF)));
    run_to(end);
    EXPECT_EQ(r(5), bsr_at + 4) << "PR = BSR address + 4";
    EXPECT_EQ(r(1), 111u);
}

TEST_F(Sh4Test, JsrJmpBrafBsrf) {
    // JSR @r2 to a subroutine that returns with RTS; then BRAF over two adds;
    // then BSRF to the same subroutine; finally JMP @r4 to the end.
    r(1) = 0;
    prog << jsr(2) << nop();            // base+0
    prog << braf(3) << nop();           // base+4: target = base+8+r3
    prog << add_i(100, 1);              // skipped
    prog << add_i(100, 1);              // skipped
    const u32 after_braf = prog.here(); // base+12
    prog << bsrf(6) << nop();           // base+12: target = base+16+r6
    prog << jmp(4) << nop();
    prog << add_i(100, 1);              // skipped
    const u32 end = prog.finish();
    const u32 sub = prog.here();
    prog << add_i(1, 1) << rts() << nop();
    r(2) = sub;
    r(3) = after_braf - (kCodeBase + 8);
    r(6) = sub - (after_braf + 4);
    r(4) = end;
    run_to(end);
    EXPECT_EQ(r(1), 2u);
    EXPECT_EQ(s().pr, after_braf + 4);
}

TEST_F(Sh4Test, DelaySlotUsesSlotAddressForPcRelative) {
    // The branch is at 2 mod 4 and the slot at 0 mod 4, so using the branch's
    // address instead of the slot's would give a different MOVA result.
    prog << nop();                      // base+0
    const u32 bra_at = prog.reserve();  // base+2
    const u32 slot = prog.reserve();    // base+4: MOVA in the slot
    const u32 end = prog.finish();      // base+6
    const u32 lit = prog.literal(0);    // base+12
    prog.set(bra_at, static_cast<u16>(0xA000 | (((end - (bra_at + 4)) / 2) & 0xFFF)));
    prog.set(slot, mova((lit - ((slot & ~3u) + 4)) / 4));
    run_to(end);
    EXPECT_EQ(r(0), lit);
}

TEST_F(Sh4Test, BranchTargetComputedBeforeSlotModifiesRegister) {
    // JMP @r2 with a slot that overwrites r2: the jump uses the old value.
    const u32 end_marker = kCodeBase + 0x100;
    prog << jmp(2) << mov_i(0, 2);
    Program tail(end_marker);
    const u32 end = tail.finish();
    tail.load(*memory);
    r(2) = end_marker;
    run_to(end);
    EXPECT_EQ(r(2), 0u);
}

// ----- exceptions -------------------------------------------------------------

TEST_F(Sh4Test, TrapaEntersHandler) {
    install_trap_loop(0x100);
    r(0) = 111;
    const u32 sr_before = s().sr;
    const u32 trap_at = prog.here();
    prog << trapa(0x20);
    run_to(kVbr + 0x100);
    EXPECT_EQ(expevt(*cpu), exception_code::kTrap);
    EXPECT_EQ(cpu->read_onchip(ccn::TRA, 4), 0x80u);
    EXPECT_EQ(s().spc, trap_at + 2);
    EXPECT_EQ(s().ssr, sr_before);
    EXPECT_EQ(s().sr & (sr::MD | sr::RB | sr::BL), sr::MD | sr::RB | sr::BL);
    // Register bank 1 is now active: bank 0's R0 moved to r_bank.
    EXPECT_EQ(s().r_bank[0], 111u);
}

TEST_F(Sh4Test, UndefinedOpcodeRaisesIllegalInstruction) {
    install_trap_loop(0x100);
    prog << nop();
    const u32 bad = prog.here();
    prog << 0xFFFD;
    run_to(kVbr + 0x100);
    EXPECT_EQ(expevt(*cpu), exception_code::kIllegalInstruction);
    EXPECT_EQ(s().spc, bad);
}

TEST_F(Sh4Test, BranchInDelaySlotIsSlotIllegal) {
    install_trap_loop(0x100);
    const u32 branch = prog.here();
    prog << 0xA002 << 0xA000; // BRA with a BRA in its slot
    run_to(kVbr + 0x100);
    EXPECT_EQ(expevt(*cpu), exception_code::kSlotIllegalInstruction);
    EXPECT_EQ(s().spc, branch) << "SPC points at the branch, not the slot";
}

TEST_F(Sh4Test, UndefinedOpcodeInDelaySlotIsSlotIllegal) {
    install_trap_loop(0x100);
    prog << 0xA002 << 0xFFFD;
    run_to(kVbr + 0x100);
    EXPECT_EQ(expevt(*cpu), exception_code::kSlotIllegalInstruction);
}

TEST_F(Sh4Test, MisalignedLoadRaisesAddressErrorAndKeepsRegisters) {
    install_trap_loop(0x100);
    r(1) = kDataBase + 2;
    r(2) = 0x12345678;
    prog << movl_postinc(1, 2);
    run_to(kVbr + 0x100);
    EXPECT_EQ(expevt(*cpu), exception_code::kAddressErrorRead);
    EXPECT_EQ(cpu->read_onchip(ccn::TEA, 4), kDataBase + 2);
    // Bank 1 is active in the handler; the faulting instruction's registers
    // (R1/R2 of bank 0) must be unchanged.
    EXPECT_EQ(s().r_bank[1], kDataBase + 2);
    EXPECT_EQ(s().r_bank[2], 0x12345678u);
}

TEST_F(Sh4Test, MisalignedStoreRaisesWriteAddressError) {
    install_trap_loop(0x100);
    r(1) = kDataBase + 1;
    prog << as::nm(0x2001, 0, 1); // MOV.W r0,@r1
    run_to(kVbr + 0x100);
    EXPECT_EQ(expevt(*cpu), exception_code::kAddressErrorWrite);
}

TEST_F(Sh4Test, UserModeCannotUsePrivilegedInstructions) {
    install_trap_loop(0x100);
    prog = Program(0x0C010000); // user code must run from U0 (P0)
    cpu->set_sr(0);              // user mode
    prog << stc_sr(0);
    run_to(kVbr + 0x100);
    EXPECT_EQ(expevt(*cpu), exception_code::kIllegalInstruction);
    EXPECT_EQ(s().spc, 0x0C010000u);
}

TEST_F(Sh4Test, UserModeCannotAccessPrivilegedArea) {
    install_trap_loop(0x100);
    prog = Program(0x0C010000);
    cpu->set_sr(0);
    r(1) = kDataBase; // 0x8C...: P1, privileged
    prog << movl_load(1, 2);
    run_to(kVbr + 0x100);
    EXPECT_EQ(expevt(*cpu), exception_code::kAddressErrorRead);
}

TEST_F(Sh4Test, RteRestoresStateAndRunsDelaySlot) {
    const u32 target = kCodeBase + 0x200;
    Program dest(target);
    const u32 end = dest.finish();
    dest.load(*memory);
    s().spc = target;
    s().ssr = sr::MD | 0x30; // privileged, IMASK=3, bank 0
    r(1) = 0;
    prog << rte() << add_i(5, 1);
    run_to(end);
    EXPECT_EQ(s().sr, sr::MD | 0x30u);
    EXPECT_EQ(r(1), 5u);
}

TEST_F(Sh4Test, ExceptionWithBlockedExceptionsResets) {
    cpu->set_sr(sr::MD | sr::BL | sr::IMASK);
    prog << trapa(1);
    prog.load(*memory);
    s().pc = kCodeBase;
    cpu->step();
    EXPECT_EQ(s().pc, 0xA0000000u);
    EXPECT_EQ(expevt(*cpu), exception_code::kManualReset);
    EXPECT_EQ(s().sr, sr::kResetValue);
}

TEST_F(Sh4Test, FpuDisabledRaisesException) {
    install_trap_loop(0x100);
    cpu->set_sr(kTestSr | sr::FD);
    prog << fadd(0, 1);
    run_to(kVbr + 0x100);
    EXPECT_EQ(expevt(*cpu), exception_code::kFpuDisable);
}

TEST_F(Sh4Test, FpuDisabledInDelaySlotUsesSlotCode) {
    install_trap_loop(0x100);
    cpu->set_sr(kTestSr | sr::FD);
    prog << 0xA002 << fadd(0, 1);
    run_to(kVbr + 0x100);
    EXPECT_EQ(expevt(*cpu), exception_code::kSlotFpuDisable);
}

// ----- interrupts -----------------------------------------------------------

TEST_F(Sh4Test, InterruptMaskedByImask) {
    cpu->set_interrupt_request(5, 0x320);
    prog << nop() << nop();
    prog.load(*memory);
    s().pc = kCodeBase;
    cpu->step(); // IMASK = 15
    EXPECT_EQ(s().pc, kCodeBase + 2);
}

TEST_F(Sh4Test, InterruptAcceptedAboveImask) {
    install_trap_loop(0x600);
    cpu->set_sr(sr::MD | (4u << 4));
    cpu->set_interrupt_request(5, 0x320);
    prog << nop() << nop();
    prog.load(*memory);
    s().pc = kCodeBase;
    cpu->step();
    EXPECT_EQ(cpu->read_onchip(ccn::INTEVT, 4), 0x320u);
    EXPECT_EQ(s().spc, kCodeBase) << "the interrupted instruction runs after RTE";
    EXPECT_EQ(s().pc, kVbr + 0x600 + 0) << "first handler instruction was the BRA . loop";
    EXPECT_TRUE(s().sr & sr::BL);
}

TEST_F(Sh4Test, InterruptBlockedByBl) {
    cpu->set_sr(sr::MD | sr::BL);
    cpu->set_interrupt_request(15, 0x200);
    prog << nop() << nop();
    prog.load(*memory);
    s().pc = kCodeBase;
    cpu->step();
    EXPECT_EQ(s().pc, kCodeBase + 2);
}

TEST_F(Sh4Test, SleepWaitsForInterrupt) {
    install_trap_loop(0x600);
    cpu->set_sr(sr::MD);
    prog << sleep() << nop();
    prog.load(*memory);
    s().pc = kCodeBase;
    cpu->step();
    EXPECT_TRUE(cpu->sleeping());
    EXPECT_EQ(cpu->run(1000), 1000u) << "a sleeping CPU consumes its whole budget";
    EXPECT_TRUE(cpu->sleeping());
    cpu->set_interrupt_request(3, 0x400);
    cpu->run(1);
    EXPECT_FALSE(cpu->sleeping());
    EXPECT_EQ(s().spc, kCodeBase + 2) << "execution resumes after SLEEP";
    EXPECT_EQ(cpu->read_onchip(ccn::INTEVT, 4), 0x400u);
}

// ----- register banks ---------------------------------------------------------

TEST_F(Sh4Test, SrRbSwapsBanks) {
    r(0) = 1;
    r(7) = 7;
    r(8) = 8;
    cpu->set_sr(kTestSr | sr::RB);
    EXPECT_EQ(r(0), 0u);
    EXPECT_EQ(r(8), 8u) << "R8-R15 are not banked";
    r(0) = 100;
    cpu->set_sr(kTestSr);
    EXPECT_EQ(r(0), 1u);
    EXPECT_EQ(r(7), 7u);
    EXPECT_EQ(s().r_bank[0], 100u);
}

TEST_F(Sh4Test, UserModeAlwaysUsesBank0) {
    r(0) = 42;
    cpu->set_sr(sr::RB); // MD=0: RB is ignored
    EXPECT_EQ(r(0), 42u);
}

TEST_F(Sh4Test, LdcStcBankRegisters) {
    r(1) = 0xBEEF;
    prog << ldc_bank(1, 3) << stc_bank(3, 2);
    run_program();
    EXPECT_EQ(s().r_bank[3], 0xBEEFu);
    EXPECT_EQ(r(2), 0xBEEFu);
}

// ----- on-chip: CCN, store queues, OC RAM, P4 alias ---------------------------

TEST_F(Sh4Test, StoreQueueFlushWritesBurst) {
    cpu->write_onchip(ccn::QACR0, 3u << 2, 4); // external area 3 (0x0C000000)
    // Fill SQ0 through the CPU so the P4 write path is exercised.
    r(1) = 0xE0100000;
    r(2) = 0xA0000000;
    prog << mov(2, 3);
    for (u32 i = 0; i < 8; ++i)
        prog << add_i(1, 3) << movl_store_disp(3, i, 1); // @(i*4, r1) = r3
    prog << pref(1);
    run_program();
    for (u32 i = 0; i < 8; ++i)
        EXPECT_EQ(memory->peek32(0x0C100000 + i * 4), 0xA0000001u + i) << i;
}

TEST_F(Sh4Test, CcnRegistersReadBackAndAliasInArea7) {
    cpu->write_onchip(ccn::PTEH, 0x12345678, 4);
    EXPECT_EQ(cpu->read_onchip(ccn::PTEH, 4), 0x12345678u);
    r(1) = 0x1F000000; // P0 alias of 0xFF000000
    prog << movl_load(1, 2);
    run_program();
    EXPECT_EQ(r(2), 0x12345678u);
}

TEST_F(Sh4Test, CcrInvalidateBitsDoNotStick) {
    cpu->write_onchip(ccn::CCR, 0x0000090B, 4); // OCE|WT|OCI|ICE|ICI
    EXPECT_EQ(cpu->read_onchip(ccn::CCR, 4), 0x00000103u);
}

TEST_F(Sh4Test, OperandCacheRam) {
    cpu->write_onchip(ccn::CCR, 1u << 5, 4); // ORA
    r(1) = 0x7C001000;
    r(2) = 0x55AA55AA;
    prog << movl_store(2, 1) << movl_load(1, 3);
    run_program();
    EXPECT_EQ(r(3), 0x55AA55AAu);
    EXPECT_EQ(memory->unmapped, 0) << "OC RAM accesses must not reach external memory";
}

TEST_F(Sh4Test, UnemulatedOnchipRegisterReadsBackWrites) {
    cpu->write_onchip(0xFFD80004, 0x1234, 4); // TMU TOCR region: not emulated
    EXPECT_EQ(cpu->read_onchip(0xFFD80004, 4), 0x1234u);
}

// ----- robustness ------------------------------------------------------------

// Executes every one of the 65,536 opcodes once, in privileged mode, in FPU
// single-precision, double-precision and paired-move modes, with registers
// pointing at RAM. This checks that no handler crashes the host or (under
// the UBSan build) triggers undefined behaviour, and that every opcode really
// executes (or raises a guest exception). It says nothing about whether the
// results are correct: that is what the other tests are for.
TEST_F(Sh4Test, EveryOpcodeExecutesWithoutHostFaults) {
    const LogLevel saved = log_level();
    log_set_level(LogLevel::Off); // undefined-PR and similar warnings are expected here
    std::vector<std::string> not_executed;
    for (u32 fpu_mode : {u32{0}, fpscr::PR, fpscr::SZ}) {
        for (u32 op = 0; op < 65536; ++op) {
            // A full reset each time: opcodes such as SLEEP leave internal
            // state behind that would make the following steps do nothing.
            cpu->reset();
            cpu->set_sr(kTestSr);
            cpu->set_fpscr(fpscr::DN | fpu_mode);
            s().vbr = kVbr;
            for (u32 i = 0; i < 16; ++i)
                s().r[i] = kDataBase + 0x1000 * i;
            s().gbr = kDataBase;
            s().pr = s().spc = kCodeBase + 0x100;
            const u32 at = kCodeBase;
            memory->poke16(at & 0x1FFFFFFF, static_cast<u16>(op));
            memory->poke16((at + 2) & 0x1FFFFFFF, as::nop());
            s().pc = at;
            const u64 before = cpu->instructions_executed();
            cpu->step();
            if (cpu->instructions_executed() == before && not_executed.size() < 20) {
                const char* name = sh4_mnemonic(static_cast<u16>(op));
                char text[96];
                std::snprintf(text, sizeof(text), "%04X %s (fpscr mode %X)", op, name ? name : "?", fpu_mode);
                not_executed.emplace_back(text);
            }
        }
    }
    log_set_level(saved);
    std::string list;
    for (const auto& m : not_executed)
        list += "\n  " + m;
    EXPECT_TRUE(not_executed.empty()) << "opcodes that did not execute:" << list;
}

// ----- decoder ---------------------------------------------------------------

TEST(Sh4Decoder, PatternsDoNotOverlap) {
    EXPECT_EQ(sh4_decode_overlaps(), 0);
}

TEST(Sh4Decoder, KnownOpcodes) {
    EXPECT_STREQ(sh4_mnemonic(0x0009), "NOP");
    EXPECT_STREQ(sh4_mnemonic(0x6123), "MOV Rm,Rn");
    EXPECT_STREQ(sh4_mnemonic(0xF3FD), "FSCHG");
    EXPECT_STREQ(sh4_mnemonic(0xFBFD), "FRCHG");
    EXPECT_STREQ(sh4_mnemonic(0xF1FD), "FTRV XMTRX,FVn");
    EXPECT_STREQ(sh4_mnemonic(0xF0FD), "FSCA FPUL,DRn");
    EXPECT_STREQ(sh4_mnemonic(0x408E), "LDC Rm,Rn_BANK");
    EXPECT_EQ(sh4_mnemonic(0xFFFD), nullptr);
    EXPECT_EQ(sh4_mnemonic(0x0000), nullptr);
    EXPECT_EQ(sh4_mnemonic(0x2003), nullptr);
}

TEST(Sh4Decoder, SlotIllegalSet) {
    for (u16 op : {u16{0xA000}, u16{0xB000}, u16{0x0023}, u16{0x0003}, u16{0x402B}, u16{0x400B}, u16{0x000B},
                   u16{0x002B}, u16{0x8900}, u16{0x8B00}, u16{0x8D00}, u16{0x8F00}, u16{0xC300}})
        EXPECT_TRUE(sh4_is_slot_illegal(op)) << std::hex << op;
    EXPECT_FALSE(sh4_is_slot_illegal(0x0009));
    EXPECT_FALSE(sh4_is_slot_illegal(0x6123));
}

TEST(Sh4Decoder, PatternCount) {
    // Tripwire: the table has 209 patterns (SHAL and SHLL share a handler but
    // are separate encodings). Adding or removing one must be deliberate.
    EXPECT_EQ(sh4_pattern_count(), 209);
}

} // namespace
} // namespace nc::cpu::test
