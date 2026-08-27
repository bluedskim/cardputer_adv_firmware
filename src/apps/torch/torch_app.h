#pragma once

#include <M5Cardputer.h>

#include "apps/app_base.h"

class TorchApp final : public AppBase {
public:
    const char* name() const override;
    void onEnter() override;
    void onExit() override;
    void onUpdate(const Keyboard_Class::KeysState* keys) override;

private:
    void draw() const;
    void applyBrightness() const;
    bool handleInput(const Keyboard_Class::KeysState& status);
    void checkTimeout();

    uint8_t brightness_ = 255;
    uint32_t timeout_sec_ = 600;
    uint32_t started_ms_ = 0;
    uint32_t last_remaining_sec_ = 0;
    bool blinker_enabled_ = false;
    bool blink_on_ = true;
    uint32_t last_blink_ms_ = 0;
};
