#pragma once

#define SDL_MAIN_HANDLED
#include <SDL2/SDL.h>

#include <M5GFX.h>

#include "sim_button.h"
#include "sim_keyboard.h"

namespace m5 {

struct M5Unified_Config_t {};

class M5Unified {
public:
    using config_t = M5Unified_Config_t;
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
