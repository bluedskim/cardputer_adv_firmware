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
    screen_ = Screen::Main;
    focus_ = MainRow::Sleep;
    ble_focus_ = BleRow::Action;
    last_battery_ms_ = 0;
    refreshBattery();
    markDirty();
    requestChromeFocus();
}

void SettingsApp::onExit() { M5Cardputer.Display.fillScreen(TFT_BLACK); }

void SettingsApp::onDisplayWake() { markDirty(); }

void SettingsApp::refreshBattery() {
    const BatterySnapshot next = readBatterySnapshot();
    if (next != battery_) {
        battery_ = next;
        markDirty();
    }
    last_battery_ms_ = millis();
}

bool SettingsApp::showForgetRow() const {
    const auto& link = BleLink::instance();
    return link.state() == BleLink::State::Off && link.isPaired();
}

const char* SettingsApp::primaryBleActionLabel() const {
#if defined(CARDPUTER_SIM)
    return nullptr;
#else
    const auto& link = BleLink::instance();
    switch (link.state()) {
        case BleLink::State::Off:
            return link.isPaired() ? "Connect" : "Pair";
        case BleLink::State::Pairing:
        case BleLink::State::Advertising:
            return "Cancel";
        case BleLink::State::Connected:
            return "Disconnect";
    }
    return "Pair";
#endif
}

void SettingsApp::enterBleScreen() {
    screen_ = Screen::Ble;
    ble_focus_ = BleRow::Action;
    markDirty();
}

void SettingsApp::leaveBleScreen() {
    screen_ = Screen::Main;
    focus_ = MainRow::Ble;
    ble_focus_ = BleRow::Action;
    markDirty();
}

void SettingsApp::runPrimaryBleAction() {
    auto& link = BleLink::instance();
    switch (link.state()) {
        case BleLink::State::Off:
            if (link.isPaired()) {
                link.startConnect();
            } else {
                link.startPairing();
            }
            break;
        case BleLink::State::Pairing:
        case BleLink::State::Advertising:
        case BleLink::State::Connected:
            link.disconnect();
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

    static char last_ble_status[24] = {};
    const char* ble_status = BleLink::instance().statusSummary();
    if (std::strncmp(last_ble_status, ble_status, sizeof(last_ble_status)) != 0) {
        std::snprintf(last_ble_status, sizeof(last_ble_status), "%s", ble_status);
        markDirty();
    }

    // Forget row can appear/disappear when pairing state changes.
    if (screen_ == Screen::Ble && !showForgetRow() && ble_focus_ == BleRow::Forget) {
        ble_focus_ = BleRow::Action;
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
    if (isBackKey(status)) {
        if (screen_ == Screen::BleForgetConfirm) {
            screen_ = Screen::Ble;
            ble_focus_ = BleRow::Forget;
            return NavRequest::None;
        }
        if (screen_ == Screen::Ble) {
            leaveBleScreen();
            return NavRequest::None;
        }
    }

    switch (screen_) {
        case Screen::Main:
            return handleMainInput(status);
        case Screen::Ble:
            return handleBleInput(status);
        case Screen::BleForgetConfirm:
            return handleBleForgetConfirmInput(status);
    }
    return NavRequest::None;
}

NavRequest SettingsApp::handleMainInput(const Keyboard_Class::KeysState& status) {
    switch (detectArrowKey(status)) {
        case ArrowKey::Left:
            if (focus_ == MainRow::Sleep) {
                SystemSettings::instance().cycleSleepOption(-1);
            } else if (focus_ == MainRow::Bright) {
                SystemSettings::instance().cycleBrightnessOption(-1);
            }
            return NavRequest::None;
        case ArrowKey::Right:
            if (focus_ == MainRow::Sleep) {
                SystemSettings::instance().cycleSleepOption(1);
            } else if (focus_ == MainRow::Bright) {
                SystemSettings::instance().cycleBrightnessOption(1);
            }
            return NavRequest::None;
        case ArrowKey::Up:
            if (focus_ == MainRow::Sleep) {
                return NavRequest::FocusChrome;
            }
            focus_ = static_cast<MainRow>(static_cast<uint8_t>(focus_) - 1);
            return NavRequest::None;
        case ArrowKey::Down:
            if (static_cast<uint8_t>(focus_) + 1 < static_cast<uint8_t>(MainRow::Count)) {
                focus_ = static_cast<MainRow>(static_cast<uint8_t>(focus_) + 1);
            }
            return NavRequest::None;
        default:
            break;
    }

    if (status.enter && focus_ == MainRow::Ble) {
        enterBleScreen();
    }
    return NavRequest::None;
}

NavRequest SettingsApp::handleBleInput(const Keyboard_Class::KeysState& status) {
    switch (detectArrowKey(status)) {
        case ArrowKey::Left:
            leaveBleScreen();
            return NavRequest::None;
        case ArrowKey::Up:
            if (ble_focus_ == BleRow::Action) {
                leaveBleScreen();
                return NavRequest::None;
            }
            ble_focus_ = BleRow::Action;
            return NavRequest::None;
        case ArrowKey::Down:
            if (showForgetRow() && ble_focus_ == BleRow::Action) {
                ble_focus_ = BleRow::Forget;
            }
            return NavRequest::None;
        case ArrowKey::Right:
            return NavRequest::None;
        default:
            break;
    }

    if (status.enter) {
        if (ble_focus_ == BleRow::Forget && showForgetRow()) {
            screen_ = Screen::BleForgetConfirm;
            return NavRequest::None;
        }
        if (ble_focus_ == BleRow::Action && primaryBleActionLabel() != nullptr) {
            runPrimaryBleAction();
        }
    }
    return NavRequest::None;
}

NavRequest SettingsApp::handleBleForgetConfirmInput(const Keyboard_Class::KeysState& status) {
    if (isBackKey(status) || detectArrowKey(status) == ArrowKey::Left) {
        screen_ = Screen::Ble;
        ble_focus_ = BleRow::Forget;
        return NavRequest::None;
    }
    if (status.enter) {
        BleLink::instance().forget();
        screen_ = Screen::Ble;
        ble_focus_ = BleRow::Action;
    }
    return NavRequest::None;
}

void SettingsApp::draw() const {
    auto& display = M5Cardputer.Display;
    display.fillRect(0, kContentTop, display.width(), display.height() - kContentTop, TFT_BLACK);

    switch (screen_) {
        case Screen::Main:
            drawMain();
            break;
        case Screen::Ble:
            drawBle();
            break;
        case Screen::BleForgetConfirm:
            drawBleForgetConfirm();
            break;
    }
}

void SettingsApp::drawMain() const {
    auto& display = M5Cardputer.Display;
    const int width = display.width();
    int y = kListTop;

    auto drawSelectable = [&](MainRow row, const char* text) {
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
        drawSelectable(MainRow::Sleep, line);
    }
    {
        char line[32];
        std::snprintf(line, sizeof(line), "Bright %s",
                      SystemSettings::instance().brightnessOptionLabel());
        drawSelectable(MainRow::Bright, line);
    }
    {
        char line[32];
        std::snprintf(line, sizeof(line), "BLE %s", BleLink::instance().statusSummary());
        drawSelectable(MainRow::Ble, line);
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

void SettingsApp::drawBle() const {
    auto& display = M5Cardputer.Display;
    const int width = display.width();
    int y = kListTop;

    auto drawInfoLine = [&](const char* text) {
        display.setTextColor(TFT_WHITE, TFT_BLACK);
        display.setTextSize(2);
        display.setCursor(8, y);
        display.print(text);
        y += kLineHeight;
    };

    auto drawSelectable = [&](BleRow row, const char* text) {
        const bool selected = ble_focus_ == row && !chrome_focused_;
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

    drawInfoLine(BleLink::instance().statusSummary());

    const char* peer = BleLink::instance().peerLabel();
    if (peer[0] != '\0' && BleLink::instance().isPaired()) {
        char line[24];
        std::snprintf(line, sizeof(line), "peer %s", peer);
        drawInfoLine(line);
    }

    const char* action = primaryBleActionLabel();
    if (action != nullptr) {
        drawSelectable(BleRow::Action, action);
    } else {
        drawInfoLine("(sim no BLE)");
    }

    if (showForgetRow()) {
        drawSelectable(BleRow::Forget, "Forget");
    }
}

void SettingsApp::drawBleForgetConfirm() const {
    auto& display = M5Cardputer.Display;
    int y = kListTop;
    display.setTextColor(TFT_WHITE, TFT_BLACK);
    display.setTextSize(2);
    display.setCursor(8, y);
    display.print("Forget?");
    y += kLineHeight;
    display.setTextColor(TFT_YELLOW, TFT_BLACK);
    display.setCursor(8, y);
    display.print("> Enter yes");
    y += kLineHeight;
    display.setTextColor(TFT_WHITE, TFT_BLACK);
    display.setCursor(8, y);
    display.print("  Del/BS cancel");
}
