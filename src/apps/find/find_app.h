#pragma once

#include <cstdint>

#include "apps/app_base.h"
#include "ble/ble_link.h"

class FindApp final : public AppBase {
public:
    FindApp();

    const char* name() const override;
    void onEnter() override;
    void onExit() override;
    void onDisplayWake() override;
    void onBackgroundTick() override;
    bool hasActiveBackgroundWork() const override;
    NavRequest onUpdate(const Keyboard_Class::KeysState* keys, bool header_focused) override;

private:
    enum class Row : uint8_t { Sound = 0, Action = 1, Count = 2 };
    enum class Pending : uint8_t { None = 0, Start = 1, Stop = 2 };

    void markDirty() { dirty_ = true; }
    void requestHeaderFocus() { pending_header_focus_ = true; }
    void setFooter(const char* msg);
    void draw() const;
    NavRequest handleInput(const Keyboard_Class::KeysState& status);
    void loadSound();
    void saveSound() const;
    void toggleSound();
    bool ensureBle(Pending pending);
    void flushPending();
    void sendStart();
    void sendStop();
    void processBleFrames();
    void stopLocal(const char* footer);

    Row focus_ = Row::Action;
    Pending pending_ = Pending::None;
    bool sound_on_ = false;
    bool finding_ = false;
    bool awaiting_ = false;
    uint16_t req_id_ = 0;
    uint32_t connect_started_ms_ = 0;
    char footer_[40] = {};
    bool dirty_ = true;
    bool pending_header_focus_ = false;
    bool header_focused_ = false;
};
