#pragma once

#include <cstdint>

#include "apps/app_base.h"

// Reference pitches for Standard guitar open strings (EADGBE, A4=440).
class TunerApp final : public AppBase {
public:
    TunerApp();

    const char* name() const override;
    void onEnter() override;
    void onExit() override;
    void onDisplayWake() override;
    bool hasActiveBackgroundWork() const override;
    NavRequest onUpdate(const Keyboard_Class::KeysState* keys, bool header_focused) override;

private:
    void load();
    void save() const;
    void markDirty() { dirty_ = true; }
    void requestHeaderFocus() { pending_header_focus_ = true; }

    void ensureSpeaker();
    void releaseSpeaker();
    void startTone();
    void stopTone();
    void togglePlay();
    void selectString(int delta);
    void draw();
    void paintFooter() const;

    static constexpr uint8_t kStringCount = 6;

    uint8_t string_index_ = 0;  // 0=E2 .. 5=E4
    bool playing_ = false;
    bool speaker_ok_ = false;

    bool dirty_ = true;
    bool pending_header_focus_ = false;
    bool header_focused_ = false;
};
