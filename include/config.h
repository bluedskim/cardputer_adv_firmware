#pragma once

#include <cstdint>

namespace config {

    constexpr const char* kFirmwareVersion = "0.8.1";

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

// Cardputer display: 240x135 landscape, header bar at top.
constexpr int kHeaderHeight = 22;
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
constexpr const char* kSshStorageNamespace = "card_adv_ssh";
constexpr const char* kFindStorageNamespace = "card_adv_find";

constexpr uint8_t kHttpMaxPresets = 12;
constexpr uint8_t kHttpNameMax = 20;
constexpr uint16_t kHttpBodyMax = 512;

constexpr uint8_t kSshMaxPresets = kHttpMaxPresets;
constexpr uint8_t kSshNameMax = kHttpNameMax;
constexpr uint16_t kSshBodyMax = kHttpBodyMax;

// Voice recorder (microSD WAV).
constexpr uint32_t kRecorderSampleRate = 16000;
constexpr uint32_t kRecorderMaxDurationSec = 600;  // 10 min
constexpr uint32_t kRecorderMinSaveMs = 500;
constexpr uint8_t kRecorderMaxFiles = 32;
constexpr const char* kRecorderDir = "/rec";
constexpr int kSdSpiSck = 40;
constexpr int kSdSpiMiso = 39;
constexpr int kSdSpiMosi = 14;
constexpr int kSdSpiCs = 12;

}  // namespace config
