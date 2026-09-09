#include "apps/settings/settings_app.h"

#include <M5Cardputer.h>

#include <cstdio>
#include <cstring>

#include "ble/ble_link.h"
#include "config.h"
#include "input/keyboard_input.h"
#include "system_settings.h"

namespace {

constexpr int kContentTop = config::kContentTop;
constexpr int kLineHeight = 16;
constexpr int kListTop = kContentTop + 4;
constexpr uint32_t kBatteryRefreshMs = 1000;

}  // namespace

const char* SettingsApp::name() const { return "Settings"; }

void SettingsApp::onEnter() {
    focus_ = Row::Sleep;
    ble_action_ = BleAction::Pair;
    last_battery_ms_ = 0;
    refreshBattery();
    markDirty();
    requestChromeFocus();
}

void SettingsApp::onExit() { M5Cardputer.Display.fillScreen(TFT_BLACK); }

void SettingsApp::refreshBattery() {
    const BatterySnapshot next = readBatterySnapshot();
    if (next != battery_) {
        battery_ = next;
        markDirty();
    }
    last_battery_ms_ = millis();
}

const char* SettingsApp::bleActionLabel() const {
    switch (ble_action_) {
        case BleAction::Pair:
            return "Pair";
        case BleAction::Connect:
            return "Connect";
        case BleAction::Disconnect:
            return "Disconnect";
        case BleAction::Forget:
            return "Forget";
        default:
            return "Pair";
    }
}

void SettingsApp::cycleBleAction(const int delta) {
    if (delta == 0) {
        return;
    }
    const int count = static_cast<int>(BleAction::Count);
    int next = static_cast<int>(ble_action_) + delta;
    while (next < 0) {
        next += count;
    }
    while (next >= count) {
        next -= count;
    }
    ble_action_ = static_cast<BleAction>(next);
}

void SettingsApp::activateBleAction() {
    auto& link = BleLink::instance();
    switch (ble_action_) {
        case BleAction::Pair:
            link.startPairing();
            break;
        case BleAction::Connect:
            link.startConnect();
            break;
        case BleAction::Disconnect:
            link.disconnect();
            break;
        case BleAction::Forget:
            link.forget();
            break;
        default:
            break;
    }
    markDirty();
}

NavRequest SettingsApp::onUpdate(const Keyboard_Class::KeysState* keys, bool chrome_focused) {
    if (chrome_focused_ != chrome_focused) {
        chrome_focused_ = chrome_focused;
        markDirty();
    }

    if (millis() - last_battery_ms_ >= kBatteryRefreshMs) {
        refreshBattery();
    }

    static char last_ble_status[16] = {};
    const char* ble_status = BleLink::instance().statusLabel();
    if (std::strncmp(last_ble_status, ble_status, sizeof(last_ble_status)) != 0) {
        std::snprintf(last_ble_status, sizeof(last_ble_status), "%s", ble_status);
        markDirty();
    }

    NavRequest req = NavRequest::None;
    if (keys != nullptr) {
        req = handleInput(*keys);
        markDirty();
    }

    if (dirty_) {
        draw();
        dirty_ = false;
    }
    if (pending_chrome_focus_) {
        pending_chrome_focus_ = false;
        return NavRequest::FocusChrome;
    }
    return req;
}

NavRequest SettingsApp::handleInput(const Keyboard_Class::KeysState& status) {
    switch (detectArrowKey(status)) {
        case ArrowKey::Left:
            if (focus_ == Row::Sleep) {
                SystemSettings::instance().cycleSleepOption(-1);
            } else if (focus_ == Row::Bright) {
                SystemSettings::instance().cycleBrightnessOption(-1);
            } else if (focus_ == Row::BleAction) {
                cycleBleAction(-1);
            }
            return NavRequest::None;
        case ArrowKey::Right:
            if (focus_ == Row::Sleep) {
                SystemSettings::instance().cycleSleepOption(1);
            } else if (focus_ == Row::Bright) {
                SystemSettings::instance().cycleBrightnessOption(1);
            } else if (focus_ == Row::BleAction) {
                cycleBleAction(1);
            }
            return NavRequest::None;
        case ArrowKey::Up:
            if (focus_ == Row::Sleep) {
                return NavRequest::FocusChrome;
            }
            focus_ = static_cast<Row>(static_cast<uint8_t>(focus_) - 1);
            return NavRequest::None;
        case ArrowKey::Down:
            if (static_cast<uint8_t>(focus_) + 1 < static_cast<uint8_t>(Row::Count)) {
                focus_ = static_cast<Row>(static_cast<uint8_t>(focus_) + 1);
            }
            return NavRequest::None;
        default:
            break;
    }

    if (status.enter && focus_ == Row::BleAction) {
        activateBleAction();
    }

    return NavRequest::None;
}

void SettingsApp::draw() const {
    auto& display = M5Cardputer.Display;
    display.fillRect(0, kContentTop, display.width(), display.height() - kContentTop, TFT_BLACK);

    const int width = display.width();
    int y = kListTop;

    auto drawSelectable = [&](Row row, const char* text) {
        const bool selected = focus_ == row && !chrome_focused_;
        const uint16_t fg = selected ? TFT_YELLOW : TFT_WHITE;
        const uint16_t bg = selected ? TFT_NAVY : TFT_BLACK;
        if (selected) {
            display.fillRect(0, y, width, kLineHeight, bg);
        }
        display.setTextColor(fg, bg);
        display.setTextSize(2);
        display.setCursor(8, y);
        display.printf("%c %s", selected ? '>' : ' ', text);
        y += kLineHeight;
    };

    auto drawInfoLine = [&](const char* text) {
        display.setTextColor(TFT_WHITE, TFT_BLACK);
        display.setTextSize(2);
        display.setCursor(8, y);
        display.print(text);
        y += kLineHeight;
    };

    {
        char line[32];
        std::snprintf(line, sizeof(line), "Sleep  %s", SystemSettings::instance().sleepOptionLabel());
        drawSelectable(Row::Sleep, line);
    }
    {
        char line[32];
        std::snprintf(line, sizeof(line), "Bright %s",
                      SystemSettings::instance().brightnessOptionLabel());
        drawSelectable(Row::Bright, line);
    }
    {
        char line[32];
        std::snprintf(line, sizeof(line), "BLE %s", bleActionLabel());
        drawSelectable(Row::BleAction, line);
    }

    {
        char line[32];
        const auto& link = BleLink::instance();
        if (link.peerLabel()[0] != '\0' && link.state() != BleLink::State::Off) {
            std::snprintf(line, sizeof(line), "St %s", link.statusLabel());
        } else {
            std::snprintf(line, sizeof(line), "St %s", link.statusLabel());
        }
        drawInfoLine(line);
    }

    {
        char line[32];
        if (battery_.level >= 0 && battery_.voltage_mv > 0) {
            const unsigned volts = static_cast<unsigned>(battery_.voltage_mv / 1000);
            const unsigned centi = static_cast<unsigned>((battery_.voltage_mv % 1000) / 10);
            std::snprintf(line, sizeof(line), "Bat %d%% %u.%02uV", battery_.level, volts, centi);
        } else if (battery_.level >= 0) {
            std::snprintf(line, sizeof(line), "Bat %d%%", battery_.level);
        } else if (battery_.voltage_mv > 0) {
            const unsigned volts = static_cast<unsigned>(battery_.voltage_mv / 1000);
            const unsigned centi = static_cast<unsigned>((battery_.voltage_mv % 1000) / 10);
            std::snprintf(line, sizeof(line), "Bat %u.%02uV", volts, centi);
        } else {
            std::snprintf(line, sizeof(line), "Bat --");
        }
        drawInfoLine(line);
    }

    {
        char line[32];
        std::snprintf(line, sizeof(line), "S/W Ver %s", config::kFirmwareVersion);
        drawInfoLine(line);
    }
}
