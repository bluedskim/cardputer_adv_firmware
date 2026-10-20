#include "apps/timer/timer_app.h"

#include <M5Cardputer.h>

#include <algorithm>
#include <cstdio>

#include "config.h"
#include "input/keyboard_input.h"

namespace {

constexpr int kContentTop = config::kContentTop;
constexpr int kLineHeight = 16;
constexpr int kListTop = kContentTop + 4;
constexpr int kListTopWithTitle = kContentTop + 22;
constexpr int kFooterReserve = 0;

}  // namespace

TimerApp::TimerApp() { storage_.begin(); }

const char* TimerApp::name() const { return "Timer"; }

void TimerApp::onEnter() { enterList(); }

void TimerApp::onExit() { M5Cardputer.Display.fillScreen(TFT_BLACK); }

void TimerApp::onDisplayWake() { markDirty(); }

bool TimerApp::isRunning(const TimerEntry& entry) const {
    return entry.end_ms != 0 && !entry.done && entry.end_ms > millis();
}

uint32_t TimerApp::remainingMs(const TimerEntry& entry) const {
    if (!isRunning(entry)) {
        return 0;
    }
    return entry.end_ms - millis();
}

void TimerApp::formatPreset(char* buf, const std::size_t len, const uint8_t hours,
                            const uint8_t minutes, const uint8_t seconds) const {
    if (hours > 0) {
        std::snprintf(buf, len, seconds > 0 ? "%uh%02um%02us" : "%uh%02um", hours, minutes, seconds);
    } else if (minutes > 0) {
        std::snprintf(buf, len, seconds > 0 ? "%um%02us" : "%um", minutes, seconds);
    } else {
        std::snprintf(buf, len, "%us", seconds);
    }
}

void TimerApp::formatRemaining(char* buf, const std::size_t len, const uint32_t ms) const {
    const uint32_t total_sec = (ms + 999) / 1000;
    const uint32_t hours = total_sec / 3600;
    const uint32_t minutes = (total_sec % 3600) / 60;
    const uint32_t seconds = total_sec % 60;
    if (hours > 0) {
        std::snprintf(buf, len, "%u:%02u:%02u", hours, minutes, seconds);
    } else {
        std::snprintf(buf, len, "%u:%02u", minutes, seconds);
    }
}

void TimerApp::adjustEditField(const int delta) {
    // Wraps within the field: 0-99 h, 0-59 m, 0-55 s in kTimerSecondsStep steps.
    auto wrap = [delta](std::size_t& value, const int step, const int modulo) {
        value = static_cast<std::size_t>((static_cast<int>(value) + delta * step + modulo) % modulo);
    };
    if (edit_field_ == 0) {
        wrap(edit_hours_, 1, config::kTimerMaxHours + 1);
    } else if (edit_field_ == 1) {
        wrap(edit_minutes_, 1, 60);
    } else if (edit_field_ == 2) {
        edit_seconds_ -= edit_seconds_ % config::kTimerSecondsStep;
        wrap(edit_seconds_, config::kTimerSecondsStep, 60);
    }
}

void TimerApp::enterList() {
    screen_ = Screen::List;
    if (storage_.timers().empty()) {
        selected_ = 0;
        cursor_ = 0;
    } else {
        selected_ = std::min(selected_, storage_.timers().size() - 1);
        cursor_ = selected_;
    }
    scroll_ = 0;
    ensureCursorVisible();
    markDirty();
    requestHeaderFocus();
}

void TimerApp::enterEdit(const std::size_t index) {
    screen_ = Screen::Edit;
    selected_ = index;
    if (index < storage_.timers().size()) {
        const auto& entry = storage_.timers()[index];
        edit_hours_ = entry.hours;
        edit_minutes_ = entry.minutes;
        edit_seconds_ = entry.seconds;
    }
    edit_field_ = -1;
    cursor_ = 1;
    scroll_ = 0;
    markDirty();
}

int TimerApp::visibleRowCount() const {
    const int top = screen_ == Screen::Edit ? kListTopWithTitle : kListTop;
    const int available = M5Cardputer.Display.height() - kFooterReserve - top;
    return std::max(1, available / kLineHeight);
}

void TimerApp::ensureCursorVisible() {
    if (screen_ == Screen::Edit && cursor_ == 0) {
        return;
    }
    const std::size_t list_cursor =
        screen_ == Screen::Edit ? cursor_ - 1 : cursor_;
    const std::size_t visible = static_cast<std::size_t>(visibleRowCount());
    if (list_cursor < scroll_) {
        scroll_ = list_cursor;
    } else if (list_cursor >= scroll_ + visible) {
        scroll_ = list_cursor + 1 - visible;
    }
}

std::size_t TimerApp::listRowCount() const {
    if (screen_ == Screen::List) {
        return storage_.timers().size() + 1;
    }
    if (screen_ == Screen::Edit) {
        return 3;
    }
    return 0;
}

bool TimerApp::isActionRow(const std::size_t index, ListAction* out) const {
    if (screen_ == Screen::List) {
        if (index == storage_.timers().size()) {
            if (out) {
                *out = ListAction::New;
            }
            return true;
        }
        return false;
    }

    if (screen_ == Screen::Edit) {
        if (index == 1) {
            if (out) {
                *out = ListAction::Start;
            }
            return true;
        }
        if (index == 2) {
            if (out) {
                *out = ListAction::Remove;
            }
            return true;
        }
    }
    return false;
}

void TimerApp::tickTimers() {
    const uint32_t now = millis();
    bool changed = false;

    for (std::size_t i = 0; i < storage_.timers().size(); ++i) {
        auto& entry = storage_.timers()[i];
        if (entry.end_ms != 0 && !entry.done && now >= entry.end_ms) {
            entry.end_ms = 0;
            entry.done = true;
            storage_.setTimerEnd(i, 0, true);
            changed = true;
        }
    }

    const uint32_t sec = now / 1000;
    if (sec != last_tick_sec_) {
        last_tick_sec_ = sec;
        for (const auto& entry : storage_.timers()) {
            if (isRunning(entry)) {
                if (screen_ == Screen::List) {
                    markDirty();
                }
                break;
            }
        }
    }

    if (changed) {
        markDirty();
    }
}

void TimerApp::onBackgroundTick() {
    tickTimers();
}

bool TimerApp::hasActiveBackgroundWork() const {
    for (const auto& entry : storage_.timers()) {
        if (isRunning(entry)) {
            return true;
        }
    }
    return false;
}

NavRequest TimerApp::onUpdate(const Keyboard_Class::KeysState* keys, bool header_focused) {
    if (header_focused_ != header_focused) {
        header_focused_ = header_focused;
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
    if (pending_header_focus_) {
        pending_header_focus_ = false;
        return NavRequest::FocusHeader;
    }
    return req;
}

void TimerApp::startSelected() {
    if (selected_ >= storage_.timers().size()) {
        return;
    }

    const uint8_t hours = static_cast<uint8_t>(edit_hours_);
    const uint8_t minutes = static_cast<uint8_t>(edit_minutes_);
    uint8_t seconds = static_cast<uint8_t>(edit_seconds_);
    if (hours == 0 && minutes == 0 && seconds < config::kTimerMinSec) {
        seconds = config::kTimerMinSec;
    }

    storage_.updateTimer(selected_, hours, minutes, seconds);
    const uint32_t duration_ms = (static_cast<uint32_t>(hours) * 3600U +
                                  static_cast<uint32_t>(minutes) * 60U + seconds) *
                                 1000U;
    storage_.setTimerEnd(selected_, millis() + duration_ms, false);
    enterList();
}

void TimerApp::activateSelection() {
    ListAction action = ListAction::None;
    const std::size_t row =
        (screen_ == Screen::Edit && cursor_ > 0) ? cursor_ - 1 : cursor_;
    if (isActionRow(row, &action)) {
        switch (action) {
            case ListAction::New: {
                const std::size_t n = storage_.timers().size() + 1;
                char name[16];
                std::snprintf(name, sizeof(name), "Timer %u", static_cast<unsigned>(n));
                if (storage_.addTimer(name, 0, 5)) {
                    enterEdit(storage_.timers().size() - 1);
                }
                break;
            }
            case ListAction::Start:
                startSelected();
                break;
            case ListAction::Remove:
                screen_ = Screen::ConfirmDelete;
                markDirty();
                break;
            default:
                break;
        }
        return;
    }

    if (screen_ == Screen::List) {
        if (cursor_ >= storage_.timers().size()) {
            return;
        }
        selected_ = cursor_;
        auto& entry = storage_.timers()[selected_];
        if (entry.done) {
            storage_.setTimerEnd(selected_, 0, false);
            markDirty();
            return;
        }
        if (isRunning(entry)) {
            storage_.setTimerEnd(selected_, 0, false);
            markDirty();
            return;
        }
        enterEdit(selected_);
        return;
    }

    if (screen_ == Screen::Edit && cursor_ == 0) {
        return;
    }

    if (screen_ == Screen::Edit && cursor_ == 1) {
        edit_field_ = 0;
        markDirty();
    }
}

void TimerApp::goBack() {
    if (screen_ == Screen::ConfirmDelete) {
        enterEdit(selected_);
        return;
    }
    if (screen_ == Screen::Edit) {
        if (selected_ < storage_.timers().size()) {
            storage_.updateTimer(selected_, static_cast<uint8_t>(edit_hours_),
                                 static_cast<uint8_t>(edit_minutes_),
                                 static_cast<uint8_t>(edit_seconds_));
        }
        enterList();
    }
}

NavRequest TimerApp::handleInput(const Keyboard_Class::KeysState& status) {
    if (screen_ == Screen::ConfirmDelete) {
        if (status.enter) {
            if (selected_ < storage_.timers().size()) {
                storage_.removeTimer(selected_);
            }
            enterList();
            return NavRequest::None;
        }
        if (isBackKey(status) || detectArrowKey(status) == ArrowKey::Left) {
            enterEdit(selected_);
        }
        return NavRequest::None;
    }

    if (screen_ == Screen::Edit) {
        // README: `←` (on the title row) or Del/Backspace returns to the list.
        if (isBackKey(status) || (cursor_ == 0 && detectArrowKey(status) == ArrowKey::Left)) {
            goBack();
            return NavRequest::None;
        }
        if (edit_field_ >= 0) {
            switch (detectArrowKey(status)) {
                case ArrowKey::Left:
                    edit_field_ = std::max(0, edit_field_ - 1);
                    break;
                case ArrowKey::Right:
                    edit_field_ = std::min(2, edit_field_ + 1);
                    break;
                case ArrowKey::Up:
                    adjustEditField(1);
                    break;
                case ArrowKey::Down:
                    adjustEditField(-1);
                    break;
                default:
                    if (status.enter) {
                        edit_field_ = -1;
                    }
                    break;
            }
            return NavRequest::None;
        }
    }

    if (status.enter) {
        activateSelection();
        return NavRequest::None;
    }

    switch (detectArrowKey(status)) {
        case ArrowKey::Up:
            if (cursor_ == 0) {
                if (isRootScreen()) {
                    return NavRequest::FocusHeader;
                }
                return NavRequest::None;
            }
            --cursor_;
            if (screen_ == Screen::List && cursor_ < storage_.timers().size()) {
                selected_ = cursor_;
            }
            ensureCursorVisible();
            return NavRequest::None;
        case ArrowKey::Down: {
            if (screen_ == Screen::Edit) {
                if (cursor_ == 0) {
                    cursor_ = 1;
                } else if (cursor_ < 3) {
                    ++cursor_;
                }
                ensureCursorVisible();
                return NavRequest::None;
            }
            const std::size_t rows = listRowCount();
            if (rows == 0) {
                return NavRequest::None;
            }
            if (cursor_ + 1 < rows) {
                ++cursor_;
            }
            if (screen_ == Screen::List && cursor_ < storage_.timers().size()) {
                selected_ = cursor_;
            }
            ensureCursorVisible();
            return NavRequest::None;
        }
        default:
            break;
    }

    return NavRequest::None;
}

void TimerApp::drawListRow(const int y, const std::size_t index, const bool selected) const {
    auto& display = M5Cardputer.Display;
    const int width = display.width();
    const uint16_t fg = selected ? TFT_YELLOW : TFT_WHITE;
    const uint16_t bg = selected ? TFT_NAVY : TFT_BLACK;

    if (selected) {
        display.fillRect(0, y, width, kLineHeight, bg);
    }
    display.setTextColor(fg, bg);
    display.setTextSize(2);
    display.setCursor(8, y);

    ListAction action = ListAction::None;
    if (isActionRow(index, &action)) {
        const char* label = "+ New";
        if (action == ListAction::Start) {
            label = "> Start";
        } else if (action == ListAction::Remove) {
            label = "x Remove";
        }
        display.printf("%c %s", selected ? '>' : ' ', label);
        return;
    }

    if (screen_ == Screen::Edit) {
        if (index == 0) {
            const std::size_t values[] = {edit_hours_, edit_minutes_, edit_seconds_};
            const char units[] = {'h', 'm', 's'};
            display.printf("%c Time", selected ? '>' : ' ');
            for (int field = 0; field < 3; ++field) {
                const bool editing = field == edit_field_;
                display.setTextColor(fg, bg);
                display.print(' ');
                display.setTextColor(editing ? TFT_BLACK : fg, editing ? TFT_YELLOW : bg);
                display.printf("%02u%c", static_cast<unsigned>(values[field]), units[field]);
            }
        }
        return;
    }

    if (index >= storage_.timers().size()) {
        return;
    }

    const auto& entry = storage_.timers()[index];
    char detail[16];
    if (entry.done) {
        std::snprintf(detail, sizeof(detail), "DONE");
        display.setTextColor(selected ? TFT_YELLOW : TFT_ORANGE, bg);
    } else if (isRunning(entry)) {
        formatRemaining(detail, sizeof(detail), remainingMs(entry));
    } else {
        formatPreset(detail, sizeof(detail), entry.hours, entry.minutes, entry.seconds);
    }
    display.printf("%c %s  %s", selected ? '>' : ' ', entry.name.c_str(), detail);
}

void TimerApp::draw() const {
    auto& display = M5Cardputer.Display;
    display.fillRect(0, kContentTop, display.width(), display.height() - kContentTop, TFT_BLACK);

    if (screen_ == Screen::Edit && selected_ < storage_.timers().size()) {
        const bool title_selected = cursor_ == 0 && !header_focused_;
        const uint16_t fg = title_selected ? TFT_YELLOW : TFT_CYAN;
        const uint16_t bg = title_selected ? TFT_NAVY : TFT_BLACK;
        if (title_selected) {
            display.fillRect(0, kContentTop, display.width(), kListTopWithTitle - kContentTop, bg);
        }
        display.setTextSize(2);
        display.setTextColor(fg, bg);
        display.setCursor(4, kContentTop + 2);
        display.printf("%c %s", title_selected ? '>' : ' ',
                       storage_.timers()[selected_].name.c_str());
    }

    const int top = screen_ == Screen::Edit ? kListTopWithTitle : kListTop;
    int y = top;

    if (screen_ == Screen::ConfirmDelete) {
        display.setTextSize(2);
        display.setTextColor(TFT_ORANGE, TFT_BLACK);
        display.setCursor(8, y);
        if (selected_ < storage_.timers().size()) {
            display.printf("Del %s?", storage_.timers()[selected_].name.c_str());
        } else {
            display.print("Delete?");
        }
        return;
    }

    const std::size_t rows = listRowCount();
    const std::size_t visible = static_cast<std::size_t>(visibleRowCount());
    const std::size_t end = std::min(rows, scroll_ + visible);
    for (std::size_t i = scroll_; i < end; ++i) {
        const std::size_t row_cursor = screen_ == Screen::Edit ? i + 1 : i;
        drawListRow(y, i, row_cursor == cursor_ && !header_focused_);
        y += kLineHeight;
    }
}
