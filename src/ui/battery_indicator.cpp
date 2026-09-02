#include "ui/battery_indicator.h"

#include <M5Cardputer.h>

#include <algorithm>

namespace {

constexpr int kBodyWidth = 18;
constexpr int kBodyHeight = 10;
constexpr int kTipWidth = 2;

uint16_t levelColor(const int level) {
    if (level <= 20) {
        return TFT_RED;
    }
    if (level <= 50) {
        return TFT_YELLOW;
    }
    return TFT_GREEN;
}

void drawLightningBolt(M5GFX& display, const int x, const int y) {
    const int cx = x + kBodyWidth / 2;
    const int cy = y + kBodyHeight / 2;

    // Horizontal bolt (vertical glyph rotated 90° CW), fits the 18x10 body.
    display.fillTriangle(cx - 4, cy - 2, cx + 1, cy + 3, cx, cy - 1, TFT_WHITE);
    display.fillTriangle(cx + 1, cy + 1, cx + 4, cy - 2, cx, cy - 3, TFT_WHITE);
}

}  // namespace

BatterySnapshot readBatterySnapshot() {
    BatterySnapshot snapshot;
    snapshot.level = static_cast<int>(M5.Power.getBatteryLevel());
    const auto charge = M5.Power.isCharging();
    snapshot.charge_known = charge != m5::Power_Class::is_charging_t::charge_unknown;
    snapshot.charging = charge == m5::Power_Class::is_charging_t::is_charging;
    snapshot.voltage_mv = M5.Power.getBatteryVoltage();
    return snapshot;
}

void drawBatteryIndicator(M5GFX& display, int x, int y, const BatterySnapshot& battery) {
    display.drawRect(x, y, kBodyWidth, kBodyHeight, TFT_WHITE);
    display.fillRect(x + kBodyWidth, y + 3, kTipWidth, kBodyHeight - 6, TFT_WHITE);

    if (battery.level >= 0) {
        const int fill_width =
            std::max(1, (battery.level * (kBodyWidth - 4)) / 100);
        display.fillRect(x + 2, y + 2, fill_width, kBodyHeight - 4, levelColor(battery.level));
    }

    if (battery.charging) {
        drawLightningBolt(display, x, y);
    }
}
