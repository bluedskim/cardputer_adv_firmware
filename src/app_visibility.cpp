#include "app_visibility.h"

#include <Preferences.h>

#include <cstring>

#include "config.h"

namespace {

// Stable bits — never reorder; new apps append. Settings is not listed.
constexpr const char* kHideable[] = {
    "Torch",     "Timer", "Stopwatch", "Recorder", "Level", "Seismo",
    "Web",       "SSH",   "Find",      "Tuner",    "Metro",
};
constexpr uint8_t kHideableCount = sizeof(kHideable) / sizeof(kHideable[0]);

}  // namespace

AppVisibility& AppVisibility::instance() {
    static AppVisibility visibility;
    return visibility;
}

bool AppVisibility::begin() {
    load();
    return true;
}

uint8_t AppVisibility::hideableCount() const { return kHideableCount; }

const char* AppVisibility::hideableName(const uint8_t index) const {
    if (index >= kHideableCount) {
        return "";
    }
    return kHideable[index];
}

int AppVisibility::bitForName(const char* app_name) const {
    if (app_name == nullptr) {
        return -1;
    }
    for (uint8_t i = 0; i < kHideableCount; ++i) {
        if (std::strcmp(app_name, kHideable[i]) == 0) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

bool AppVisibility::canHide(const char* app_name) const { return bitForName(app_name) >= 0; }

bool AppVisibility::isHidden(const char* app_name) const {
    const int bit = bitForName(app_name);
    if (bit < 0) {
        return false;
    }
    return (hidden_mask_ & (1u << bit)) != 0;
}

void AppVisibility::setHidden(const char* app_name, const bool hidden) {
    const int bit = bitForName(app_name);
    if (bit < 0) {
        return;
    }
    const uint16_t flag = static_cast<uint16_t>(1u << bit);
    if (hidden) {
        hidden_mask_ = static_cast<uint16_t>(hidden_mask_ | flag);
    } else {
        hidden_mask_ = static_cast<uint16_t>(hidden_mask_ & ~flag);
    }
    save();
}

void AppVisibility::toggleHidden(const uint8_t hideable_index) {
    if (hideable_index >= kHideableCount) {
        return;
    }
    setHidden(kHideable[hideable_index], !isHidden(kHideable[hideable_index]));
}

void AppVisibility::load() {
    hidden_mask_ = 0;
    Preferences prefs;
    if (!prefs.begin(config::kSettingsStorageNamespace, true)) {
        return;
    }
    hidden_mask_ = static_cast<uint16_t>(prefs.getUInt("hide_mask", 0));
    prefs.end();
    // Drop bits beyond the known table (forward-compatible).
    const uint16_t valid = static_cast<uint16_t>((1u << kHideableCount) - 1u);
    hidden_mask_ = static_cast<uint16_t>(hidden_mask_ & valid);
}

bool AppVisibility::save() const {
    Preferences prefs;
    if (!prefs.begin(config::kSettingsStorageNamespace, false)) {
        return false;
    }
    prefs.putUInt("hide_mask", hidden_mask_);
    prefs.end();
    return true;
}
