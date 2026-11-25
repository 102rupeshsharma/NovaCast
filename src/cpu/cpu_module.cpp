#include "cpu/cpu_module.h"

namespace nc::cpu {

ModuleInfo module_info() {
    return ModuleInfo{
        "cpu",
        "Hitachi SH-4 (SH7750 family), 200 MHz, with FPU and on-chip peripherals",
        ModuleStatus::Partial,
        "Interpreter: full integer/FPU instruction set, delay slots, exceptions, interrupt acceptance, "
        "store queues. Missing: MMU, TMU/INTC/DMAC/SCIF peripherals, caches, cycle timing, JIT.",
    };
}

} // namespace nc::cpu
