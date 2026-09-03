#pragma once

#include <cstdint>
#include <string>

#include "apps/app_base.h"
#include "apps/timer/timer_storage.h"

class TimerApp final : public AppBase {
public:
    TimerApp();

    const char* name() const override;
    void onEnter() override;
    void onExit() override;
    void onBackgroundTick() override;
    bool hasActiveBackgroundWork() const override;
    NavRequest onUpdate(const Keyboard_Class::KeysState* keys, bool chrome_focused) override;

private:
    enum class Screen {
        List,
        Edit,
        ConfirmDelete,
    };

    enum class ListAction : uint8_t {
        None = 0,
        New,
        Start,
        Remove,
    };

    void enterList();
    void enterEdit(std::size_t index);
    void tickTimers();
    void draw() const;
    void drawListRow(int y, std::size_t index, bool selected) const;
    NavRequest handleInput(const Keyboard_Class::KeysState& status);
    void activateSelection();
    void startSelected();
    void goBack();
    void ensureCursorVisible();
    int visibleRowCount() const;
    std::size_t listRowCount() const;
    bool isActionRow(std::size_t index, ListAction* out = nullptr) const;
    bool isRunning(const TimerEntry& entry) const;
    uint32_t remainingMs(const TimerEntry& entry) const;
    void formatPreset(char* buf, std::size_t len, uint8_t hours, uint8_t minutes) const;
    void formatRemaining(char* buf, std::size_t len, uint32_t ms) const;
    void markDirty() { dirty_ = true; }
    void requestChromeFocus() { pending_chrome_focus_ = true; }
    bool isRootScreen() const { return screen_ == Screen::List; }

    TimerStorage storage_;
    Screen screen_ = Screen::List;
    std::size_t selected_ = 0;
    std::size_t cursor_ = 0;
    std::size_t scroll_ = 0;
    std::size_t edit_hours_ = 0;
    std::size_t edit_minutes_ = 5;
    uint32_t last_tick_sec_ = 0;
    bool dirty_ = true;
    bool pending_chrome_focus_ = false;
    bool chrome_focused_ = false;
};
