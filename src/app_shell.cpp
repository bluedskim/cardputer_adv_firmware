#include "app_shell.h"

#include <M5Cardputer.h>

#include <algorithm>
#include <cstring>

#include "config.h"
#include "input/keyboard_input.h"
#include "launcher_exit.h"
#include "ui/battery_indicator.h"

namespace {

constexpr uint32_t kBatteryPollMs = 1000;
constexpr int kBatteryWidth = 22;
constexpr int kDotSpacing = 7;
constexpr int kDotRadius = 2;
constexpr int kDotActiveRing = 3;
constexpr int kDotsRightMargin = 4;
constexpr int kBatteryDotGap = 4;
constexpr int kTitleClusterGap = 4;
constexpr int kCharWidth = 12;

char arrowToChar(ArrowKey arrow) {
    switch (arrow) {
        case ArrowKey::Up:
            return ';';
        case ArrowKey::Down:
            return '.';
        case ArrowKey::Left:
            return ',';
        case ArrowKey::Right:
            return '/';
        default:
            return 0;
    }
}

}  // namespace

void AppShell::addApp(std::unique_ptr<AppBase> app) {
    apps_.push_back(std::move(app));
}

void AppShell::begin() {
    if (apps_.empty()) {
        return;
    }

    SystemSettings::instance().begin();
    M5Cardputer.Display.setRotation(1);
    M5Cardputer.Display.fillScreen(TFT_BLACK);
    saved_brightness_ = M5Cardputer.Display.getBrightness();
    if (saved_brightness_ == 0) {
        saved_brightness_ = 128;
    }
    last_activity_ms_ = millis();
    display_asleep_ = false;
    chrome_focused_ = true;
    apps_[current_index_]->onEnter();
    battery_ = readBatterySnapshot();
    last_battery_poll_ms_ = millis();
    chrome_dirty_ = true;
    drawChrome();
    chrome_dirty_ = false;
}

void AppShell::update() {
    if (apps_.empty()) {
        return;
    }

    M5Cardputer.update();

    for (const auto& app : apps_) {
        app->onBackgroundTick();
    }

    if (display_asleep_) {
        if (M5Cardputer.Keyboard.isPressed()) {
            wakeDisplay();
        } else {
            return;
        }
    }

    if (M5Cardputer.Keyboard.isPressed()) {
        noteUserActivity();
    }

    if (M5Cardputer.BtnA.wasClicked()) {
        launcher_exit::rebootToLauncher();
    }

    const auto status = M5Cardputer.Keyboard.keysState();
    const bool battery_changed = refreshBatterySnapshot();
    if (battery_changed) {
        chrome_dirty_ = true;
    }

    if (chrome_focused_) {
        if (consumeChromeKeys(status)) {
            chrome_dirty_ = true;
            noteUserActivity();
        }
        apps_[current_index_]->onUpdate(nullptr, chrome_focused_);
        if (chrome_dirty_) {
            drawChrome();
            chrome_dirty_ = false;
        }
        return;
    }

    const ArrowKey arrow = pollArrowEdge(status, keyboard_tracker_);
    const KeyEvent keys = pollAppKeyEdge(status, keyboard_tracker_);

    Keyboard_Class::KeysState delivered = keys.status;
    delivered.word.erase(std::remove_if(delivered.word.begin(), delivered.word.end(), isArrowChar),
                         delivered.word.end());
    if (arrow != ArrowKey::None) {
        delivered.word.insert(delivered.word.begin(), arrowToChar(arrow));
    }

    const bool has_input = keys.active || arrow != ArrowKey::None;
    if (has_input) {
        noteUserActivity();
    }
    const NavRequest req =
        apps_[current_index_]->onUpdate(has_input ? &delivered : nullptr, false);
    if (req == NavRequest::FocusChrome) {
        chrome_focused_ = true;
        chrome_dirty_ = true;
    }

    if (chrome_dirty_) {
        drawChrome();
        chrome_dirty_ = false;
    }

    maybeSleepDisplay();
}

void AppShell::noteUserActivity() {
    last_activity_ms_ = millis();
}

void AppShell::maybeSleepDisplay() {
    const uint32_t timeout_sec = SystemSettings::instance().sleepTimeoutSec();
    if (timeout_sec == 0 || display_asleep_) {
        return;
    }

    if (millis() - last_activity_ms_ >= timeout_sec * 1000U) {
        sleepDisplay();
    }
}

void AppShell::sleepDisplay() {
    if (display_asleep_) {
        return;
    }

    const uint8_t brightness = M5Cardputer.Display.getBrightness();
    if (brightness > 0) {
        saved_brightness_ = brightness;
    }
    M5Cardputer.Display.setBrightness(0);
    display_asleep_ = true;
}

void AppShell::wakeDisplay() {
    M5Cardputer.Display.setBrightness(saved_brightness_ > 0 ? saved_brightness_ : 128);
    display_asleep_ = false;
    noteUserActivity();
    chrome_dirty_ = true;
    apps_[current_index_]->onExit();
    M5Cardputer.Display.fillScreen(TFT_BLACK);
    apps_[current_index_]->onEnter();
    drawChrome();
    chrome_dirty_ = false;
}

bool AppShell::refreshBatterySnapshot() {
    const uint32_t now = millis();
    if (now - last_battery_poll_ms_ < kBatteryPollMs) {
        return false;
    }

    last_battery_poll_ms_ = now;
    const BatterySnapshot next = readBatterySnapshot();
    if (next == battery_) {
        return false;
    }
    battery_ = next;
    return true;
}

void AppShell::switchTo(const std::size_t index) {
    if (index >= apps_.size() || index == current_index_) {
        return;
    }

    apps_[current_index_]->onExit();
    current_index_ = index;
    M5Cardputer.Display.fillScreen(TFT_BLACK);
    apps_[current_index_]->onEnter();
    battery_ = readBatterySnapshot();
    last_battery_poll_ms_ = millis();
    noteUserActivity();
    chrome_focused_ = true;
    chrome_dirty_ = true;
}

bool AppShell::consumeChromeKeys(const Keyboard_Class::KeysState& status) {
    const ArrowKey arrow = pollArrowEdge(status, keyboard_tracker_);
    const bool enter_edge = status.enter && !keyboard_tracker_.enter_down;
    keyboard_tracker_.enter_down = status.enter;
    keyboard_tracker_.del_down = status.del;

    switch (arrow) {
        case ArrowKey::Left: {
            const std::size_t next =
                current_index_ == 0 ? apps_.size() - 1 : current_index_ - 1;
            switchTo(next);
            return true;
        }
        case ArrowKey::Right:
            switchTo((current_index_ + 1) % apps_.size());
            return true;
        case ArrowKey::Down:
            chrome_focused_ = false;
            return true;
        default:
            break;
    }

    if (enter_edge) {
        chrome_focused_ = false;
        return true;
    }
    return false;
}

void AppShell::drawChrome() const {
    auto& display = M5Cardputer.Display;
    const int width = display.width();
    const std::size_t app_count = apps_.size();
    const int dots_span =
        app_count > 0 ? static_cast<int>((app_count - 1) * kDotSpacing + kDotRadius * 2) : 0;
    const int dots_width = dots_span + kDotsRightMargin;
    const int cluster_width = kBatteryWidth + kBatteryDotGap + dots_width;
    const int battery_x = width - cluster_width;
    const int arrow_right_x = battery_x - kTitleClusterGap - 2 * kCharWidth;
    const int title_x = 4 + 2 * kCharWidth;
    const uint16_t bar = chrome_focused_ ? TFT_NAVY : TFT_DARKGREY;
    const uint16_t title = chrome_focused_ ? TFT_YELLOW : TFT_WHITE;

    display.fillRect(0, 0, width, config::kChromeHeight, bar);
    display.setTextSize(2);
    display.setTextColor(title, bar);

    display.setCursor(4, 4);
    display.print("< ");

    const char* app_name = apps_[current_index_]->name();
    const int max_name_chars = std::max(0, (arrow_right_x - title_x) / kCharWidth);
    display.setCursor(title_x, 4);
    if (max_name_chars > 0) {
        const std::size_t name_len = std::strlen(app_name);
        if (static_cast<int>(name_len) <= max_name_chars) {
            display.print(app_name);
        } else {
            char truncated[32];
            std::strncpy(truncated, app_name, static_cast<std::size_t>(max_name_chars));
            truncated[max_name_chars] = '\0';
            display.print(truncated);
        }
    }

    display.setCursor(arrow_right_x, 4);
    display.print(" >");

    drawBatteryIndicator(display, battery_x, 6, battery_);

    for (std::size_t i = 0; i < app_count; ++i) {
        const int dot_x = width - kDotsRightMargin - kDotRadius -
                          static_cast<int>((app_count - 1 - i) * kDotSpacing);
        const uint16_t color = i == current_index_ ? TFT_YELLOW : TFT_DARKGREY;
        display.fillCircle(dot_x, 11, kDotRadius, color);
        if (i == current_index_) {
            display.drawCircle(dot_x, 11, kDotActiveRing, TFT_WHITE);
        }
    }
}
