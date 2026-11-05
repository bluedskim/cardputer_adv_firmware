#include "apps/timer/timer_storage.h"

#include <Preferences.h>

#include <algorithm>

namespace {

std::string truncateName(const std::string& name) {
    if (name.size() <= config::kMaxNameLength) {
        return name;
    }
    return name.substr(0, config::kMaxNameLength);
}

}  // namespace

bool TimerStorage::begin() {
    load();
    return true;
}

void TimerStorage::load() {
    timers_.clear();

    Preferences prefs;
    if (!prefs.begin(config::kTimerStorageNamespace, true)) {
        return;
    }

    const uint8_t count = prefs.getUChar("count", 0);
    for (uint8_t i = 0; i < count; ++i) {
        const std::string prefix = "t" + std::to_string(i) + "_";
        TimerEntry entry;
        entry.name = prefs.getString((prefix + "name").c_str(), "").c_str();
        if (entry.name.empty()) {
            continue;
        }
        entry.hours = prefs.getUChar((prefix + "h").c_str(), 0);
        entry.minutes = prefs.getUChar((prefix + "m").c_str(), 5);
        entry.seconds = prefs.getUChar((prefix + "s").c_str(), 0);
        entry.done = prefs.getUChar((prefix + "done").c_str(), 0) != 0;
        entry.end_ms = 0;
        timers_.push_back(std::move(entry));
    }

    prefs.end();
}

bool TimerStorage::save() const {
    Preferences prefs;
    if (!prefs.begin(config::kTimerStorageNamespace, false)) {
        return false;
    }

    prefs.clear();
    const uint8_t count =
        static_cast<uint8_t>(std::min(timers_.size(), static_cast<std::size_t>(config::kMaxTimers)));
    prefs.putUChar("count", count);

    for (uint8_t i = 0; i < count; ++i) {
        const auto& entry = timers_[i];
        const std::string prefix = "t" + std::to_string(i) + "_";
        prefs.putString((prefix + "name").c_str(), entry.name.c_str());
        prefs.putUChar((prefix + "h").c_str(), entry.hours);
        prefs.putUChar((prefix + "m").c_str(), entry.minutes);
        prefs.putUChar((prefix + "s").c_str(), entry.seconds);
        prefs.putUChar((prefix + "done").c_str(), entry.done ? 1 : 0);
    }

    prefs.end();
    return true;
}

bool TimerStorage::addTimer(const std::string& name, const uint8_t hours, const uint8_t minutes) {
    if (timers_.size() >= config::kMaxTimers) {
        return false;
    }

    const std::string trimmed = truncateName(name);
    if (trimmed.empty()) {
        return false;
    }

    timers_.push_back({});
    timers_.back().name = trimmed;
    timers_.back().hours = hours;
    timers_.back().minutes = minutes;
    return save();
}

bool TimerStorage::removeTimer(const std::size_t index) {
    if (index >= timers_.size()) {
        return false;
    }

    timers_.erase(timers_.begin() + static_cast<std::ptrdiff_t>(index));
    return save();
}

bool TimerStorage::updateTimer(const std::size_t index, const uint8_t hours, const uint8_t minutes,
                               const uint8_t seconds) {
    if (index >= timers_.size()) {
        return false;
    }

    timers_[index].hours = hours;
    timers_[index].minutes = minutes;
    timers_[index].seconds = seconds;
    return save();
}

bool TimerStorage::setTimerEnd(const std::size_t index, const uint32_t end_ms, const bool done) {
    if (index >= timers_.size()) {
        return false;
    }

    const bool done_changed = timers_[index].done != done;
    timers_[index].end_ms = end_ms;
    timers_[index].done = done;
    return done_changed ? save() : true;
}
