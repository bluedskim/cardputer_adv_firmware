#include "apps/seismo/seismo_app.h"

#include <M5Cardputer.h>

#include <cmath>
#include <cstdio>
#include <cstring>

#include "config.h"
#include "input/keyboard_input.h"
#include "ui/fitted_text.h"

namespace {

constexpr int kContentTop = config::kContentTop;
constexpr uint32_t kSampleMs = 50;
constexpr float kBaseAlpha = 0.033f;
constexpr int kScalesMg[] = {20, 50, 100, 200};
constexpr uint16_t kCursorColor = TFT_YELLOW;
constexpr int kGapWidth = 3;

struct Plot {
    int y;
    int w;
    int h;
    int center_y;
};

Plot plotFor(const M5GFX& display) {
    Plot plot{};
    plot.y = kContentTop;
    plot.w = display.width();
    plot.h = display.height() - config::kFooterHeight - kContentTop;
    plot.center_y = plot.y + plot.h / 2;
    return plot;
}

void clearColumn(M5GFX& display, const Plot& plot, const int x) {
    display.drawFastVLine(x, plot.y, plot.h, TFT_BLACK);
}

void restoreCenter(M5GFX& display, const Plot& plot, const int x) {
    display.drawPixel(x, plot.center_y, TFT_DARKGREY);
}

void drawTip(M5GFX& display, const int x, const int y) {
    display.fillRect(x - 2, y - 2, 5, 5, kCursorColor);
}

void drawCursorMark(M5GFX& display, const Plot& plot, const int x, const int y) {
    display.fillRect(x - 1, plot.y, 3, plot.h, kCursorColor);
    drawTip(display, x, y);
}

bool columnInGap(const int x, const int cursor, const int count) {
    for (int i = 1; i <= kGapWidth; ++i) {
        if (x == (cursor + i) % count) {
            return true;
        }
    }
    return false;
}

void clearGap(M5GFX& display, const Plot& plot, const int cursor, const int count) {
    for (int i = 1; i <= kGapWidth; ++i) {
        const int x = (cursor + i) % count;
        clearColumn(display, plot, x);
        restoreCenter(display, plot, x);
    }
}

}  // namespace

const char* SeismoApp::name() const { return "Seismo"; }

int SeismoApp::columns() const {
    const int width = M5Cardputer.Display.width();
    if (width < 2) {
        return 2;
    }
    if (width > kTraceColumns) {
        return kTraceColumns;
    }
    return width;
}

int SeismoApp::scaleMg() const { return kScalesMg[scale_index_]; }

int SeismoApp::sampleToY(const int mg) const {
    const Plot plot = plotFor(M5Cardputer.Display);
    int half = plot.h / 2;
    if (half < 1) {
        half = 1;
    }
    int px = mg * half / scaleMg();
    if (px > half) {
        px = half;
    } else if (px < -half) {
        px = -half;
    }
    int y = plot.center_y - px;
    const int top = plot.y;
    const int bottom = plot.y + plot.h - 1;
    if (y < top) {
        y = top;
    } else if (y > bottom) {
        y = bottom;
    }
    return y;
}

void SeismoApp::resetTrace() {
    cursor_ = 0;
    peak_mg_ = 0;
    prev_y_ = sampleToY(0);
    cursor_drawn_ = false;
    std::memset(samples_, 0, sizeof(samples_));
}

bool SeismoApp::readShake(int* mg) {
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
    const float mag = std::sqrt(ax * ax + ay * ay + az * az);
    if (!base_ready_) {
        base_g_ = mag;
        base_ready_ = true;
        *mg = 0;
        return true;
    }
    base_g_ += kBaseAlpha * (mag - base_g_);
    *mg = static_cast<int>(std::lround((mag - base_g_) * 1000.0f));
    return true;
#else
    (void)kBaseAlpha;
    if (!base_ready_) {
        base_g_ = 1.0f;
        base_ready_ = true;
    }
    *mg = 0;
    return true;
#endif
}

void SeismoApp::pushSample(const int mg) {
    const int count = columns();
    int stored = mg;
    if (stored > 32767) {
        stored = 32767;
    } else if (stored < -32767) {
        stored = -32767;
    }
    cursor_ = (cursor_ + 1) % count;
    if (cursor_ == 0) {
        peak_mg_ = 0;
    }
    samples_[cursor_] = static_cast<int16_t>(stored);
    const int abs_mg = stored < 0 ? -stored : stored;
    if (abs_mg > peak_mg_) {
        peak_mg_ = abs_mg;
    }
}

void SeismoApp::paintFooter(const bool force) {
    char line[32];
    if (imu_ok_) {
        std::snprintf(line, sizeof(line), "+-%dmg peak %d", scaleMg(), peak_mg_);
    } else {
        std::snprintf(line, sizeof(line), "No IMU");
    }
    if (!force && std::strcmp(line, footer_) == 0) {
        return;
    }
    std::snprintf(footer_, sizeof(footer_), "%s", line);
    ui::drawFittedFooter(footer_);
}

void SeismoApp::drawFull() {
    auto& display = M5Cardputer.Display;
    const Plot plot = plotFor(display);
    const int count = columns();
    display.startWrite();
    display.fillRect(0, plot.y, display.width(), plot.h, TFT_BLACK);
    display.drawFastHLine(0, plot.center_y, count, TFT_DARKGREY);
    cursor_drawn_ = false;
    if (imu_ok_) {
        int prev_x = -1;
        int prev_y = 0;
        for (int x = 0; x < count; ++x) {
            if (columnInGap(x, cursor_, count)) {
                prev_x = -1;
                continue;
            }
            const int y = sampleToY(samples_[x]);
            if (prev_x >= 0 && prev_x + 1 == x) {
                display.drawLine(prev_x, prev_y, x, y, TFT_WHITE);
            } else {
                display.drawPixel(x, y, TFT_WHITE);
            }
            prev_x = x;
            prev_y = y;
        }
        clearGap(display, plot, cursor_, count);
        const int y = sampleToY(samples_[cursor_]);
        drawCursorMark(display, plot, cursor_, y);
        prev_y_ = y;
        painted_cursor_ = cursor_;
        cursor_drawn_ = true;
    }
    paintFooter(true);
    display.endWrite();
}

void SeismoApp::paintLatestColumn() {
    auto& display = M5Cardputer.Display;
    const Plot plot = plotFor(display);
    const int count = columns();
    const int y = sampleToY(samples_[cursor_]);
    const int prev_x = (cursor_ + count - 1) % count;
    display.startWrite();
    if (cursor_drawn_ && painted_cursor_ != cursor_) {
        for (int dx = -2; dx <= 2; ++dx) {
            const int x = (painted_cursor_ + dx + count) % count;
            clearColumn(display, plot, x);
            restoreCenter(display, plot, x);
            if (columnInGap(x, cursor_, count)) {
                continue;
            }
            const int old_y = sampleToY(samples_[x]);
            const int left = (x + count - 1) % count;
            if (!columnInGap(left, cursor_, count) && left != cursor_) {
                display.drawLine(left, sampleToY(samples_[left]), x, old_y, TFT_WHITE);
            } else {
                display.drawPixel(x, old_y, TFT_WHITE);
            }
        }
    }
    for (int dx = -2; dx <= 2; ++dx) {
        const int x = (cursor_ + dx + count) % count;
        clearColumn(display, plot, x);
        restoreCenter(display, plot, x);
    }
    clearGap(display, plot, cursor_, count);
    if (cursor_ != 0) {
        display.drawLine(prev_x, prev_y_, cursor_, y, TFT_WHITE);
    } else {
        display.drawPixel(cursor_, y, TFT_WHITE);
    }
    drawCursorMark(display, plot, cursor_, y);
    prev_y_ = y;
    painted_cursor_ = cursor_;
    cursor_drawn_ = true;
    paintFooter(false);
    display.endWrite();
}

void SeismoApp::onEnter() {
    imu_ok_ = false;
    base_ready_ = false;
    base_g_ = 1.0f;
    scale_index_ = 2;
    header_focused_ = false;
    footer_[0] = '\0';
    resetTrace();
    last_sample_ms_ = millis() - kSampleMs;
    need_full_ = true;
}

void SeismoApp::onExit() {}

void SeismoApp::onDisplayWake() { need_full_ = true; }

NavRequest SeismoApp::onUpdate(const Keyboard_Class::KeysState* keys, const bool header_focused) {
    header_focused_ = header_focused;

    NavRequest req = NavRequest::None;
    if (keys != nullptr && !header_focused_) {
        const ArrowKey arrow = detectArrowKey(*keys);
        if (arrow == ArrowKey::Up) {
            req = NavRequest::FocusHeader;
        } else if (arrow == ArrowKey::Left && scale_index_ > 0) {
            --scale_index_;
            need_full_ = true;
        } else if (arrow == ArrowKey::Right && scale_index_ + 1 < kScaleCount) {
            ++scale_index_;
            need_full_ = true;
        }
    }

    const uint32_t now = millis();
    const bool due = static_cast<uint32_t>(now - last_sample_ms_) >= kSampleMs;
    if (due) {
        int mg = 0;
        const bool ok = readShake(&mg);
        last_sample_ms_ = now;
        if (!ok) {
            if (imu_ok_ || footer_[0] == '\0') {
                imu_ok_ = false;
                base_ready_ = false;
                need_full_ = true;
            }
        } else if (!imu_ok_) {
            imu_ok_ = true;
            base_ready_ = false;
            resetTrace();
            readShake(&mg);
            need_full_ = true;
        } else {
            pushSample(mg);
            if (!need_full_) {
                paintLatestColumn();
            }
        }
    }

    if (need_full_) {
        drawFull();
        need_full_ = false;
    }
    return req;
}
