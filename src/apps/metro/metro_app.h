#pragma once

#include <cstdint>

#include "apps/app_base.h"

// Yamaha ME-110–style clip-on metronome: beat LEDs, tempo, beat, volume.
class MetroApp final : public AppBase {
public:
    MetroApp();

    const char* name() const override;
    void onEnter() override;
    void onExit() override;
    void onDisplayWake() override;
    void onBackgroundTick() override;
    bool hasActiveBackgroundWork() const override;
    NavRequest onUpdate(const Keyboard_Class::KeysState* keys, bool header_focused) override;

private:
    enum class Focus : uint8_t {
        Tempo = 0,
        Beat,
        Vol,
        Count,
    };

    void load();
    void save() const;
    void markDirty() {
        dirty_ = true;
        chrome_valid_ = false;
    }
    void markLedsDirty() { leds_dirty_ = true; }
    void requestHeaderFocus() { pending_header_focus_ = true; }
    uint8_t ledCount() const;

    void ensureSpeaker();
    void releaseSpeaker();
    void toggleRun();
    void tickBeats();
    void fireBeat();
    void adjustFocused(int delta);
    void draw();
    void drawChrome();
    void paintLeds();
    void paintFooter() const;

    uint16_t bpm_ = 120;
    uint8_t beats_ = 4;  // 0 = no accent (Yamaha 0拍子); 1–7 meter
    uint8_t vol_ = 6;    // 0–10 → Speaker volume
    Focus focus_ = Focus::Tempo;

    bool running_ = false;
    bool speaker_ok_ = false;
    uint8_t beat_index_ = 0;
    int8_t led_index_ = -1;  // lit LED index; -1 = none
    bool led_accent_ = false;
    uint32_t next_beat_ms_ = 0;
    uint32_t led_until_ms_ = 0;

    int8_t painted_led_index_ = -2;
    bool painted_led_accent_ = false;
    uint8_t painted_led_count_ = 0;

    bool dirty_ = true;
    bool leds_dirty_ = false;
    bool chrome_valid_ = false;
    bool pending_header_focus_ = false;
    bool header_focused_ = false;
};
