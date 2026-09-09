#pragma once

#include <cstdint>

namespace ble_protocol {

constexpr const char* kDeviceName = "CardputerAdv";

constexpr const char* kServiceUuid = "c7a1f001-5b2e-4a1d-9c3f-0d1e2f3a4b5c";
constexpr const char* kRxUuid = "c7a1f002-5b2e-4a1d-9c3f-0d1e2f3a4b5c";
constexpr const char* kTxUuid = "c7a1f003-5b2e-4a1d-9c3f-0d1e2f3a4b5c";

constexpr uint8_t kHeaderSize = 5;
constexpr uint16_t kMaxPayload = 48;
constexpr uint16_t kMaxFrame = kHeaderSize + kMaxPayload;

enum class Type : uint8_t {
    Hello = 0x01,
    HelloAck = 0x02,
    Ping = 0x03,
    Pong = 0x04,
    Error = 0x7F,
};

enum class ErrorCode : uint8_t {
    BadFrame = 0x01,
    UnknownType = 0x02,
    NotReady = 0x03,
};

}  // namespace ble_protocol
