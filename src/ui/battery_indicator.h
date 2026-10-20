#pragma once

#include <M5GFX.h>

#include <cstdint>

struct BatterySnapshot {
    int level = -1;
    bool charging = false;
    bool charge_known = false;
    int16_t voltage_mv = -1;

    bool operator==(const BatterySnapshot& other) const {
        return level == other.level && charging == other.charging &&
               charge_known == other.charge_known && voltage_mv == other.voltage_mv;
    }
    bool operator!=(const BatterySnapshot& other) const { return !(*this == other); }
};

BatterySnapshot readBatterySnapshot();
void drawBatteryIndicator(lgfx::LovyanGFX& display, int x, int y, const BatterySnapshot& battery);
