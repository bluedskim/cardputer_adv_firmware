#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "config.h"

struct TimerEntry {
    std::string name;
    uint8_t hours = 0;
    uint8_t minutes = 5;
    uint32_t end_ms = 0;  // 0 = not running
    bool done = false;
};

class TimerStorage {
public:
    bool begin();
    void load();
    bool save() const;

    const std::vector<TimerEntry>& timers() const { return timers_; }
    std::vector<TimerEntry>& timers() { return timers_; }

    bool addTimer(const std::string& name, uint8_t hours, uint8_t minutes);
    bool removeTimer(std::size_t index);
    bool updateTimer(std::size_t index, uint8_t hours, uint8_t minutes);
    bool setTimerEnd(std::size_t index, uint32_t end_ms, bool done);

private:
    std::vector<TimerEntry> timers_;
};
