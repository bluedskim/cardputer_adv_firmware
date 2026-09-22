#pragma once

#define SDL_MAIN_HANDLED
#include <SDL2/SDL.h>

#include <M5GFX.h>

#include <cstddef>
#include <cstdint>

#include "sim_button.h"
#include "sim_keyboard.h"

namespace m5 {

struct M5Unified_Config_t {};

class Power_Class {
public:
    enum is_charging_t { is_charging, is_discharging, charge_unknown };
    static constexpr uint64_t sleep_no_timer = ~0ull;

    int32_t getBatteryLevel() { return level_; }
    is_charging_t isCharging() { return charging_; }
    int16_t getBatteryVoltage() { return voltage_mv_; }
    void lightSleep(uint64_t micro_seconds = sleep_no_timer, bool touch_wakeup = true) {
        (void)touch_wakeup;
        if (micro_seconds != 0 && micro_seconds != sleep_no_timer) {
            SDL_Delay(static_cast<Uint32>(micro_seconds / 1000ULL));
        }
    }

private:
    int32_t level_ = 78;
    is_charging_t charging_ = is_charging;
    int16_t voltage_mv_ = 4120;
};

// Desktop stubs — no real audio; Recorder shows "SD unavailable".
class Mic_Class {
public:
    bool begin() { running_ = true; return true; }
    void end() { running_ = false; }
    bool isRunning() const { return running_; }
    bool isEnabled() const { return true; }
    size_t isRecording() const { return 0; }
    bool record(int16_t*, size_t, uint32_t, bool = false) { return false; }
    bool record(uint8_t*, size_t, uint32_t, bool = false) { return false; }
    bool record(int16_t*, size_t) { return false; }
    bool record(uint8_t*, size_t) { return false; }

private:
    bool running_ = false;
};

class Speaker_Class {
public:
    bool begin() { running_ = true; return true; }
    void end() {
        running_ = false;
        playing_ = false;
    }
    bool isRunning() const { return running_; }
    bool isEnabled() const { return true; }
    bool isPlaying() const { return playing_; }
    size_t isPlaying(uint8_t) const { return playing_ ? 2 : 0; }
    void setVolume(uint8_t) {}
    void stop() { playing_ = false; }
    void stop(uint8_t) { playing_ = false; }
    bool playRaw(const int16_t*, size_t, uint32_t = 44100, bool = false, uint32_t = 1, int = -1,
                 bool = false) {
        return false;
    }

private:
    bool running_ = false;
    bool playing_ = false;
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
    m5::Mic_Class Mic;
    m5::Speaker_Class Speaker;

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
