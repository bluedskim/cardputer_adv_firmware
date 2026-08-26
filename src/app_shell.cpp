#include "app_shell.h"

#include <M5Cardputer.h>

namespace {

constexpr int kChromeHeight = 14;

bool isPrevKey(char key) {
    return key == ';' || key == ',' || key == 'j' || key == 'J';
}

bool isNextKey(char key) {
    return key == '.' || key == 'l' || key == 'L';
}

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

    if (consumeNavKeys()) {
        return;
    }

    apps_[current_index_]->onUpdate();

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

bool AppShell::consumeNavKeys() {
    if (!M5Cardputer.Keyboard.isChange()) {
        return false;
    }

    if (M5Cardputer.Keyboard.isPressed()) {
        const auto status = M5Cardputer.Keyboard.keysState();

        if (status.enter) {
            return false;
        }

        for (const auto key : status.word) {
            if (isPrevKey(key)) {
                const std::size_t next =
                    current_index_ == 0 ? apps_.size() - 1 : current_index_ - 1;
                switchTo(next);
                return true;
            }
            if (isNextKey(key)) {
                switchTo((current_index_ + 1) % apps_.size());
                return true;
            }
        }
    }

    return false;
}

void AppShell::drawChrome() const {
    auto& display = M5Cardputer.Display;
    const int width = display.width();

    display.fillRect(0, 0, width, kChromeHeight, TFT_DARKGREY);
    display.setTextSize(1);
    display.setTextColor(TFT_WHITE, TFT_DARKGREY);
    display.setCursor(4, 3);
    display.printf("< %s >", apps_[current_index_]->name());

    for (std::size_t i = 0; i < apps_.size(); ++i) {
        const int dot_x = static_cast<int>(width - 8 - static_cast<int>((apps_.size() - 1 - i) * 8));
        const uint16_t color = i == current_index_ ? TFT_YELLOW : TFT_DARKGREY;
        display.fillCircle(dot_x, 7, 2, color);
        if (i == current_index_) {
            display.drawCircle(dot_x, 7, 3, TFT_WHITE);
        }
    }
}
