#pragma once

#include "apps/app_base.h"

class LevelApp final : public AppBase {
public:
    const char* name() const override;
    void onEnter() override;
    void onExit() override;
    void onDisplayWake() override;
    NavRequest onUpdate(const Keyboard_Class::KeysState* keys, bool header_focused) override;

private:
    void markDirty() { dirty_ = true; }
    bool sample();
    void draw();

    bool imu_ok_ = false;
    bool level_ = false;
    bool smooth_ready_ = false;
    bool chrome_valid_ = false;
    float roll_deg_ = 0.0f;
    float pitch_deg_ = 0.0f;
    int roll_px_ = 0;
    int pitch_px_ = 0;
    int roll_shown_ = 0;
    int pitch_shown_ = 0;
    int painted_roll_px_ = 0;
    int painted_pitch_px_ = 0;
    int painted_roll_shown_ = 0;
    int painted_pitch_shown_ = 0;
    bool painted_roll_near_ = false;
    bool painted_pitch_near_ = false;
    bool painted_level_ = false;
    bool painted_imu_ = false;
    bool dirty_ = true;
    bool header_focused_ = false;
};
