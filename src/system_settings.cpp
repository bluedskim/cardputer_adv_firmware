#include "system_settings.h"

#include <Preferences.h>

#include <algorithm>

#include "config.h"

namespace {

constexpr uint32_t kSleepTimeoutsSec[] = {30, 60, 300, 600};
constexpr const char* kSleepLabels[] = {"30 sec", "1 min", "5 min", "10 min"};

}  // namespace

SystemSettings& SystemSettings::instance() {
    static SystemSettings settings;
    return settings;
}

bool SystemSettings::begin() {
    load();
    return true;
}

void SystemSettings::load() {
    sleep_option_index_ = config::kDefaultSleepOptionIndex;

    Preferences prefs;
    if (!prefs.begin(config::kSettingsStorageNamespace, true)) {
        return;
    }

    sleep_option_index_ = prefs.getUChar("sleep_idx", config::kDefaultSleepOptionIndex);
    prefs.end();
    clampIndex();
}

bool SystemSettings::save() const {
    Preferences prefs;
    if (!prefs.begin(config::kSettingsStorageNamespace, false)) {
        return false;
    }

    prefs.putUChar("sleep_idx", sleep_option_index_);
    prefs.end();
    return true;
}

void SystemSettings::clampIndex() {
    const uint8_t max_index = static_cast<uint8_t>(sleepOptionCount() - 1);
    sleep_option_index_ = std::min(sleep_option_index_, max_index);
}

uint32_t SystemSettings::sleepTimeoutSec() const {
    return kSleepTimeoutsSec[sleep_option_index_];
}

const char* SystemSettings::sleepOptionLabel() const {
    return sleepOptionLabel(sleep_option_index_);
}

const char* SystemSettings::sleepOptionLabel(const uint8_t index) const {
    if (index >= sleepOptionCount()) {
        return kSleepLabels[config::kDefaultSleepOptionIndex];
    }
    return kSleepLabels[index];
}

uint8_t SystemSettings::sleepOptionCount() const {
    return static_cast<uint8_t>(sizeof(kSleepTimeoutsSec) / sizeof(kSleepTimeoutsSec[0]));
}

void SystemSettings::cycleSleepOption(const int delta) {
    if (delta == 0) {
        return;
    }

    const int count = static_cast<int>(sleepOptionCount());
    int next = static_cast<int>(sleep_option_index_) + delta;
    while (next < 0) {
        next += count;
    }
    while (next >= count) {
        next -= count;
    }

    sleep_option_index_ = static_cast<uint8_t>(next);
    save();
}
