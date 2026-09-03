#include "apps/stopwatch/stopwatch_app.h"

#include <M5Cardputer.h>

#include <algorithm>
#include <cstdio>

#include "config.h"
#include "input/keyboard_input.h"

namespace {

constexpr int kContentTop = config::kContentTop;
constexpr int kLineHeight = 16;
constexpr int kListTop = kContentTop + 32;
constexpr int kFooterReserve = 0;
constexpr int kElapsedY = kContentTop + 18;
constexpr int kElapsedBandTop = kContentTop + 10;
constexpr int kElapsedBandHeight = 16;
constexpr int kElapsedBandWidth = 168;

}  // namespace

int historyVisibleRows() {
    const int available = M5Cardputer.Display.height() - kFooterReserve - kListTop;
    return std::max(1, available / kLineHeight);
}

StopwatchApp::StopwatchApp() { storage_.begin(); }

const char* StopwatchApp::name() const { return "Stopwatch"; }

void StopwatchApp::onEnter() {
    screen_ = Screen::Main;
    focus_ = MainRow::Name;
    last_draw_cs_ = 0;
    markDirty();
    requestChromeFocus();
}

void StopwatchApp::onExit() { M5Cardputer.Display.fillScreen(TFT_BLACK); }

bool StopwatchApp::hasActiveBackgroundWork() const { return running_; }

void StopwatchApp::formatDuration(char* buf, const std::size_t len, const uint32_t ms) const {
    const uint32_t cs = ms / 10;
    const uint32_t seconds = cs / 100;
    const uint32_t minutes = seconds / 60;
    const uint32_t hours = minutes / 60;
    const uint32_t sec = seconds % 60;
    const uint32_t min = minutes % 60;
    const uint32_t centis = cs % 100;

    std::snprintf(buf, len, "%02u:%02u:%02u.%02u", hours, min, sec, centis);
}

uint32_t StopwatchApp::currentElapsedMs() const {
    if (running_) {
        return elapsed_ms_ + (millis() - start_ms_);
    }
    return elapsed_ms_;
}

void StopwatchApp::drawElapsed() const {
    auto& display = M5Cardputer.Display;
    char buf[20];
    formatDuration(buf, sizeof(buf), currentElapsedMs());

    const int band_x = (display.width() - kElapsedBandWidth) / 2;
    display.fillRect(band_x, kElapsedBandTop, kElapsedBandWidth, kElapsedBandHeight, TFT_BLACK);
    display.setTextDatum(middle_center);
    display.setTextSize(2);
    display.setTextColor(TFT_WHITE, TFT_BLACK);
    display.drawString(buf, display.width() / 2, kElapsedY);
    display.setTextDatum(top_left);
}

void StopwatchApp::toggleRunStop() {
    if (running_) {
        elapsed_ms_ += millis() - start_ms_;
        running_ = false;
        if (elapsed_ms_ > 0) {
            storage_.addRecord(storage_.name(), elapsed_ms_);
        }
        elapsed_ms_ = 0;
    } else {
        running_ = true;
        start_ms_ = millis();
    }
    markDirty();
}

void StopwatchApp::beginTextInput() {
    screen_ = Screen::TextInput;
    input_buffer_ = storage_.name();
    markDirty();
}

void StopwatchApp::commitTextInput() {
    if (!input_buffer_.empty()) {
        storage_.setName(input_buffer_);
    }
    screen_ = Screen::Main;
    markDirty();
}

void StopwatchApp::goBack() {
    if (screen_ == Screen::TextInput || screen_ == Screen::History) {
        screen_ = Screen::Main;
        markDirty();
    }
}

bool StopwatchApp::consumeTextChar(const char key) {
    if (key >= 32 && key <= 126 && !isArrowChar(key) &&
        input_buffer_.size() < config::kMaxNameLength) {
        input_buffer_.push_back(key);
        return true;
    }
    return false;
}

NavRequest StopwatchApp::onUpdate(const Keyboard_Class::KeysState* keys, bool chrome_focused) {
    if (chrome_focused_ != chrome_focused) {
        chrome_focused_ = chrome_focused;
        markDirty();
    }

    if (screen_ == Screen::Main && running_) {
        const uint32_t cs = currentElapsedMs() / 10;
        if (cs != last_draw_cs_) {
            last_draw_cs_ = cs;
            drawElapsed();
        }
    }

    NavRequest req = NavRequest::None;
    if (keys != nullptr) {
        req = handleInput(*keys);
        markDirty();
    }

    if (dirty_) {
        draw();
        dirty_ = false;
        if (screen_ == Screen::Main && running_) {
            last_draw_cs_ = currentElapsedMs() / 10;
        }
    }
    if (pending_chrome_focus_) {
        pending_chrome_focus_ = false;
        return NavRequest::FocusChrome;
    }
    return req;
}

NavRequest StopwatchApp::handleInput(const Keyboard_Class::KeysState& status) {
    if (screen_ == Screen::TextInput) {
        if (status.enter) {
            commitTextInput();
            return NavRequest::None;
        }
        if (status.del) {
            if (!input_buffer_.empty()) {
                input_buffer_.pop_back();
            }
            return NavRequest::None;
        }
        if (detectArrowKey(status) == ArrowKey::Left) {
            goBack();
            return NavRequest::None;
        }
        for (const char key : status.word) {
            consumeTextChar(key);
        }
        return NavRequest::None;
    }

    if (screen_ == Screen::History) {
        if (status.del || detectArrowKey(status) == ArrowKey::Left) {
            goBack();
            return NavRequest::None;
        }
        switch (detectArrowKey(status)) {
            case ArrowKey::Up:
                if (history_cursor_ > 0) {
                    --history_cursor_;
                }
                break;
            case ArrowKey::Down:
                if (history_cursor_ + 1 < storage_.historyCount()) {
                    ++history_cursor_;
                }
                break;
            default:
                break;
        }
        const std::size_t visible = static_cast<std::size_t>(historyVisibleRows());
        if (history_cursor_ < history_scroll_) {
            history_scroll_ = history_cursor_;
        } else if (history_cursor_ >= history_scroll_ + visible) {
            history_scroll_ = history_cursor_ + 1 - visible;
        }
        return NavRequest::None;
    }

    if (status.enter) {
        switch (focus_) {
            case MainRow::Name:
                beginTextInput();
                break;
            case MainRow::RunStop:
                toggleRunStop();
                break;
            case MainRow::History:
                screen_ = Screen::History;
                history_cursor_ = 0;
                history_scroll_ = 0;
                markDirty();
                break;
            default:
                break;
        }
        return NavRequest::None;
    }

    switch (detectArrowKey(status)) {
        case ArrowKey::Up:
            if (focus_ == MainRow::Name && isRootScreen()) {
                return NavRequest::FocusChrome;
            }
            if (focus_ == MainRow::Name) {
                return NavRequest::None;
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

    return NavRequest::None;
}

void StopwatchApp::draw() const {
    auto& display = M5Cardputer.Display;
    display.fillRect(0, kContentTop, display.width(), display.height() - kContentTop, TFT_BLACK);

    if (screen_ == Screen::TextInput) {
        display.setTextSize(2);
        display.setTextColor(TFT_CYAN, TFT_BLACK);
        display.setCursor(4, kContentTop + 2);
        display.println("Name");
        display.setTextColor(TFT_WHITE, TFT_BLACK);
        display.setCursor(8, kListTop);
        display.print("> ");
        display.print(input_buffer_.c_str());
        display.print("_");
        return;
    }

    if (screen_ == Screen::History) {
        display.setTextSize(2);
        display.setTextColor(TFT_CYAN, TFT_BLACK);
        display.setCursor(4, kContentTop + 2);
        display.println("History");

        int y = kListTop;
        const int width = display.width();
        const std::size_t count = storage_.historyCount();
        const std::size_t visible = static_cast<std::size_t>(historyVisibleRows());
        const std::size_t end = std::min(count, history_scroll_ + visible);
        for (std::size_t i = history_scroll_; i < end; ++i) {
            const bool selected = i == history_cursor_ && !chrome_focused_;
            const uint16_t fg = selected ? TFT_YELLOW : TFT_WHITE;
            const uint16_t bg = selected ? TFT_NAVY : TFT_BLACK;
            if (selected) {
                display.fillRect(0, y, width, kLineHeight, bg);
            }
            char time_buf[20];
            formatDuration(time_buf, sizeof(time_buf), storage_.history()[i].duration_ms);
            display.setTextColor(fg, bg);
            display.setTextSize(2);
            display.setCursor(8, y);
            display.printf("%c %s  %s", selected ? '>' : ' ', storage_.history()[i].name.c_str(),
                           time_buf);
            y += kLineHeight;
        }

        if (count == 0) {
            display.setTextColor(TFT_DARKGREY, TFT_BLACK);
            display.setTextSize(1);
            display.setCursor(8, kListTop);
            display.print("(empty)");
        }
        return;
    }

    drawElapsed();

    const int width = display.width();
    int y = kListTop;
    auto drawRow = [&](MainRow row, const char* label) {
        const bool selected = focus_ == row && !chrome_focused_;
        const uint16_t fg = selected ? TFT_YELLOW : TFT_WHITE;
        const uint16_t bg = selected ? TFT_NAVY : TFT_BLACK;
        if (selected) {
            display.fillRect(0, y, width, kLineHeight, bg);
        }
        display.setTextColor(fg, bg);
        display.setTextSize(2);
        display.setCursor(8, y);
        display.printf("%c %s", selected ? '>' : ' ', label);
        y += kLineHeight;
    };

    char name_line[40];
    std::snprintf(name_line, sizeof(name_line), "Name  %s", storage_.name().c_str());
    drawRow(MainRow::Name, name_line);
    drawRow(MainRow::RunStop, running_ ? "Stop" : "Start");
    drawRow(MainRow::History, "History");
}
