#pragma once

#include <cstddef>
#include <cstdint>

#include "ble/ble_protocol.h"

struct BleRxFrame {
    ble_protocol::Type type = ble_protocol::Type::Error;
    uint16_t req_id = 0;
    uint16_t len = 0;
    uint8_t payload[ble_protocol::kMaxPayload] = {};
};

class BleLink {
public:
    enum class State : uint8_t {
        Off = 0,
        Pairing,
        Advertising,
        Connected,
    };

    static BleLink& instance();

    void begin();
    void tick();

    State state() const { return state_; }
    bool isPaired() const { return paired_; }
    bool isLinkUp() const { return state_ == State::Connected; }
    // Phone CCCD + HelloAck done — safe to send HTTP frames (notifies are not dropped).
    bool isReady() const { return state_ == State::Connected && hello_ok_; }
    bool helloOk() const { return hello_ok_; }
    void resendHello();
    bool blocksLightSleep() const;
    const char* statusSummary() const;
    const char* peerLabel() const { return peer_label_; }

    bool startPairing();
    bool startConnect();
    void disconnect();
    void forget();
    void radioOff();

    uint16_t allocReqId();
    bool send(ble_protocol::Type type, uint16_t req_id, const uint8_t* payload = nullptr,
              uint16_t payload_len = 0);
    // Pop one queued app frame (HTTP/SSH/…). Returns false if empty.
    bool takeRxFrame(BleRxFrame* out);
    void clearRxQueue();

    void notifyConnected(const char* peer);
    void notifyDisconnected();
    void notifySubscribed();
    void notifyRx(const uint8_t* data, size_t len);

private:
    BleLink() = default;

    void loadPersisted();
    void savePersisted() const;
    void setPeerLabel(const char* label);
    void noteTraffic();
    void maybeIdleDisconnect();
    void handleRx(const uint8_t* data, size_t len);
    bool sendFrame(ble_protocol::Type type, uint16_t req_id, const uint8_t* payload,
                   uint16_t payload_len);
    void sendHello();
    void sendError(uint16_t req_id, ble_protocol::ErrorCode code);
    bool ensureStack();
    bool startAdvertising();
    void stopAdvertising();
    bool disconnectPeers();
    void quiesceToOff();
    bool enqueueRx(ble_protocol::Type type, uint16_t req_id, const uint8_t* payload,
                   uint16_t payload_len);
    void rejectUnexpectedPeer();

    State state_ = State::Off;
    bool paired_ = false;
    bool hello_ok_ = false;
    bool stack_ready_ = false;
    // True only while Pair/Connect/Web intentionally wants inbound links.
    bool accept_peers_ = false;
    char peer_label_[17] = {};
    uint32_t last_traffic_ms_ = 0;
    uint16_t next_req_id_ = 1;

    static constexpr uint8_t kRxQueueSize = 12;
    BleRxFrame rx_queue_[kRxQueueSize] = {};
    uint8_t rx_head_ = 0;
    uint8_t rx_tail_ = 0;
    uint8_t rx_count_ = 0;
};
