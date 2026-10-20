#include "ui/frame.h"

#include <M5Cardputer.h>

#include "config.h"

namespace ui {

namespace {

M5Canvas g_canvas;
bool g_allocated = false;
bool g_ready = false;

}  // namespace

lgfx::LovyanGFX& frame() {
    auto& display = M5Cardputer.Display;
    if (!g_allocated) {
        g_allocated = true;
        // 16-bit full screen: 240 x 135 x 2 = ~64 KB of internal RAM (no PSRAM).
        g_ready = g_canvas.createSprite(display.width(), display.height()) != nullptr;
    }
    if (!g_ready) {
        return display;
    }
    return g_canvas;
}

void present(const int y, const int h) {
    if (!g_ready) {
        return;
    }
    auto& display = M5Cardputer.Display;
    display.setClipRect(0, y, display.width(), h);
    g_canvas.pushSprite(&display, 0, 0);
    display.clearClipRect();
}

void presentContent() {
    present(config::kContentTop, M5Cardputer.Display.height() - config::kContentTop);
}

}  // namespace ui
