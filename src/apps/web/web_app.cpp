#include "apps/web/web_app.h"

#include <M5Cardputer.h>
#include <Preferences.h>

#include <algorithm>
#include <cstdio>
#include <cstring>

#include "ble/ble_protocol.h"
#include "input/keyboard_input.h"

namespace {

constexpr int kContentTop = config::kContentTop;
constexpr int kLineHeight = 16;
constexpr int kListTop = kContentTop + 4;
constexpr int kFooterH = config::kFooterHeight;
constexpr uint32_t kConnectWaitMs = 15000;
constexpr int kResultCols = 18;

bool isBackKey(const Keyboard_Class::KeysState& status) {
    return status.del || status.backspace;
}

}  // namespace

const char* WebApp::name() const { return "Web"; }

void WebApp::setFooter(const char* msg) {
    if (msg == nullptr) {
        footer_[0] = '\0';
        return;
    }
    std::snprintf(footer_, sizeof(footer_), "%s", msg);
}

void WebApp::onEnter() {
    screen_ = Screen::List;
    clearResult();
    active_site_[0] = '\0';
    clearFooter();
    awaiting_list_ = false;
    awaiting_run_ = false;
    pending_ = Pending::None;
    cursor_ = 0;
    scroll_ = 0;
    was_ready_ = false;
    connect_failed_ = false;
    pending_content_focus_ = false;
    BleLink::instance().clearRxQueue();

#if defined(CARDPUTER_SIM)
    loadPresetCache();
    if (preset_count_ == 0) {
        std::snprintf(presets_[0], sizeof(presets_[0]), "ipify");
        std::snprintf(presets_[1], sizeof(presets_[1]), "httpbin get");
        preset_count_ = 2;
        savePresetCache();
    }
    clearFooter();
    markDirty();
    requestContentFocus();
    return;
#endif

    // Keep last downloaded names even if BLE is down.
    loadPresetCache();
    if (preset_count_ > 0) {
        requestContentFocus();
    }

    if (!BleLink::instance().isPaired()) {
        setFooter(preset_count_ > 0 ? "Offline" : "Pair in Settings");
        markDirty();
        if (preset_count_ == 0) {
            requestChromeFocus();
        }
        return;
    }

    ensureBle(Pending::List);
    markDirty();
    if (preset_count_ == 0) {
        requestChromeFocus();
    }
}

void WebApp::onExit() {
    awaiting_list_ = false;
    awaiting_run_ = false;
    pending_ = Pending::None;
    BleLink::instance().disconnect();
    M5Cardputer.Display.fillScreen(TFT_BLACK);
}

void WebApp::onDisplayWake() { markDirty(); }

bool WebApp::hasActiveBackgroundWork() const {
    return awaiting_list_ || awaiting_run_ || pending_ != Pending::None;
}

void WebApp::clearPresets() {
    preset_count_ = 0;
    for (auto& name : presets_) {
        name[0] = '\0';
    }
}

void WebApp::loadPresetCache() {
    clearPresets();
#if defined(CARDPUTER_SIM)
    // Fall through to Preferences when available in sim stubs — usually no-op empty.
#endif
    Preferences prefs;
    if (!prefs.begin(config::kWebStorageNamespace, true)) {
        return;
    }
    const uint8_t count = prefs.getUChar("count", 0);
    const uint8_t n = std::min(count, config::kHttpMaxPresets);
    for (uint8_t i = 0; i < n; ++i) {
        char key[8];
        std::snprintf(key, sizeof(key), "n%u", static_cast<unsigned>(i));
        const String name = prefs.getString(key, "");
        if (name.length() == 0) {
            continue;
        }
        std::snprintf(presets_[preset_count_], sizeof(presets_[0]), "%s", name.c_str());
        ++preset_count_;
    }
    prefs.end();
}

void WebApp::savePresetCache() const {
    Preferences prefs;
    if (!prefs.begin(config::kWebStorageNamespace, false)) {
        return;
    }
    prefs.clear();
    prefs.putUChar("count", preset_count_);
    for (uint8_t i = 0; i < preset_count_; ++i) {
        char key[8];
        std::snprintf(key, sizeof(key), "n%u", static_cast<unsigned>(i));
        prefs.putString(key, presets_[i]);
    }
    prefs.end();
}

void WebApp::clearResult() {
    result_body_[0] = '\0';
    result_len_ = 0;
    result_http_status_ = 0;
    result_err_ = 0;
    result_scroll_ = 0;
}

void WebApp::setActiveSite(const char* name) {
    if (name == nullptr || name[0] == '\0') {
        active_site_[0] = '\0';
        return;
    }
    std::snprintf(active_site_, sizeof(active_site_), "%s", name);
}

void WebApp::onConnectFailed() {
    pending_ = Pending::None;
    awaiting_list_ = false;
    awaiting_run_ = false;
    connect_failed_ = true;
    BleLink::instance().disconnect();
    screen_ = Screen::List;
    // Do not clear preset_count_ — keep last downloaded site names.
    setFooter(preset_count_ > 0 ? "No phone · Enter" : "No phone · Enter");
    markDirty();
}

void WebApp::onListFailed(const char* footer_msg) {
    awaiting_list_ = false;
    pending_ = Pending::None;
    screen_ = Screen::List;
    setFooter(footer_msg);
    markDirty();
}

bool WebApp::ensureBle(Pending pending) {
    auto& link = BleLink::instance();
    if (!link.isPaired()) {
        pending_ = Pending::None;
        setFooter("Pair in Settings");
        screen_ = Screen::List;
        markDirty();
        return false;
    }

    pending_ = pending;
    connect_failed_ = false;

    // Settings may already show Connected — request list immediately on GATT up.
    if (link.isLinkUp()) {
        link.startConnect();  // refresh accept_peers_ / Hello if needed
        flushPending();
        return true;
    }

    connect_started_ms_ = millis();
    setFooter("Connecting…");
    if (pending == Pending::Run) {
        if (pending_run_index_ < preset_count_) {
            setActiveSite(presets_[pending_run_index_]);
        }
        screen_ = Screen::Running;
        awaiting_run_ = true;
    } else {
        screen_ = Screen::List;
    }
    if (!link.startConnect()) {
        onConnectFailed();
        setFooter("BLE start fail");
        return false;
    }
    markDirty();
    return false;
}

void WebApp::flushPending() {
    const Pending action = pending_;
    pending_ = Pending::None;
    switch (action) {
        case Pending::List:
            sendListReq();
            break;
        case Pending::Run:
            sendRunReq(pending_run_index_);
            break;
        case Pending::None:
            break;
    }
}

void WebApp::sendListReq() {
    awaiting_list_ = true;
    list_started_ms_ = millis();
    screen_ = Screen::List;
    setFooter("Loading…");
    list_req_id_ = BleLink::instance().allocReqId();
    const bool sent =
        BleLink::instance().send(ble_protocol::Type::HttpListReq, list_req_id_);
    if (!sent) {
        onListFailed(preset_count_ > 0 ? "Cached · Enter" : "Send fail · Enter");
    }
    markDirty();
}

void WebApp::sendRunReq(uint8_t index) {
    clearResult();
    awaiting_run_ = true;
    screen_ = Screen::Running;
    cursor_ = index;
    if (index < preset_count_) {
        setActiveSite(presets_[index]);
    }
    setFooter("Running…");

#if defined(CARDPUTER_SIM)
    awaiting_run_ = false;
    screen_ = Screen::Result;
    result_http_status_ = 200;
    result_err_ = 0;
    static const char kMockBody[] =
        "{\"ok\":true,\"mock\":1}\nsim body line2\nsim body line3";
    result_len_ = static_cast<uint16_t>(std::strlen(kMockBody));
    std::memcpy(result_body_, kMockBody, result_len_ + 1);
    result_scroll_ = 0;
    setFooter("HTTP 200");
    requestContentFocus();
    markDirty();
    return;
#endif

    run_req_id_ = BleLink::instance().allocReqId();
    BleLink::instance().send(ble_protocol::Type::HttpRunReq, run_req_id_, &index, 1);
    markDirty();
}

int WebApp::listVisibleRows() const {
    const int h = M5Cardputer.Display.height() - kListTop - kFooterH;
    return std::max(1, h / kLineHeight);
}

void WebApp::ensureCursorVisible() {
    const int vis = listVisibleRows();
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
                // New list size from phone — replace in place; keep cache if refresh aborts.
                if (preset_count_ != total) {
                    clearPresets();
                    preset_count_ = total;
                }
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
                screen_ = Screen::List;
                if (preset_count_ == 0) {
                    // Phone sent empty list — restore last good download if any.
                    loadPresetCache();
                    setFooter(preset_count_ > 0 ? "Cached" : "No presets");
                } else {
                    savePresetCache();
                    clearFooter();
                    cursor_ = std::min(cursor_, static_cast<uint8_t>(preset_count_ - 1));
                    ensureCursorVisible();
                    requestContentFocus();
                }
                markDirty();
                break;
            case ble_protocol::Type::HttpRunMeta: {
                if (frame.len >= 4) {
                    result_http_status_ =
                        static_cast<uint16_t>(frame.payload[0] | (frame.payload[1] << 8));
                    result_err_ = frame.payload[2];
                    if (result_err_ != 0) {
                        std::snprintf(footer_, sizeof(footer_), "err %u",
                                      static_cast<unsigned>(result_err_));
                    } else {
                        std::snprintf(footer_, sizeof(footer_), "HTTP %u",
                                      static_cast<unsigned>(result_http_status_));
                    }
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
                if (footer_[0] == '\0') {
                    setFooter("Done");
                }
                requestContentFocus();
                markDirty();
                break;
            case ble_protocol::Type::Error:
                awaiting_list_ = false;
                awaiting_run_ = false;
                pending_ = Pending::None;
                screen_ = (active_site_[0] != '\0' && preset_count_ > 0) ? Screen::List
                                                                         : Screen::List;
                if (frame.len >= 1) {
                    const uint8_t code = frame.payload[0];
                    if (code == static_cast<uint8_t>(ble_protocol::ErrorCode::NoPresets)) {
                        setFooter("No presets");
                    } else if (code == static_cast<uint8_t>(ble_protocol::ErrorCode::HttpTimeout)) {
                        setFooter("HTTP timeout");
                    } else if (code == static_cast<uint8_t>(ble_protocol::ErrorCode::HttpFailed)) {
                        setFooter("HTTP failed");
                    } else {
                        std::snprintf(footer_, sizeof(footer_), "Err %u",
                                      static_cast<unsigned>(code));
                    }
                } else {
                    setFooter("Error");
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
    const bool ready = link.isReady();
    if (ready && !was_ready_) {
        connect_failed_ = false;
        if (pending_ != Pending::None) {
            flushPending();
        }
    }
    if (!ready && was_ready_ && link.isPaired()) {
        if (pending_ != Pending::None) {
            ensureBle(pending_);
        } else if (awaiting_list_) {
            ensureBle(Pending::List);
        } else if (awaiting_run_) {
            pending_run_index_ = cursor_;
            ensureBle(Pending::Run);
        }
    }
    was_ready_ = ready;

    if (pending_ != Pending::None && link.isPaired() && !ready && !connect_failed_) {
        if (millis() - connect_started_ms_ > kConnectWaitMs) {
            onConnectFailed();
        }
    }

    // Connected but phone never answered HttpListReq — keep cached names.
    if (awaiting_list_ && !connect_failed_ &&
        millis() - list_started_ms_ > kConnectWaitMs) {
        onListFailed(preset_count_ > 0 ? "Cached · Enter" : "List timeout · Enter");
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
    if (pending_content_focus_) {
        pending_content_focus_ = false;
        pending_chrome_focus_ = false;
        return NavRequest::FocusContent;
    }
    if (pending_chrome_focus_) {
        pending_chrome_focus_ = false;
        return NavRequest::FocusChrome;
    }
    return req;
}

NavRequest WebApp::handleInput(const Keyboard_Class::KeysState& status) {
    const ArrowKey arrow = detectArrowKey(status);

    if (isBackKey(status) || arrow == ArrowKey::Left) {
        if (screen_ == Screen::Result || screen_ == Screen::Running) {
            screen_ = Screen::List;
            awaiting_run_ = false;
            if (!connect_failed_ && footer_[0] != '\0' &&
                std::strncmp(footer_, "HTTP", 4) == 0) {
                clearFooter();
            }
            requestContentFocus();
            return NavRequest::None;
        }
        return NavRequest::FocusChrome;
    }

    switch (arrow) {
        case ArrowKey::Up:
            if (screen_ == Screen::List) {
                if (cursor_ == 0 || preset_count_ == 0) {
                    return NavRequest::FocusChrome;
                }
                --cursor_;
                ensureCursorVisible();
                return NavRequest::None;
            }
            if (screen_ == Screen::Result) {
                if (result_scroll_ == 0) {
                    return NavRequest::FocusChrome;
                }
                --result_scroll_;
                return NavRequest::None;
            }
            if (screen_ == Screen::Running) {
                return NavRequest::FocusChrome;
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
            // Run (reconnects on demand). Cached names stay visible offline.
            pending_run_index_ = cursor_;
            ensureBle(Pending::Run);
        } else if (screen_ == Screen::List) {
            connect_failed_ = false;
            ensureBle(Pending::List);
        }
    }
    return NavRequest::None;
}

void WebApp::draw() const {
    auto& display = M5Cardputer.Display;
    display.fillRect(0, kContentTop, display.width(), display.height() - kContentTop, TFT_BLACK);
    switch (screen_) {
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
    drawFooter();
}

void WebApp::drawFooter() const {
    if (footer_[0] == '\0') {
        return;
    }
    auto& display = M5Cardputer.Display;
    const int y = display.height() - kFooterH;
    display.fillRect(0, y, display.width(), kFooterH, TFT_BLACK);
    display.setTextSize(1);
    display.setTextColor(TFT_DARKGREY, TFT_BLACK);
    display.setCursor(8, y + 4);
    display.print(footer_);
}

void WebApp::drawList() const {
    auto& display = M5Cardputer.Display;
    const int width = display.width();
    const int vis = listVisibleRows();
    int y = kListTop;

    if (preset_count_ == 0) {
        // Placeholder only — not a link-status string in the site slot.
        display.setTextSize(2);
        display.setTextColor(TFT_DARKGREY, TFT_BLACK);
        display.setCursor(8, y);
        display.print("(no sites)");
        return;
    }

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
    display.setTextColor(TFT_YELLOW, TFT_BLACK);
    display.setCursor(8, kListTop);
    // Title slot: site name only.
    display.print(active_site_[0] != '\0' ? active_site_ : "(site)");
}

void WebApp::drawResult() const {
    auto& display = M5Cardputer.Display;
    display.setTextSize(2);

    // Row 0: site name only (never HTTP/timeout text).
    display.setTextColor(chrome_focused_ ? TFT_WHITE : TFT_YELLOW, TFT_BLACK);
    display.setCursor(8, kListTop);
    display.print(active_site_[0] != '\0' ? active_site_ : "(site)");

    display.setTextColor(TFT_WHITE, TFT_BLACK);
    const int body_rows = std::max(1, listVisibleRows() - 1);
    uint16_t line = 0;
    uint16_t drawn = 0;
    size_t i = 0;
    const size_t n = result_len_;
    while (i < n && drawn < static_cast<uint16_t>(body_rows)) {
        char row[kResultCols + 1];
        size_t col = 0;
        while (i < n && col < static_cast<size_t>(kResultCols)) {
            const char ch = result_body_[i++];
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
