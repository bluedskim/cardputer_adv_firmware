#pragma once

#include <cstddef>
#include <memory>
#include <vector>

#include <M5Cardputer.h>

#include "apps/app_base.h"
#include "input/keyboard_input.h"
#include "ui/battery_indicator.h"

class AppShell {
public:
    void addApp(std::unique_ptr<AppBase> app);
    void begin();
    void update();

private:
    void switchTo(std::size_t index);
    void drawChrome() const;
    void refreshBatterySnapshot();
    bool consumeChromeKeys(const Keyboard_Class::KeysState& status);

    std::vector<std::unique_ptr<AppBase>> apps_;
    std::size_t current_index_ = 0;
    bool chrome_focused_ = false;
    KeyboardInputTracker keyboard_tracker_;
    BatterySnapshot battery_;
    uint32_t last_battery_poll_ms_ = 0;
};
