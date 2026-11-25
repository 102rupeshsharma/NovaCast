#include "common/fileio.h"
#include "core/emulator.h"
#include "core/scheduler.h"
#include "cpu/sh4.h"
#include "memory/bus.h"
#include "test_helpers.h"

#include <gtest/gtest.h>

#include <vector>

namespace nc {
namespace {

std::unique_ptr<Emulator> make_emulator() {
    auto created = Emulator::create();
    EXPECT_TRUE(created.ok());
    return std::move(created).value();
}

// A synthetic 2 MB boot ROM (not Sega code): at the reset vector it stores
// 42 to RAM address 0x8C000100, then loops forever.
std::vector<u8> synthetic_rom() {
    std::vector<u8> rom(memory::kBiosSize, 0);
    const u16 code[] = {
        0xD102, // 0: MOV.L @(8,PC),R1   -> literal at offset 12
        0xE02A, // 2: MOV #42,R0
        0x2102, // 4: MOV.L R0,@R1
        0xAFFE, // 6: BRA .  (offset 6)
        0x0009, // 8: NOP (delay slot)
        0x0009, // 10: padding
        0x0100, // 12: literal 0x8C000100, low half
        0x8C00, // 14: high half
    };
    for (std::size_t i = 0; i < std::size(code); ++i) {
        rom[i * 2] = static_cast<u8>(code[i]);
        rom[i * 2 + 1] = static_cast<u8>(code[i] >> 8);
    }
    return rom;
}

TEST(Emulator, CreateAndDestroyRepeatedly) {
    for (int i = 0; i < 20; ++i) {
        auto emulator = make_emulator();
        ASSERT_NE(emulator, nullptr);
        EXPECT_EQ(emulator->modules().size(), Emulator::kModuleCount);
    }
}

TEST(Emulator, SeveralInstancesCoexist) {
    auto a = make_emulator();
    auto b = make_emulator();
    a->bus().write32(0x0C000000, 1);
    EXPECT_EQ(b->bus().read32(0x0C000000), 0u) << "instances share no state";
}

TEST(Emulator, CannotRunSoftwareYet) {
    EXPECT_FALSE(make_emulator()->can_run_software());
}

TEST(Emulator, BiosMissingFile) {
    test::TempDir dir;
    auto r = make_emulator()->load_bios(dir / "dc_boot.bin");
    ASSERT_FALSE(r.ok());
    EXPECT_EQ(r.error().code, ErrorCode::FileNotFound);
}

TEST(Emulator, BiosWrongSize) {
    test::TempDir dir;
    for (std::size_t size : {std::size_t{0}, std::size_t{1024}, std::size_t{memory::kBiosSize - 1},
                             std::size_t{memory::kBiosSize + 1}, std::size_t{memory::kBiosSize * 2}}) {
        const auto path = dir / "bios.bin";
        ASSERT_TRUE(write_file(path, std::vector<u8>(size, 0x11)).ok());
        auto r = make_emulator()->load_bios(path);
        ASSERT_FALSE(r.ok()) << size;
        EXPECT_EQ(r.error().code, ErrorCode::InvalidFormat) << size;
    }
}

TEST(Emulator, BiosBlankImageRejected) {
    auto emulator = make_emulator();
    EXPECT_EQ(emulator->load_bios(std::vector<u8>(memory::kBiosSize, 0x00)).error().code, ErrorCode::InvalidFormat);
    EXPECT_EQ(emulator->load_bios(std::vector<u8>(memory::kBiosSize, 0xFF)).error().code, ErrorCode::InvalidFormat);
    EXPECT_FALSE(emulator->bus().bios_loaded());
}

TEST(Emulator, BiosFromFile) {
    test::TempDir dir;
    const auto path = dir / "bios.bin";
    ASSERT_TRUE(write_file(path, synthetic_rom()).ok());
    auto emulator = make_emulator();
    ASSERT_TRUE(emulator->load_bios(path).ok());
    EXPECT_EQ(emulator->bus().read16(0), 0xD102);
}

TEST(Emulator, FlashWrongSizeRejected) {
    test::TempDir dir;
    const auto path = dir / "flash.bin";
    ASSERT_TRUE(write_file(path, std::vector<u8>(1000, 0)).ok());
    EXPECT_EQ(make_emulator()->load_flash(path).error().code, ErrorCode::InvalidFormat);
}

TEST(Emulator, RunWithoutBiosFails) {
    auto r = make_emulator()->run_for(100);
    ASSERT_FALSE(r.ok());
    EXPECT_EQ(r.error().code, ErrorCode::InvalidArgument);
}

TEST(Emulator, BootsFromResetVector) {
    auto emulator = make_emulator();
    ASSERT_TRUE(emulator->load_bios(synthetic_rom()).ok());
    emulator->reset();
    EXPECT_EQ(emulator->cpu().state().pc, 0xA0000000u);
    ASSERT_TRUE(emulator->run_for(1000).ok());
    EXPECT_EQ(emulator->bus().read32(0x0C000100), 42u);
    EXPECT_EQ(emulator->cpu().state().pc, 0xA0000006u) << "parked in the BRA . loop";
    // A branch and its delay slot run together, so time can pass the target
    // by at most one cycle.
    EXPECT_GE(emulator->scheduler().now(), 1000u);
    EXPECT_LE(emulator->scheduler().now(), 1001u);
}

TEST(Emulator, ResetIsRepeatable) {
    auto emulator = make_emulator();
    ASSERT_TRUE(emulator->load_bios(synthetic_rom()).ok());
    for (int i = 0; i < 3; ++i) {
        emulator->reset();
        EXPECT_EQ(emulator->bus().read32(0x0C000100), 0u) << "reset clears RAM";
        ASSERT_TRUE(emulator->run_for(500).ok());
        EXPECT_EQ(emulator->bus().read32(0x0C000100), 42u);
    }
}

struct Fired {
    int count = 0;
    u64 at = 0;
    u64 late = 0;
    Scheduler* scheduler = nullptr;
};

void on_event(void* p, u64 cycles_late) {
    auto* f = static_cast<Fired*>(p);
    ++f->count;
    f->at = f->scheduler->now();
    f->late = cycles_late;
}

TEST(Emulator, ScheduledEventsFireWhileRunning) {
    auto emulator = make_emulator();
    ASSERT_TRUE(emulator->load_bios(synthetic_rom()).ok());
    emulator->reset();
    Fired fired;
    fired.scheduler = &emulator->scheduler();
    const int id = emulator->scheduler().add_event("test", on_event, &fired);
    emulator->scheduler().schedule(id, 250);
    ASSERT_TRUE(emulator->run_for(1000).ok());
    EXPECT_EQ(fired.count, 1);
    // The CPU slice stops at the deadline; the only allowed overrun is one
    // cycle (a delayed branch plus its slot), and it is reported to the event.
    EXPECT_GE(fired.at, 250u);
    EXPECT_LE(fired.at, 251u);
    EXPECT_EQ(fired.late, fired.at - 250u);
}

// The synthetic ROM's final loop is a BRA with a NOP in its slot: every step
// there costs two cycles. Slices that end in the middle must not lose time.
TEST(Emulator, ManySmallSlicesKeepTimeConsistent) {
    auto emulator = make_emulator();
    ASSERT_TRUE(emulator->load_bios(synthetic_rom()).ok());
    emulator->reset();
    u64 requested = 0;
    for (int i = 0; i < 500; ++i) {
        ASSERT_TRUE(emulator->run_for(3).ok());
        requested += 3;
        ASSERT_GE(emulator->scheduler().now(), requested);
        ASSERT_LE(emulator->scheduler().now(), requested + 1);
    }
    EXPECT_EQ(emulator->scheduler().now(), emulator->cpu().instructions_executed());
}

TEST(Emulator, LoadGameRejectsMissingFile) {
    test::TempDir dir;
    auto r = make_emulator()->load_game(dir / "missing.gdi");
    ASSERT_FALSE(r.ok());
    EXPECT_EQ(r.error().code, ErrorCode::FileNotFound);
}

TEST(Emulator, LoadGameRejectsUnknownExtension) {
    test::TempDir dir;
    const auto path = dir / "notes.txt";
    ASSERT_TRUE(write_file(path, std::vector<u8>{1, 2, 3}).ok());
    auto r = make_emulator()->load_game(path);
    ASSERT_FALSE(r.ok());
    EXPECT_EQ(r.error().code, ErrorCode::Unsupported);
}

// Disc loading is not implemented, so a plausible file must be reported as
// such and never as a successful load.
TEST(Emulator, LoadGameReportsNotImplemented) {
    test::TempDir dir;
    for (const char* name : {"game.gdi", "GAME.CDI", "game.cue"}) {
        const auto path = dir / name;
        ASSERT_TRUE(write_file(path, std::vector<u8>{0}).ok());
        auto r = make_emulator()->load_game(path);
        ASSERT_FALSE(r.ok()) << name;
        EXPECT_EQ(r.error().code, ErrorCode::NotImplemented) << name;
    }
}

} // namespace
} // namespace nc
