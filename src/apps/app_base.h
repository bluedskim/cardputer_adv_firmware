#pragma once

#include <M5Cardputer.h>

#include <cstdint>

// Returned by apps to participate in the shared two-level navigation model.
enum class NavRequest {
    None,
    FocusChrome,  // ↑ from top of app content
};

class AppBase {
public:
    virtual ~AppBase() = default;

    virtual const char* name() const = 0;
    virtual void onEnter() = 0;
    virtual void onExit() = 0;
    // keys may be null (no new edge). Still called every frame for timers/UI.
    virtual NavRequest onUpdate(const Keyboard_Class::KeysState* keys) = 0;
};
