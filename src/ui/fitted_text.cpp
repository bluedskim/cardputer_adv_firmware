#include "ui/fitted_text.h"

#include <M5Cardputer.h>

#include <algorithm>

#include "config.h"
#include "ui/frame.h"

namespace ui {

void fitTextSize(lgfx::LovyanGFX& display, const char* text, int max_width) {
    float size = 2.0f;
    display.setTextSize(size);
    if (text == nullptr || max_width <= 0) {
        return;
    }
    while (size > 1.0f && display.textWidth(text) > max_width) {
        size -= 0.25f;
        if (size < 1.0f) {
            size = 1.0f;
        }
        display.setTextSize(size);
    }
}

void drawFittedFooter(const char* text) {
    if (text == nullptr || text[0] == '\0') {
        return;
    }
    auto& display = frame();
    const int y = display.height() - config::kFooterHeight;
    constexpr int kMargin = 4;
    const int max_w = display.width() - kMargin * 2;
    display.fillRect(0, y, display.width(), config::kFooterHeight, TFT_BLACK);
    display.setTextColor(TFT_DARKGREY, TFT_BLACK);
    display.setTextDatum(top_left);
    fitTextSize(display, text, max_w);
    const int text_h = display.fontHeight();
    const int text_y = y + std::max(0, (config::kFooterHeight - text_h) / 2);
    display.setCursor(kMargin, text_y);
    display.print(text);
    present(y, config::kFooterHeight);
}

}  // namespace ui
