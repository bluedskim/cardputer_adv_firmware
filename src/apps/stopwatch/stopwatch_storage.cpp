#include "apps/stopwatch/stopwatch_storage.h"

#include <Preferences.h>

#include <algorithm>
#include <cstring>

namespace {

std::string truncateName(const std::string& name) {
    if (name.size() <= config::kMaxNameLength) {
        return name;
    }
    return name.substr(0, config::kMaxNameLength);
}

}  // namespace

bool StopwatchStorage::begin() {
    load();
    return true;
}

void StopwatchStorage::load() {
    history_count_ = 0;
    name_ = "Run";

    Preferences prefs;
    if (!prefs.begin(config::kStopwatchStorageNamespace, true)) {
        return;
    }

    name_ = prefs.getString("name", "Run").c_str();
    const uint8_t count = prefs.getUChar("hist_count", 0);
    history_count_ = std::min(static_cast<std::size_t>(count),
                              static_cast<std::size_t>(config::kMaxStopwatchHistory));

    for (std::size_t i = 0; i < history_count_; ++i) {
        const std::string prefix = "h" + std::to_string(i) + "_";
        history_[i].name = prefs.getString((prefix + "name").c_str(), "").c_str();
        history_[i].duration_ms = prefs.getUInt((prefix + "ms").c_str(), 0);
    }

    prefs.end();
}

bool StopwatchStorage::save() const {
    Preferences prefs;
    if (!prefs.begin(config::kStopwatchStorageNamespace, false)) {
        return false;
    }

    prefs.clear();
    prefs.putString("name", name_.c_str());
    prefs.putUChar("hist_count", static_cast<uint8_t>(history_count_));

    for (std::size_t i = 0; i < history_count_; ++i) {
        const std::string prefix = "h" + std::to_string(i) + "_";
        prefs.putString((prefix + "name").c_str(), history_[i].name.c_str());
        prefs.putUInt((prefix + "ms").c_str(), history_[i].duration_ms);
    }

    prefs.end();
    return true;
}

void StopwatchStorage::setName(const std::string& name) {
    const std::string trimmed = truncateName(name);
    if (trimmed.empty()) {
        return;
    }
    name_ = trimmed;
    save();
}

bool StopwatchStorage::addRecord(const std::string& name, const uint32_t duration_ms) {
    if (duration_ms == 0) {
        return false;
    }

    const std::string trimmed = truncateName(name);
    if (trimmed.empty()) {
        return false;
    }

    const std::size_t limit = config::kMaxStopwatchHistory;
    if (history_count_ < limit) {
        ++history_count_;
    }
    for (std::size_t i = history_count_ - 1; i > 0; --i) {
        history_[i] = history_[i - 1];
    }
    history_[0].name = trimmed;
    history_[0].duration_ms = duration_ms;
    return save();
}
