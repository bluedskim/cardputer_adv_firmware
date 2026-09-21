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

    // M2 HTTP commander (phone executes; device displays).
    HttpListReq = 0x10,   // device → phone
    HttpListItem = 0x11,  // phone → device: index(1) total(1) nameutf8
    HttpListEnd = 0x12,   // phone → device
    HttpRunReq = 0x13,    // device → phone: index(1)
    HttpRunMeta = 0x14,   // phone → device: http_status_u16_le, err_u8, reserved_u8
    HttpRunChunk = 0x15,  // phone → device: seq_u8 + bytes
    HttpRunEnd = 0x16,    // phone → device

    // M3 SSH commander (phone executes; device displays).
    SshListReq = 0x20,    // device → phone
    SshListItem = 0x21,   // phone → device: index(1) total(1) nameutf8
    SshListEnd = 0x22,    // phone → device
    SshRunReq = 0x23,     // device → phone: index(1)
    SshRunMeta = 0x24,    // phone → device: exit_status_i16_le, err_u8, reserved_u8
    SshRunChunk = 0x25,   // phone → device: seq_u8 + bytes
    SshRunEnd = 0x26,     // phone → device

    Error = 0x7F,
};

enum class ErrorCode : uint8_t {
    BadFrame = 0x01,
    UnknownType = 0x02,
    NotReady = 0x03,
    HttpFailed = 0x10,
    HttpTimeout = 0x11,
    BleNotReady = 0x12,
    NoPresets = 0x13,
    SshFailed = 0x20,
    SshTimeout = 0x21,
    SshAuthFailed = 0x22,
    SshHostKey = 0x23,
    NoSshPresets = 0x24,
};

inline bool isHttpType(Type type) {
    const auto v = static_cast<uint8_t>(type);
    return v >= 0x10 && v <= 0x1F;
}

inline bool isSshType(Type type) {
    const auto v = static_cast<uint8_t>(type);
    return v >= 0x20 && v <= 0x2F;
}

}  // namespace ble_protocol
