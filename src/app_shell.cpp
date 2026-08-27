#include "app_shell.h"

#include <M5Cardputer.h>

#include "config.h"
#include "input/keyboard_input.h"
#include "launcher_exit.h"
#include "ui/battery_indicator.h"

namespace {

constexpr uint32_t kBatteryPollMs = 1000;
constexpr int kBatteryWidth = 46;

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
    chrome_focused_ = false;
    apps_[current_index_]->onEnter();
    battery_ = readBatterySnapshot();
    last_battery_poll_ms_ = millis();
    drawChrome();
}

void AppShell::update() {
    if (apps_.empty()) {
        return;
    }

    M5Cardputer.update();

    if (M5Cardputer.BtnA.wasClicked()) {
        launcher_exit::rebootToLauncher();
    }

    const auto status = M5Cardputer.Keyboard.keysState();

    if (chrome_focused_) {
        consumeChromeKeys(status);
        apps_[current_index_]->onUpdate(nullptr);
        refreshBatterySnapshot();
        drawChrome();
        return;
    }

    const ArrowKey arrow = pollArrowEdge(status, keyboard_tracker_);
    const KeyEvent keys = pollAppKeyEdge(status, keyboard_tracker_);

    Keyboard_Class::KeysState delivered = keys.status;
    if (arrow != ArrowKey::None) {
        // Synthesize a one-shot arrow into word so apps can use detectArrowKey.
        delivered.word.clear();
        switch (arrow) {
            case ArrowKey::Up:
                delivered.word.push_back(';');
                break;
            case ArrowKey::Down:
                delivered.word.push_back('.');
                break;
            case ArrowKey::Left:
                delivered.word.push_back(',');
                break;
            case ArrowKey::Right:
                delivered.word.push_back('/');
                break;
            default:
                break;
        }
    }

    const bool has_input = keys.active || arrow != ArrowKey::None;
    const NavRequest req =
        apps_[current_index_]->onUpdate(has_input ? &delivered : nullptr);
    if (req == NavRequest::FocusChrome) {
        chrome_focused_ = true;
    }

    refreshBatterySnapshot();
    drawChrome();
}

void AppShell::refreshBatterySnapshot() {
    const uint32_t now = millis();
    if (now - last_battery_poll_ms_ < kBatteryPollMs) {
        return;
    }

    last_battery_poll_ms_ = now;
    battery_ = readBatterySnapshot();
}

void AppShell::switchTo(const std::size_t index) {
    if (index >= apps_.size() || index == current_index_) {
        return;
    }

    apps_[current_index_]->onExit();
    current_index_ = index;
    M5Cardputer.Display.fillScreen(TFT_BLACK);
    apps_[current_index_]->onEnter();
    battery_ = readBatterySnapshot();
    last_battery_poll_ms_ = millis();
}

bool AppShell::consumeChromeKeys(const Keyboard_Class::KeysState& status) {
    const ArrowKey arrow = pollArrowEdge(status, keyboard_tracker_);
    const bool enter_edge = status.enter && !keyboard_tracker_.enter_down;
    keyboard_tracker_.enter_down = status.enter;
    keyboard_tracker_.del_down = status.del;

    switch (arrow) {
        case ArrowKey::Left: {
            const std::size_t next =
                current_index_ == 0 ? apps_.size() - 1 : current_index_ - 1;
            switchTo(next);
            return true;
        }
        case ArrowKey::Right:
            switchTo((current_index_ + 1) % apps_.size());
            return true;
        case ArrowKey::Down:
            chrome_focused_ = false;
            return true;
        default:
            break;
    }

    if (enter_edge) {
        chrome_focused_ = false;
        return true;
    }
    return false;
}

void AppShell::drawChrome() const {
    auto& display = M5Cardputer.Display;
    const int width = display.width();
    const int dots_width = static_cast<int>(apps_.size()) * 12 + 8;
    const int battery_x = width - dots_width - kBatteryWidth;
    const uint16_t bar = chrome_focused_ ? TFT_NAVY : TFT_DARKGREY;
    const uint16_t title = chrome_focused_ ? TFT_YELLOW : TFT_WHITE;

    display.fillRect(0, 0, width, config::kChromeHeight, bar);
    display.setTextSize(2);
    display.setTextColor(title, bar);
    display.setCursor(4, 4);
    display.printf("< %s >", apps_[current_index_]->name());

    drawBatteryIndicator(display, battery_x, 6, battery_);

    for (std::size_t i = 0; i < apps_.size(); ++i) {
        const int dot_x = static_cast<int>(width - 12 - static_cast<int>((apps_.size() - 1 - i) * 12));
        const uint16_t color = i == current_index_ ? TFT_YELLOW : TFT_DARKGREY;
        display.fillCircle(dot_x, 11, 3, color);
        if (i == current_index_) {
            display.drawCircle(dot_x, 11, 4, TFT_WHITE);
        }
    }
}
