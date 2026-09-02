#include "apps/settings/settings_app.h"

#include <M5Cardputer.h>

#include <cstdio>

#include "config.h"
#include "input/keyboard_input.h"
#include "system_settings.h"

namespace {

constexpr int kContentTop = config::kContentTop;
constexpr int kLineHeight = 16;
constexpr int kListTop = kContentTop + 4;

}  // namespace

const char* SettingsApp::name() const { return "Settings"; }

void SettingsApp::onEnter() {
    focus_ = Row::Sleep;
    markDirty();
    requestChromeFocus();
}

void SettingsApp::onExit() { M5Cardputer.Display.fillScreen(TFT_BLACK); }

NavRequest SettingsApp::onUpdate(const Keyboard_Class::KeysState* keys, bool chrome_focused) {
    if (chrome_focused_ != chrome_focused) {
        chrome_focused_ = chrome_focused;
        markDirty();
    }

    NavRequest req = NavRequest::None;
    if (keys != nullptr) {
        req = handleInput(*keys);
        markDirty();
    }

    if (dirty_) {
        draw();
        dirty_ = false;
    }
    if (pending_chrome_focus_) {
        pending_chrome_focus_ = false;
        return NavRequest::FocusChrome;
    }
    return req;
}

NavRequest SettingsApp::handleInput(const Keyboard_Class::KeysState& status) {
    switch (detectArrowKey(status)) {
        case ArrowKey::Left:
            if (focus_ == Row::Sleep) {
                SystemSettings::instance().cycleSleepOption(-1);
            }
            return NavRequest::None;
        case ArrowKey::Right:
            if (focus_ == Row::Sleep) {
                SystemSettings::instance().cycleSleepOption(1);
            }
            return NavRequest::None;
        case ArrowKey::Up:
            if (focus_ == Row::Sleep) {
                return NavRequest::FocusChrome;
            }
            return NavRequest::None;
        case ArrowKey::Down:
            if (static_cast<uint8_t>(focus_) + 1 < static_cast<uint8_t>(Row::Count)) {
                focus_ = static_cast<Row>(static_cast<uint8_t>(focus_) + 1);
            }
            return NavRequest::None;
        default:
            break;
    }

    return NavRequest::None;
}

void SettingsApp::draw() const {
    auto& display = M5Cardputer.Display;
    display.fillRect(0, kContentTop, display.width(), display.height() - kContentTop, TFT_BLACK);

    const int width = display.width();
    int y = kListTop;
    const bool selected = focus_ == Row::Sleep && !chrome_focused_;
    const uint16_t fg = selected ? TFT_YELLOW : TFT_WHITE;
    const uint16_t bg = selected ? TFT_NAVY : TFT_BLACK;
    if (selected) {
        display.fillRect(0, y, width, kLineHeight, bg);
    }

    char line[32];
    std::snprintf(line, sizeof(line), "Sleep  %s", SystemSettings::instance().sleepOptionLabel());
    display.setTextColor(fg, bg);
    display.setTextSize(2);
    display.setCursor(8, y);
    display.printf("%c %s", selected ? '>' : ' ', line);
}
