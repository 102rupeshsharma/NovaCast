#include "memory/bus.h"

#include <gtest/gtest.h>

#include <memory>
#include <vector>

namespace nc::memory {
namespace {

class BusTest : public ::testing::Test {
protected:
    std::unique_ptr<Bus> bus = std::make_unique<Bus>();
};

TEST_F(BusTest, RamReadWriteAllSizes) {
    bus->write32(0x0C000000, 0x11223344);
    EXPECT_EQ(bus->read32(0x0C000000), 0x11223344u);
    EXPECT_EQ(bus->read16(0x0C000000), 0x3344u) << "little endian";
    EXPECT_EQ(bus->read16(0x0C000002), 0x1122u);
    EXPECT_EQ(bus->read8(0x0C000003), 0x11u);
    bus->write8(0x0C000001, 0xAA);
    EXPECT_EQ(bus->read32(0x0C000000), 0x1122AA44u);
    bus->write16(0x0C000002, 0xBEEF);
    EXPECT_EQ(bus->read32(0x0C000000), 0xBEEFAA44u);
}

TEST_F(BusTest, RamIsMirroredFourTimes) {
    bus->write32(0x0C123450, 0xCAFEBABE);
    EXPECT_EQ(bus->read32(0x0D123450), 0xCAFEBABEu);
    EXPECT_EQ(bus->read32(0x0E123450), 0xCAFEBABEu);
    EXPECT_EQ(bus->read32(0x0F123450), 0xCAFEBABEu);
    EXPECT_EQ(bus->ram()[0x123450], 0xBE);
}

TEST_F(BusTest, UpperAddressBitsAreIgnored) {
    // Callers pass 29-bit physical addresses; region bits must not matter.
    bus->write32(0x0C000010, 0x01020304);
    EXPECT_EQ(bus->read32(0xAC000010), 0x01020304u);
}

TEST_F(BusTest, RamEdges) {
    bus->write32(0x0CFFFFFC, 0xDEADBEEF);
    EXPECT_EQ(bus->read32(0x0CFFFFFC), 0xDEADBEEFu);
    EXPECT_EQ(bus->read8(0x0CFFFFFF), 0xDE);
}

TEST_F(BusTest, BiosIsReadOnly) {
    std::vector<u8> image(kBiosSize, 0);
    image[0] = 0x12;
    image[kBiosSize - 1] = 0x34;
    ASSERT_TRUE(bus->load_bios(image).ok());
    EXPECT_TRUE(bus->bios_loaded());
    EXPECT_EQ(bus->read8(0x00000000), 0x12);
    EXPECT_EQ(bus->read8(0x001FFFFF), 0x34);
    bus->write8(0x00000000, 0xFF);
    EXPECT_EQ(bus->read8(0x00000000), 0x12);
}

TEST_F(BusTest, BiosAndFlashSizesAreValidated) {
    EXPECT_EQ(bus->load_bios(std::vector<u8>(kBiosSize - 1)).error().code, ErrorCode::InvalidFormat);
    EXPECT_EQ(bus->load_bios(std::vector<u8>(kBiosSize + 1)).error().code, ErrorCode::InvalidFormat);
    EXPECT_FALSE(bus->bios_loaded());
    EXPECT_EQ(bus->load_flash(std::vector<u8>(100)).error().code, ErrorCode::InvalidFormat);
}

TEST_F(BusTest, FlashReadsAndIgnoresWrites) {
    EXPECT_EQ(bus->read32(0x00200000), 0xFFFFFFFFu) << "an empty chip reads as erased";
    std::vector<u8> image(kFlashSize, 0x5A);
    ASSERT_TRUE(bus->load_flash(image).ok());
    EXPECT_EQ(bus->read8(0x0021FFFF), 0x5A);
    bus->write8(0x00205555, 0xAA); // flash command protocol: not implemented
    EXPECT_EQ(bus->read8(0x00205555), 0x5A);
}

TEST_F(BusTest, Vram64BitPathIsLinear) {
    bus->write32(0x04000008, 0x89ABCDEF);
    EXPECT_EQ(bus->vram()[8], 0xEF);
    EXPECT_EQ(bus->read32(0x06000008), 0x89ABCDEFu) << "0x06 mirrors the 64-bit path";
    EXPECT_EQ(bus->read32(0x04800008), 0x89ABCDEFu) << "8 MB mirror inside the area";
}

TEST_F(BusTest, Vram32BitPathInterleavesBanks) {
    // 32-bit view offset 0 -> 64-bit offset 0; offset 4 -> 8;
    // offset 0x400000 (second bank) -> 4.
    EXPECT_EQ(vram32_to_64(0x000000), 0x000000u);
    EXPECT_EQ(vram32_to_64(0x000004), 0x000008u);
    EXPECT_EQ(vram32_to_64(0x400000), 0x000004u);
    EXPECT_EQ(vram32_to_64(0x400006), 0x00000Eu);
    EXPECT_EQ(vram32_to_64(0x7FFFFC), 0x7FFFFCu);

    bus->write32(0x05400000, 0x11111111);
    bus->write32(0x05000004, 0x22222222);
    EXPECT_EQ(bus->read32(0x04000004), 0x11111111u);
    EXPECT_EQ(bus->read32(0x04000008), 0x22222222u);
    EXPECT_EQ(bus->read32(0x07000004), 0x22222222u) << "0x07 mirrors the 32-bit path";
}

TEST_F(BusTest, Vram32MappingIsABijection) {
    std::vector<bool> seen(kVramSize / 4, false);
    for (u32 off = 0; off < kVramSize; off += 4) {
        const u32 mapped = vram32_to_64(off);
        ASSERT_LT(mapped, kVramSize);
        ASSERT_EQ(mapped % 4, 0u);
        ASSERT_FALSE(seen[mapped / 4]) << off;
        seen[mapped / 4] = true;
    }
}

TEST_F(BusTest, UnmappedReadsZeroAndCounts) {
    EXPECT_EQ(bus->read32(0x01000000), 0u);
    bus->write32(0x01000000, 1);
    EXPECT_EQ(bus->read32(0x01000000), 0u);
    EXPECT_EQ(bus->unmapped_access_count(), 3u);
}

class FakeDevice final : public MmioDevice {
public:
    u32 mmio_read(u32 address, u32 size) override {
        last_read = address;
        last_size = size;
        return 0xA5A5A5A5;
    }
    void mmio_write(u32 address, u32 value, u32 size) override {
        last_write = address;
        last_value = value;
        last_size = size;
    }
    u32 last_read = 0, last_write = 0, last_value = 0, last_size = 0;
};

TEST_F(BusTest, DeviceRangesDispatch) {
    FakeDevice device;
    bus->map_device(0x005F0000, 0x005FFFFF, device);
    EXPECT_EQ(bus->read32(0x005F6800), 0xA5A5A5A5u);
    EXPECT_EQ(device.last_read, 0x005F6800u);
    bus->write16(0x005F8040, 0x1234);
    EXPECT_EQ(device.last_write, 0x005F8040u);
    EXPECT_EQ(device.last_value, 0x1234u);
    EXPECT_EQ(device.last_size, 2u);
}

TEST_F(BusTest, ResetClearsRamButKeepsRom) {
    std::vector<u8> image(kBiosSize, 0x77);
    ASSERT_TRUE(bus->load_bios(image).ok());
    bus->write32(0x0C000000, 0x12345678);
    bus->write32(0x04000000, 0x12345678);
    bus->reset();
    EXPECT_EQ(bus->read32(0x0C000000), 0u);
    EXPECT_EQ(bus->read32(0x04000000), 0u);
    EXPECT_EQ(bus->read8(0x00000000), 0x77);
}

} // namespace
} // namespace nc::memory
