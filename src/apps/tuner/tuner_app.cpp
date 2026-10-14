#include "apps/tuner/tuner_app.h"

#include <M5Cardputer.h>
#include <Preferences.h>

#include <cstdio>

#include "config.h"
#include "input/keyboard_input.h"
#include "ui/fitted_text.h"

namespace {

constexpr int kContentTop = config::kContentTop;
// Below full-scale (255) — 255 clips/tears on the Cardputer speaker path.
constexpr uint8_t kSpeakerVolume = 200;

struct StringDef {
    const char* name;
    float hz;
};

// Standard EADGBE, A4 = 440 Hz.
constexpr StringDef kStrings[] = {
    {"E", 82.4069f}, {"A", 110.0000f}, {"D", 146.8324f},
    {"G", 195.9977f}, {"B", 246.9417f}, {"e", 329.6276f},
};
static_assert(sizeof(kStrings) / sizeof(kStrings[0]) == 6, "six open strings");

}  // namespace

TunerApp::TunerApp() { load(); }

const char* TunerApp::name() const { return "Tuner"; }

void TunerApp::load() {
    Preferences prefs;
    if (!prefs.begin(config::kTunerStorageNamespace, true) &&
        !prefs.begin(config::kTunerStorageNamespace, false)) {
        return;
    }
    string_index_ = prefs.getUChar("str", 0);
    prefs.end();

    if (string_index_ >= kStringCount) {
        string_index_ = 0;
    }
}

void TunerApp::save() const {
    Preferences prefs;
    if (!prefs.begin(config::kTunerStorageNamespace, false)) {
        return;
    }
    prefs.putUChar("str", string_index_);
    prefs.end();
}

void TunerApp::onEnter() {
    load();
    playing_ = false;
    speaker_ok_ = false;
    markDirty();
    requestHeaderFocus();
}

void TunerApp::onExit() {
    stopTone();
    releaseSpeaker();
    save();
    M5Cardputer.Display.fillScreen(TFT_BLACK);
}

void TunerApp::onDisplayWake() { markDirty(); }

bool TunerApp::hasActiveBackgroundWork() const { return playing_; }

void TunerApp::ensureSpeaker() {
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
    M5Cardputer.Speaker.setVolume(kSpeakerVolume);
    speaker_ok_ = true;
#else
    speaker_ok_ = true;
#endif
}

void TunerApp::releaseSpeaker() {
#ifndef CARDPUTER_SIM
    if (M5Cardputer.Speaker.isRunning()) {
        M5Cardputer.Speaker.stop();
        M5Cardputer.Speaker.end();
    }
#endif
    speaker_ok_ = false;
}

void TunerApp::startTone() {
    ensureSpeaker();
    if (!speaker_ok_) {
        playing_ = false;
        return;
    }
#ifndef CARDPUTER_SIM
    M5Cardputer.Speaker.setVolume(kSpeakerVolume);
    // UINT32_MAX = sustain until stop(); synth sine via M5Unified.
    M5Cardputer.Speaker.tone(kStrings[string_index_].hz, UINT32_MAX, 0, true);
#endif
    playing_ = true;
}

void TunerApp::stopTone() {
#ifndef CARDPUTER_SIM
    if (speaker_ok_) {
        M5Cardputer.Speaker.stop();
    }
#endif
    playing_ = false;
}

void TunerApp::togglePlay() {
    if (playing_) {
        stopTone();
    } else {
        startTone();
    }
    markDirty();
}

void TunerApp::selectString(const int delta) {
    int v = static_cast<int>(string_index_) + delta;
    if (v < 0) {
        v = static_cast<int>(kStringCount) - 1;
    }
    if (v >= static_cast<int>(kStringCount)) {
        v = 0;
    }
    if (static_cast<uint8_t>(v) == string_index_) {
        return;
    }
    string_index_ = static_cast<uint8_t>(v);
    save();
    if (playing_) {
        startTone();
    }
    markDirty();
}

void TunerApp::paintFooter() const {
    if (playing_) {
        ui::drawFittedFooter("Ent stop  <- -> string");
    } else {
        ui::drawFittedFooter("Ent play  <- -> string");
    }
}

void TunerApp::draw() {
    auto& display = M5Cardputer.Display;
    display.startWrite();
    display.fillRect(0, kContentTop, display.width(), display.height() - kContentTop, TFT_BLACK);

    const auto& s = kStrings[string_index_];

    display.setTextDatum(top_center);
    display.setTextSize(5);
    display.setTextColor(playing_ ? TFT_GREEN : TFT_WHITE, TFT_BLACK);
    display.drawString(s.name, display.width() / 2, kContentTop + 6);

    char hz_line[16];
    std::snprintf(hz_line, sizeof(hz_line), "%.1f Hz", static_cast<double>(s.hz));
    display.setTextSize(2);
    display.setTextColor(TFT_DARKGREY, TFT_BLACK);
    display.drawString(hz_line, display.width() / 2, kContentTop + 52);

    // Six string slots; highlight the selected one.
    const int slot_y = kContentTop + 76;
    const int slot_w = display.width() / static_cast<int>(kStringCount);
    for (uint8_t i = 0; i < kStringCount; ++i) {
        const int cx = i * slot_w + slot_w / 2;
        const bool sel = i == string_index_;
        display.setTextColor(sel ? (playing_ ? TFT_GREEN : TFT_YELLOW) : TFT_DARKGREY, TFT_BLACK);
        display.drawString(kStrings[i].name, cx, slot_y);
    }

    paintFooter();
    display.setTextDatum(top_left);
    display.endWrite();
}

NavRequest TunerApp::onUpdate(const Keyboard_Class::KeysState* keys, const bool header_focused) {
    if (header_focused_ != header_focused) {
        header_focused_ = header_focused;
        markDirty();
    }

    if (pending_header_focus_) {
        pending_header_focus_ = false;
        return NavRequest::FocusHeader;
    }

    NavRequest req = NavRequest::None;
    if (keys != nullptr && !header_focused_) {
        const ArrowKey arrow = detectArrowKey(*keys);
        if (arrow == ArrowKey::Up) {
            req = NavRequest::FocusHeader;
        } else if (arrow == ArrowKey::Left) {
            selectString(-1);
        } else if (arrow == ArrowKey::Right) {
            selectString(1);
        }
        if (keys->enter) {
            togglePlay();
        }
    }

    if (dirty_) {
        draw();
        dirty_ = false;
    }
    return req;
}
