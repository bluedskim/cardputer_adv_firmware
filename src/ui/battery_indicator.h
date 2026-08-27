#pragma once

#include <M5GFX.h>

#include <cstdint>

struct BatterySnapshot {
    int level = -1;
    bool charging = false;
};

BatterySnapshot readBatterySnapshot();
void drawBatteryIndicator(M5GFX& display, int x, int y, const BatterySnapshot& battery);
