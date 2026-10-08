#include "apps/metro/metro_app.h"

#include <M5Cardputer.h>
#include <Preferences.h>

#include <cstdio>

#include "config.h"
#include "input/keyboard_input.h"
#include "ui/fitted_text.h"

namespace {

constexpr int kContentTop = config::kContentTop;
constexpr uint16_t kBpmMin = 30;
constexpr uint16_t kBpmMax = 250;
constexpr uint8_t kBeatsMax = 7;
constexpr uint8_t kVolMax = 10;
constexpr uint32_t kLedFlashMs = 90;
constexpr uint16_t kAccentHz = 1320;
constexpr uint16_t kClickHz = 880;
constexpr uint32_t kClickMs = 35;
constexpr int kHeroTop = kContentTop + 2;
constexpr int kHeroH = 48;
constexpr int kLedPadX = 4;
constexpr int kBpmGap = 6;

uint8_t volumeByte(const uint8_t vol) {
    if (vol == 0) {
        return 0;
    }
    return static_cast<uint8_t>(24 + vol * 22);
}

int ledRadius(const uint8_t count) {
    if (count <= 3) {
        return 8;
    }
    if (count <= 4) {
        return 7;
    }
    if (count <= 5) {
        return 6;
    }
    return 5;
}

int ledGap(const uint8_t count) { return count >= 6 ? 4 : 6; }

int ledRowWidth(const uint8_t count) {
    const int r = ledRadius(count);
    const int gap = ledGap(count);
    return static_cast<int>(count) * (2 * r) + (static_cast<int>(count) - 1) * gap;
}

int ledPaneWidth(const uint8_t count) { return 2 * kLedPadX + ledRowWidth(count); }

int ledCenterX(const uint8_t count, const uint8_t index) {
    const int r = ledRadius(count);
    const int gap = ledGap(count);
    return kLedPadX + r + static_cast<int>(index) * (2 * r + gap);
}

int ledCenterY() { return kHeroTop + kHeroH / 2; }

}  // namespace

MetroApp::MetroApp() { load(); }

const char* MetroApp::name() const { return "Metro"; }

uint8_t MetroApp::ledCount() const {
    // One circle per beat; Beat 0 (no meter) keeps a single pulse LED.
    return beats_ == 0 ? 1 : beats_;
}

void MetroApp::load() {
    Preferences prefs;
    // RO begin fails when the namespace has never been created; fall back to RW.
    if (!prefs.begin(config::kMetroStorageNamespace, true) &&
        !prefs.begin(config::kMetroStorageNamespace, false)) {
        return;
    }
    bpm_ = static_cast<uint16_t>(prefs.getUInt("bpm", 120));
    beats_ = prefs.getUChar("beats", 4);
    vol_ = prefs.getUChar("vol", 6);
    prefs.end();

    if (bpm_ < kBpmMin) {
        bpm_ = kBpmMin;
    }
    if (bpm_ > kBpmMax) {
        bpm_ = kBpmMax;
    }
    if (beats_ > kBeatsMax) {
        beats_ = 4;
    }
    if (vol_ > kVolMax) {
        vol_ = 6;
    }
}

void MetroApp::save() const {
    Preferences prefs;
    if (!prefs.begin(config::kMetroStorageNamespace, false)) {
        return;
    }
    prefs.putUInt("bpm", bpm_);
    prefs.putUChar("beats", beats_);
    prefs.putUChar("vol", vol_);
    prefs.end();
}

void MetroApp::onEnter() {
    load();
    focus_ = Focus::Tempo;
    running_ = false;
    beat_index_ = 0;
    led_index_ = -1;
    led_accent_ = false;
    led_until_ms_ = 0;
    painted_led_index_ = -2;
    painted_led_accent_ = false;
    painted_led_count_ = 0;
    speaker_ok_ = false;
    chrome_valid_ = false;
    leds_dirty_ = false;
    markDirty();
    requestHeaderFocus();
}

void MetroApp::onExit() {
    running_ = false;
    releaseSpeaker();
    save();
    M5Cardputer.Display.fillScreen(TFT_BLACK);
}

void MetroApp::onDisplayWake() {
    chrome_valid_ = false;
    painted_led_index_ = -2;
    markDirty();
}

void MetroApp::onBackgroundTick() { tickBeats(); }

bool MetroApp::hasActiveBackgroundWork() const { return running_; }

void MetroApp::ensureSpeaker() {
#ifndef CARDPUTER_SIM
    if (speaker_ok_) {
        return;
    }
    if (M5Cardputer.Mic.isRunning()) {
        M5Cardputer.Mic.end();
    }
    if (M5Cardputer.Speaker.isRunning()) {
        M5Cardputer.Speaker.stop();
        M5Cardputer.Speaker.end();
    }
    auto spk_cfg = M5Cardputer.Speaker.config();
    spk_cfg.sample_rate = 16000;
    M5Cardputer.Speaker.config(spk_cfg);
    if (!M5Cardputer.Speaker.begin()) {
        speaker_ok_ = false;
        return;
    }
    M5Cardputer.Speaker.setVolume(volumeByte(vol_));
    speaker_ok_ = true;
#else
    speaker_ok_ = true;
#endif
}

void MetroApp::releaseSpeaker() {
#ifndef CARDPUTER_SIM
    if (M5Cardputer.Speaker.isRunning()) {
        M5Cardputer.Speaker.stop();
        M5Cardputer.Speaker.end();
    }
#endif
    speaker_ok_ = false;
}

void MetroApp::toggleRun() {
    if (running_) {
        running_ = false;
        led_index_ = -1;
        led_accent_ = false;
        beat_index_ = 0;
#ifndef CARDPUTER_SIM
        if (speaker_ok_) {
            M5Cardputer.Speaker.stop();
        }
#endif
    } else {
        ensureSpeaker();
        running_ = true;
        beat_index_ = 0;
        next_beat_ms_ = millis();
        fireBeat();
    }
    // Running toggles BPM color + footer; LEDs also change.
    markDirty();
    markLedsDirty();
}

void MetroApp::fireBeat() {
    const uint8_t count = ledCount();
    const bool accent = beats_ > 0 && beat_index_ == 0;
    led_index_ = static_cast<int8_t>(beat_index_ % count);
    led_accent_ = accent;
    led_until_ms_ = millis() + kLedFlashMs;

#ifndef CARDPUTER_SIM
    if (speaker_ok_ && vol_ > 0) {
        M5Cardputer.Speaker.setVolume(volumeByte(vol_));
        M5Cardputer.Speaker.tone(accent ? kAccentHz : kClickHz, kClickMs, 0, true);
    }
#else
    (void)accent;
#endif

    if (beats_ == 0) {
        beat_index_ = 0;
    } else {
        beat_index_ = static_cast<uint8_t>((beat_index_ + 1) % beats_);
    }
    markLedsDirty();
}

void MetroApp::tickBeats() {
    const uint32_t now = millis();

    if (!running_) {
        if (led_index_ >= 0 && static_cast<int32_t>(now - led_until_ms_) >= 0) {
            led_index_ = -1;
            led_accent_ = false;
            markLedsDirty();
        }
        return;
    }

    const uint32_t interval = 60000u / static_cast<uint32_t>(bpm_);
    while (static_cast<int32_t>(now - next_beat_ms_) >= 0) {
        fireBeat();
        next_beat_ms_ += interval;
        if (static_cast<int32_t>(now - next_beat_ms_) > static_cast<int32_t>(interval * 2)) {
            next_beat_ms_ = now + interval;
            break;
        }
    }

    if (led_index_ >= 0 && static_cast<int32_t>(now - led_until_ms_) >= 0) {
        led_index_ = -1;
        led_accent_ = false;
        markLedsDirty();
    }
}

void MetroApp::adjustFocused(const int delta) {
    switch (focus_) {
        case Focus::Tempo: {
            int v = static_cast<int>(bpm_) + delta;
            if (v < static_cast<int>(kBpmMin)) {
                v = kBpmMin;
            }
            if (v > static_cast<int>(kBpmMax)) {
                v = kBpmMax;
            }
            if (static_cast<uint16_t>(v) != bpm_) {
                bpm_ = static_cast<uint16_t>(v);
                if (running_) {
                    next_beat_ms_ = millis() + (60000u / static_cast<uint32_t>(bpm_));
                }
                save();
                markDirty();
            }
            break;
        }
        case Focus::Beat: {
            int v = static_cast<int>(beats_) + delta;
            if (v < 0) {
                v = 0;
            }
            if (v > static_cast<int>(kBeatsMax)) {
                v = kBeatsMax;
            }
            if (static_cast<uint8_t>(v) != beats_) {
                beats_ = static_cast<uint8_t>(v);
                beat_index_ = 0;
                save();
                markDirty();
            }
            break;
        }
        case Focus::Vol: {
            int v = static_cast<int>(vol_) + delta;
            if (v < 0) {
                v = 0;
            }
            if (v > static_cast<int>(kVolMax)) {
                v = kVolMax;
            }
            if (static_cast<uint8_t>(v) != vol_) {
                vol_ = static_cast<uint8_t>(v);
#ifndef CARDPUTER_SIM
                if (speaker_ok_) {
                    M5Cardputer.Speaker.setVolume(volumeByte(vol_));
                }
#endif
                save();
                markDirty();
            }
            break;
        }
        default:
            break;
    }
}

void MetroApp::paintLeds() {
    auto& display = M5Cardputer.Display;
    const uint8_t count = ledCount();
    const int r = ledRadius(count);
    const int pane_w = ledPaneWidth(count);
    const int cy = ledCenterY();
    const bool count_changed = painted_led_count_ != count || painted_led_index_ == -2;

    display.startWrite();
    if (count_changed) {
        display.fillRect(0, kHeroTop, pane_w, kHeroH, TFT_BLACK);
    }

    for (uint8_t i = 0; i < count; ++i) {
        const int x = ledCenterX(count, i);
        const bool on = running_ && led_index_ == static_cast<int8_t>(i);
        const bool was_on = !count_changed && painted_led_index_ == static_cast<int8_t>(i);
        const bool accent = on && led_accent_;
        const bool was_accent = was_on && painted_led_accent_;
        if (!count_changed && on == was_on && accent == was_accent) {
            continue;
        }
        const uint16_t color = on ? (accent ? TFT_RED : TFT_YELLOW) : TFT_DARKGREY;
        display.fillCircle(x, cy, r, on ? color : TFT_BLACK);
        display.drawCircle(x, cy, r, color);
    }
    display.endWrite();

    painted_led_index_ = led_index_;
    painted_led_accent_ = led_accent_;
    painted_led_count_ = count;
    leds_dirty_ = false;
}

void MetroApp::paintFooter() const {
    if (running_) {
        ui::drawFittedFooter("Ent stop");
    } else {
        ui::drawFittedFooter("Ent start");
    }
}

void MetroApp::drawChrome() {
    auto& display = M5Cardputer.Display;
    display.startWrite();
    display.fillRect(0, kContentTop, display.width(), display.height() - kContentTop, TFT_BLACK);

    // Force all LEDs to paint with chrome.
    painted_led_index_ = -2;
    display.endWrite();
    paintLeds();

    display.startWrite();
    char bpm_line[8];
    std::snprintf(bpm_line, sizeof(bpm_line), "%u", static_cast<unsigned>(bpm_));

    const uint8_t count = ledCount();
    const int bpm_left = ledPaneWidth(count) + 4;
    const int bpm_right = display.width() - 4;
    const int avail_w = bpm_right - bpm_left;

    display.setTextDatum(top_left);
    display.setTextSize(2);
    const int label_w = display.textWidth("BPM");

    // Largest integer size that fits "[digits] BPM" in the right pane.
    int bpm_size = 6;
    int num_w = 0;
    for (; bpm_size >= 2; --bpm_size) {
        display.setTextSize(bpm_size);
        num_w = display.textWidth(bpm_line);
        if (num_w + kBpmGap + label_w <= avail_w) {
            break;
        }
    }

    const int total_w = num_w + kBpmGap + label_w;
    const int x0 = bpm_left + (avail_w - total_w) / 2;
    const int digit_h = 8 * bpm_size;
    const int y0 = kHeroTop + (kHeroH - digit_h) / 2;

    display.setTextSize(bpm_size);
    display.setTextColor(running_ ? TFT_WHITE : TFT_DARKGREY, TFT_BLACK);
    display.setCursor(x0, y0);
    display.print(bpm_line);

    display.setTextSize(2);
    display.setTextColor(TFT_DARKGREY, TFT_BLACK);
    const int label_y = y0 + (digit_h - 16) / 2;
    display.setCursor(x0 + num_w + kBpmGap, label_y);
    display.print("BPM");

    const int row_y = kHeroTop + kHeroH + 4;
    const int col_w = display.width() / 3;
    auto paint_row = [&](const Focus f, const int col, const char* label, const char* value) {
        const int cx = col * col_w + col_w / 2;
        const bool sel = !header_focused_ && focus_ == f;
        display.setTextDatum(top_center);
        display.setTextColor(sel ? TFT_YELLOW : TFT_WHITE, TFT_BLACK);
        display.drawString(label, cx, row_y);
        display.setTextColor(sel ? TFT_YELLOW : TFT_DARKGREY, TFT_BLACK);
        display.drawString(value, cx, row_y + 16);
    };

    char tempo_val[8];
    std::snprintf(tempo_val, sizeof(tempo_val), "%u", static_cast<unsigned>(bpm_));
    char beat_val[8];
    std::snprintf(beat_val, sizeof(beat_val), "%u", static_cast<unsigned>(beats_));
    char vol_val[8];
    std::snprintf(vol_val, sizeof(vol_val), "%u", static_cast<unsigned>(vol_));

    paint_row(Focus::Tempo, 0, "Tempo", tempo_val);
    paint_row(Focus::Beat, 1, "Beat", beat_val);
    paint_row(Focus::Vol, 2, "Vol", vol_val);

    paintFooter();
    display.setTextDatum(top_left);
    display.endWrite();

    chrome_valid_ = true;
    dirty_ = false;
}

void MetroApp::draw() {
    if (!chrome_valid_ || dirty_) {
        drawChrome();
        return;
    }
    if (leds_dirty_) {
        paintLeds();
    }
}

NavRequest MetroApp::onUpdate(const Keyboard_Class::KeysState* keys, const bool header_focused) {
    if (header_focused_ != header_focused) {
        header_focused_ = header_focused;
        markDirty();
    }

    if (pending_header_focus_) {
        pending_header_focus_ = false;
        return NavRequest::FocusHeader;
    }

    tickBeats();

    NavRequest req = NavRequest::None;
    if (keys != nullptr && !header_focused_) {
        const ArrowKey arrow = detectArrowKey(*keys);
        if (arrow == ArrowKey::Up) {
            if (focus_ == Focus::Tempo) {
                req = NavRequest::FocusHeader;
            } else {
                focus_ = static_cast<Focus>(static_cast<uint8_t>(focus_) - 1);
                markDirty();
            }
        } else if (arrow == ArrowKey::Down) {
            if (static_cast<uint8_t>(focus_) + 1 < static_cast<uint8_t>(Focus::Count)) {
                focus_ = static_cast<Focus>(static_cast<uint8_t>(focus_) + 1);
                markDirty();
            }
        } else if (arrow == ArrowKey::Left) {
            adjustFocused(-1);
        } else if (arrow == ArrowKey::Right) {
            adjustFocused(1);
        }

        if (keys->enter) {
            toggleRun();
        }
    }

    draw();
    return req;
}
