#include "system/scheduler.h"

#include <gtest/gtest.h>

#include <string>
#include <vector>

namespace nc {
namespace {

struct Log {
    std::vector<std::string> fired;
    std::vector<u64> late;
};

struct Ctx {
    Log* log;
    const char* name;
    Scheduler* scheduler = nullptr;
    int reschedule_id = -1;
    u64 reschedule_delay = 0;
};

void record(void* p, u64 cycles_late) {
    auto* c = static_cast<Ctx*>(p);
    c->log->fired.emplace_back(c->name);
    c->log->late.push_back(cycles_late);
    if (c->scheduler && c->reschedule_id >= 0)
        c->scheduler->schedule(c->reschedule_id, c->reschedule_delay);
}

TEST(Scheduler, FiresInDeadlineOrder) {
    Scheduler s;
    Log log;
    Ctx a{&log, "a"}, b{&log, "b"}, c{&log, "c"};
    const int ia = s.add_event("a", record, &a);
    const int ib = s.add_event("b", record, &b);
    const int ic = s.add_event("c", record, &c);
    s.schedule(ia, 300);
    s.schedule(ib, 100);
    s.schedule(ic, 200);
    EXPECT_EQ(s.next_deadline(), 100u);
    s.advance(1000);
    s.run_due();
    EXPECT_EQ(log.fired, (std::vector<std::string>{"b", "c", "a"}));
    EXPECT_EQ(log.late, (std::vector<u64>{900, 800, 700}));
    EXPECT_EQ(s.next_deadline(), Scheduler::kNever);
}

TEST(Scheduler, TiesFireInRegistrationOrder) {
    Scheduler s;
    Log log;
    Ctx a{&log, "a"}, b{&log, "b"};
    const int ia = s.add_event("a", record, &a);
    const int ib = s.add_event("b", record, &b);
    s.schedule(ib, 50);
    s.schedule(ia, 50);
    s.advance(50);
    s.run_due();
    EXPECT_EQ(log.fired, (std::vector<std::string>{"a", "b"}));
}

TEST(Scheduler, DoesNotFireEarly) {
    Scheduler s;
    Log log;
    Ctx a{&log, "a"};
    const int ia = s.add_event("a", record, &a);
    s.schedule(ia, 100);
    s.advance(99);
    s.run_due();
    EXPECT_TRUE(log.fired.empty());
    s.advance(1);
    s.run_due();
    EXPECT_EQ(log.fired.size(), 1u);
    EXPECT_EQ(log.late[0], 0u);
}

TEST(Scheduler, CancelAndReschedule) {
    Scheduler s;
    Log log;
    Ctx a{&log, "a"}, b{&log, "b"};
    const int ia = s.add_event("a", record, &a);
    const int ib = s.add_event("b", record, &b);
    s.schedule(ia, 10);
    s.schedule(ib, 20);
    s.cancel(ia);
    EXPECT_FALSE(s.is_scheduled(ia));
    EXPECT_EQ(s.next_deadline(), 20u);
    s.schedule(ib, 500); // replaces the earlier deadline
    EXPECT_EQ(s.next_deadline(), 500u);
    s.advance(100);
    s.run_due();
    EXPECT_TRUE(log.fired.empty());
}

TEST(Scheduler, PeriodicEventReschedulesItself) {
    Scheduler s;
    Log log;
    Ctx tick{&log, "tick", &s};
    const int id = s.add_event("tick", record, &tick);
    tick.reschedule_id = id;
    tick.reschedule_delay = 100;
    s.schedule(id, 100);
    for (int i = 0; i < 10; ++i) {
        s.advance(100);
        s.run_due();
    }
    EXPECT_EQ(log.fired.size(), 10u);
}

TEST(Scheduler, CallbackCanScheduleAnEventThatIsAlreadyDue) {
    Scheduler s;
    Log log;
    Ctx b{&log, "b"};
    Ctx a{&log, "a", &s};
    const int ia = s.add_event("a", record, &a);
    const int ib = s.add_event("b", record, &b);
    a.reschedule_id = ib;
    a.reschedule_delay = 0;
    s.schedule(ia, 10);
    s.advance(10);
    s.run_due();
    EXPECT_EQ(log.fired, (std::vector<std::string>{"a", "b"}));
}

TEST(Scheduler, HugeDelayDoesNotWrap) {
    Scheduler s;
    Log log;
    Ctx a{&log, "a"};
    const int ia = s.add_event("a", record, &a);
    s.advance(1000);
    s.schedule(ia, ~0ull);
    EXPECT_TRUE(s.is_scheduled(ia));
    EXPECT_GT(s.deadline(ia), s.now());
    s.run_due();
    EXPECT_TRUE(log.fired.empty());
}

TEST(Scheduler, ResetKeepsRegistrations) {
    Scheduler s;
    Log log;
    Ctx a{&log, "a"};
    const int ia = s.add_event("a", record, &a);
    s.schedule(ia, 5);
    s.advance(3);
    s.reset();
    EXPECT_EQ(s.now(), 0u);
    EXPECT_FALSE(s.is_scheduled(ia));
    EXPECT_EQ(s.event_count(), 1);
    s.schedule(ia, 1);
    s.advance(1);
    s.run_due();
    EXPECT_EQ(log.fired.size(), 1u);
}

TEST(Scheduler, TableFullThrows) {
    Scheduler s;
    Log log;
    Ctx a{&log, "a"};
    for (int i = 0; i < Scheduler::kMaxEvents; ++i)
        s.add_event("x", record, &a);
    EXPECT_THROW(s.add_event("overflow", record, &a), std::length_error);
}

} // namespace
} // namespace nc
