#include "apps/torch/torch_app.h"

#include <M5Cardputer.h>

#include "config.h"

namespace {

constexpr uint32_t kBlinkPeriodMs = 500;
constexpr int kLineStep = 18;

bool isBrightnessUp(char key) {
    return key == '=' || key == '+';
}

bool isBrightnessDown(char key) {
    return key == '-' || key == '_';
}

}  // namespace

const char* TorchApp::name() const { return "Torch"; }

void TorchApp::onEnter() {
    brightness_ = config::kTorchDefaultBrightness;
    timeout_sec_ = config::kTorchDefaultTimeoutSec;
    started_ms_ = millis();
    last_remaining_sec_ = timeout_sec_;
    blinker_enabled_ = false;
    blink_on_ = true;
    last_blink_ms_ = started_ms_;
    applyBrightness();
    draw();
}

void TorchApp::onExit() {
    M5Cardputer.Display.setBrightness(128);
    M5Cardputer.Display.fillScreen(TFT_BLACK);
}

void TorchApp::onUpdate(const Keyboard_Class::KeysState* keys) {
    bool need_draw = false;

    if (keys != nullptr && handleInput(*keys)) {
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
        need_draw = true;
    }

    if (need_draw) {
        draw();
    }

    checkTimeout();
}

void TorchApp::draw() const {
    auto& display = M5Cardputer.Display;
    if (blinker_enabled_ && !blink_on_) {
        return;
    }

    const int width = display.width();
    const int height = display.height();
    const int center_x = width / 2;
    const int content_bottom = height - config::kFooterHeight;
    const int base_y = config::kContentTop + 12;

    display.setTextDatum(middle_center);
    display.setTextColor(TFT_BLACK, TFT_WHITE);
    display.setTextSize(2);
    display.drawString(String("Bright ") + String(static_cast<int>(brightness_)), center_x, base_y);
    display.drawString(String("Time ") + String(static_cast<unsigned>(timeout_sec_)) + "s", center_x,
                       base_y + kLineStep);

    const uint32_t elapsed_sec = (millis() - started_ms_) / 1000;
    const uint32_t remaining = elapsed_sec >= timeout_sec_ ? 0 : timeout_sec_ - elapsed_sec;
    display.drawString(String("Left ") + String(static_cast<unsigned>(remaining)) + "s", center_x,
                       base_y + kLineStep * 2);

    display.setTextColor(blinker_enabled_ ? TFT_RED : TFT_DARKGREY, TFT_WHITE);
    display.drawString(String("BLINK ") + (blinker_enabled_ ? "ON" : "OFF"), center_x,
                       base_y + kLineStep * 3);

    display.setTextSize(1);
    display.setTextColor(TFT_DARKGREY, TFT_WHITE);
    display.setTextDatum(top_center);
    display.drawString("+/- bright  B blink", center_x, content_bottom);
    display.drawString("[/] time  ,/ switch", center_x, content_bottom + 8);
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

    display.fillRect(0, content_y, display.width(), content_h, TFT_WHITE);
    display.setBrightness(brightness_);
}

bool TorchApp::handleInput(const Keyboard_Class::KeysState& status) {
    bool changed = false;

    for (const auto key : status.word) {
        if (isBrightnessUp(key) && brightness_ < config::kTorchMaxBrightness) {
            const int next = static_cast<int>(brightness_) + 8;
            brightness_ = static_cast<uint8_t>(
                next > config::kTorchMaxBrightness ? config::kTorchMaxBrightness : next);
            changed = true;
        } else if (isBrightnessDown(key) && brightness_ > config::kTorchMinBrightness) {
            const int next = static_cast<int>(brightness_) - 8;
            brightness_ = static_cast<uint8_t>(
                next < config::kTorchMinBrightness ? config::kTorchMinBrightness : next);
            changed = true;
        } else if (key == 'b' || key == 'B') {
            blinker_enabled_ = !blinker_enabled_;
            blink_on_ = true;
            last_blink_ms_ = millis();
            changed = true;
        } else if (key == ']') {
            timeout_sec_ += 60;
            changed = true;
        } else if (key == '[' && timeout_sec_ > 60) {
            timeout_sec_ -= 60;
            changed = true;
        }
    }

    if (changed) {
        applyBrightness();
    }

    return changed;
}

void TorchApp::checkTimeout() {
    const uint32_t elapsed_sec = (millis() - started_ms_) / 1000;
    if (elapsed_sec < timeout_sec_) {
        return;
    }

    onExit();
    auto& display = M5Cardputer.Display;
    display.fillScreen(TFT_BLACK);
    display.setTextDatum(middle_center);
    display.setTextSize(2);
    display.setTextColor(TFT_ORANGE, TFT_BLACK);
    display.drawString("Torch auto-off", display.width() / 2, display.height() / 2 - 12);
    display.setTextSize(1);
    display.drawString(", / to switch app", display.width() / 2, display.height() / 2 + 16);
    display.setTextDatum(top_left);
    delay(1200);
    onEnter();
}
