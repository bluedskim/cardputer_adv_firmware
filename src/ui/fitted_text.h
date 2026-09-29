#pragma once

namespace ui {

// Largest text size in [1, 2], stepping down by 0.25, that fits max_width.
// Sets the display text size as a side effect.
void fitTextSize(const char* text, int max_width);

// Bottom status line. Prefers text size 2 and shrinks until the string fits.
void drawFittedFooter(const char* text);

}  // namespace ui
