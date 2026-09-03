#include "system_settings.h"

#include <Preferences.h>

#include <M5Cardputer.h>

#include <algorithm>
#include <cstdio>

#include "config.h"

namespace {

constexpr uint32_t kSleepTimeoutsSec[] = {30, 60, 300, 600};
constexpr const char* kSleepLabels[] = {"30 sec", "1 min", "5 min", "10 min"};

constexpr uint8_t kBrightnessLevels[] = {32, 64, 128, 192, 255};

}  // namespace

SystemSettings& SystemSettings::instance() {
    static SystemSettings settings;
    return settings;
}

bool SystemSettings::begin() {
    load();
    applyBrightness();
    return true;
}

void SystemSettings::load() {
    sleep_option_index_ = config::kDefaultSleepOptionIndex;
    brightness_option_index_ = config::kDefaultBrightnessOptionIndex;

    Preferences prefs;
    if (!prefs.begin(config::kSettingsStorageNamespace, true)) {
        clampIndices();
        return;
    }

    sleep_option_index_ = prefs.getUChar("sleep_idx", config::kDefaultSleepOptionIndex);
    brightness_option_index_ =
        prefs.getUChar("bright_idx", config::kDefaultBrightnessOptionIndex);
    prefs.end();
    clampIndices();
}

bool SystemSettings::save() const {
    Preferences prefs;
    if (!prefs.begin(config::kSettingsStorageNamespace, false)) {
        return false;
    }

    prefs.putUChar("sleep_idx", sleep_option_index_);
    prefs.putUChar("bright_idx", brightness_option_index_);
    prefs.end();
    return true;
}

void SystemSettings::clampIndices() {
    const uint8_t max_sleep = static_cast<uint8_t>(sleepOptionCount() - 1);
    sleep_option_index_ = std::min(sleep_option_index_, max_sleep);
    const uint8_t max_bright = static_cast<uint8_t>(brightnessOptionCount() - 1);
    brightness_option_index_ = std::min(brightness_option_index_, max_bright);
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

uint8_t SystemSettings::brightness() const {
    return kBrightnessLevels[brightness_option_index_];
}

const char* SystemSettings::brightnessOptionLabel() const {
    return brightnessOptionLabel(brightness_option_index_);
}

const char* SystemSettings::brightnessOptionLabel(const uint8_t index) const {
    static char label[8];
    const uint8_t level =
        index < brightnessOptionCount() ? kBrightnessLevels[index]
                                        : kBrightnessLevels[config::kDefaultBrightnessOptionIndex];
    std::snprintf(label, sizeof(label), "%u", static_cast<unsigned>(level));
    return label;
}

uint8_t SystemSettings::brightnessOptionCount() const {
    return static_cast<uint8_t>(sizeof(kBrightnessLevels) / sizeof(kBrightnessLevels[0]));
}

void SystemSettings::cycleBrightnessOption(const int delta) {
    if (delta == 0) {
        return;
    }

    const int count = static_cast<int>(brightnessOptionCount());
    int next = static_cast<int>(brightness_option_index_) + delta;
    while (next < 0) {
        next += count;
    }
    while (next >= count) {
        next -= count;
    }

    brightness_option_index_ = static_cast<uint8_t>(next);
    save();
    applyBrightness();
}

void SystemSettings::applyBrightness() const {
    M5Cardputer.Display.setBrightness(brightness());
}
