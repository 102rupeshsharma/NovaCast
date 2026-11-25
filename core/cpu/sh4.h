#pragma once

// Hitachi SH-4 (SH7750 family) interpreter.
//
// Implemented:
//   - The complete SH-4 integer, system-control and FPU instruction set
//     (single and double precision, paired moves, FIPR, FTRV, FSCA, FSRRA),
//     decoded through a 65,536-entry table.
//   - Delayed branches, including slot-illegal detection and exceptions
//     raised from a delay slot.
//   - Privilege checks, register banks (SR.MD/SR.RB), FPU register banks
//     (FPSCR.FR), and FPSCR.PR/SZ/RM/DN.
//   - General exceptions: illegal/slot-illegal instruction, TRAPA, address
//     errors (misalignment and user-mode access to privileged areas), FPU
//     disable. Exceptions with SR.BL set cause a manual reset, as on hardware.
//   - Interrupt acceptance through one request line (priority + INTEVT code),
//     honouring SR.IMASK and SR.BL; SLEEP and wake-up.
//   - Address regions P0-P4, the CCN registers, store queues and PREF flushes,
//     and the operand cache used as RAM (CCR.ORA).
//
// Not implemented yet (documented in docs/PROGRESS.md):
//   - The MMU (address translation with MMUCR.AT=1, LDTLB, TLB exceptions).
//   - On-chip peripherals: TMU, INTC priority logic, DMAC, BSC, SCIF, RTC,
//     UBC. Their registers read back what was written and log once.
//   - Instruction and operand caches (cache operations are no-ops).
//   - Cycle-accurate timing: every instruction costs one cycle.
//   - FPU exception flags/cause bits and FPU exception traps.

#include "common/types.h"

#include <array>
#include <bitset>
#include <unordered_map>

namespace nc::cpu {

// Physical memory as seen by the CPU. Addresses are 29-bit physical
// addresses and accesses are naturally aligned.
class Sh4Memory {
public:
    virtual ~Sh4Memory() = default;
    virtual u8 read8(u32 address) = 0;
    virtual u16 read16(u32 address) = 0;
    virtual u32 read32(u32 address) = 0;
    virtual void write8(u32 address, u8 value) = 0;
    virtual void write16(u32 address, u16 value) = 0;
    virtual void write32(u32 address, u32 value) = 0;
};

namespace sr {
inline constexpr u32 T = 1u << 0;
inline constexpr u32 S = 1u << 1;
inline constexpr u32 IMASK = 0xFu << 4;
inline constexpr u32 Q = 1u << 8;
inline constexpr u32 M = 1u << 9;
inline constexpr u32 FD = 1u << 15;
inline constexpr u32 BL = 1u << 28;
inline constexpr u32 RB = 1u << 29;
inline constexpr u32 MD = 1u << 30;
inline constexpr u32 kWritableMask = 0x700083F3;
inline constexpr u32 kResetValue = 0x700000F0;
} // namespace sr

namespace fpscr {
inline constexpr u32 RM = 3u << 0;
inline constexpr u32 DN = 1u << 18;
inline constexpr u32 PR = 1u << 19;
inline constexpr u32 SZ = 1u << 20;
inline constexpr u32 FR = 1u << 21;
inline constexpr u32 kWritableMask = 0x003FFFFF;
inline constexpr u32 kResetValue = 0x00040001;
} // namespace fpscr

// Exception event codes (EXPEVT / INTEVT values).
namespace exception_code {
inline constexpr u32 kPowerOnReset = 0x000;
inline constexpr u32 kManualReset = 0x020;
inline constexpr u32 kAddressErrorRead = 0x0E0;
inline constexpr u32 kAddressErrorWrite = 0x100;
inline constexpr u32 kTrap = 0x160;
inline constexpr u32 kIllegalInstruction = 0x180;
inline constexpr u32 kSlotIllegalInstruction = 0x1A0;
inline constexpr u32 kFpuDisable = 0x800;
inline constexpr u32 kSlotFpuDisable = 0x820;
} // namespace exception_code

// Addresses of the cache/exception control (CCN) registers in P4.
namespace ccn {
inline constexpr u32 PTEH = 0xFF000000;
inline constexpr u32 PTEL = 0xFF000004;
inline constexpr u32 TTB = 0xFF000008;
inline constexpr u32 TEA = 0xFF00000C;
inline constexpr u32 MMUCR = 0xFF000010;
inline constexpr u32 CCR = 0xFF00001C;
inline constexpr u32 TRA = 0xFF000020;
inline constexpr u32 EXPEVT = 0xFF000024;
inline constexpr u32 INTEVT = 0xFF000028;
inline constexpr u32 PTEA = 0xFF000034;
inline constexpr u32 QACR0 = 0xFF000038;
inline constexpr u32 QACR1 = 0xFF00003C;
} // namespace ccn

struct Sh4State {
    std::array<u32, 16> r{};     // R0-R15 of the active bank
    std::array<u32, 8> r_bank{}; // R0-R7 of the inactive bank
    u32 sr = sr::kResetValue;
    u32 gbr = 0;
    u32 vbr = 0;
    u32 ssr = 0;
    u32 spc = 0;
    u32 sgr = 0;
    u32 dbr = 0;
    u32 mach = 0;
    u32 macl = 0;
    u32 pr = 0;
    u32 pc = 0xA0000000;
    u32 fpscr = fpscr::kResetValue;
    u32 fpul = 0;
    std::array<u32, 16> fr{}; // FR0-FR15 of the active FPU bank (raw bits)
    std::array<u32, 16> xf{}; // XF0-XF15 (inactive FPU bank)
};

class Sh4 {
public:
    explicit Sh4(Sh4Memory& memory);

    // Power-on reset: registers to their documented reset values, PC=0xA0000000.
    void reset();

    // Executes instructions until at least `cycles` cycles have passed.
    // Returns the cycles used, which can exceed `cycles` by one when the
    // budget ends on a delayed branch: a branch and its delay slot always
    // execute together. A sleeping CPU with no acceptable interrupt consumes
    // the whole budget at once.
    u64 run(u64 cycles);

    // Executes one instruction (a delayed branch together with its slot).
    void step();

    Sh4State& state() { return s_; }
    const Sh4State& state() const { return s_; }

    // Writes SR/FPSCR with all side effects (register bank and FPU bank swaps).
    void set_sr(u32 value);
    void set_fpscr(u32 value);

    // The interrupt controller drives this line. priority 0 means no request.
    void set_interrupt_request(u32 priority, u32 intevt);
    bool sleeping() const { return sleeping_; }

    u64 instructions_executed() const { return instructions_; }

    // Virtual-address accesses, as performed by instructions. They can raise
    // guest exceptions, so they must only be called while an instruction runs.
    u8 read8(u32 address);
    u16 read16(u32 address);
    u32 read32(u32 address);
    void write8(u32 address, u8 value);
    void write16(u32 address, u16 value);
    void write32(u32 address, u32 value);

    // P4 / on-chip register access without privilege checks (for tests,
    // debuggers and the future peripheral modules).
    u32 read_onchip(u32 address, u32 size);
    void write_onchip(u32 address, u32 value, u32 size);

private:
    friend struct Interp;

    struct GuestException {
        u32 code;
        u32 vector_offset;
    };

    template <typename T>
    T read_mem(u32 address);
    template <typename T>
    void write_mem(u32 address, T value);
    void check_access(u32 address, u32 size, bool is_write);

    u16 fetch(u32 address);
    void execute(u16 opcode);
    void delayed_branch(u32 target);
    void flush_store_queue(u32 address);
    u32 oc_ram_offset(u32 address) const;

    [[noreturn]] void raise(u32 code, u32 vector_offset = 0x100);
    [[noreturn]] void raise_illegal();
    [[noreturn]] void raise_address_error(u32 address, bool is_write);
    void check_privileged();
    void check_fpu();
    void enter_exception(u32 code, u32 spc, u32 vector_offset);
    bool accept_interrupt();
    void apply_host_rounding();

    bool privileged() const { return (s_.sr & sr::MD) != 0; }

    Sh4Memory& memory_;
    Sh4State s_;

    // CCN registers.
    u32 pteh_ = 0, ptel_ = 0, ttb_ = 0, tea_ = 0, mmucr_ = 0, ccr_ = 0;
    u32 tra_ = 0, expevt_ = 0, intevt_ = 0, ptea_ = 0, qacr0_ = 0, qacr1_ = 0;

    // Registers of on-chip modules that are not emulated yet.
    std::unordered_map<u32, u32> unemulated_regs_;

    std::array<std::array<u32, 8>, 2> store_queues_{};
    std::array<u8, 8 * 1024> oc_ram_{};

    u32 irq_priority_ = 0;
    u32 irq_intevt_ = 0;

    bool sleeping_ = false;
    bool in_delay_slot_ = false;
    bool pc_written_ = false; // Set by instructions that leave PC at its next value
    bool running_ = false;    // Inside run(): host rounding follows FPSCR.RM
    u64 instructions_ = 0;
};

} // namespace nc::cpu
