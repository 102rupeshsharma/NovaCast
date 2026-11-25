#include "memory/bus.h"

#include "common/log.h"

#include <algorithm>
#include <cstring>
#include <string>

namespace nc::memory {

namespace {

u32 load(const u8* p, u32 size) {
    switch (size) {
    case 1:
        return *p;
    case 2: {
        u16 v;
        std::memcpy(&v, p, 2);
        return v;
    }
    default: {
        u32 v;
        std::memcpy(&v, p, 4);
        return v;
    }
    }
}

void store(u8* p, u32 value, u32 size) {
    switch (size) {
    case 1:
        *p = static_cast<u8>(value);
        break;
    case 2: {
        const u16 v = static_cast<u16>(value);
        std::memcpy(p, &v, 2);
        break;
    }
    default:
        std::memcpy(p, &value, 4);
        break;
    }
}

} // namespace

Bus::Bus()
    : bios_(std::make_unique<u8[]>(kBiosSize)),
      flash_(std::make_unique<u8[]>(kFlashSize)),
      ram_(std::make_unique<u8[]>(kRamSize)),
      vram_(std::make_unique<u8[]>(kVramSize)) {
    // An empty flash chip reads as erased (all ones).
    std::memset(flash_.get(), 0xFF, kFlashSize);

    map_memory(0x00000000, 0x001FFFFF, Kind::ReadOnly, bios_.get(), kBiosSize - 1);
    map_memory(0x00200000, 0x0021FFFF, Kind::ReadOnly, flash_.get(), kFlashSize - 1);
    map_memory(0x04000000, 0x04FFFFFF, Kind::Direct, vram_.get(), kVramSize - 1);
    map_memory(0x05000000, 0x05FFFFFF, Kind::Vram32, vram_.get(), kVramSize - 1);
    map_memory(0x06000000, 0x06FFFFFF, Kind::Direct, vram_.get(), kVramSize - 1);
    map_memory(0x07000000, 0x07FFFFFF, Kind::Vram32, vram_.get(), kVramSize - 1);
    map_memory(0x0C000000, 0x0FFFFFFF, Kind::Direct, ram_.get(), kRamSize - 1);
}

Bus::~Bus() = default;

Result<void> Bus::load_bios(std::span<const u8> image) {
    if (image.size() != kBiosSize) {
        return make_error(ErrorCode::InvalidFormat, "boot ROM must be exactly " + std::to_string(kBiosSize) +
                                                        " bytes, got " + std::to_string(image.size()));
    }
    std::memcpy(bios_.get(), image.data(), kBiosSize);
    bios_loaded_ = true;
    return {};
}

Result<void> Bus::load_flash(std::span<const u8> image) {
    if (image.size() != kFlashSize) {
        return make_error(ErrorCode::InvalidFormat, "flash ROM must be exactly " + std::to_string(kFlashSize) +
                                                        " bytes, got " + std::to_string(image.size()));
    }
    std::memcpy(flash_.get(), image.data(), kFlashSize);
    return {};
}

void Bus::map_memory(u32 start, u32 end, Kind kind, u8* base, u32 mask) {
    for (u32 page = (start & kPhysicalMask) >> kPageShift; page <= ((end & kPhysicalMask) >> kPageShift); ++page)
        pages_[page] = Page{kind, base, mask, nullptr};
}

void Bus::map_device(u32 start, u32 end, MmioDevice& device) {
    for (u32 page = (start & kPhysicalMask) >> kPageShift; page <= ((end & kPhysicalMask) >> kPageShift); ++page)
        pages_[page] = Page{Kind::Device, nullptr, 0, &device};
}

void Bus::reset() {
    std::memset(ram_.get(), 0, kRamSize);
    std::memset(vram_.get(), 0, kVramSize);
}

u32 Bus::read(u32 address, u32 size) {
    address &= kPhysicalMask;
    const Page& page = pages_[address >> kPageShift];
    switch (page.kind) {
    case Kind::Direct:
    case Kind::ReadOnly:
        return load(page.base + (address & page.mask), size);
    case Kind::Vram32:
        return load(page.base + vram32_to_64(address), size);
    case Kind::Device:
        return page.device->mmio_read(address, size);
    case Kind::Unmapped:
        break;
    }
    note_unmapped(address, false, size, 0);
    return 0;
}

void Bus::write(u32 address, u32 value, u32 size) {
    address &= kPhysicalMask;
    const Page& page = pages_[address >> kPageShift];
    switch (page.kind) {
    case Kind::Direct:
        store(page.base + (address & page.mask), value, size);
        return;
    case Kind::Vram32:
        store(page.base + vram32_to_64(address), value, size);
        return;
    case Kind::Device:
        page.device->mmio_write(address, value, size);
        return;
    case Kind::ReadOnly:
        if (address < kBiosSize) {
            LOG_DEBUG("bus", "write%u to boot ROM at %08X ignored", size * 8, address);
        } else {
            // The BIOS programs flash with an AMD-style command sequence.
            // That protocol is not emulated yet, so nothing is stored.
            LOG_WARN("bus", "flash write %08X <- %08X (size %u) ignored: flash programming not implemented",
                     address, value, size);
        }
        return;
    case Kind::Unmapped:
        break;
    }
    note_unmapped(address, true, size, value);
}

void Bus::note_unmapped(u32 address, bool is_write, u32 size, u32 value) {
    ++unmapped_accesses_;
    const u32 page = address >> kPageShift;
    if (logged_pages_.test(page))
        return;
    logged_pages_.set(page);
    if (is_write) {
        LOG_WARN("bus", "unmapped write%u %08X <- %08X (further accesses to this 64 KB page not logged)",
                 size * 8, address, value);
    } else {
        LOG_WARN("bus", "unmapped read%u %08X (further accesses to this 64 KB page not logged)", size * 8,
                 address);
    }
}

} // namespace nc::memory
