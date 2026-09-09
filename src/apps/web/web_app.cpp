#include "apps/web/web_app.h"

#include <M5Cardputer.h>

#include <algorithm>
#include <cstdio>
#include <cstring>

#include "ble/ble_protocol.h"
#include "input/keyboard_input.h"

namespace {

constexpr int kContentTop = config::kContentTop;
constexpr int kLineHeight = 16;
constexpr int kListTop = kContentTop + 4;
constexpr uint32_t kConnectWaitMs = 15000;
constexpr int kResultCols = 18;

bool isBackKey(const Keyboard_Class::KeysState& status) {
    return status.del || status.backspace;
}

}  // namespace

const char* WebApp::name() const { return "Web"; }

void WebApp::onEnter() {
    screen_ = Screen::Status;
    clearPresets();
    clearResult();
    awaiting_list_ = false;
    awaiting_run_ = false;
    list_requested_ = false;
    cursor_ = 0;
    scroll_ = 0;
    connect_started_ms_ = millis();
    was_link_up_ = false;
    BleLink::instance().clearRxQueue();

    auto& link = BleLink::instance();
    if (!link.isPaired()) {
        std::snprintf(status_line_, sizeof(status_line_), "Pair in Settings");
    } else if (link.isLinkUp()) {
        std::snprintf(status_line_, sizeof(status_line_), "Loading…");
        requestList();
    } else {
        std::snprintf(status_line_, sizeof(status_line_), "Connecting…");
        link.startConnect();
    }
    markDirty();
    requestChromeFocus();
}

void WebApp::onExit() {
    awaiting_list_ = false;
    awaiting_run_ = false;
    list_requested_ = false;
    BleLink::instance().disconnect();
    M5Cardputer.Display.fillScreen(TFT_BLACK);
}

void WebApp::onDisplayWake() { markDirty(); }

bool WebApp::hasActiveBackgroundWork() const {
    return awaiting_list_ || awaiting_run_ ||
           (screen_ == Screen::Status && BleLink::instance().isPaired() &&
            !BleLink::instance().isLinkUp() &&
            std::strcmp(status_line_, "Pair in Settings") != 0 &&
            std::strcmp(status_line_, "Connect timeout") != 0);
}

void WebApp::clearPresets() {
    preset_count_ = 0;
    for (auto& name : presets_) {
        name[0] = '\0';
    }
}

void WebApp::clearResult() {
    result_body_[0] = '\0';
    result_len_ = 0;
    result_http_status_ = 0;
    result_err_ = 0;
    result_scroll_ = 0;
}

void WebApp::requestList() {
    if (!BleLink::instance().isLinkUp()) {
        return;
    }
    clearPresets();
    awaiting_list_ = true;
    list_requested_ = true;
    screen_ = Screen::Status;
    std::snprintf(status_line_, sizeof(status_line_), "Loading…");
    list_req_id_ = BleLink::instance().allocReqId();
    BleLink::instance().send(ble_protocol::Type::HttpListReq, list_req_id_);
    markDirty();
}

void WebApp::runSelected() {
    if (cursor_ >= preset_count_ || !BleLink::instance().isLinkUp()) {
        return;
    }
    clearResult();
    awaiting_run_ = true;
    screen_ = Screen::Running;
    run_req_id_ = BleLink::instance().allocReqId();
    const uint8_t idx = cursor_;
    BleLink::instance().send(ble_protocol::Type::HttpRunReq, run_req_id_, &idx, 1);
    markDirty();
}

int WebApp::visibleRows() const {
    const int h = M5Cardputer.Display.height() - kListTop;
    return std::max(1, h / kLineHeight);
}

void WebApp::ensureCursorVisible() {
    const int vis = visibleRows();
    if (cursor_ < scroll_) {
        scroll_ = cursor_;
    } else if (cursor_ >= scroll_ + vis) {
        scroll_ = static_cast<uint8_t>(cursor_ - vis + 1);
    }
}

void WebApp::processBleFrames() {
    BleRxFrame frame;
    while (BleLink::instance().takeRxFrame(&frame)) {
        switch (frame.type) {
            case ble_protocol::Type::HttpListItem: {
                if (frame.len < 2) {
                    break;
                }
                const uint8_t index = frame.payload[0];
                const uint8_t total = frame.payload[1];
                if (total > config::kHttpMaxPresets || index >= config::kHttpMaxPresets) {
                    break;
                }
                preset_count_ = total;
                const uint16_t name_len = static_cast<uint16_t>(frame.len - 2);
                const uint16_t copy =
                    std::min(name_len, static_cast<uint16_t>(config::kHttpNameMax));
                std::memcpy(presets_[index], frame.payload + 2, copy);
                presets_[index][copy] = '\0';
                markDirty();
                break;
            }
            case ble_protocol::Type::HttpListEnd:
                awaiting_list_ = false;
                if (preset_count_ == 0) {
                    screen_ = Screen::Status;
                    std::snprintf(status_line_, sizeof(status_line_), "No presets");
                } else {
                    screen_ = Screen::List;
                    cursor_ = 0;
                    scroll_ = 0;
                }
                markDirty();
                break;
            case ble_protocol::Type::HttpRunMeta: {
                if (frame.len >= 4) {
                    result_http_status_ =
                        static_cast<uint16_t>(frame.payload[0] | (frame.payload[1] << 8));
                    result_err_ = frame.payload[2];
                }
                markDirty();
                break;
            }
            case ble_protocol::Type::HttpRunChunk: {
                if (frame.len < 1) {
                    break;
                }
                const uint16_t data_len = static_cast<uint16_t>(frame.len - 1);
                if (result_len_ + data_len > config::kHttpBodyMax) {
                    break;
                }
                std::memcpy(result_body_ + result_len_, frame.payload + 1, data_len);
                result_len_ = static_cast<uint16_t>(result_len_ + data_len);
                result_body_[result_len_] = '\0';
                markDirty();
                break;
            }
            case ble_protocol::Type::HttpRunEnd:
                awaiting_run_ = false;
                screen_ = Screen::Result;
                result_scroll_ = 0;
                markDirty();
                break;
            case ble_protocol::Type::Error:
                awaiting_list_ = false;
                awaiting_run_ = false;
                screen_ = Screen::Status;
                if (frame.len >= 1) {
                    const uint8_t code = frame.payload[0];
                    if (code == static_cast<uint8_t>(ble_protocol::ErrorCode::NoPresets)) {
                        std::snprintf(status_line_, sizeof(status_line_), "No presets");
                    } else if (code == static_cast<uint8_t>(ble_protocol::ErrorCode::HttpTimeout)) {
                        std::snprintf(status_line_, sizeof(status_line_), "HTTP timeout");
                    } else if (code == static_cast<uint8_t>(ble_protocol::ErrorCode::HttpFailed)) {
                        std::snprintf(status_line_, sizeof(status_line_), "HTTP failed");
                    } else {
                        std::snprintf(status_line_, sizeof(status_line_), "Err %u",
                                      static_cast<unsigned>(code));
                    }
                } else {
                    std::snprintf(status_line_, sizeof(status_line_), "Error");
                }
                markDirty();
                break;
            default:
                break;
        }
    }
}

NavRequest WebApp::onUpdate(const Keyboard_Class::KeysState* keys, bool chrome_focused) {
    if (chrome_focused_ != chrome_focused) {
        chrome_focused_ = chrome_focused;
        markDirty();
    }

    processBleFrames();

    auto& link = BleLink::instance();
    const bool up = link.isLinkUp();
    if (up && !was_link_up_ && link.isPaired() && !list_requested_ &&
        screen_ == Screen::Status && std::strcmp(status_line_, "Pair in Settings") != 0) {
        requestList();
    }
    was_link_up_ = up;

    if (screen_ == Screen::Status && link.isPaired() && !up && !awaiting_list_ &&
        std::strcmp(status_line_, "Pair in Settings") != 0 &&
        std::strcmp(status_line_, "No presets") != 0 &&
        std::strncmp(status_line_, "Err", 3) != 0 &&
        std::strcmp(status_line_, "HTTP timeout") != 0 &&
        std::strcmp(status_line_, "HTTP failed") != 0) {
        if (millis() - connect_started_ms_ > kConnectWaitMs) {
            if (std::strcmp(status_line_, "Connect timeout") != 0) {
                std::snprintf(status_line_, sizeof(status_line_), "Connect timeout");
                markDirty();
            }
        }
    }

    NavRequest req = NavRequest::None;
    if (keys != nullptr && !chrome_focused_) {
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

NavRequest WebApp::handleInput(const Keyboard_Class::KeysState& status) {
    if (isBackKey(status) || detectArrowKey(status) == ArrowKey::Left) {
        if (screen_ == Screen::Result || screen_ == Screen::Running) {
            screen_ = Screen::List;
            awaiting_run_ = false;
            return NavRequest::None;
        }
        return NavRequest::FocusChrome;
    }

    switch (detectArrowKey(status)) {
        case ArrowKey::Up:
            if (screen_ == Screen::List) {
                if (cursor_ == 0) {
                    return NavRequest::FocusChrome;
                }
                --cursor_;
                ensureCursorVisible();
            } else if (screen_ == Screen::Result && result_scroll_ > 0) {
                --result_scroll_;
            }
            return NavRequest::None;
        case ArrowKey::Down:
            if (screen_ == Screen::List && preset_count_ > 0 && cursor_ + 1 < preset_count_) {
                ++cursor_;
                ensureCursorVisible();
            } else if (screen_ == Screen::Result) {
                ++result_scroll_;
            }
            return NavRequest::None;
        default:
            break;
    }

    if (status.enter) {
        if (screen_ == Screen::List && preset_count_ > 0) {
            runSelected();
        } else if (screen_ == Screen::Status &&
                   (std::strcmp(status_line_, "Connect timeout") == 0 ||
                    std::strcmp(status_line_, "No presets") == 0 ||
                    std::strcmp(status_line_, "HTTP timeout") == 0 ||
                    std::strcmp(status_line_, "HTTP failed") == 0 ||
                    std::strncmp(status_line_, "Err", 3) == 0)) {
            list_requested_ = false;
            clearPresets();
            connect_started_ms_ = millis();
            std::snprintf(status_line_, sizeof(status_line_), "Connecting…");
            if (BleLink::instance().isPaired()) {
                if (BleLink::instance().isLinkUp()) {
                    requestList();
                } else {
                    BleLink::instance().startConnect();
                }
            }
        }
    }
    return NavRequest::None;
}

void WebApp::draw() const {
    auto& display = M5Cardputer.Display;
    display.fillRect(0, kContentTop, display.width(), display.height() - kContentTop, TFT_BLACK);
    switch (screen_) {
        case Screen::Status:
            drawStatus();
            break;
        case Screen::List:
            drawList();
            break;
        case Screen::Running:
            drawRunning();
            break;
        case Screen::Result:
            drawResult();
            break;
    }
}

void WebApp::drawStatus() const {
    auto& display = M5Cardputer.Display;
    display.setTextSize(2);
    display.setTextColor(TFT_WHITE, TFT_BLACK);
    display.setCursor(8, kListTop);
    display.print(status_line_);
    display.setCursor(8, kListTop + kLineHeight);
    display.print(BleLink::instance().statusSummary());
}

void WebApp::drawList() const {
    auto& display = M5Cardputer.Display;
    const int width = display.width();
    const int vis = visibleRows();
    int y = kListTop;
    for (int i = 0; i < vis; ++i) {
        const uint8_t idx = static_cast<uint8_t>(scroll_ + i);
        if (idx >= preset_count_) {
            break;
        }
        const bool selected = idx == cursor_ && !chrome_focused_;
        const uint16_t fg = selected ? TFT_YELLOW : TFT_WHITE;
        const uint16_t bg = selected ? TFT_NAVY : TFT_BLACK;
        if (selected) {
            display.fillRect(0, y, width, kLineHeight, bg);
        }
        display.setTextColor(fg, bg);
        display.setTextSize(2);
        display.setCursor(8, y);
        display.printf("%c %s", selected ? '>' : ' ', presets_[idx]);
        y += kLineHeight;
    }
}

void WebApp::drawRunning() const {
    auto& display = M5Cardputer.Display;
    display.setTextSize(2);
    display.setTextColor(TFT_WHITE, TFT_BLACK);
    display.setCursor(8, kListTop);
    display.print("Running…");
    if (cursor_ < preset_count_) {
        display.setCursor(8, kListTop + kLineHeight);
        display.print(presets_[cursor_]);
    }
}

void WebApp::drawResult() const {
    auto& display = M5Cardputer.Display;
    display.setTextSize(2);
    display.setTextColor(TFT_YELLOW, TFT_BLACK);
    display.setCursor(8, kListTop);
    if (result_err_ != 0) {
        display.printf("Err %u", static_cast<unsigned>(result_err_));
    } else {
        display.printf("HTTP %u", static_cast<unsigned>(result_http_status_));
    }

    display.setTextColor(TFT_WHITE, TFT_BLACK);
    const char* body = result_body_;
    const int body_rows = visibleRows() - 1;
    uint16_t line = 0;
    uint16_t drawn = 0;
    size_t i = 0;
    const size_t n = result_len_;
    while (i < n && drawn < static_cast<uint16_t>(body_rows)) {
        char row[kResultCols + 1];
        size_t col = 0;
        while (i < n && col < static_cast<size_t>(kResultCols)) {
            const char ch = body[i++];
            if (ch == '\n') {
                break;
            }
            if (ch == '\r') {
                continue;
            }
            row[col++] = ch;
        }
        row[col] = '\0';
        if (line >= result_scroll_) {
            display.setCursor(8, kListTop + kLineHeight * (1 + drawn));
            display.print(row);
            ++drawn;
        }
        ++line;
        if (i >= n) {
            break;
        }
    }
}
