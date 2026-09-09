#pragma once

#include <cstdint>

#include "apps/app_base.h"
#include "ui/battery_indicator.h"

class SettingsApp final : public AppBase {
public:
    const char* name() const override;
    void onEnter() override;
    void onExit() override;
    void onDisplayWake() override;
    NavRequest onUpdate(const Keyboard_Class::KeysState* keys, bool chrome_focused) override;

private:
    enum class Row : uint8_t {
        Sleep = 0,
        Bright = 1,
        BleAction = 2,
        Count = 3,
    };

    enum class BleAction : uint8_t {
        Pair = 0,
        Connect = 1,
        Disconnect = 2,
        Forget = 3,
        Count = 4,
    };

    void refreshBattery();
    void draw() const;
    NavRequest handleInput(const Keyboard_Class::KeysState& status);
    void activateBleAction();
    void cycleBleAction(int delta);
    const char* bleActionLabel() const;
    void markDirty() { dirty_ = true; }
    void requestChromeFocus() { pending_chrome_focus_ = true; }

    Row focus_ = Row::Sleep;
    BleAction ble_action_ = BleAction::Pair;
    BatterySnapshot battery_;
    uint32_t last_battery_ms_ = 0;
    bool dirty_ = true;
    bool pending_chrome_focus_ = false;
    bool chrome_focused_ = false;
};
