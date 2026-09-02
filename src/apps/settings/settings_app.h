#pragma once

#include <cstdint>

#include "apps/app_base.h"

class SettingsApp final : public AppBase {
public:
    const char* name() const override;
    void onEnter() override;
    void onExit() override;
    NavRequest onUpdate(const Keyboard_Class::KeysState* keys, bool chrome_focused) override;

private:
    enum class Row : uint8_t {
        Sleep = 0,
        Count = 1,
    };

    void draw() const;
    NavRequest handleInput(const Keyboard_Class::KeysState& status);
    void markDirty() { dirty_ = true; }
    void requestChromeFocus() { pending_chrome_focus_ = true; }

    Row focus_ = Row::Sleep;
    bool dirty_ = true;
    bool pending_chrome_focus_ = false;
    bool chrome_focused_ = false;
};
