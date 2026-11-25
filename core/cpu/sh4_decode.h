#pragma once

#include "common/types.h"

namespace nc::cpu {

// Mnemonic of an SH-4 opcode as listed in the decode table, or nullptr if the
// opcode is undefined (it raises an illegal instruction exception).
const char* sh4_mnemonic(u16 opcode);

// True for instructions that may not appear in a delay slot.
bool sh4_is_slot_illegal(u16 opcode);

// Number of opcodes matched by more than one decode pattern. Always 0 for a
// correct table; checked by the tests.
int sh4_decode_overlaps();

// Number of distinct instruction patterns in the decode table.
int sh4_pattern_count();

} // namespace nc::cpu
