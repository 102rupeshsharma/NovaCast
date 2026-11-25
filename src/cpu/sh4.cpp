#include "cpu/sh4.h"

#include "common/log.h"

#include <cfenv>
#include <cstring>
#include <utility>

namespace nc::cpu {

namespace {

constexpr u32 kCcrOra = 1u << 5;  // Operand cache RAM mode
constexpr u32 kCcrOix = 1u << 7;  // Operand cache index mode
constexpr u32 kCcrOci = 1u << 3;  // OC invalidate (write-only, self-clearing)
constexpr u32 kCcrIci = 1u << 11; // IC invalidate (write-only, self-clearing)
constexpr u32 kMmucrAt = 1u << 0;
constexpr u32 kMmucrTi = 1u << 2; // TLB invalidate (write-only)
constexpr u32 kMmucrSqmd = 1u << 9;

bool in_store_queue_area(u32 address) {
    return (address & 0xFC000000) == 0xE0000000;
}

// Restores the host floating-point rounding mode when the CPU stops running.
class RoundingGuard {
public:
    RoundingGuard() : saved_(std::fegetround()) {}
    ~RoundingGuard() { std::fesetround(saved_); }
    RoundingGuard(const RoundingGuard&) = delete;
    RoundingGuard& operator=(const RoundingGuard&) = delete;

private:
    int saved_;
};

} // namespace

Sh4::Sh4(Sh4Memory& memory) : memory_(memory) {
    reset();
}

void Sh4::reset() {
    s_ = Sh4State{};
    pteh_ = ptel_ = ttb_ = tea_ = mmucr_ = ccr_ = 0;
    tra_ = expevt_ = intevt_ = ptea_ = qacr0_ = qacr1_ = 0;
    expevt_ = exception_code::kPowerOnReset;
    unemulated_regs_.clear();
    store_queues_ = {};
    oc_ram_ = {};
    irq_priority_ = 0;
    irq_intevt_ = 0;
    sleeping_ = false;
    in_delay_slot_ = false;
    pc_written_ = false;
}

void Sh4::set_sr(u32 value) {
    value &= sr::kWritableMask;
    const bool old_bank = (s_.sr & sr::MD) && (s_.sr & sr::RB);
    const bool new_bank = (value & sr::MD) && (value & sr::RB);
    if (old_bank != new_bank) {
        for (int i = 0; i < 8; ++i)
            std::swap(s_.r[i], s_.r_bank[i]);
    }
    s_.sr = value;
}

void Sh4::set_fpscr(u32 value) {
    value &= fpscr::kWritableMask;
    if ((value ^ s_.fpscr) & fpscr::FR)
        std::swap(s_.fr, s_.xf);
    s_.fpscr = value;
    if (running_)
        apply_host_rounding();
}

void Sh4::apply_host_rounding() {
    // RM=1 is round to zero; RM=0 (and the reserved values) round to nearest.
    std::fesetround((s_.fpscr & fpscr::RM) == 1 ? FE_TOWARDZERO : FE_TONEAREST);
}

void Sh4::set_interrupt_request(u32 priority, u32 intevt) {
    irq_priority_ = priority & 0xF;
    irq_intevt_ = intevt;
}

// ---------------------------------------------------------------------------
// Execution
// ---------------------------------------------------------------------------

u64 Sh4::run(u64 cycles) {
    RoundingGuard guard;
    running_ = true;
    apply_host_rounding();

    const u64 start = instructions_;
    while (instructions_ - start < cycles) {
        if (sleeping_ && !accept_interrupt()) {
            running_ = false;
            return cycles; // Idle until something wakes the CPU.
        }
        step();
    }
    running_ = false;
    return instructions_ - start;
}

void Sh4::step() {
    accept_interrupt();
    if (sleeping_)
        return;

    const u32 pc = s_.pc;
    pc_written_ = false;
    in_delay_slot_ = false;
    try {
        const u16 opcode = fetch(pc);
        execute(opcode);
        ++instructions_;
        if (!pc_written_)
            s_.pc = pc + 2;
    } catch (const GuestException& e) {
        // For an exception in a delay slot, SPC is the branch's address.
        in_delay_slot_ = false;
        ++instructions_;
        enter_exception(e.code, pc, e.vector_offset);
    }
}

void Sh4::delayed_branch(u32 target) {
    const u32 slot_pc = s_.pc + 2;
    in_delay_slot_ = true;
    const u16 opcode = fetch(slot_pc);
    s_.pc = slot_pc; // PC-relative instructions in the slot use the slot's address.
    execute(opcode);
    ++instructions_;
    in_delay_slot_ = false;
    s_.pc = target;
    pc_written_ = true;
}

u16 Sh4::fetch(u32 address) {
    return read_mem<u16>(address);
}

bool Sh4::accept_interrupt() {
    if (irq_priority_ == 0 || (s_.sr & sr::BL))
        return false;
    if (irq_priority_ <= ((s_.sr & sr::IMASK) >> 4))
        return false;

    sleeping_ = false;
    intevt_ = irq_intevt_;
    s_.ssr = s_.sr;
    s_.spc = s_.pc;
    s_.sgr = s_.r[15];
    set_sr(s_.sr | sr::MD | sr::RB | sr::BL);
    s_.pc = s_.vbr + 0x600;
    return true;
}

void Sh4::enter_exception(u32 code, u32 spc, u32 vector_offset) {
    if (s_.sr & sr::BL) {
        // A general exception while exceptions are blocked resets the CPU.
        LOG_ERROR("sh4", "exception %03X at PC=%08X with SR.BL=1: manual reset", code, spc);
        const u64 executed = instructions_;
        reset();
        instructions_ = executed;
        expevt_ = exception_code::kManualReset;
        return;
    }
    s_.ssr = s_.sr;
    s_.spc = spc;
    s_.sgr = s_.r[15];
    expevt_ = code;
    set_sr(s_.sr | sr::MD | sr::RB | sr::BL);
    s_.pc = s_.vbr + vector_offset;
}

void Sh4::raise(u32 code, u32 vector_offset) {
    throw GuestException{code, vector_offset};
}

void Sh4::raise_illegal() {
    raise(in_delay_slot_ ? exception_code::kSlotIllegalInstruction : exception_code::kIllegalInstruction);
}

void Sh4::raise_address_error(u32 address, bool is_write) {
    tea_ = address;
    raise(is_write ? exception_code::kAddressErrorWrite : exception_code::kAddressErrorRead);
}

void Sh4::check_privileged() {
    if (!privileged())
        raise_illegal();
}

void Sh4::check_fpu() {
    if (s_.sr & sr::FD)
        raise(in_delay_slot_ ? exception_code::kSlotFpuDisable : exception_code::kFpuDisable);
}

// ---------------------------------------------------------------------------
// Memory access
// ---------------------------------------------------------------------------

void Sh4::check_access(u32 address, u32 size, bool is_write) {
    if (address & (size - 1))
        raise_address_error(address, is_write);
    if (!privileged() && address >= 0x80000000) {
        // User mode may only touch the store queues, and only when MMUCR.SQMD=0.
        if (!(in_store_queue_area(address) && !(mmucr_ & kMmucrSqmd)))
            raise_address_error(address, is_write);
    }
}

u32 Sh4::oc_ram_offset(u32 address) const {
    const u32 half = (ccr_ & kCcrOix) ? (address >> 25) & 1 : (address >> 13) & 1;
    return (half << 12) | (address & 0xFFF);
}

template <typename T>
T Sh4::read_mem(u32 address) {
    constexpr u32 size = sizeof(T);
    check_access(address, size, false);

    if (address >= 0xE0000000)
        return static_cast<T>(read_onchip(address, size));
    if ((address & 0xFC000000) == 0x7C000000 && (ccr_ & kCcrOra)) {
        T value;
        std::memcpy(&value, &oc_ram_[oc_ram_offset(address)], size);
        return value;
    }
    const u32 physical = address & 0x1FFFFFFF;
    if (physical >= 0x1C000000) {
        if (physical >= 0x1F000000)
            return static_cast<T>(read_onchip(physical | 0xE0000000, size));
        LOG_WARN("sh4", "read%u from reserved area 7 address %08X", size * 8, address);
        return 0;
    }
    if constexpr (size == 1)
        return memory_.read8(physical);
    else if constexpr (size == 2)
        return memory_.read16(physical);
    else
        return memory_.read32(physical);
}

template <typename T>
void Sh4::write_mem(u32 address, T value) {
    constexpr u32 size = sizeof(T);
    check_access(address, size, true);

    if (address >= 0xE0000000) {
        write_onchip(address, value, size);
        return;
    }
    if ((address & 0xFC000000) == 0x7C000000 && (ccr_ & kCcrOra)) {
        std::memcpy(&oc_ram_[oc_ram_offset(address)], &value, size);
        return;
    }
    const u32 physical = address & 0x1FFFFFFF;
    if (physical >= 0x1C000000) {
        if (physical >= 0x1F000000) {
            write_onchip(physical | 0xE0000000, value, size);
            return;
        }
        LOG_WARN("sh4", "write%u to reserved area 7 address %08X ignored", size * 8, address);
        return;
    }
    if constexpr (size == 1)
        memory_.write8(physical, value);
    else if constexpr (size == 2)
        memory_.write16(physical, value);
    else
        memory_.write32(physical, value);
}

u8 Sh4::read8(u32 address) {
    return read_mem<u8>(address);
}
u16 Sh4::read16(u32 address) {
    return read_mem<u16>(address);
}
u32 Sh4::read32(u32 address) {
    return read_mem<u32>(address);
}
void Sh4::write8(u32 address, u8 value) {
    write_mem<u8>(address, value);
}
void Sh4::write16(u32 address, u16 value) {
    write_mem<u16>(address, value);
}
void Sh4::write32(u32 address, u32 value) {
    write_mem<u32>(address, value);
}

// ---------------------------------------------------------------------------
// On-chip registers (P4)
// ---------------------------------------------------------------------------

u32 Sh4::read_onchip(u32 address, u32 size) {
    if (in_store_queue_area(address)) {
        const u32 word = store_queues_[(address >> 5) & 1][(address >> 2) & 7];
        return size == 4 ? word : (word >> ((address & 3) * 8)) & ((1u << (size * 8)) - 1);
    }
    if (address < 0xFF000000) {
        // Cache and TLB address/data arrays (0xF0000000-0xF7FFFFFF).
        LOG_DEBUG("sh4", "read%u from cache/TLB array %08X: caches are not emulated", size * 8, address);
        return 0;
    }
    switch (address) {
    case ccn::PTEH:
        return pteh_;
    case ccn::PTEL:
        return ptel_;
    case ccn::TTB:
        return ttb_;
    case ccn::TEA:
        return tea_;
    case ccn::MMUCR:
        return mmucr_;
    case ccn::CCR:
        return ccr_;
    case ccn::TRA:
        return tra_;
    case ccn::EXPEVT:
        return expevt_;
    case ccn::INTEVT:
        return intevt_;
    case ccn::PTEA:
        return ptea_;
    case ccn::QACR0:
        return qacr0_;
    case ccn::QACR1:
        return qacr1_;
    default:
        break;
    }
    auto [it, inserted] = unemulated_regs_.try_emplace(address, 0);
    if (inserted)
        LOG_WARN("sh4", "read%u from on-chip register %08X: module not emulated yet", size * 8, address);
    return it->second;
}

void Sh4::write_onchip(u32 address, u32 value, u32 size) {
    if (in_store_queue_area(address)) {
        u32& word = store_queues_[(address >> 5) & 1][(address >> 2) & 7];
        if (size == 4) {
            word = value;
        } else {
            const u32 shift = (address & 3) * 8;
            const u32 mask = ((1u << (size * 8)) - 1) << shift;
            word = (word & ~mask) | ((value << shift) & mask);
        }
        return;
    }
    if (address < 0xFF000000) {
        LOG_DEBUG("sh4", "write%u to cache/TLB array %08X ignored: caches are not emulated", size * 8, address);
        return;
    }
    switch (address) {
    case ccn::PTEH:
        pteh_ = value;
        return;
    case ccn::PTEL:
        ptel_ = value;
        return;
    case ccn::TTB:
        ttb_ = value;
        return;
    case ccn::TEA:
        tea_ = value;
        return;
    case ccn::MMUCR:
        if (value & kMmucrAt)
            LOG_WARN("sh4", "MMUCR.AT=1 written: address translation is not implemented");
        mmucr_ = value & ~kMmucrTi;
        return;
    case ccn::CCR:
        ccr_ = value & ~(kCcrOci | kCcrIci);
        return;
    case ccn::TRA:
        tra_ = value;
        return;
    case ccn::EXPEVT:
        expevt_ = value;
        return;
    case ccn::INTEVT:
        intevt_ = value;
        return;
    case ccn::PTEA:
        ptea_ = value;
        return;
    case ccn::QACR0:
        qacr0_ = value;
        return;
    case ccn::QACR1:
        qacr1_ = value;
        return;
    default:
        break;
    }
    auto [it, inserted] = unemulated_regs_.try_emplace(address, value);
    if (inserted) {
        LOG_WARN("sh4", "write%u %08X to on-chip register %08X: module not emulated yet", size * 8, value,
                 address);
    }
    it->second = value;
}

void Sh4::flush_store_queue(u32 address) {
    const u32 queue = (address >> 5) & 1;
    const u32 qacr = queue ? qacr1_ : qacr0_;
    const u32 target = (((qacr >> 2) & 7) << 26) | (address & 0x03FFFFE0);
    for (u32 i = 0; i < 8; ++i)
        memory_.write32(target + i * 4, store_queues_[queue][i]);
}

} // namespace nc::cpu
