#pragma once

#include <M5Cardputer.h>

#include "apps/app_base.h"

class TorchApp final : public AppBase {
public:
    const char* name() const override;
    void onEnter() override;
    void onExit() override;
    void onDisplayWake() override;
    NavRequest onUpdate(const Keyboard_Class::KeysState* keys, bool header_focused) override;
    bool hidesHeader() const override { return full_screen_; }

private:
    enum class Focus : uint8_t {
        Bright = 0,
        Timeout = 1,
        Blink = 2,
        Color = 3,
        FullScreen = 4,
        Count = 5,
    };

    void paintLight() const;
    void draw() const;
    void drawCountdown() const;
    void applyBrightness() const;
    NavRequest handleInput(const Keyboard_Class::KeysState& status);
    void adjustFocused(int delta);
    void setFullScreen(bool on);
    void checkTimeout();

    uint8_t brightness_ = 255;
    uint32_t timeout_sec_ = 600;
    uint32_t started_ms_ = 0;
    uint32_t last_remaining_sec_ = 0;
    Focus focus_ = Focus::Bright;
    bool blinker_enabled_ = false;
    bool blink_on_ = true;
    uint32_t last_blink_ms_ = 0;
    bool auto_off_ = false;
    uint32_t auto_off_until_ms_ = 0;
    bool header_focused_ = false;
    uint8_t color_ = 0;  // kColors index; kept when leaving the app (not across reboot)
    bool full_screen_ = false;
};
