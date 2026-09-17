#pragma once

#include <cstdint>

namespace config {

    constexpr const char* kFirmwareVersion = "0.5.16-dev";

constexpr uint32_t kTorchDefaultTimeoutSec = 600;
constexpr uint32_t kTorchMaxTimeoutSec = 3600;
constexpr uint8_t kTorchMinBrightness = 10;
constexpr uint8_t kTorchMaxBrightness = 255;
constexpr uint8_t kTorchDefaultBrightness = 255;

constexpr uint8_t kMaxNameLength = 24;

constexpr uint8_t kMaxTimers = 8;
constexpr uint8_t kTimerMaxHours = 23;

constexpr uint8_t kMaxStopwatchHistory = 10;
constexpr uint8_t kDefaultSleepOptionIndex = 3;  // 5 min (Immediate, 30s, 1m, 5m, …)
constexpr uint8_t kDefaultBrightnessOptionIndex = 2;  // 128
constexpr const char* kTimerStorageNamespace = "card_adv_tm";
constexpr const char* kStopwatchStorageNamespace = "card_adv_sw";
constexpr const char* kSettingsStorageNamespace = "card_adv_cfg";

// Cardputer display: 240x135 landscape, chrome bar at top.
constexpr int kChromeHeight = 22;
constexpr int kContentTop = 22;
constexpr int kFooterHeight = 18;

// Power: Cardputer-Adv only. TCA8418 INT (GPIO11, active-low) wakes light sleep.
// No timer wake — skip keyboard polling while display-asleep with no running timers.
constexpr int kKeyboardIntGpio = 11;
constexpr uint32_t kActiveLoopDelayMs = 5;
constexpr uint32_t kIdleLoopDelayMs = 33;
constexpr uint32_t kIdleLoopGraceMs = 500;

// BLE link (M1): idle connected session ends and radio powers down.
constexpr uint32_t kBleIdleDisconnectSec = 60;
constexpr uint8_t kBlePeerLabelMax = 16;

constexpr const char* kWebStorageNamespace = "card_adv_web";

constexpr uint8_t kHttpMaxPresets = 12;
constexpr uint8_t kHttpNameMax = 20;
constexpr uint16_t kHttpBodyMax = 512;

}  // namespace config
