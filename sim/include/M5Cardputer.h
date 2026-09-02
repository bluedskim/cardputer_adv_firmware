#pragma once

#define SDL_MAIN_HANDLED
#include <SDL2/SDL.h>

#include <M5GFX.h>

#include "sim_button.h"
#include "sim_keyboard.h"

namespace m5 {

struct M5Unified_Config_t {};

class Power_Class {
public:
    enum is_charging_t { is_charging, is_discharging, charge_unknown };

    int32_t getBatteryLevel() { return level_; }
    is_charging_t isCharging() { return charging_; }
    int16_t getBatteryVoltage() { return voltage_mv_; }

private:
    int32_t level_ = 78;
    is_charging_t charging_ = is_charging;
    int16_t voltage_mv_ = 4120;
};

class M5Unified {
public:
    using config_t = M5Unified_Config_t;
    Power_Class Power;

    config_t config() const { return {}; }
};

}  // namespace m5

class M5_CARDPUTER {
public:
    M5GFX Display;
    Keyboard_Class Keyboard;
    SimButton BtnA;

    void begin(bool enable_keyboard = true) {
        enable_keyboard_ = enable_keyboard;
        initDisplay();
        if (enable_keyboard_) {
            sim_keyboard_init();
        }
    }

    void begin(m5::M5Unified_Config_t, bool enable_keyboard = true) { begin(enable_keyboard); }

    void update() {
        Keyboard.updateKeyList();
        Keyboard.updateKeysState();

        bool g0 = false;
        for (const char ch : Keyboard.keysState().word) {
            if (ch == '`') {
                g0 = true;
                break;
            }
        }
        BtnA.update(g0);
    }

private:
    bool enable_keyboard_ = true;

    void initDisplay() {
        Display.init();
        SDL_StopTextInput();
        Display.fillScreen(TFT_BLACK);
    }
};

inline m5::M5Unified M5;
extern M5_CARDPUTER M5Cardputer;

#include <Arduino.h>
