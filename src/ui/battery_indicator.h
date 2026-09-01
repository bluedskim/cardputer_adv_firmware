#pragma once

#include <M5GFX.h>

#include <cstdint>

struct BatterySnapshot {
    int level = -1;
    bool charging = false;

    bool operator==(const BatterySnapshot& other) const {
        return level == other.level && charging == other.charging;
    }
    bool operator!=(const BatterySnapshot& other) const { return !(*this == other); }
};

BatterySnapshot readBatterySnapshot();
void drawBatteryIndicator(M5GFX& display, int x, int y, const BatterySnapshot& battery);
