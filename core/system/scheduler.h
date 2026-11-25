#pragma once

#include "common/types.h"

#include <array>

namespace nc {

// The emulator's single timebase, counted in SH-4 cycles (200 MHz), plus a
// small fixed table of timed events. Devices register an event once at
// start-up and then (re)schedule it; nothing is allocated while running.
class Scheduler {
public:
    static constexpr u64 kNever = ~0ull;
    static constexpr int kMaxEvents = 32;
    static constexpr u64 kCyclesPerSecond = 200'000'000;

    // `cycles_late` is how far past its deadline the event actually ran.
    using Callback = void (*)(void* context, u64 cycles_late);

    // Returns an id that stays valid for the scheduler's lifetime.
    int add_event(const char* name, Callback callback, void* context);

    // Fires `delay` cycles from now, replacing any earlier deadline.
    void schedule(int id, u64 delay);
    void cancel(int id);
    bool is_scheduled(int id) const;
    u64 deadline(int id) const;

    u64 now() const { return now_; }
    // Earliest pending deadline, or kNever.
    u64 next_deadline() const { return next_; }

    void advance(u64 cycles) { now_ += cycles; }

    // Runs every event whose deadline is <= now, in deadline order (ties in
    // registration order). Callbacks may schedule or cancel any event.
    void run_due();

    // Cancels every event and rewinds time to zero. Registrations are kept.
    void reset();

    int event_count() const { return count_; }

private:
    struct Event {
        const char* name = nullptr;
        Callback callback = nullptr;
        void* context = nullptr;
        u64 when = kNever;
    };

    void recompute_next();

    std::array<Event, kMaxEvents> events_{};
    int count_ = 0;
    u64 now_ = 0;
    u64 next_ = kNever;
};

} // namespace nc
