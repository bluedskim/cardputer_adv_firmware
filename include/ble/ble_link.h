#pragma once

#include <cstddef>
#include <cstdint>

#include "ble/ble_protocol.h"

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
    bool blocksLightSleep() const;
    const char* statusLabel() const;
    const char* peerLabel() const { return peer_label_; }

    bool startPairing();
    bool startConnect();
    void disconnect();
    void forget();
    void radioOff();

    // Called from NimBLE callbacks (device build only).
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
    bool startAdvertising(bool bondable);
    void stopAdvertising();
    void deinitStack();

    State state_ = State::Off;
    bool paired_ = false;
    bool hello_ok_ = false;
    bool stack_ready_ = false;
    char peer_label_[17] = {};
    uint32_t last_traffic_ms_ = 0;
    uint16_t next_req_id_ = 1;
};
