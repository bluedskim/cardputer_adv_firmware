#include "apps/torch/torch_app.h"

#include <M5Cardputer.h>

#include "config.h"
#include "input/keyboard_input.h"
#include "system_settings.h"
#include "ui/fitted_text.h"

#include <algorithm>

namespace {

constexpr uint32_t kBlinkPeriodMs = 500;
constexpr int kLineStep = 18;
constexpr uint32_t kTimeoutStepSec = 60;

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
    applyBrightness();
    auto& display = M5Cardputer.Display;
    display.fillRect(0, config::kContentTop, display.width(),
                     display.height() - config::kContentTop, TFT_WHITE);
    draw();
}

void TorchApp::onExit() {
    SystemSettings::instance().applyBrightness();
    M5Cardputer.Display.fillScreen(TFT_BLACK);
}

void TorchApp::onDisplayWake() {
    applyBrightness();
    auto& display = M5Cardputer.Display;
    display.fillRect(0, config::kContentTop, display.width(),
                     display.height() - config::kContentTop, TFT_WHITE);
    draw();
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
            need_draw = true;
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

void TorchApp::draw() const {
    auto& display = M5Cardputer.Display;
    if (blinker_enabled_ && !blink_on_) {
        return;
    }

    const int width = display.width();
    const int center_x = width / 2;
    const int base_y = config::kContentTop + 12;

    auto drawRow = [&](Focus row, int y, const String& text, uint16_t idle_fg = TFT_BLACK) {
        const bool selected = focus_ == row && !header_focused_;
        const uint16_t fg = selected ? TFT_YELLOW : idle_fg;
        const uint16_t bg = selected ? TFT_NAVY : TFT_WHITE;
        // Text background + full-width padding repaint the row in one pass (no blank flash).
        display.setTextDatum(middle_center);
        display.setTextColor(fg, bg);
        display.setTextSize(2);
        display.setTextPadding(width);
        display.drawString(text, center_x, y);
    };

    drawRow(Focus::Bright, base_y, String("Bright ") + String(static_cast<int>(brightness_)));
    drawRow(Focus::Timeout, base_y + kLineStep,
            String("Time ") + String(static_cast<unsigned>(timeout_sec_)) + "s");
    drawCountdown();

    drawRow(Focus::Blink, base_y + kLineStep * 3,
            String("BLINK ") + (blinker_enabled_ ? "ON" : "OFF"),
            blinker_enabled_ ? TFT_RED : TFT_DARKGREY);
    display.setTextPadding(0);
    display.setTextDatum(top_left);
}

void TorchApp::drawCountdown() const {
    auto& display = M5Cardputer.Display;
    if (blinker_enabled_ && !blink_on_) {
        return;
    }

    const int width = display.width();
    const int center_x = width / 2;
    const int base_y = config::kContentTop + 12;
    const int y = base_y + kLineStep * 2;

    const uint32_t elapsed_sec = (millis() - started_ms_) / 1000;
    const uint32_t remaining = elapsed_sec >= timeout_sec_ ? 0 : timeout_sec_ - elapsed_sec;

    display.setTextDatum(middle_center);
    display.setTextColor(TFT_DARKGREY, TFT_WHITE);
    display.setTextSize(2);
    display.setTextPadding(width);
    display.drawString(String("Left ") + String(static_cast<unsigned>(remaining)) + "s", center_x, y);
    display.setTextPadding(0);
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
        default:
            break;
    }
}

NavRequest TorchApp::handleInput(const Keyboard_Class::KeysState& status) {
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
    ui::fitTextSize(display, "any key / wait", display.width() - 8);
    display.drawString("any key / wait", display.width() / 2, display.height() / 2 + 16);
    display.setTextDatum(top_left);
}
