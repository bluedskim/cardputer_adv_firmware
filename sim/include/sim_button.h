#pragma once

#include <Arduino.h>

// Matches M5Unified Button_Class short/hold semantics for the desktop sim.
class SimButton {
public:
    static constexpr uint32_t kHoldMs = 500;

    void notifyPressed(bool pressed) { pressed_ = pressed; }

    bool wasClicked() {
        if (!clicked_) {
            return false;
        }
        clicked_ = false;
        return true;
    }

    bool wasHold() {
        if (!held_) {
            return false;
        }
        held_ = false;
        return true;
    }

    bool isPressed() const { return pressed_; }

    void update(bool pressed) {
        const uint32_t now = millis();
        if (pressed && !prev_pressed_) {
            press_start_ms_ = now;
            hold_fired_ = false;
        } else if (pressed && prev_pressed_) {
            if (!hold_fired_ && (now - press_start_ms_) >= kHoldMs) {
                held_ = true;
                hold_fired_ = true;
            }
        } else if (!pressed && prev_pressed_) {
            if (!hold_fired_) {
                clicked_ = true;
            }
        }
        prev_pressed_ = pressed;
        pressed_ = pressed;
    }

private:
    bool pressed_ = false;
    bool prev_pressed_ = false;
    bool clicked_ = false;
    bool held_ = false;
    bool hold_fired_ = false;
    uint32_t press_start_ms_ = 0;
};
