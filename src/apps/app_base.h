#pragma once

#include <cstdint>

class AppBase {
public:
    virtual ~AppBase() = default;

    virtual const char* name() const = 0;
    virtual void onEnter() = 0;
    virtual void onExit() = 0;
    virtual void onUpdate() = 0;
};
