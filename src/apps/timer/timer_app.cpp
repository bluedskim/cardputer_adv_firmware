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

// Full-time whistle: two short pips in the last two seconds, then a long one at zero.
constexpr uint32_t kBeepHz = 2000;
constexpr uint32_t kPipMs = 150;
constexpr uint32_t kFinalMs = 1000;
constexpr uint8_t kBeepVolume = 200;  // 255 clips on the Cardputer speaker

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
                            const uint8_t minutes) const {
    if (hours > 0) {
        std::snprintf(buf, len, "%uh %02um", hours, minutes);
    } else {
        std::snprintf(buf, len, "%um", minutes);
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
    }
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
        return 4;
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
        if (index == 2) {
            if (out) {
                *out = ListAction::Start;
            }
            return true;
        }
        if (index == 3) {
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
    const uint32_t prev = last_cue_ms_;
    last_cue_ms_ = now;
    auto crossed = [&](const uint32_t t) { return prev < t && now >= t; };
    bool changed = false;

    for (std::size_t i = 0; i < storage_.timers().size(); ++i) {
        auto& entry = storage_.timers()[i];
        if (entry.end_ms != 0 && !entry.done &&
            (crossed(entry.end_ms - 2000) || crossed(entry.end_ms - 1000))) {
            beep(kPipMs);
        }
        if (entry.end_ms != 0 && !entry.done && now >= entry.end_ms) {
            entry.end_ms = 0;
            entry.done = true;
            storage_.setTimerEnd(i, 0, true);
            changed = true;
            beep(kFinalMs);
        }
    }
    releaseSpeakerWhenDone(now);

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
    if (owns_speaker_) {
        return true;  // keep ticking (no light sleep) until the last beep has played
    }
    for (const auto& entry : storage_.timers()) {
        if (isRunning(entry)) {
            return true;
        }
    }
    return false;
}

void TimerApp::beep(const uint32_t duration_ms) {
#ifndef CARDPUTER_SIM
    if (M5Cardputer.Mic.isRunning()) {
        return;  // the recorder owns the codec
    }
    if (!M5Cardputer.Speaker.isRunning()) {
        if (!M5Cardputer.Speaker.begin()) {
            return;
        }
        M5Cardputer.Speaker.setVolume(kBeepVolume);
        owns_speaker_ = true;
    }
    M5Cardputer.Speaker.tone(kBeepHz, duration_ms);
    speaker_release_ms_ = millis() + duration_ms + 100;
#else
    (void)duration_ms;
#endif
}

void TimerApp::releaseSpeakerWhenDone(const uint32_t now) {
#ifndef CARDPUTER_SIM
    if (owns_speaker_ && now >= speaker_release_ms_) {
        M5Cardputer.Speaker.end();
        owns_speaker_ = false;
    }
#else
    (void)now;
#endif
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

    uint8_t hours = static_cast<uint8_t>(edit_hours_);
    uint8_t minutes = static_cast<uint8_t>(edit_minutes_);
    if (hours == 0 && minutes == 0) {
        minutes = 1;
    }

    storage_.updateTimer(selected_, hours, minutes);
    const uint32_t duration_ms =
        (static_cast<uint32_t>(hours) * 3600U + static_cast<uint32_t>(minutes) * 60U) * 1000U;
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

    if (screen_ == Screen::Edit) {
        if (cursor_ == 1) {
            edit_hours_ = (edit_hours_ + 1) % (config::kTimerMaxHours + 1);
        } else if (cursor_ == 2) {
            edit_minutes_ = (edit_minutes_ + 1) % 60;
        }
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
                                 static_cast<uint8_t>(edit_minutes_));
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
        if (cursor_ == 1 || cursor_ == 2) {
            switch (detectArrowKey(status)) {
                case ArrowKey::Left:
                    if (cursor_ == 1) {
                        edit_hours_ = edit_hours_ == 0 ? config::kTimerMaxHours : edit_hours_ - 1;
                    } else {
                        edit_minutes_ = edit_minutes_ == 0 ? 59 : edit_minutes_ - 1;
                    }
                    return NavRequest::None;
                case ArrowKey::Right:
                    if (cursor_ == 1) {
                        edit_hours_ = (edit_hours_ + 1) % (config::kTimerMaxHours + 1);
                    } else {
                        edit_minutes_ = (edit_minutes_ + 1) % 60;
                    }
                    return NavRequest::None;
                default:
                    break;
            }
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
                } else if (cursor_ < 4) {
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
            display.printf("%c Hours   %02u", selected ? '>' : ' ', static_cast<unsigned>(edit_hours_));
        } else if (index == 1) {
            display.printf("%c Minutes %02u", selected ? '>' : ' ',
                           static_cast<unsigned>(edit_minutes_));
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
        formatPreset(detail, sizeof(detail), entry.hours, entry.minutes);
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
