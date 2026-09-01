#pragma once

#include <cstdint>
#include <string>

#include "config.h"

struct StopwatchRecord {
    std::string name;
    uint32_t duration_ms = 0;
};

class StopwatchStorage {
public:
    bool begin();
    void load();
    bool save() const;

    const std::string& name() const { return name_; }
    void setName(const std::string& name);

    const StopwatchRecord* history() const { return history_; }
    std::size_t historyCount() const { return history_count_; }

    bool addRecord(const std::string& name, uint32_t duration_ms);

private:
    std::string name_ = "Run";
    StopwatchRecord history_[config::kMaxStopwatchHistory]{};
    std::size_t history_count_ = 0;
};
