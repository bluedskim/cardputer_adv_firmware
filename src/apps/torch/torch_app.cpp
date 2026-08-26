#include "apps/torch/torch_app.h"

#include <M5Cardputer.h>

#include "config.h"

namespace {

constexpr int kContentTop = 24;
constexpr uint32_t kBlinkPeriodMs = 500;

bool isBrightnessUp(char key) {
    return key == '=' || key == '+' || key == ']' || key == 'k' || key == 'K';
}

bool isBrightnessDown(char key) {
    return key == '-' || key == '_' || key == '[' || key == 'i' || key == 'I';
}

}  // namespace

const char* TorchApp::name() const { return "Torch"; }

void TorchApp::onEnter() {
    brightness_ = config::kTorchDefaultBrightness;
    timeout_sec_ = config::kTorchDefaultTimeoutSec;
    started_ms_ = millis();
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
    if (keys != nullptr && handleInput(*keys)) {
        draw();
    }

    if (blinker_enabled_) {
        const uint32_t now = millis();
        if (now - last_blink_ms_ >= kBlinkPeriodMs) {
            blink_on_ = !blink_on_;
            last_blink_ms_ = now;
            applyBrightness();
            draw();
        }
    }

    checkTimeout();
}

void TorchApp::draw() const {
    if (blinker_enabled_ && !blink_on_) {
        return;
    }

    auto& display = M5Cardputer.Display;
    const int width = display.width();
    const int height = display.height();
    const int center_x = width / 2;
    const int center_y = (kContentTop + height) / 2;

    display.setTextDatum(middle_center);
    display.setTextColor(TFT_BLACK, TFT_WHITE);
    display.setTextSize(3);
    display.drawString("TORCH", center_x, kContentTop + 24);

    display.setTextSize(2);
    display.drawString(String("Bright ") + brightness_, center_x, center_y - 28);
    display.drawString(String("Time ") + timeout_sec_ + "s", center_x, center_y);

    const uint32_t elapsed_sec = (millis() - started_ms_) / 1000;
    const uint32_t remaining = elapsed_sec >= timeout_sec_ ? 0 : timeout_sec_ - elapsed_sec;
    display.drawString(String("Left ") + remaining + "s", center_x, center_y + 28);

    display.setTextSize(2);
    display.setTextColor(blinker_enabled_ ? TFT_RED : TFT_DARKGREY, TFT_WHITE);
    display.drawString(String("BLINK ") + (blinker_enabled_ ? "ON" : "OFF"), center_x, center_y + 56);

    display.setTextSize(1);
    display.setTextColor(TFT_DARKGREY, TFT_WHITE);
    display.setTextDatum(bottom_center);
    display.drawString("+/- bright   B blink   [/] time   ,/ switch", center_x, height - 2);
    display.setTextDatum(top_left);
}

void TorchApp::applyBrightness() const {
    uint8_t level = brightness_;
    if (blinker_enabled_ && !blink_on_) {
        level = 0;
    }
    M5Cardputer.Display.fillScreen(TFT_WHITE);
    M5Cardputer.Display.setBrightness(level);
}

bool TorchApp::handleInput(const Keyboard_Class::KeysState& status) {
    bool changed = false;

    for (const auto key : status.word) {
        if (isBrightnessUp(key) && brightness_ < config::kTorchMaxBrightness) {
            brightness_ = static_cast<uint8_t>(brightness_ + 8);
            if (brightness_ > config::kTorchMaxBrightness) {
                brightness_ = config::kTorchMaxBrightness;
            }
            changed = true;
        } else if (isBrightnessDown(key) && brightness_ > config::kTorchMinBrightness) {
            brightness_ = static_cast<uint8_t>(brightness_ - 8);
            if (brightness_ < config::kTorchMinBrightness) {
                brightness_ = config::kTorchMinBrightness;
            }
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
