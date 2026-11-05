#pragma once

#include <M5GFX.h>

namespace ui {

// Off-screen copy of the display. Draw into frame(), then present() the rows that
// changed: the panel gets the finished pixels in one transfer instead of a
// clear-then-redraw it can show half done (flicker). Falls back to the display
// itself if the buffer cannot be allocated.
lgfx::LovyanGFX& frame();

// Copy full-width rows [y, y + h) from the frame to the display.
void present(int y, int h);

// Everything below the header.
void presentContent();

}  // namespace ui
