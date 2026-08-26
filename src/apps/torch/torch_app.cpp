#include "apps/torch/torch_app.h"

#include <M5Cardputer.h>

#include "config.h"

namespace {

constexpr int kContentTop = 16;
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

void TorchApp::onUpdate() {
    if (handleInput()) {
        draw();
    }

    if (blinker_enabled_) {
        const uint32_t now = millis();
        if (now - last_blink_ms_ >= kBlinkPeriodMs) {
            blink_on_ = !blink_on_;
            last_blink_ms_ = now;
            applyBrightness();
            if (blink_on_) {
                draw();
            }
        }
    }

    checkTimeout();
}

void TorchApp::draw() const {
    auto& display = M5Cardputer.Display;
    display.fillRect(0, kContentTop, display.width(), display.height() - kContentTop, TFT_BLACK);
    display.setTextSize(1);
    display.setTextColor(TFT_BLACK, TFT_WHITE);
    display.setCursor(8, kContentTop + 8);
    display.printf("Brightness: %3u", brightness_);
    display.setCursor(8, kContentTop + 22);
    display.printf("Timeout: %4lus", timeout_sec_);

    const uint32_t elapsed_sec = (millis() - started_ms_) / 1000;
    const uint32_t remaining = elapsed_sec >= timeout_sec_ ? 0 : timeout_sec_ - elapsed_sec;
    display.setCursor(8, kContentTop + 36);
    display.printf("Remaining: %4lus", remaining);

    display.setCursor(8, kContentTop + 54);
    display.printf("Blinker: %s", blinker_enabled_ ? "ON " : "OFF");

    display.setCursor(8, display.height() - 18);
    display.setTextColor(TFT_DARKGREY, TFT_WHITE);
    display.print("+/- brightness  b blink  [/] timeout");
}

void TorchApp::applyBrightness() const {
    uint8_t level = brightness_;
    if (blinker_enabled_ && !blink_on_) {
        level = 0;
    }
    M5Cardputer.Display.fillScreen(TFT_WHITE);
    M5Cardputer.Display.setBrightness(level);
}

bool TorchApp::handleInput() {
    if (!M5Cardputer.Keyboard.isChange() || !M5Cardputer.Keyboard.isPressed()) {
        return false;
    }

    bool changed = false;
    const auto status = M5Cardputer.Keyboard.keysState();

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
    display.setTextColor(TFT_ORANGE, TFT_BLACK);
    display.setCursor(20, 50);
    display.println("Torch auto-off");
    display.setCursor(20, 66);
    display.println(", / to switch app");
    delay(1200);
    onEnter();
}
