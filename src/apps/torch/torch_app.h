#pragma once

#include "apps/app_base.h"

class TorchApp final : public AppBase {
public:
    const char* name() const override;
    void onEnter() override;
    void onExit() override;
    void onUpdate() override;

private:
    void draw() const;
    void applyBrightness() const;
    bool handleInput();
    void checkTimeout();

    uint8_t brightness_ = 255;
    uint32_t timeout_sec_ = 600;
    uint32_t started_ms_ = 0;
    bool blinker_enabled_ = false;
    bool blink_on_ = true;
    uint32_t last_blink_ms_ = 0;
};
