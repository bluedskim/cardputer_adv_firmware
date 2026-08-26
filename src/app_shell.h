#pragma once

#include <cstddef>
#include <memory>
#include <vector>

#include <M5Cardputer.h>

#include "apps/app_base.h"

class AppShell {
public:
    void addApp(std::unique_ptr<AppBase> app);
    void begin();
    void update();

private:
    void switchTo(std::size_t index);
    void drawChrome() const;
    bool consumeNavKeys(const Keyboard_Class::KeysState& status);

    std::vector<std::unique_ptr<AppBase>> apps_;
    std::size_t current_index_ = 0;
    bool chrome_dirty_ = true;
};
