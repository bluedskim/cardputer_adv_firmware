#include "apps/level/level_app.h"

#include <M5Cardputer.h>

#include <cmath>
#include <cstdio>

#include "config.h"
#include "input/keyboard_input.h"
#include "ui/fitted_text.h"

namespace {

constexpr int kContentTop = config::kContentTop;
constexpr float kLevelDeadbandDeg = 1.0f;
constexpr float kBarFullScaleDeg = 30.0f;
constexpr float kRadToDeg = 57.2957795f;
constexpr float kSmooth = 0.25f;

// Screen up, keyboard toward the user. Flip a sign if that axis moves backward
// on the device; M5Unified does not set a Cardputer-Adv axis order.
constexpr float kRollSign = 1.0f;
constexpr float kPitchSign = 1.0f;

struct Bars {
    int roll_x;
    int roll_y;
    int roll_w;
    int roll_h;
    int pitch_x;
    int pitch_y;
    int pitch_w;
    int pitch_h;
    int roll_num_x;
    int roll_num_y;
    int pitch_num_x;
    int pitch_num_y;
    int label_y;
};

int shownDegrees(const float deg) {
    return static_cast<int>(deg);
}

int markerOffset(const float deg, const int track) {
    float clamped = deg;
    if (clamped > kBarFullScaleDeg) {
        clamped = kBarFullScaleDeg;
    } else if (clamped < -kBarFullScaleDeg) {
        clamped = -kBarFullScaleDeg;
    }
    const int half = (track - 8) / 2;
    return static_cast<int>(clamped / kBarFullScaleDeg * static_cast<float>(half));
}

int holdPixel(const int next, const int held) {
    if (std::abs(next - held) < 2) {
        return held;
    }
    return next;
}

Bars barsFor(M5GFX& display) {
    const int width = display.width();
    const int footer_top = display.height() - config::kFooterHeight;
    Bars b{};
    b.pitch_w = 14;
    b.pitch_x = width - 8 - b.pitch_w;
    b.pitch_y = kContentTop + 4;
    b.pitch_h = footer_top - 4 - b.pitch_y;
    b.roll_num_y = kContentTop + 6;
    b.roll_num_x = 4 + 5 * 12;
    b.roll_x = 8;
    b.roll_y = b.roll_num_y + 18;
    b.roll_h = 14;
    b.roll_w = b.pitch_x - 10 - b.roll_x;
    b.label_y = b.roll_y + b.roll_h + 8;
    b.pitch_num_x = 4 + 6 * 12;
    b.pitch_num_y = b.label_y;
    return b;
}

bool readTilt(float* roll_deg, float* pitch_deg) {
#ifndef CARDPUTER_SIM
    if (!M5.Imu.isEnabled()) {
        return false;
    }
    M5.Imu.update();
    float ax = 0.0f;
    float ay = 0.0f;
    float az = 0.0f;
    if (!M5.Imu.getAccel(&ax, &ay, &az)) {
        return false;
    }
    *roll_deg = std::atan2(kRollSign * ax, az) * kRadToDeg;
    *pitch_deg = std::atan2(kPitchSign * ay, az) * kRadToDeg;
    return true;
#else
    (void)kRollSign;
    (void)kPitchSign;
    *roll_deg = 0.0f;
    *pitch_deg = 0.0f;
    return true;
#endif
}

void paintNumber(M5GFX& display, const int x, const int y, const int value) {
    display.fillRect(x, y, 4 * 12, 16, TFT_BLACK);
    display.setTextSize(2);
    display.setTextColor(TFT_WHITE, TFT_BLACK);
    display.setCursor(x, y);
    char line[8];
    std::snprintf(line, sizeof(line), "%+d", value);
    display.print(line);
}

void drawRollMarker(M5GFX& display, const Bars& b, const int offset, const bool near) {
    const int center = b.roll_x + b.roll_w / 2;
    const int marker = center + offset;
    display.fillRect(marker - 2, b.roll_y + 1, 5, b.roll_h - 2, near ? TFT_YELLOW : TFT_WHITE);
}

void eraseRollMarker(M5GFX& display, const Bars& b, const int offset) {
    const int center = b.roll_x + b.roll_w / 2;
    const int marker = center + offset;
    display.fillRect(marker - 2, b.roll_y + 1, 5, b.roll_h - 2, TFT_BLACK);
    if (std::abs(offset) <= 4) {
        display.drawFastVLine(center, b.roll_y, b.roll_h, TFT_DARKGREY);
    }
}

void drawPitchMarker(M5GFX& display, const Bars& b, const int offset, const bool near) {
    const int center = b.pitch_y + b.pitch_h / 2;
    const int marker = center - offset;
    display.fillRect(b.pitch_x + 1, marker - 2, b.pitch_w - 2, 5, near ? TFT_YELLOW : TFT_WHITE);
}

void erasePitchMarker(M5GFX& display, const Bars& b, const int offset) {
    const int center = b.pitch_y + b.pitch_h / 2;
    const int marker = center - offset;
    display.fillRect(b.pitch_x + 1, marker - 2, b.pitch_w - 2, 5, TFT_BLACK);
    if (std::abs(offset) <= 4) {
        display.drawFastHLine(b.pitch_x, center, b.pitch_w, TFT_DARKGREY);
    }
}

void clearFooter(M5GFX& display) {
    const int y = display.height() - config::kFooterHeight;
    display.fillRect(0, y, display.width(), config::kFooterHeight, TFT_BLACK);
}

}  // namespace

const char* LevelApp::name() const { return "Level"; }

void LevelApp::onEnter() {
    imu_ok_ = false;
    level_ = false;
    smooth_ready_ = false;
    chrome_valid_ = false;
    roll_deg_ = 0.0f;
    pitch_deg_ = 0.0f;
    roll_px_ = 0;
    pitch_px_ = 0;
    roll_shown_ = 0;
    pitch_shown_ = 0;
    header_focused_ = false;
    markDirty();
}

void LevelApp::onExit() {}

void LevelApp::onDisplayWake() {
    chrome_valid_ = false;
    markDirty();
}

bool LevelApp::sample() {
    float roll = 0.0f;
    float pitch = 0.0f;
    const bool ok = readTilt(&roll, &pitch);
    if (ok) {
        if (!smooth_ready_) {
            roll_deg_ = roll;
            pitch_deg_ = pitch;
            smooth_ready_ = true;
        } else {
            roll_deg_ += kSmooth * (roll - roll_deg_);
            pitch_deg_ += kSmooth * (pitch - pitch_deg_);
        }
    } else {
        smooth_ready_ = false;
        roll_deg_ = 0.0f;
        pitch_deg_ = 0.0f;
    }

    const bool level = ok && std::fabs(roll_deg_) < kLevelDeadbandDeg &&
                       std::fabs(pitch_deg_) < kLevelDeadbandDeg;
    const int roll_shown = ok ? shownDegrees(roll_deg_) : 0;
    const int pitch_shown = ok ? shownDegrees(pitch_deg_) : 0;
    const Bars b = barsFor(M5Cardputer.Display);
    int roll_px = ok ? markerOffset(roll_deg_, b.roll_w) : 0;
    int pitch_px = ok ? markerOffset(pitch_deg_, b.pitch_h) : 0;
    if (ok && smooth_ready_) {
        roll_px = holdPixel(roll_px, roll_px_);
        pitch_px = holdPixel(pitch_px, pitch_px_);
    }
    const bool changed = ok != imu_ok_ || level != level_ || roll_shown != roll_shown_ ||
                         pitch_shown != pitch_shown_ || roll_px != roll_px_ || pitch_px != pitch_px_;
    imu_ok_ = ok;
    level_ = level;
    roll_shown_ = roll_shown;
    pitch_shown_ = pitch_shown;
    roll_px_ = roll_px;
    pitch_px_ = pitch_px;
    return changed;
}

NavRequest LevelApp::onUpdate(const Keyboard_Class::KeysState* keys, const bool header_focused) {
    header_focused_ = header_focused;

    if (sample()) {
        markDirty();
    }

    NavRequest req = NavRequest::None;
    if (keys != nullptr && !header_focused_ && detectArrowKey(*keys) == ArrowKey::Up) {
        req = NavRequest::FocusHeader;
    }

    if (dirty_) {
        draw();
        dirty_ = false;
    }
    return req;
}

void LevelApp::draw() {
    auto& display = M5Cardputer.Display;
    const Bars b = barsFor(display);
    const bool roll_near = std::fabs(roll_deg_) < kLevelDeadbandDeg;
    const bool pitch_near = std::fabs(pitch_deg_) < kLevelDeadbandDeg;
    display.startWrite();

    if (!chrome_valid_ || imu_ok_ != painted_imu_) {
        display.fillRect(0, kContentTop, display.width(), display.height() - kContentTop, TFT_BLACK);
        display.setTextDatum(top_left);
        if (imu_ok_) {
            display.setTextSize(2);
            display.setTextColor(TFT_WHITE, TFT_BLACK);
            display.setCursor(4, b.roll_num_y);
            display.print("Roll");
            display.setCursor(4, b.label_y);
            display.print("Pitch");
            display.drawRect(b.roll_x, b.roll_y, b.roll_w, b.roll_h, TFT_DARKGREY);
            display.drawFastVLine(b.roll_x + b.roll_w / 2, b.roll_y, b.roll_h, TFT_DARKGREY);
            display.drawRect(b.pitch_x, b.pitch_y, b.pitch_w, b.pitch_h, TFT_DARKGREY);
            display.drawFastHLine(b.pitch_x, b.pitch_y + b.pitch_h / 2, b.pitch_w, TFT_DARKGREY);
            paintNumber(display, b.roll_num_x, b.roll_num_y, roll_shown_);
            paintNumber(display, b.pitch_num_x, b.pitch_num_y, pitch_shown_);
            drawRollMarker(display, b, roll_px_, roll_near);
            drawPitchMarker(display, b, pitch_px_, pitch_near);
            if (level_) {
                ui::drawFittedFooter("LEVEL");
            }
        } else {
            ui::drawFittedFooter("No IMU");
        }
        chrome_valid_ = true;
        painted_imu_ = imu_ok_;
        painted_level_ = level_;
        painted_roll_px_ = roll_px_;
        painted_pitch_px_ = pitch_px_;
        painted_roll_shown_ = roll_shown_;
        painted_pitch_shown_ = pitch_shown_;
        painted_roll_near_ = roll_near;
        painted_pitch_near_ = pitch_near;
        display.endWrite();
        return;
    }

    if (roll_shown_ != painted_roll_shown_) {
        paintNumber(display, b.roll_num_x, b.roll_num_y, roll_shown_);
        painted_roll_shown_ = roll_shown_;
    }
    if (pitch_shown_ != painted_pitch_shown_) {
        paintNumber(display, b.pitch_num_x, b.pitch_num_y, pitch_shown_);
        painted_pitch_shown_ = pitch_shown_;
    }
    if (roll_px_ != painted_roll_px_) {
        eraseRollMarker(display, b, painted_roll_px_);
        drawRollMarker(display, b, roll_px_, roll_near);
        painted_roll_px_ = roll_px_;
        painted_roll_near_ = roll_near;
    } else if (roll_near != painted_roll_near_) {
        drawRollMarker(display, b, roll_px_, roll_near);
        painted_roll_near_ = roll_near;
    }
    if (pitch_px_ != painted_pitch_px_) {
        erasePitchMarker(display, b, painted_pitch_px_);
        drawPitchMarker(display, b, pitch_px_, pitch_near);
        painted_pitch_px_ = pitch_px_;
        painted_pitch_near_ = pitch_near;
    } else if (pitch_near != painted_pitch_near_) {
        drawPitchMarker(display, b, pitch_px_, pitch_near);
        painted_pitch_near_ = pitch_near;
    }
    if (level_ != painted_level_) {
        if (level_) {
            ui::drawFittedFooter("LEVEL");
        } else {
            clearFooter(display);
        }
        painted_level_ = level_;
    }
    display.endWrite();
}
