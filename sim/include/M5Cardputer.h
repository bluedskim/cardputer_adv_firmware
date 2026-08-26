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

private:
    int32_t level_ = 78;
    is_charging_t charging_ = is_discharging;
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
    }

    void begin(m5::M5Unified_Config_t, bool enable_keyboard = true) { begin(enable_keyboard); }

    void update() {
        Keyboard.updateKeyList();
        Keyboard.updateKeysState();

        const Uint8* state = SDL_GetKeyboardState(nullptr);
        const bool g0 = state != nullptr && state[SDL_SCANCODE_GRAVE];
        BtnA.update(g0);
    }

private:
    bool enable_keyboard_ = true;

    void initDisplay() {
        auto* panel = new lgfx::Panel_sdl();
        panel->setScaling(3, 3);
        panel->setWindowTitle("cardputer_adv_firmware (sim)");
        Display.setPanel(panel);
        Display.init();
        Display.setRotation(1);
        Display.fillScreen(TFT_BLACK);
    }
};

inline m5::M5Unified M5;
extern M5_CARDPUTER M5Cardputer;

#include <Arduino.h>
