#pragma once

#include <M5Cardputer.h>

#include <cstdint>

class AppBase {
public:
    virtual ~AppBase() = default;

    virtual const char* name() const = 0;
    virtual void onEnter() = 0;
    virtual void onExit() = 0;
    virtual void onUpdate(const Keyboard_Class::KeysState* keys) = 0;
};
