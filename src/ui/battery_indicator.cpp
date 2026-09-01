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

}  // namespace

BatterySnapshot readBatterySnapshot() {
    BatterySnapshot snapshot;
    snapshot.level = static_cast<int>(M5.Power.getBatteryLevel());
    snapshot.charging =
        M5.Power.isCharging() == m5::Power_Class::is_charging_t::is_charging;
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
        const int cx = x + kBodyWidth / 2;
        display.drawLine(cx, y + 2, cx - 3, y + 6, TFT_WHITE);
        display.drawLine(cx, y + 2, cx + 3, y + 6, TFT_WHITE);
        display.drawLine(cx - 3, y + 6, cx + 3, y + 6, TFT_WHITE);
    }
}
