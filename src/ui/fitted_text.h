#pragma once

#include <M5GFX.h>

namespace ui {

// Largest text size in [1, 2], stepping down by 0.25, that fits max_width.
// Sets the text size of display as a side effect.
void fitTextSize(lgfx::LovyanGFX& display, const char* text, int max_width);

// Bottom status line. Prefers text size 2 and shrinks until the string fits.
// Draws into the frame and presents the footer rows.
void drawFittedFooter(const char* text);

}  // namespace ui
