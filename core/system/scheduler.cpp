#include "system/scheduler.h"

#include <stdexcept>
#include <string>

namespace nc {

int Scheduler::add_event(const char* name, Callback callback, void* context) {
    if (count_ >= kMaxEvents)
        throw std::length_error(std::string("scheduler event table full while adding '") + name + "'");
    if (callback == nullptr)
        throw std::invalid_argument("scheduler event needs a callback");
    events_[count_] = Event{name, callback, context, kNever};
    return count_++;
}

void Scheduler::schedule(int id, u64 delay) {
    Event& e = events_[id];
    const u64 old_when = e.when;
    e.when = (delay >= kNever - now_) ? kNever - 1 : now_ + delay;
    if (e.when < next_)
        next_ = e.when;
    else if (old_when == next_)
        recompute_next();
}

void Scheduler::cancel(int id) {
    const u64 old_when = events_[id].when;
    events_[id].when = kNever;
    if (old_when == next_)
        recompute_next();
}

bool Scheduler::is_scheduled(int id) const {
    return events_[id].when != kNever;
}

u64 Scheduler::deadline(int id) const {
    return events_[id].when;
}

void Scheduler::run_due() {
    while (next_ <= now_) {
        int best = -1;
        u64 best_when = kNever;
        for (int i = 0; i < count_; ++i) {
            if (events_[i].when < best_when) {
                best = i;
                best_when = events_[i].when;
            }
        }
        if (best < 0 || best_when > now_)
            break;
        Event& e = events_[best];
        e.when = kNever;
        recompute_next();
        e.callback(e.context, now_ - best_when);
    }
}

void Scheduler::reset() {
    for (int i = 0; i < count_; ++i)
        events_[i].when = kNever;
    now_ = 0;
    next_ = kNever;
}

void Scheduler::recompute_next() {
    u64 next = kNever;
    for (int i = 0; i < count_; ++i) {
        if (events_[i].when < next)
            next = events_[i].when;
    }
    next_ = next;
}

} // namespace nc
