#include "apps/find/find_app.h"

#include <M5Cardputer.h>
#include <Preferences.h>

#include <cstdio>
#include <cstring>

#include "config.h"
#include "input/keyboard_input.h"

namespace {

constexpr int kContentTop = config::kContentTop;
constexpr int kLineHeight = 18;
constexpr uint32_t kConnectTimeoutMs = 20000;

}  // namespace

FindApp::FindApp() { loadSound(); }

const char* FindApp::name() const { return "Find"; }

void FindApp::loadSound() {
    Preferences prefs;
    if (!prefs.begin(config::kFindStorageNamespace, true)) {
        sound_on_ = false;
        return;
    }
    sound_on_ = prefs.getUChar("sound", 0) != 0;
    prefs.end();
}

void FindApp::saveSound() const {
    Preferences prefs;
    if (!prefs.begin(config::kFindStorageNamespace, false)) {
        return;
    }
    prefs.putUChar("sound", sound_on_ ? 1 : 0);
    prefs.end();
}

void FindApp::setFooter(const char* msg) {
    if (msg == nullptr) {
        footer_[0] = '\0';
        return;
    }
    std::snprintf(footer_, sizeof(footer_), "%s", msg);
}

void FindApp::onEnter() {
    focus_ = Row::Action;
    finding_ = false;
    awaiting_ = false;
    pending_ = Pending::None;
    BleLink::instance().clearRxQueue();
    setFooter(BleLink::instance().isPaired() ? "Enter to find" : "Pair phone in Settings");
    markDirty();
    requestHeaderFocus();
}

void FindApp::onExit() {
    if (finding_ || awaiting_) {
        sendStop();
    }
    finding_ = false;
    awaiting_ = false;
    pending_ = Pending::None;
    BleLink::instance().disconnect();
    M5Cardputer.Display.fillScreen(TFT_BLACK);
}

void FindApp::onDisplayWake() { markDirty(); }

bool FindApp::hasActiveBackgroundWork() const {
    return finding_ || awaiting_ || pending_ != Pending::None;
}

void FindApp::toggleSound() {
    if (finding_) {
        return;
    }
    sound_on_ = !sound_on_;
    saveSound();
    markDirty();
}

bool FindApp::ensureBle(const Pending pending) {
    auto& link = BleLink::instance();
    if (!link.isPaired()) {
        pending_ = Pending::None;
        setFooter("Pair phone in Settings");
        markDirty();
        return false;
    }
    pending_ = pending;
    if (link.isLinkUp() && link.isReady()) {
        flushPending();
        return true;
    }
    connect_started_ms_ = millis();
    setFooter("Connecting...");
    markDirty();
    if (!link.isLinkUp() && !link.startConnect()) {
        pending_ = Pending::None;
        setFooter("BLE start failed");
        markDirty();
        return false;
    }
    if (link.isLinkUp()) {
        link.startConnect();
    }
    return false;
}

void FindApp::flushPending() {
    const Pending action = pending_;
    pending_ = Pending::None;
    if (action == Pending::Start) {
        sendStart();
    } else if (action == Pending::Stop) {
        sendStop();
    }
}

void FindApp::sendStart() {
    auto& link = BleLink::instance();
    req_id_ = link.allocReqId();
    const uint8_t flags = sound_on_ ? ble_protocol::kFindFlagSound : 0;
    awaiting_ = true;
    finding_ = false;
    setFooter("Starting...");
    if (!link.send(ble_protocol::Type::FindStart, req_id_, &flags, 1)) {
        awaiting_ = false;
        setFooter("Send failed");
    }
    markDirty();
}

void FindApp::sendStop() {
    auto& link = BleLink::instance();
    if (!link.isReady()) {
        stopLocal("Link lost");
        return;
    }
    req_id_ = link.allocReqId();
    awaiting_ = true;
    link.send(ble_protocol::Type::FindStop, req_id_);
}

void FindApp::stopLocal(const char* footer) {
    finding_ = false;
    awaiting_ = false;
    pending_ = Pending::None;
    setFooter(footer);
    markDirty();
}

void FindApp::onBackgroundTick() {
    // Display sleep skips onUpdate. Hold the idle timer so a 60 s Find alert
    // is not cut off by maybeIdleDisconnect. Drain only while a Find session
    // is active: onExit clears these flags before another app runs.
    if (finding_ || awaiting_ || pending_ != Pending::None) {
        BleLink::instance().holdIdleTimer();
        processBleFrames();
    }
    if (pending_ != Pending::None) {
        auto& link = BleLink::instance();
        if (link.isReady()) {
            flushPending();
        } else if (millis() - connect_started_ms_ > kConnectTimeoutMs) {
            pending_ = Pending::None;
            setFooter("Connect timeout");
            markDirty();
        }
    } else if ((finding_ || awaiting_) && !BleLink::instance().isLinkUp()) {
        stopLocal("Link lost");
    }
}

void FindApp::processBleFrames() {
    BleRxFrame frame;
    while (BleLink::instance().takeRxFrame(&frame)) {
        if (frame.req_id != req_id_) {
            continue;
        }
        if (frame.type == ble_protocol::Type::Error) {
            if (!awaiting_ && !finding_) {
                continue;
            }
            stopLocal("Find failed");
            continue;
        }
        if (frame.type == ble_protocol::Type::FindStarted) {
            const uint8_t ok = frame.len > 0 ? frame.payload[0] : 0;
            if (ok != 0) {
                stopLocal("Find failed");
                continue;
            }
            awaiting_ = false;
            finding_ = true;
            setFooter(sound_on_ ? "Finding + sound" : "Finding");
            markDirty();
        } else if (frame.type == ble_protocol::Type::FindStopped) {
            const uint8_t reason = frame.len > 0 ? frame.payload[0] : 0;
            const char* msg = "Stopped";
            if (reason == 1) {
                msg = "Timed out";
            } else if (reason == 2) {
                msg = "Find failed";
            }
            stopLocal(msg);
        }
    }
}

NavRequest FindApp::onUpdate(const Keyboard_Class::KeysState* keys, const bool header_focused) {
    if (header_focused_ != header_focused) {
        header_focused_ = header_focused;
        markDirty();
    }

    processBleFrames();

    NavRequest req = NavRequest::None;
    if (keys != nullptr && !header_focused) {
        req = handleInput(*keys);
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

NavRequest FindApp::handleInput(const Keyboard_Class::KeysState& status) {
    const ArrowKey arrow = detectArrowKey(status);

    if (arrow == ArrowKey::Up) {
        if (focus_ == Row::Sound) {
            return NavRequest::FocusHeader;
        }
        focus_ = Row::Sound;
        markDirty();
        return NavRequest::None;
    }
    if (arrow == ArrowKey::Down) {
        if (focus_ == Row::Sound) {
            focus_ = Row::Action;
            markDirty();
        }
        return NavRequest::None;
    }
    if (arrow == ArrowKey::Left || arrow == ArrowKey::Right) {
        if (focus_ == Row::Sound) {
            toggleSound();
        }
        return NavRequest::None;
    }

    if (status.enter) {
        if (focus_ == Row::Sound) {
            toggleSound();
            return NavRequest::None;
        }
        if (finding_ || awaiting_) {
            ensureBle(Pending::Stop);
        } else {
            ensureBle(Pending::Start);
        }
        return NavRequest::None;
    }

    if (isBackKey(status) && (finding_ || awaiting_)) {
        ensureBle(Pending::Stop);
    }
    return NavRequest::None;
}

void FindApp::draw() const {
    auto& display = M5Cardputer.Display;
    display.fillRect(0, kContentTop, display.width(), display.height() - kContentTop, TFT_BLACK);
    display.setTextSize(2);
    display.setTextDatum(top_left);

    const char* rows[2];
    char sound_line[24];
    char action_line[24];
    std::snprintf(sound_line, sizeof(sound_line), "Sound %s", sound_on_ ? "On" : "Off");
    std::snprintf(action_line, sizeof(action_line), "%s",
                  (finding_ || awaiting_) ? "Stop" : "Find phone");
    rows[0] = sound_line;
    rows[1] = action_line;

    int y = kContentTop + 6;
    for (uint8_t i = 0; i < 2; ++i) {
        const bool selected = !header_focused_ && static_cast<Row>(i) == focus_;
        const uint16_t fg = selected ? TFT_YELLOW : TFT_WHITE;
        const uint16_t bg = selected ? TFT_NAVY : TFT_BLACK;
        if (selected) {
            display.fillRect(0, y, display.width(), kLineHeight, bg);
        }
        display.setTextColor(fg, bg);
        display.setCursor(4, y);
        display.printf("%c %s", selected ? '>' : ' ', rows[i]);
        y += kLineHeight + 4;
    }

    display.setTextSize(1);
    display.setTextColor(TFT_DARKGREY, TFT_BLACK);
    display.setCursor(4, display.height() - 14);
    display.print(footer_);
}
