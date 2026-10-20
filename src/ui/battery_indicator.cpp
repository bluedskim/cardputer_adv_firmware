#include "ui/battery_indicator.h"

#include <M5Cardputer.h>

#include <algorithm>

#if !defined(CARDPUTER_SIM)
#include "HWCDC.h"
#endif

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

void drawLightningBolt(lgfx::LovyanGFX& display, const int x, const int y) {
    // Classic vertical ⚡ (6x7) — recognizable zigzag, white + black outline.
    static constexpr const char* kRows[] = {
        "...##.",
        "..###.",
        ".####.",
        "#####.",
        "..###.",
        ".###..",
        "##....",
    };
    constexpr int kCols = 6;
    constexpr int kRowCount = 7;
    const int ox = x + (kBodyWidth - kCols) / 2;
    const int oy = y + 2;

    auto plot = [&](const int px, const int py, const uint16_t color) {
        if (px >= x + 1 && px < x + kBodyWidth - 1 && py >= y + 1 && py < y + kBodyHeight - 1) {
            display.drawPixel(px, py, color);
        }
    };

    for (int row = 0; row < kRowCount; ++row) {
        for (int col = 0; col < kCols; ++col) {
            if (kRows[row][col] != '#') {
                continue;
            }
            const int px = ox + col;
            const int py = oy + row;
            for (int dy = -1; dy <= 1; ++dy) {
                for (int dx = -1; dx <= 1; ++dx) {
                    if (dx == 0 && dy == 0) {
                        continue;
                    }
                    plot(px + dx, py + dy, TFT_BLACK);
                }
            }
        }
    }
    for (int row = 0; row < kRowCount; ++row) {
        for (int col = 0; col < kCols; ++col) {
            if (kRows[row][col] == '#') {
                plot(ox + col, oy + row, TFT_WHITE);
            }
        }
    }
}

bool usbLikelyPowered() {
#if defined(CARDPUTER_SIM)
    return false;
#else
    // Cardputer has no charge-status pin; SOF presence means USB host is plugged.
    return HWCDC::isPlugged();
#endif
}

}  // namespace

BatterySnapshot readBatterySnapshot() {
    BatterySnapshot snapshot;
    snapshot.level = static_cast<int>(M5.Power.getBatteryLevel());
    const auto charge = M5.Power.isCharging();
    snapshot.charge_known = charge != m5::Power_Class::is_charging_t::charge_unknown;
    if (snapshot.charge_known) {
        snapshot.charging = charge == m5::Power_Class::is_charging_t::is_charging;
    } else {
        // Hardware cannot report CHG/STDBY; USB plug is the best available proxy.
        snapshot.charging = usbLikelyPowered();
    }
    snapshot.voltage_mv = M5.Power.getBatteryVoltage();
    return snapshot;
}

void drawBatteryIndicator(lgfx::LovyanGFX& display, int x, int y, const BatterySnapshot& battery) {
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
