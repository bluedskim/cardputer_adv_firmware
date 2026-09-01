#pragma once

#include <cstdint>
#include <string>

#include "apps/app_base.h"
#include "apps/stopwatch/stopwatch_storage.h"

class StopwatchApp final : public AppBase {
public:
    StopwatchApp();

    const char* name() const override;
    void onEnter() override;
    void onExit() override;
    NavRequest onUpdate(const Keyboard_Class::KeysState* keys) override;

private:
    enum class Screen {
        Main,
        TextInput,
        History,
    };

    enum class MainRow : uint8_t {
        Name = 0,
        RunStop = 1,
        History = 2,
        Count = 3,
    };

    void draw() const;
    void drawElapsed() const;
    NavRequest handleInput(const Keyboard_Class::KeysState& status);
    void toggleRunStop();
    void beginTextInput();
    void commitTextInput();
    void goBack();
    void formatDuration(char* buf, std::size_t len, uint32_t ms) const;
    uint32_t currentElapsedMs() const;
    bool consumeTextChar(char key);
    void markDirty() { dirty_ = true; }

    StopwatchStorage storage_;
    Screen screen_ = Screen::Main;
    MainRow focus_ = MainRow::Name;
    std::size_t history_cursor_ = 0;
    std::size_t history_scroll_ = 0;
    std::string input_buffer_;
    bool running_ = false;
    uint32_t start_ms_ = 0;
    uint32_t elapsed_ms_ = 0;
    uint32_t last_draw_cs_ = 0;
    bool dirty_ = true;
};
