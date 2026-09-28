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
    NavRequest onUpdate(const Keyboard_Class::KeysState* keys, bool header_focused) override;

private:
    enum class Screen : uint8_t {
        Main = 0,
        Ble = 1,
        BleForgetConfirm = 2,
    };

    enum class MainRow : uint8_t {
        Sleep = 0,
        Bright = 1,
        Ble = 2,
        Count = 3,
    };

    // Context actions on the BLE screen (not all are always visible).
    enum class BleRow : uint8_t {
        Action = 0,  // Pair / Connect / Cancel / Disconnect
        Forget = 1,  // only when Off Paired
    };

    void refreshBattery();
    void draw() const;
    void drawMain() const;
    void drawBle() const;
    void drawBleForgetConfirm() const;
    NavRequest handleInput(const Keyboard_Class::KeysState& status);
    NavRequest handleMainInput(const Keyboard_Class::KeysState& status);
    NavRequest handleBleInput(const Keyboard_Class::KeysState& status);
    NavRequest handleBleForgetConfirmInput(const Keyboard_Class::KeysState& status);
    void enterBleScreen();
    void leaveBleScreen();
    void runPrimaryBleAction();
    const char* primaryBleActionLabel() const;
    bool showForgetRow() const;
    void markDirty() { dirty_ = true; }
    void requestHeaderFocus() { pending_header_focus_ = true; }

    Screen screen_ = Screen::Main;
    MainRow focus_ = MainRow::Sleep;
    BleRow ble_focus_ = BleRow::Action;
    BatterySnapshot battery_;
    uint32_t last_battery_ms_ = 0;
    bool dirty_ = true;
    bool pending_header_focus_ = false;
    bool header_focused_ = false;
};
