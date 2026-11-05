#pragma once

#include <cstddef>
#include <memory>
#include <vector>

#include <M5Cardputer.h>

#include "apps/app_base.h"
#include "input/keyboard_input.h"
#include "system_settings.h"
#include "ui/battery_indicator.h"

class AppShell {
public:
    static AppShell* instance() { return instance_; }

    void addApp(std::unique_ptr<AppBase> app);
    void begin();
    void update();
    uint32_t loopDelayMs() const;
    void markHeaderDirty() { header_dirty_ = true; }
    void sleepDisplay();

private:
    void switchTo(std::size_t index);
    std::size_t adjacentVisible(std::size_t from, int delta) const;
    bool isAppVisible(std::size_t index) const;
    void handleG0Short();
    std::size_t findVoiceRecorderIndex() const;
    void drawHeader() const;
    bool refreshBatterySnapshot();
    bool consumeHeaderKeys(const Keyboard_Class::KeysState& status);
    void noteUserActivity();
    void maybeSleepDisplay();
    void wakeDisplay();
    bool anyBackgroundWork() const;
    void enterLowPowerWait() const;

    static AppShell* instance_;

    std::vector<std::unique_ptr<AppBase>> apps_;
    std::size_t current_index_ = 0;
    bool header_focused_ = false;
    bool header_dirty_ = true;
    bool display_asleep_ = false;
    bool ignore_keys_until_release_ = false;
    uint8_t saved_brightness_ = 128;
    uint32_t last_activity_ms_ = 0;
    KeyboardInputTracker keyboard_tracker_;
    BatterySnapshot battery_;
    uint32_t last_battery_poll_ms_ = 0;
};
