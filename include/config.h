#pragma once

#include <cstdint>

namespace config {

constexpr const char* kFirmwareVersion = "0.2.1";

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

constexpr uint8_t kMaxTimers = 8;
constexpr uint8_t kTimerMaxHours = 23;

constexpr uint8_t kMaxStopwatchHistory = 10;
constexpr uint8_t kDefaultSleepOptionIndex = 2;
constexpr const char* kIrStorageNamespace = "card_adv";
constexpr const char* kTimerStorageNamespace = "card_adv_tm";
constexpr const char* kStopwatchStorageNamespace = "card_adv_sw";
constexpr const char* kSettingsStorageNamespace = "card_adv_cfg";

constexpr const char* kStorageNamespace = kIrStorageNamespace;

// Cardputer display: 240x135 landscape, chrome bar at top.
constexpr int kChromeHeight = 22;
constexpr int kContentTop = 22;
constexpr int kFooterHeight = 18;

}  // namespace config
