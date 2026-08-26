#pragma once

#include <cstdint>

namespace config {

constexpr uint8_t kIrTxPin = 44;
constexpr uint8_t kIrRxPin = 1;

constexpr uint32_t kTorchDefaultTimeoutSec = 600;
constexpr uint8_t kTorchMinBrightness = 10;
constexpr uint8_t kTorchMaxBrightness = 255;
constexpr uint8_t kTorchDefaultBrightness = 255;

constexpr uint8_t kMaxDevices = 16;
constexpr uint8_t kMaxButtonsPerDevice = 16;
constexpr uint8_t kMaxNameLength = 24;

constexpr const char* kStorageNamespace = "card_adv";

}  // namespace config
