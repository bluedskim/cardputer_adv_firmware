#include "apps/torch/torch_app.h"

#include <M5Cardputer.h>

#include "app_shell.h"
#include "config.h"
#include "input/keyboard_input.h"
#include "system_settings.h"
#include "ui/fitted_text.h"

#include <algorithm>

namespace {

constexpr uint32_t kBlinkPeriodMs = 500;
constexpr int kLineStep = 18;
constexpr uint32_t kTimeoutStepSec = 60;

struct LightColor {
    const char* name;
    uint16_t light;  // torch fill
    uint16_t ink;    // row text drawn on the fill
};

constexpr LightColor kColors[] = {
    {"White", TFT_WHITE, TFT_BLACK},
    {"Red", TFT_RED, TFT_BLACK},
    {"Green", TFT_GREEN, TFT_BLACK},
    {"Blue", TFT_BLUE, TFT_WHITE},
    {"Yellow", TFT_YELLOW, TFT_BLACK},
};
constexpr uint8_t kColorCount = sizeof(kColors) / sizeof(kColors[0]);

}  // namespace

const char* TorchApp::name() const { return "Torch"; }

void TorchApp::onEnter() {
    brightness_ = config::kTorchDefaultBrightness;
    timeout_sec_ = config::kTorchDefaultTimeoutSec;
    started_ms_ = millis();
    last_remaining_sec_ = timeout_sec_;
    focus_ = Focus::Bright;
    blinker_enabled_ = false;
    blink_on_ = true;
    last_blink_ms_ = started_ms_;
    auto_off_ = false;
    auto_off_until_ms_ = 0;
    full_screen_ = false;
    applyBrightness();
    paintLight();
}

void TorchApp::onExit() {
    SystemSettings::instance().applyBrightness();
    M5Cardputer.Display.fillScreen(TFT_BLACK);
}

void TorchApp::onDisplayWake() {
    applyBrightness();
    paintLight();
}

NavRequest TorchApp::onUpdate(const Keyboard_Class::KeysState* keys, bool header_focused) {
    if (auto_off_) {
        if (keys != nullptr) {
            onEnter();
            return NavRequest::None;
        }
        checkTimeout();
        return NavRequest::None;
    }

    NavRequest req = NavRequest::None;
    bool need_draw = false;

    if (header_focused_ != header_focused) {
        header_focused_ = header_focused;
        need_draw = true;
    }

    if (keys != nullptr) {
        req = handleInput(*keys);
        need_draw = true;
    }

    if (blinker_enabled_) {
        const uint32_t now = millis();
        if (now - last_blink_ms_ >= kBlinkPeriodMs) {
            blink_on_ = !blink_on_;
            last_blink_ms_ = now;
            applyBrightness();
            if (blink_on_) {
                paintLight();
            }
        }
    }

    const uint32_t elapsed_sec = (millis() - started_ms_) / 1000;
    const uint32_t remaining =
        elapsed_sec >= timeout_sec_ ? 0 : timeout_sec_ - elapsed_sec;
    if (remaining != last_remaining_sec_) {
        last_remaining_sec_ = remaining;
        drawCountdown();
    }

    if (need_draw) {
        draw();
    }

    checkTimeout();
    return req;
}

void TorchApp::paintLight() const {
    auto& display = M5Cardputer.Display;
    const int top = full_screen_ ? 0 : config::kContentTop;
    display.fillRect(0, top, display.width(), display.height() - top, kColors[color_].light);
    draw();
}

void TorchApp::draw() const {
    auto& display = M5Cardputer.Display;
    if (full_screen_ || (blinker_enabled_ && !blink_on_)) {
        return;
    }

    const int width = display.width();
    const int center_x = width / 2;
    const int base_y = config::kContentTop + 12;

    auto drawRow = [&](Focus row, int y, const String& text) {
        const bool selected = focus_ == row && !header_focused_;
        const uint16_t fg = selected ? TFT_YELLOW : kColors[color_].ink;
        const uint16_t bg = selected ? TFT_NAVY : kColors[color_].light;
        display.fillRect(0, y - 8, width, 16, bg);
        display.setTextDatum(middle_center);
        display.setTextColor(fg, bg);
        display.setTextSize(2);
        display.drawString(text, center_x, y);
    };

    drawRow(Focus::Bright, base_y, String("Bright ") + String(static_cast<int>(brightness_)));
    drawRow(Focus::Timeout, base_y + kLineStep,
            String("Time ") + String(static_cast<unsigned>(timeout_sec_)) + "s");
    drawCountdown();

    drawRow(Focus::Blink, base_y + kLineStep * 3,
            String("BLINK ") + (blinker_enabled_ ? "ON" : "OFF"));
    drawRow(Focus::Color, base_y + kLineStep * 4, String("Color ") + kColors[color_].name);
    drawRow(Focus::FullScreen, base_y + kLineStep * 5, "Full screen");
    display.setTextDatum(top_left);
}

void TorchApp::drawCountdown() const {
    auto& display = M5Cardputer.Display;
    if (full_screen_ || (blinker_enabled_ && !blink_on_)) {
        return;
    }

    const int width = display.width();
    const int center_x = width / 2;
    const int base_y = config::kContentTop + 12;
    const int y = base_y + kLineStep * 2;

    const uint32_t elapsed_sec = (millis() - started_ms_) / 1000;
    const uint32_t remaining = elapsed_sec >= timeout_sec_ ? 0 : timeout_sec_ - elapsed_sec;

    display.fillRect(0, y - 8, width, 16, kColors[color_].light);
    display.setTextDatum(middle_center);
    display.setTextColor(kColors[color_].ink, kColors[color_].light);
    display.setTextSize(2);
    display.drawString(String("Left ") + String(static_cast<unsigned>(remaining)) + "s", center_x, y);
    display.setTextDatum(top_left);
}

void TorchApp::applyBrightness() const {
    auto& display = M5Cardputer.Display;
    const int content_y = config::kContentTop;
    const int content_h = display.height() - content_y;

    if (blinker_enabled_ && !blink_on_) {
        display.fillRect(0, content_y, display.width(), content_h, TFT_BLACK);
        display.setBrightness(0);
        return;
    }

    display.setBrightness(brightness_);
}

void TorchApp::adjustFocused(const int delta) {
    switch (focus_) {
        case Focus::Bright: {
            const int next = static_cast<int>(brightness_) + delta * 8;
            if (next > config::kTorchMaxBrightness) {
                brightness_ = config::kTorchMaxBrightness;
            } else if (next < config::kTorchMinBrightness) {
                brightness_ = config::kTorchMinBrightness;
            } else {
                brightness_ = static_cast<uint8_t>(next);
            }
            applyBrightness();
            break;
        }
        case Focus::Timeout: {
            if (delta > 0) {
                timeout_sec_ = std::min(timeout_sec_ + kTimeoutStepSec, config::kTorchMaxTimeoutSec);
            } else if (timeout_sec_ > kTimeoutStepSec) {
                timeout_sec_ -= kTimeoutStepSec;
            }
            break;
        }
        case Focus::Blink:
            blinker_enabled_ = !blinker_enabled_;
            blink_on_ = true;
            last_blink_ms_ = millis();
            applyBrightness();
            break;
        case Focus::Color:
            color_ = static_cast<uint8_t>((color_ + kColorCount + delta) % kColorCount);
            paintLight();
            break;
        default:
            break;
    }
}

void TorchApp::setFullScreen(const bool on) {
    full_screen_ = on;
    paintLight();
    if (!on && AppShell::instance() != nullptr) {
        AppShell::instance()->markHeaderDirty();
    }
}

NavRequest TorchApp::handleInput(const Keyboard_Class::KeysState& status) {
    if (full_screen_) {
        setFullScreen(false);  // any key brings the controls back
        return NavRequest::None;
    }

    switch (detectArrowKey(status)) {
        case ArrowKey::Up:
            if (focus_ == Focus::Bright) {
                return NavRequest::FocusHeader;
            }
            focus_ = static_cast<Focus>(static_cast<uint8_t>(focus_) - 1);
            return NavRequest::None;
        case ArrowKey::Down:
            if (static_cast<uint8_t>(focus_) + 1 < static_cast<uint8_t>(Focus::Count)) {
                focus_ = static_cast<Focus>(static_cast<uint8_t>(focus_) + 1);
            }
            return NavRequest::None;
        case ArrowKey::Left:
            adjustFocused(-1);
            return NavRequest::None;
        case ArrowKey::Right:
            adjustFocused(1);
            return NavRequest::None;
        default:
            break;
    }

    if (status.enter && focus_ == Focus::Blink) {
        adjustFocused(1);
    } else if (status.enter && focus_ == Focus::FullScreen) {
        setFullScreen(true);
    }

    return NavRequest::None;
}

void TorchApp::checkTimeout() {
    if (auto_off_) {
        if (millis() >= auto_off_until_ms_) {
            onEnter();
        }
        return;
    }

    const uint32_t elapsed_sec = (millis() - started_ms_) / 1000;
    if (elapsed_sec < timeout_sec_) {
        return;
    }

    if (full_screen_) {
        setFullScreen(false);
    }
    auto_off_ = true;
    auto_off_until_ms_ = millis() + 1200;
    M5Cardputer.Display.setBrightness(128);
    auto& display = M5Cardputer.Display;
    display.fillRect(0, config::kContentTop, display.width(),
                     display.height() - config::kContentTop, TFT_BLACK);
    display.setTextDatum(middle_center);
    display.setTextSize(2);
    display.setTextColor(TFT_ORANGE, TFT_BLACK);
    display.drawString("Torch auto-off", display.width() / 2, display.height() / 2 - 12);
    display.setTextColor(TFT_DARKGREY, TFT_BLACK);
    ui::fitTextSize("any key / wait", display.width() - 8);
    display.drawString("any key / wait", display.width() / 2, display.height() / 2 + 16);
    display.setTextDatum(top_left);
}
