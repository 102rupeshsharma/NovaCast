#pragma once

#include <bit>
#include <cstddef>
#include <cstdint>

namespace nc {

using u8 = std::uint8_t;
using u16 = std::uint16_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t;
using s8 = std::int8_t;
using s16 = std::int16_t;
using s32 = std::int32_t;
using s64 = std::int64_t;

// The Dreamcast is little endian. Guest memory will be accessed directly with
// memcpy, which is only correct on a little-endian host.
static_assert(std::endian::native == std::endian::little, "NovaCast requires a little-endian host");

} // namespace nc
