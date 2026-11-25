#pragma once

// Dreamcast physical address space (29-bit, as seen after the SH-4 strips
// the region bits). Implemented regions:
//
//   0x00000000-0x001FFFFF  Boot ROM, 2 MB, read only
//   0x00200000-0x0021FFFF  Flash ROM, 128 KB (reads only, see below)
//   0x04000000-0x04FFFFFF  VRAM, 64-bit access path (8 MB, mirrored)
//   0x05000000-0x05FFFFFF  VRAM, 32-bit access path (8 MB, mirrored)
//   0x06000000-0x07FFFFFF  mirrors of the two VRAM paths
//   0x0C000000-0x0FFFFFFF  Main RAM, 16 MB, mirrored four times
//
// Device register blocks (Holly, PVR, AICA, ...) are attached later with
// map_device(). Everything else is unmapped: reads return 0, writes are
// dropped, and the first access to each 64 KB page is logged.
//
// Not implemented yet: flash programming/erase commands (writes to flash
// are logged and ignored), and the device regions themselves.

#include "common/error.h"
#include "common/types.h"

#include <array>
#include <bitset>
#include <memory>
#include <span>

namespace nc::memory {

inline constexpr u32 kBiosSize = 2u * 1024 * 1024;
inline constexpr u32 kFlashSize = 128u * 1024;
inline constexpr u32 kRamSize = 16u * 1024 * 1024;
inline constexpr u32 kVramSize = 8u * 1024 * 1024;

inline constexpr u32 kPhysicalMask = 0x1FFFFFFF;

// Translates an offset in the 32-bit VRAM view to the 64-bit (storage)
// layout. The 32-bit view interleaves the two 4 MB banks word by word.
constexpr u32 vram32_to_64(u32 offset32) {
    offset32 &= kVramSize - 1;
    return ((offset32 & 0x003FFFFC) << 1) | ((offset32 & 0x00400000) >> 20) | (offset32 & 3);
}

// A device that owns a range of the physical address space.
// `size` is 1, 2 or 4; accesses are naturally aligned.
class MmioDevice {
public:
    virtual ~MmioDevice() = default;
    virtual u32 mmio_read(u32 address, u32 size) = 0;
    virtual void mmio_write(u32 address, u32 value, u32 size) = 0;
};

class Bus {
public:
    static constexpr u32 kPageShift = 16;
    static constexpr u32 kPageCount = (kPhysicalMask + 1) >> kPageShift;

    Bus();
    ~Bus();
    Bus(const Bus&) = delete;
    Bus& operator=(const Bus&) = delete;

    // Contents must be exactly kBiosSize / kFlashSize bytes.
    Result<void> load_bios(std::span<const u8> image);
    Result<void> load_flash(std::span<const u8> image);
    bool bios_loaded() const { return bios_loaded_; }

    // Attaches `device` to [start, end], both rounded out to 64 KB pages.
    void map_device(u32 start, u32 end, MmioDevice& device);

    // Clears RAM and VRAM. ROM contents and device mappings are kept.
    void reset();

    u8 read8(u32 address) { return static_cast<u8>(read(address, 1)); }
    u16 read16(u32 address) { return static_cast<u16>(read(address, 2)); }
    u32 read32(u32 address) { return read(address, 4); }
    void write8(u32 address, u8 value) { write(address, value, 1); }
    void write16(u32 address, u16 value) { write(address, value, 2); }
    void write32(u32 address, u32 value) { write(address, value, 4); }

    // Direct views, for loaders, DMA and tests.
    std::span<u8> ram() { return {ram_.get(), kRamSize}; }
    std::span<u8> vram() { return {vram_.get(), kVramSize}; }
    std::span<const u8> bios() const { return {bios_.get(), kBiosSize}; }
    std::span<const u8> flash() const { return {flash_.get(), kFlashSize}; }

    u64 unmapped_access_count() const { return unmapped_accesses_; }

private:
    enum class Kind : u8 { Unmapped, Direct, ReadOnly, Vram32, Device };

    struct Page {
        Kind kind = Kind::Unmapped;
        u8* base = nullptr; // Direct / ReadOnly / Vram32
        u32 mask = 0;       // Offset mask inside `base`
        MmioDevice* device = nullptr;
    };

    u32 read(u32 address, u32 size);
    void write(u32 address, u32 value, u32 size);
    void map_memory(u32 start, u32 end, Kind kind, u8* base, u32 mask);
    void note_unmapped(u32 address, bool is_write, u32 size, u32 value);

    std::unique_ptr<u8[]> bios_;
    std::unique_ptr<u8[]> flash_;
    std::unique_ptr<u8[]> ram_;
    std::unique_ptr<u8[]> vram_;
    std::array<Page, kPageCount> pages_{};
    std::bitset<kPageCount> logged_pages_;
    u64 unmapped_accesses_ = 0;
    bool bios_loaded_ = false;
};

} // namespace nc::memory
