#include "ble/ble_link.h"

#include <Preferences.h>

#include <cstdio>
#include <cstring>

#include "config.h"

#if !defined(CARDPUTER_SIM)
#include <NimBLEDevice.h>
#endif

namespace {

constexpr const char* kNvsPairedKey = "ble_paired";
constexpr const char* kNvsPeerKey = "ble_peer";

#if !defined(CARDPUTER_SIM)
NimBLEServer* g_server = nullptr;
NimBLECharacteristic* g_tx = nullptr;
NimBLECharacteristic* g_rx = nullptr;
NimBLEService* g_service = nullptr;

class ServerCallbacks final : public NimBLEServerCallbacks {
    void onConnect(NimBLEServer* /*server*/) override {
        BleLink::instance().notifyConnected(nullptr);
    }

    void onDisconnect(NimBLEServer* /*server*/) override {
        BleLink::instance().notifyDisconnected();
    }
};

class RxCallbacks final : public NimBLECharacteristicCallbacks {
    void onWrite(NimBLECharacteristic* characteristic) override {
        std::string value = characteristic->getValue();
        if (value.empty()) {
            return;
        }
        BleLink::instance().notifyRx(reinterpret_cast<const uint8_t*>(value.data()), value.size());
    }
};

class TxCallbacks final : public NimBLECharacteristicCallbacks {
    void onSubscribe(NimBLECharacteristic* /*characteristic*/, ble_gap_conn_desc* /*desc*/,
                     uint16_t subValue) override {
        if ((subValue & 0x0001) != 0) {
            BleLink::instance().notifySubscribed();
        }
    }
};

ServerCallbacks g_server_callbacks;
RxCallbacks g_rx_callbacks;
TxCallbacks g_tx_callbacks;
#endif

}  // namespace

BleLink& BleLink::instance() {
    static BleLink link;
    return link;
}

void BleLink::begin() {
    loadPersisted();
    state_ = State::Off;
    hello_ok_ = false;
    stack_ready_ = false;
    accept_peers_ = false;
}

void BleLink::loadPersisted() {
    paired_ = false;
    peer_label_[0] = '\0';

    Preferences prefs;
    if (!prefs.begin(config::kSettingsStorageNamespace, true)) {
        return;
    }
    paired_ = prefs.getUChar(kNvsPairedKey, 0) != 0;
    const String peer = prefs.getString(kNvsPeerKey, "");
    prefs.end();
    if (peer.length() > 0) {
        setPeerLabel(peer.c_str());
    }
}

void BleLink::savePersisted() const {
    Preferences prefs;
    if (!prefs.begin(config::kSettingsStorageNamespace, false)) {
        return;
    }
    prefs.putUChar(kNvsPairedKey, paired_ ? 1 : 0);
    prefs.putString(kNvsPeerKey, peer_label_);
    prefs.end();
}

void BleLink::setPeerLabel(const char* label) {
    if (label == nullptr || label[0] == '\0') {
        peer_label_[0] = '\0';
        return;
    }
    std::snprintf(peer_label_, sizeof(peer_label_), "%.*s",
                  static_cast<int>(config::kBlePeerLabelMax), label);
}

bool BleLink::blocksLightSleep() const {
    // Keep NimBLE stack resident after first use — NimBLEDevice::deinit() reboots
    // this S3/USB-CDC board. Skip GPIO light-sleep while the stack is up.
    return state_ != State::Off || stack_ready_;
}

const char* BleLink::statusSummary() const {
#if defined(CARDPUTER_SIM)
    return "N/A";
#else
    switch (state_) {
        case State::Off:
            return paired_ ? "Off Paired" : "Off Unpaired";
        case State::Pairing:
            return "Pairing";
        case State::Advertising:
            return "Connecting";
        case State::Connected:
            // GATT up ⇒ Connected (Hello/HelloAck still runs in the background).
            return "Connected";
    }
    return "Off Unpaired";
#endif
}

void BleLink::noteTraffic() {
    last_traffic_ms_ = millis();
}

void BleLink::tick() {
#if defined(CARDPUTER_SIM)
    return;
#else
    maybeIdleDisconnect();
    // NimBLE can leave advertising running after a peer drop; keep radio quiet when Off.
    if (state_ == State::Off && stack_ready_ && !accept_peers_) {
        stopAdvertising();
    }
#endif
}

void BleLink::maybeIdleDisconnect() {
    if (state_ == State::Off) {
        return;
    }
    if (millis() - last_traffic_ms_ < config::kBleIdleDisconnectSec * 1000U) {
        return;
    }
    disconnect();
}

#if defined(CARDPUTER_SIM)

bool BleLink::startPairing() { return false; }
bool BleLink::startConnect() { return false; }
void BleLink::disconnect() {}
void BleLink::forget() {
    paired_ = false;
    peer_label_[0] = '\0';
    savePersisted();
}
void BleLink::radioOff() { state_ = State::Off; }
void BleLink::notifyConnected(const char*) {}
void BleLink::notifyDisconnected() {}
void BleLink::notifySubscribed() {}
void BleLink::notifyRx(const uint8_t*, size_t) {}
void BleLink::handleRx(const uint8_t*, size_t) {}
bool BleLink::sendFrame(ble_protocol::Type, uint16_t, const uint8_t*, uint16_t) { return false; }
void BleLink::sendHello() {}
void BleLink::sendError(uint16_t, ble_protocol::ErrorCode) {}
bool BleLink::ensureStack() { return false; }
bool BleLink::startAdvertising() { return false; }
void BleLink::stopAdvertising() {}
bool BleLink::disconnectPeers() { return false; }
void BleLink::quiesceToOff() { state_ = State::Off; accept_peers_ = false; }
bool BleLink::enqueueRx(ble_protocol::Type, uint16_t, const uint8_t*, uint16_t) { return false; }
bool BleLink::takeRxFrame(BleRxFrame*) { return false; }
void BleLink::clearRxQueue() {}
uint16_t BleLink::allocReqId() { return 1; }
bool BleLink::send(ble_protocol::Type, uint16_t, const uint8_t*, uint16_t) { return false; }
void BleLink::rejectUnexpectedPeer() { state_ = State::Off; accept_peers_ = false; }
void BleLink::resendHello() {}

#else

bool BleLink::ensureStack() {
    if (stack_ready_) {
        return true;
    }

    NimBLEDevice::init(ble_protocol::kDeviceName);
    NimBLEDevice::setPower(ESP_PWR_LVL_P3);
    NimBLEDevice::setSecurityAuth(true, true, true);
    NimBLEDevice::setSecurityIOCap(BLE_HS_IO_NO_INPUT_OUTPUT);

    g_server = NimBLEDevice::createServer();
    g_server->setCallbacks(&g_server_callbacks);

    g_service = g_server->createService(ble_protocol::kServiceUuid);
    g_tx = g_service->createCharacteristic(
        ble_protocol::kTxUuid, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);
    g_tx->setCallbacks(&g_tx_callbacks);
    g_rx = g_service->createCharacteristic(
        ble_protocol::kRxUuid, NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR);
    g_rx->setCallbacks(&g_rx_callbacks);
    g_service->start();
    stack_ready_ = true;
    return true;
}

bool BleLink::startAdvertising() {
    if (!ensureStack()) {
        return false;
    }
    NimBLEAdvertising* advertising = NimBLEDevice::getAdvertising();
    advertising->stop();
    advertising->reset();

    // Primary ADV must carry the local name: Companion Device Manager often does
    // passive scans (no scan-response). 128-bit UUID + name does not fit in 31 B,
    // so put the name in ADV and the service UUID only in the scan response.
    NimBLEAdvertisementData adv;
    adv.setFlags(BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP);
    adv.setName(ble_protocol::kDeviceName);
    advertising->setAdvertisementData(adv);

    NimBLEAdvertisementData scan_rsp;
    scan_rsp.setCompleteServices(NimBLEUUID(ble_protocol::kServiceUuid));
    advertising->setScanResponseData(scan_rsp);

    advertising->setScanResponse(true);
    advertising->start();
    return true;
}

void BleLink::stopAdvertising() {
    if (!stack_ready_) {
        return;
    }
    NimBLEDevice::getAdvertising()->stop();
}

bool BleLink::disconnectPeers() {
    if (!stack_ready_ || g_server == nullptr) {
        return false;
    }
    const std::vector<uint16_t> peers = g_server->getPeerDevices();
    if (peers.empty()) {
        return false;
    }
    for (uint16_t conn_id : peers) {
        g_server->disconnect(conn_id);
    }
    return true;
}

void BleLink::quiesceToOff() {
    // Do not call NimBLEDevice::deinit() — it hard-resets this board.
    accept_peers_ = false;
    stopAdvertising();
    disconnectPeers();
    hello_ok_ = false;
    state_ = State::Off;
}

bool BleLink::startPairing() {
    quiesceToOff();
    delay(50);
    accept_peers_ = true;
    if (!startAdvertising()) {
        accept_peers_ = false;
        return false;
    }
    state_ = State::Pairing;
    hello_ok_ = false;
    noteTraffic();
    return true;
}

bool BleLink::startConnect() {
    if (!paired_) {
        return startPairing();
    }
    if (state_ == State::Connected) {
        accept_peers_ = true;
        // CCCD may already be on; re-Hello so companion is definitely ready for HTTP.
        if (!hello_ok_) {
            sendHello();
        }
        noteTraffic();
        return true;
    }
    // Already advertising for connect — do not tear down (Web may call again).
    if (state_ == State::Advertising || state_ == State::Pairing) {
        accept_peers_ = true;
        noteTraffic();
        return true;
    }
    quiesceToOff();
    delay(50);
    accept_peers_ = true;
    if (!startAdvertising()) {
        accept_peers_ = false;
        return false;
    }
    state_ = State::Advertising;
    hello_ok_ = false;
    noteTraffic();
    return true;
}

void BleLink::resendHello() {
    if (state_ != State::Connected) {
        return;
    }
    hello_ok_ = false;
    sendHello();
}

void BleLink::disconnect() {
    quiesceToOff();
}

void BleLink::forget() {
    quiesceToOff();
    if (stack_ready_) {
        NimBLEDevice::deleteAllBonds();
    } else if (ensureStack()) {
        NimBLEDevice::deleteAllBonds();
        quiesceToOff();
    }
    paired_ = false;
    peer_label_[0] = '\0';
    hello_ok_ = false;
    savePersisted();
}

void BleLink::radioOff() { disconnect(); }

void BleLink::rejectUnexpectedPeer() {
    stopAdvertising();
    disconnectPeers();
    hello_ok_ = false;
    state_ = State::Off;
}

void BleLink::notifyConnected(const char* peer) {
    // Settings Disconnect / idle off: ignore stale ads that phone auto-listen may catch.
    if (!accept_peers_) {
        rejectUnexpectedPeer();
        return;
    }
    stopAdvertising();
    state_ = State::Connected;
    hello_ok_ = false;
    if (peer != nullptr && peer[0] != '\0') {
        setPeerLabel(peer);
    } else if (peer_label_[0] == '\0') {
        setPeerLabel("phone");
    }
    paired_ = true;
    savePersisted();
    noteTraffic();
}

void BleLink::notifySubscribed() {
    if (state_ != State::Connected || hello_ok_) {
        return;
    }
    noteTraffic();
    sendHello();
}

void BleLink::notifyDisconnected() {
    hello_ok_ = false;
    // Do not keep advertising after a drop — Web/Settings must call startConnect again.
    accept_peers_ = false;
    state_ = State::Off;
    stopAdvertising();
}

void BleLink::notifyRx(const uint8_t* data, size_t len) {
    noteTraffic();
    handleRx(data, len);
}

bool BleLink::sendFrame(ble_protocol::Type type, uint16_t req_id, const uint8_t* payload,
                        uint16_t payload_len) {
    if (state_ != State::Connected || g_tx == nullptr) {
        return false;
    }
    if (payload_len > ble_protocol::kMaxPayload) {
        return false;
    }

    uint8_t frame[ble_protocol::kMaxFrame];
    frame[0] = static_cast<uint8_t>(type);
    frame[1] = static_cast<uint8_t>(req_id & 0xFF);
    frame[2] = static_cast<uint8_t>((req_id >> 8) & 0xFF);
    frame[3] = static_cast<uint8_t>(payload_len & 0xFF);
    frame[4] = static_cast<uint8_t>((payload_len >> 8) & 0xFF);
    if (payload_len > 0 && payload != nullptr) {
        std::memcpy(frame + ble_protocol::kHeaderSize, payload, payload_len);
    }

    g_tx->setValue(frame, ble_protocol::kHeaderSize + payload_len);
    g_tx->notify();
    noteTraffic();
    return true;
}

void BleLink::sendHello() {
    const char* ver = config::kFirmwareVersion;
    const auto len = static_cast<uint16_t>(std::strlen(ver));
    const uint16_t req = next_req_id_++;
    if (next_req_id_ == 0) {
        next_req_id_ = 1;
    }
    sendFrame(ble_protocol::Type::Hello, req, reinterpret_cast<const uint8_t*>(ver), len);
}

void BleLink::sendError(uint16_t req_id, ble_protocol::ErrorCode code) {
    const uint8_t payload = static_cast<uint8_t>(code);
    sendFrame(ble_protocol::Type::Error, req_id, &payload, 1);
}

void BleLink::handleRx(const uint8_t* data, size_t len) {
    if (data == nullptr || len < ble_protocol::kHeaderSize) {
        sendError(0, ble_protocol::ErrorCode::BadFrame);
        return;
    }

    const auto type = static_cast<ble_protocol::Type>(data[0]);
    const uint16_t req_id = static_cast<uint16_t>(data[1] | (data[2] << 8));
    const uint16_t payload_len = static_cast<uint16_t>(data[3] | (data[4] << 8));
    if (payload_len > ble_protocol::kMaxPayload ||
        len < static_cast<size_t>(ble_protocol::kHeaderSize + payload_len)) {
        sendError(req_id, ble_protocol::ErrorCode::BadFrame);
        return;
    }

    const uint8_t* payload = data + ble_protocol::kHeaderSize;

    if (ble_protocol::isHttpType(type) || type == ble_protocol::Type::Error) {
        enqueueRx(type, req_id, payload, payload_len);
        return;
    }

    switch (type) {
        case ble_protocol::Type::HelloAck:
            hello_ok_ = true;
            if (payload_len > 0) {
                char label[17];
                const size_t copy = payload_len < 16 ? payload_len : 16;
                std::memcpy(label, payload, copy);
                label[copy] = '\0';
                setPeerLabel(label);
                savePersisted();
            }
            break;
        case ble_protocol::Type::Ping:
            sendFrame(ble_protocol::Type::Pong, req_id, payload, payload_len);
            break;
        case ble_protocol::Type::Pong:
            break;
        case ble_protocol::Type::Hello:
            break;
        default:
            sendError(req_id, ble_protocol::ErrorCode::UnknownType);
            break;
    }
}

bool BleLink::enqueueRx(ble_protocol::Type type, uint16_t req_id, const uint8_t* payload,
                        uint16_t payload_len) {
    if (rx_count_ >= kRxQueueSize) {
        return false;
    }
    BleRxFrame& slot = rx_queue_[rx_tail_];
    slot.type = type;
    slot.req_id = req_id;
    slot.len = payload_len;
    if (payload_len > 0 && payload != nullptr) {
        std::memcpy(slot.payload, payload, payload_len);
    }
    rx_tail_ = static_cast<uint8_t>((rx_tail_ + 1) % kRxQueueSize);
    ++rx_count_;
    return true;
}

bool BleLink::takeRxFrame(BleRxFrame* out) {
    if (out == nullptr || rx_count_ == 0) {
        return false;
    }
    *out = rx_queue_[rx_head_];
    rx_head_ = static_cast<uint8_t>((rx_head_ + 1) % kRxQueueSize);
    --rx_count_;
    return true;
}

void BleLink::clearRxQueue() {
    rx_head_ = 0;
    rx_tail_ = 0;
    rx_count_ = 0;
}

uint16_t BleLink::allocReqId() {
    const uint16_t id = next_req_id_++;
    if (next_req_id_ == 0) {
        next_req_id_ = 1;
    }
    return id;
}

bool BleLink::send(ble_protocol::Type type, uint16_t req_id, const uint8_t* payload,
                   uint16_t payload_len) {
    return sendFrame(type, req_id, payload, payload_len);
}

#endif
