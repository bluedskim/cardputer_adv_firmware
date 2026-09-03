#pragma once

#include <cstdint>

namespace config {

constexpr const char* kFirmwareVersion = "0.3.1";

constexpr uint32_t kTorchDefaultTimeoutSec = 600;
constexpr uint32_t kTorchMaxTimeoutSec = 3600;
constexpr uint8_t kTorchMinBrightness = 10;
constexpr uint8_t kTorchMaxBrightness = 255;
constexpr uint8_t kTorchDefaultBrightness = 255;

constexpr uint8_t kMaxNameLength = 24;

constexpr uint8_t kMaxTimers = 8;
constexpr uint8_t kTimerMaxHours = 23;

constexpr uint8_t kMaxStopwatchHistory = 10;
constexpr uint8_t kDefaultSleepOptionIndex = 2;
constexpr uint8_t kDefaultBrightnessOptionIndex = 2;  // 128
constexpr const char* kTimerStorageNamespace = "card_adv_tm";
constexpr const char* kStopwatchStorageNamespace = "card_adv_sw";
constexpr const char* kSettingsStorageNamespace = "card_adv_cfg";

// Cardputer display: 240x135 landscape, chrome bar at top.
constexpr int kChromeHeight = 22;
constexpr int kContentTop = 22;
constexpr int kFooterHeight = 18;

// Power: light-sleep duration while display is off and no timers are running.
constexpr uint32_t kDisplaySleepLightSleepUs = 200000;
constexpr uint32_t kActiveLoopDelayMs = 5;
constexpr uint32_t kIdleLoopDelayMs = 33;
constexpr uint32_t kIdleLoopGraceMs = 500;

}  // namespace config
