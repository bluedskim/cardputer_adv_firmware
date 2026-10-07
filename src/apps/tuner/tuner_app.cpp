#include "apps/tuner/tuner_app.h"

#include <M5Cardputer.h>

#include <cmath>
#include <cstdio>
#include <cstring>

#include "apps/tuner/pitch_detect.h"
#include "config.h"
#include "input/keyboard_input.h"
#include "ui/fitted_text.h"

namespace {

constexpr int kContentTop = config::kContentTop;
constexpr float kCentsFullScale = 50.0f;
constexpr float kInTuneCents = 5.0f;
constexpr float kSmooth = 0.25f;
// ES8311 reg 0x14: Mic1 differential (0x10) + PGA 18 dB (6). Lower than Recorder's 24 dB.
constexpr uint8_t kEs8311MicPga = 0x16;

struct Layout {
    int note_x;
    int note_y;
    int bar_x;
    int bar_y;
    int bar_w;
    int bar_h;
    int cents_x;
    int cents_y;
};

Layout layoutFor(M5GFX& display) {
    const int width = display.width();
    const int footer_top = display.height() - config::kFooterHeight;
    Layout L{};
    L.note_y = kContentTop + 4;
    L.note_x = width / 2;
    L.cents_y = L.note_y + 36;
    L.cents_x = width / 2;
    L.bar_h = 16;
    L.bar_y = footer_top - 8 - L.bar_h;
    L.bar_x = 12;
    L.bar_w = width - 24;
    return L;
}

int needleOffset(const float cents, const int track_w) {
    float clamped = cents;
    if (clamped > kCentsFullScale) {
        clamped = kCentsFullScale;
    } else if (clamped < -kCentsFullScale) {
        clamped = -kCentsFullScale;
    }
    const int half = (track_w - 8) / 2;
    return static_cast<int>(clamped / kCentsFullScale * static_cast<float>(half));
}

int holdPixel(const int next, const int held) {
    if (std::abs(next - held) < 2) {
        return held;
    }
    return next;
}

void drawNeedle(M5GFX& display, const Layout& L, const int offset, const bool in_tune) {
    const int center = L.bar_x + L.bar_w / 2;
    const int marker = center + offset;
    display.fillRect(marker - 2, L.bar_y + 1, 5, L.bar_h - 2, in_tune ? TFT_GREEN : TFT_YELLOW);
}

void eraseNeedle(M5GFX& display, const Layout& L, const int offset) {
    const int center = L.bar_x + L.bar_w / 2;
    const int marker = center + offset;
    display.fillRect(marker - 2, L.bar_y + 1, 5, L.bar_h - 2, TFT_BLACK);
    if (std::abs(offset) <= 4) {
        display.drawFastVLine(center, L.bar_y, L.bar_h, TFT_DARKGREY);
    }
}

}  // namespace

const char* TunerApp::name() const { return "Tuner"; }

void TunerApp::onEnter() {
    listening_ = false;
    mic_ok_ = false;
    have_pitch_ = false;
    in_tune_ = false;
    chrome_valid_ = false;
    smooth_ready_ = false;
    std::strncpy(note_name_, "--", sizeof(note_name_));
    cents_ = 0.0f;
    cents_smooth_ = 0.0f;
    cents_shown_ = 0;
    needle_px_ = 0;
    mic_queued_ = 0;
    window_fill_ = 0;
    window_pos_ = 0;
    chunks_since_analyze_ = 0;
    header_focused_ = false;
    painted_note_[0] = '\0';
    mic_ok_ = beginListen();
    markDirty();
}

void TunerApp::onExit() { stopListen(); }

void TunerApp::onDisplayWake() {
    chrome_valid_ = false;
    markDirty();
}

void TunerApp::onBackgroundTick() {
    if (listening_) {
        tickMic();
    }
}

bool TunerApp::hasActiveBackgroundWork() const { return listening_; }

bool TunerApp::beginListen() {
#ifndef CARDPUTER_SIM
    if (M5Cardputer.Mic.isRunning()) {
        M5Cardputer.Mic.end();
    }
    if (M5Cardputer.Speaker.isRunning()) {
        M5Cardputer.Speaker.stop();
        M5Cardputer.Speaker.end();
    }
    M5Cardputer.Speaker.end();

    auto mic_cfg = M5Cardputer.Mic.config();
    mic_cfg.sample_rate = config::kRecorderSampleRate;
    mic_cfg.over_sampling = 1;
    mic_cfg.magnification = 2;
    mic_cfg.noise_filter_level = 0;
    M5Cardputer.Mic.config(mic_cfg);
    if (!M5Cardputer.Mic.begin()) {
        listening_ = false;
        return false;
    }
    for (int attempt = 0; attempt < 4; ++attempt) {
        if (M5Cardputer.In_I2C.writeRegister8(0x18, 0x14, kEs8311MicPga, 100000)) {
            break;
        }
        delay(1);
    }

    mic_queued_ = 0;
    window_fill_ = 0;
    window_pos_ = 0;
    listening_ = true;

    auto queue = [this]() -> bool {
        const std::size_t slot = mic_queued_ % kBufCount;
        if (!M5Cardputer.Mic.record(mic_buf_[slot], kChunkSamples, config::kRecorderSampleRate)) {
            return false;
        }
        ++mic_queued_;
        return true;
    };
    if (!queue() || !queue()) {
        stopListen();
        return false;
    }
    return true;
#else
    listening_ = false;
    return false;
#endif
}

void TunerApp::stopListen() {
#ifndef CARDPUTER_SIM
    if (!listening_ && !M5Cardputer.Mic.isRunning()) {
        return;
    }
    listening_ = false;
    while (M5Cardputer.Mic.isRecording()) {
        delay(1);
    }
    if (M5Cardputer.Mic.isRunning()) {
        M5Cardputer.Mic.end();
    }
#else
    listening_ = false;
#endif
}

void TunerApp::tickMic() {
#ifndef CARDPUTER_SIM
    if (!listening_) {
        return;
    }

    const std::size_t slot = mic_queued_ % kBufCount;
    if (!M5Cardputer.Mic.record(mic_buf_[slot], kChunkSamples, config::kRecorderSampleRate)) {
        stopListen();
        mic_ok_ = false;
        have_pitch_ = false;
        markDirty();
        return;
    }
    ++mic_queued_;
    if (mic_queued_ < 3) {
        return;
    }
    pushChunk(mic_buf_[(mic_queued_ - 3) % kBufCount]);
    // ~32 ms between pitch estimates keeps autocorr off the critical path.
    if (++chunks_since_analyze_ >= 2) {
        chunks_since_analyze_ = 0;
        analyze();
    }
#else
    (void)0;
#endif
}

void TunerApp::pushChunk(const int16_t* samples) {
    for (std::size_t i = 0; i < kChunkSamples; ++i) {
        window_[window_pos_] = samples[i];
        window_pos_ = (window_pos_ + 1) % kWindowSamples;
        if (window_fill_ < kWindowSamples) {
            ++window_fill_;
        }
    }
}

void TunerApp::analyze() {
    if (window_fill_ < kWindowSamples) {
        return;
    }

    for (std::size_t i = 0; i < kWindowSamples; ++i) {
        ordered_[i] = window_[(window_pos_ + i) % kWindowSamples];
    }

    float hz = 0.0f;
    float clarity = 0.0f;
    const bool detected =
        pitch::detectHz(ordered_, kWindowSamples, config::kRecorderSampleRate, &hz, &clarity);
    const pitch::GuitarHit hit = detected ? pitch::mapGuitar(hz) : pitch::GuitarHit{};

    bool changed = false;
    if (hit.ok) {
        if (!smooth_ready_) {
            cents_smooth_ = hit.cents;
            smooth_ready_ = true;
        } else {
            cents_smooth_ += kSmooth * (hit.cents - cents_smooth_);
        }
        cents_ = hit.cents;
        if (!have_pitch_ || std::strcmp(note_name_, hit.name) != 0) {
            std::strncpy(note_name_, hit.name, sizeof(note_name_) - 1);
            note_name_[sizeof(note_name_) - 1] = '\0';
            changed = true;
        }
        have_pitch_ = true;
    } else {
        if (have_pitch_) {
            changed = true;
        }
        have_pitch_ = false;
        smooth_ready_ = false;
        cents_ = 0.0f;
        cents_smooth_ = 0.0f;
        std::strncpy(note_name_, "--", sizeof(note_name_));
    }

    const bool in_tune = have_pitch_ && std::fabs(cents_smooth_) <= kInTuneCents;
    const int cents_shown = have_pitch_ ? static_cast<int>(std::lround(cents_smooth_)) : 0;
    const Layout L = layoutFor(M5Cardputer.Display);
    int needle = have_pitch_ ? needleOffset(cents_smooth_, L.bar_w) : 0;
    if (have_pitch_ && smooth_ready_) {
        needle = holdPixel(needle, needle_px_);
    }

    if (in_tune != in_tune_ || cents_shown != cents_shown_ || needle != needle_px_) {
        changed = true;
    }
    in_tune_ = in_tune;
    cents_shown_ = cents_shown;
    needle_px_ = needle;
    if (changed) {
        markDirty();
    }
    (void)clarity;
}

NavRequest TunerApp::onUpdate(const Keyboard_Class::KeysState* keys, const bool header_focused) {
    header_focused_ = header_focused;

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

void TunerApp::paintFooter() {
    if (!mic_ok_) {
        ui::drawFittedFooter("No mic");
        return;
    }
    if (in_tune_) {
        ui::drawFittedFooter("IN TUNE");
        return;
    }
    if (have_pitch_) {
        char foot[16];
        std::snprintf(foot, sizeof(foot), "%s  %+dc", note_name_, cents_shown_);
        ui::drawFittedFooter(foot);
        return;
    }
    ui::drawFittedFooter("--");
}

void TunerApp::draw() {
    auto& display = M5Cardputer.Display;
    const Layout L = layoutFor(display);
    display.startWrite();

    const bool need_full = !chrome_valid_ || mic_ok_ != painted_mic_ok_ ||
                           have_pitch_ != painted_have_pitch_ ||
                           in_tune_ != painted_in_tune_ ||
                           std::strcmp(note_name_, painted_note_) != 0;

    if (need_full) {
        display.fillRect(0, kContentTop, display.width(), display.height() - kContentTop, TFT_BLACK);
        display.setTextDatum(top_center);
        display.setTextSize(4);
        display.setTextColor(have_pitch_ ? (in_tune_ ? TFT_GREEN : TFT_WHITE) : TFT_DARKGREY,
                             TFT_BLACK);
        display.drawString(note_name_, L.note_x, L.note_y);

        if (mic_ok_) {
            display.setTextSize(2);
            display.setTextColor(TFT_WHITE, TFT_BLACK);
            if (have_pitch_) {
                char cents_line[12];
                std::snprintf(cents_line, sizeof(cents_line), "%+dc", cents_shown_);
                display.drawString(cents_line, L.cents_x, L.cents_y);
            } else {
                display.drawString("--", L.cents_x, L.cents_y);
            }

            display.drawRect(L.bar_x, L.bar_y, L.bar_w, L.bar_h, TFT_DARKGREY);
            display.drawFastVLine(L.bar_x + L.bar_w / 2, L.bar_y, L.bar_h, TFT_DARKGREY);
            if (have_pitch_) {
                drawNeedle(display, L, needle_px_, in_tune_);
            }
        }
        paintFooter();

        chrome_valid_ = true;
        painted_mic_ok_ = mic_ok_;
        painted_have_pitch_ = have_pitch_;
        painted_in_tune_ = in_tune_;
        painted_needle_px_ = needle_px_;
        painted_cents_shown_ = cents_shown_;
        std::strncpy(painted_note_, note_name_, sizeof(painted_note_) - 1);
        painted_note_[sizeof(painted_note_) - 1] = '\0';
        display.setTextDatum(top_left);
        display.endWrite();
        return;
    }

    display.setTextDatum(top_center);
    const bool footer_dirty =
        in_tune_ != painted_in_tune_ || cents_shown_ != painted_cents_shown_;

    if (cents_shown_ != painted_cents_shown_) {
        display.fillRect(L.cents_x - 40, L.cents_y, 80, 18, TFT_BLACK);
        display.setTextSize(2);
        display.setTextColor(TFT_WHITE, TFT_BLACK);
        char cents_line[12];
        std::snprintf(cents_line, sizeof(cents_line), "%+dc", cents_shown_);
        display.drawString(cents_line, L.cents_x, L.cents_y);
    }

    if (needle_px_ != painted_needle_px_) {
        eraseNeedle(display, L, painted_needle_px_);
        drawNeedle(display, L, needle_px_, in_tune_);
        painted_needle_px_ = needle_px_;
    } else if (in_tune_ != painted_in_tune_) {
        drawNeedle(display, L, needle_px_, in_tune_);
    }

    if (footer_dirty) {
        paintFooter();
    }
    painted_in_tune_ = in_tune_;
    painted_cents_shown_ = cents_shown_;

    display.setTextDatum(top_left);
    display.endWrite();
}
