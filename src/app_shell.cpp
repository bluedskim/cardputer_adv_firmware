#include "app_shell.h"

#include <M5Cardputer.h>

#include <algorithm>
#include <cstring>

#if !defined(CARDPUTER_SIM)
#include <driver/gpio.h>
#include <esp_sleep.h>
#endif

#include "config.h"
#include "input/keyboard_input.h"
#include "app_visibility.h"
#include "ble/ble_link.h"
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

AppShell* AppShell::instance_ = nullptr;

void AppShell::addApp(std::unique_ptr<AppBase> app) {
    apps_.push_back(std::move(app));
}

bool AppShell::isAppVisible(const std::size_t index) const {
    if (index >= apps_.size()) {
        return false;
    }
    return !AppVisibility::instance().isHidden(apps_[index]->name());
}

std::size_t AppShell::adjacentVisible(const std::size_t from, const int delta) const {
    if (apps_.empty()) {
        return from;
    }
    std::size_t i = from;
    for (std::size_t n = 0; n < apps_.size(); ++n) {
        if (delta > 0) {
            i = (i + 1) % apps_.size();
        } else {
            i = i == 0 ? apps_.size() - 1 : i - 1;
        }
        if (isAppVisible(i)) {
            return i;
        }
    }
    return from;
}

void AppShell::begin() {
    instance_ = this;
    if (apps_.empty()) {
        return;
    }

    SystemSettings::instance().begin();
    AppVisibility::instance().begin();
    BleLink::instance().begin();
    M5Cardputer.Display.setRotation(1);
    M5Cardputer.Display.fillScreen(TFT_BLACK);
    saved_brightness_ = SystemSettings::instance().brightness();
    last_activity_ms_ = millis();
    display_asleep_ = false;
    header_focused_ = true;
    if (!isAppVisible(current_index_)) {
        current_index_ = adjacentVisible(current_index_, 1);
    }
    apps_[current_index_]->onEnter();
    battery_ = readBatterySnapshot();
    last_battery_poll_ms_ = millis();
    header_dirty_ = true;
    drawHeader();
    header_dirty_ = false;
}

void AppShell::update() {
    if (apps_.empty()) {
        return;
    }

    if (display_asleep_ && !anyBackgroundWork() && !BleLink::instance().blocksLightSleep()) {
        // Adv: sleep until TCA8418 INT. No keyboard poll and no timer wake.
        enterLowPowerWait();
        M5Cardputer.update();  // drain INT / key FIFO
        // INT fired ⇒ wake even if the key was already released.
        wakeDisplay();
    } else {
        M5Cardputer.update();

        BleLink::instance().tick();

        for (const auto& app : apps_) {
            app->onBackgroundTick();
        }

        if (display_asleep_) {
            // Timer/stopwatch running or BLE radio on: stay awake enough to tick.
            if (M5Cardputer.Keyboard.isPressed()) {
                wakeDisplay();
            } else {
                return;
            }
        }
    }

    if (M5Cardputer.Keyboard.isPressed()) {
        noteUserActivity();
    }

    // A long press is not a command. Consume it so it is not also reported as a click.
    (void)M5Cardputer.BtnA.wasHold();
    if (M5Cardputer.BtnA.wasClicked()) {
        noteUserActivity();
        if (display_asleep_) {
            wakeDisplay();
        }
        handleG0Short();
    }

    // Wake key must not navigate header/apps (Left/Right would leave Settings → Torch).
    if (ignore_keys_until_release_) {
        if (M5Cardputer.Keyboard.isPressed()) {
            apps_[current_index_]->onUpdate(nullptr, header_focused_);
            if (header_dirty_) {
                drawHeader();
                header_dirty_ = false;
            }
            maybeSleepDisplay();
            return;
        }
        ignore_keys_until_release_ = false;
        resetKeyboardTracker(keyboard_tracker_);
    }

    const auto status = M5Cardputer.Keyboard.keysState();
    const bool battery_changed = refreshBatterySnapshot();
    if (battery_changed) {
        header_dirty_ = true;
    }

    if (header_focused_) {
        if (consumeHeaderKeys(status)) {
            header_dirty_ = true;
            noteUserActivity();
        }
        const NavRequest req = apps_[current_index_]->onUpdate(nullptr, header_focused_);
        // List may arrive over BLE while header is still focused.
        if (req == NavRequest::FocusContent) {
            header_focused_ = false;
            header_dirty_ = true;
        }
    } else {
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
        if (req == NavRequest::FocusHeader) {
            header_focused_ = true;
            header_dirty_ = true;
        } else if (req == NavRequest::FocusContent) {
            header_focused_ = false;
            header_dirty_ = true;
        }
    }

    if (header_dirty_) {
        drawHeader();
        header_dirty_ = false;
    }

    maybeSleepDisplay();
}

uint32_t AppShell::loopDelayMs() const {
    if (display_asleep_) {
        // Low-power wait already consumed time in update(), or timers/BLE need a tight loop.
        return (anyBackgroundWork() || BleLink::instance().blocksLightSleep())
                   ? config::kActiveLoopDelayMs
                   : 0;
    }
    if (anyBackgroundWork() || BleLink::instance().blocksLightSleep()) {
        return config::kActiveLoopDelayMs;
    }
    if (millis() - last_activity_ms_ < config::kIdleLoopGraceMs) {
        return config::kActiveLoopDelayMs;
    }
    return config::kIdleLoopDelayMs;
}

bool AppShell::anyBackgroundWork() const {
    for (const auto& app : apps_) {
        if (app->hasActiveBackgroundWork()) {
            return true;
        }
    }
    return false;
}

void AppShell::enterLowPowerWait() const {
    if (anyBackgroundWork() || BleLink::instance().blocksLightSleep()) {
        return;
    }
#if defined(CARDPUTER_SIM)
    // Sim has no GPIO INT: wait until a key is held, then return.
    do {
        M5Cardputer.update();
        if (M5Cardputer.Keyboard.isPressed()) {
            break;
        }
        delay(50);
    } while (true);
#else
    const gpio_num_t int_pin = static_cast<gpio_num_t>(config::kKeyboardIntGpio);
    gpio_wakeup_enable(int_pin, GPIO_INTR_LOW_LEVEL);
    esp_sleep_enable_gpio_wakeup();
    M5.Power.lightSleep(m5::Power_Class::sleep_no_timer, false);
    gpio_wakeup_disable(int_pin);
#endif
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
    } else {
        saved_brightness_ = SystemSettings::instance().brightness();
    }
    M5Cardputer.Display.setBrightness(0);
    display_asleep_ = true;
    // Immediate = one-shot: flip to 30 sec as soon as we sleep so wake shows 30 sec.
    if (SystemSettings::instance().promoteImmediateToThirtySec()) {
        header_dirty_ = true;
    }
}

void AppShell::wakeDisplay() {
    // Sleep only dims the backlight; panel GRAM may still be valid, but light sleep
    // can leave a blank/white frame — always redraw the current app in place.
    const uint8_t restore = saved_brightness_ > 0 ? saved_brightness_
                                                   : SystemSettings::instance().brightness();
    M5Cardputer.Display.setBrightness(restore);
    display_asleep_ = false;
    // Backup if sleep path missed promote (older builds / odd wake).
    if (SystemSettings::instance().promoteImmediateToThirtySec()) {
        header_dirty_ = true;
    }
    noteUserActivity();
    ignore_keys_until_release_ = true;
    resetKeyboardTracker(keyboard_tracker_);
    header_dirty_ = true;
    apps_[current_index_]->onDisplayWake();
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
    header_focused_ = true;
    header_dirty_ = true;
}

std::size_t AppShell::findVoiceRecorderIndex() const {
    for (std::size_t i = 0; i < apps_.size(); ++i) {
        if (apps_[i]->isVoiceRecorder()) {
            return i;
        }
    }
    return apps_.size();
}

void AppShell::handleG0Short() {
    const std::size_t rec = findVoiceRecorderIndex();
    if (rec >= apps_.size()) {
        return;
    }

    // Already recording → stop & save (stay on Recorder).
    if (current_index_ == rec && apps_[rec]->voiceRecorderIsRecording()) {
        apps_[rec]->voiceRecorderHotkey();
        header_focused_ = false;
        header_dirty_ = true;
        return;
    }

    // Any other screen → jump to Recorder and start recording.
    if (current_index_ != rec) {
        switchTo(rec);
    }
    header_focused_ = false;
    header_dirty_ = true;
    apps_[rec]->voiceRecorderHotkey();
}

bool AppShell::consumeHeaderKeys(const Keyboard_Class::KeysState& status) {
    const ArrowKey arrow = pollArrowEdge(status, keyboard_tracker_);
    const bool enter_edge = status.enter && !keyboard_tracker_.enter_down;
    keyboard_tracker_.enter_down = status.enter;
    keyboard_tracker_.del_down = status.del;

    switch (arrow) {
        case ArrowKey::Left:
            switchTo(adjacentVisible(current_index_, -1));
            return true;
        case ArrowKey::Right:
            switchTo(adjacentVisible(current_index_, 1));
            return true;
        case ArrowKey::Down:
            header_focused_ = false;
            return true;
        default:
            break;
    }

    if (enter_edge) {
        header_focused_ = false;
        return true;
    }
    return false;
}

void AppShell::drawHeader() const {
    auto& display = M5Cardputer.Display;
    const int width = display.width();

    std::size_t visible[16];
    std::size_t visible_count = 0;
    std::size_t visible_current = 0;
    for (std::size_t i = 0; i < apps_.size() && visible_count < 16; ++i) {
        if (!isAppVisible(i)) {
            continue;
        }
        if (i == current_index_) {
            visible_current = visible_count;
        }
        visible[visible_count++] = i;
    }
    if (visible_count == 0) {
        visible[0] = current_index_;
        visible_count = 1;
        visible_current = 0;
    }

    const int dots_span =
        visible_count > 0 ? static_cast<int>((visible_count - 1) * kDotSpacing + kDotRadius * 2) : 0;
    const int dots_width = dots_span + kDotsRightMargin;
    const int cluster_width = kBatteryWidth + kBatteryDotGap + dots_width;
    const int battery_x = width - cluster_width;
    const int arrow_right_x = battery_x - kTitleClusterGap - 2 * kCharWidth;
    const int title_x = 4 + 2 * kCharWidth;
    const uint16_t bar = header_focused_ ? TFT_NAVY : TFT_DARKGREY;
    const uint16_t title = header_focused_ ? TFT_YELLOW : TFT_WHITE;

    display.fillRect(0, 0, width, config::kHeaderHeight, bar);
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

    for (std::size_t i = 0; i < visible_count; ++i) {
        const int dot_x = width - kDotsRightMargin - kDotRadius -
                          static_cast<int>((visible_count - 1 - i) * kDotSpacing);
        const bool current = isAppVisible(current_index_) && i == visible_current;
        const uint16_t color = current ? TFT_YELLOW : TFT_DARKGREY;
        display.fillCircle(dot_x, 11, kDotRadius, color);
        if (current) {
            display.drawCircle(dot_x, 11, kDotActiveRing, TFT_WHITE);
        }
    }
}
