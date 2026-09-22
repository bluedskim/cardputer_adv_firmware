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
    void addApp(std::unique_ptr<AppBase> app);
    void begin();
    void update();
    uint32_t loopDelayMs() const;

private:
    void switchTo(std::size_t index);
    void handleG0Short();
    std::size_t findVoiceRecorderIndex() const;
    void drawChrome() const;
    bool refreshBatterySnapshot();
    bool consumeChromeKeys(const Keyboard_Class::KeysState& status);
    void noteUserActivity();
    void maybeSleepDisplay();
    void sleepDisplay();
    void wakeDisplay();
    bool anyBackgroundWork() const;
    void enterLowPowerWait() const;

    std::vector<std::unique_ptr<AppBase>> apps_;
    std::size_t current_index_ = 0;
    bool chrome_focused_ = false;
    bool chrome_dirty_ = true;
    bool display_asleep_ = false;
    bool ignore_keys_until_release_ = false;
    uint8_t saved_brightness_ = 128;
    uint32_t last_activity_ms_ = 0;
    KeyboardInputTracker keyboard_tracker_;
    BatterySnapshot battery_;
    uint32_t last_battery_poll_ms_ = 0;
};
