#pragma once

#include <cstdint>

namespace config {

constexpr uint8_t kIrTxPin = 44;
constexpr uint8_t kIrRxPin = 1;

constexpr uint32_t kTorchDefaultTimeoutSec = 600;
constexpr uint32_t kTorchMaxTimeoutSec = 3600;
constexpr uint8_t kTorchMinBrightness = 10;
constexpr uint8_t kTorchMaxBrightness = 255;
constexpr uint8_t kTorchDefaultBrightness = 255;

constexpr uint8_t kMaxDevices = 16;
constexpr uint8_t kMaxButtonsPerDevice = 16;
constexpr uint8_t kMaxNameLength = 24;

constexpr const char* kStorageNamespace = "card_adv";

// Cardputer display: 240x135 landscape, chrome bar at top.
constexpr int kChromeHeight = 22;
constexpr int kContentTop = 22;
constexpr int kFooterHeight = 18;

}  // namespace config
