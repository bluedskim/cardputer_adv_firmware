#include "app_shell.h"

#include <M5Cardputer.h>

#include "input/keyboard_input.h"
#include "launcher_exit.h"

namespace {

constexpr int kChromeHeight = 22;

}  // namespace

void AppShell::addApp(std::unique_ptr<AppBase> app) {
    apps_.push_back(std::move(app));
}

void AppShell::begin() {
    if (apps_.empty()) {
        return;
    }

    M5Cardputer.Display.setRotation(1);
    M5Cardputer.Display.fillScreen(TFT_BLACK);
    apps_[current_index_]->onEnter();
    chrome_dirty_ = true;
}

void AppShell::update() {
    if (apps_.empty()) {
        return;
    }

    M5Cardputer.update();

    if (M5Cardputer.BtnA.wasClicked()) {
        launcher_exit::rebootToLauncher();
    }

    const KeyEvent keys = pollKeyEvent();

    if (keys.active && consumeNavKeys(keys.status)) {
        if (chrome_dirty_) {
            drawChrome();
            chrome_dirty_ = false;
        }
        return;
    }

    apps_[current_index_]->onUpdate(keys.active ? &keys.status : nullptr);

    if (chrome_dirty_) {
        drawChrome();
        chrome_dirty_ = false;
    }
}

void AppShell::switchTo(const std::size_t index) {
    if (index >= apps_.size() || index == current_index_) {
        return;
    }

    apps_[current_index_]->onExit();
    current_index_ = index;
    M5Cardputer.Display.fillScreen(TFT_BLACK);
    apps_[current_index_]->onEnter();
    chrome_dirty_ = true;
}

bool AppShell::consumeNavKeys(const Keyboard_Class::KeysState& status) {
    if (status.enter) {
        return false;
    }

    switch (detectArrowKey(status)) {
        case ArrowKey::Left: {
            const std::size_t next =
                current_index_ == 0 ? apps_.size() - 1 : current_index_ - 1;
            switchTo(next);
            return true;
        }
        case ArrowKey::Right:
            switchTo((current_index_ + 1) % apps_.size());
            return true;
        default:
            break;
    }

    return false;
}

void AppShell::drawChrome() const {
    auto& display = M5Cardputer.Display;
    const int width = display.width();

    display.fillRect(0, 0, width, kChromeHeight, TFT_DARKGREY);
    display.setTextSize(2);
    display.setTextColor(TFT_WHITE, TFT_DARKGREY);
    display.setCursor(4, 4);
    display.printf("< %s >", apps_[current_index_]->name());

    for (std::size_t i = 0; i < apps_.size(); ++i) {
        const int dot_x = static_cast<int>(width - 12 - static_cast<int>((apps_.size() - 1 - i) * 12));
        const uint16_t color = i == current_index_ ? TFT_YELLOW : TFT_DARKGREY;
        display.fillCircle(dot_x, 11, 3, color);
        if (i == current_index_) {
            display.drawCircle(dot_x, 11, 4, TFT_WHITE);
        }
    }
}
