#pragma once

#include <cstdint>

class SystemSettings {
public:
    static SystemSettings& instance();

    bool begin();

    uint8_t sleepOptionIndex() const { return sleep_option_index_; }
    uint32_t sleepTimeoutSec() const;
    const char* sleepOptionLabel() const;
    const char* sleepOptionLabel(uint8_t index) const;
    uint8_t sleepOptionCount() const;
    void cycleSleepOption(int delta);
    /** If Sleep is Immediate, switch to 30 sec after wake so the screen stays usable. */
    bool promoteImmediateToThirtySec();

    uint8_t brightnessOptionIndex() const { return brightness_option_index_; }
    uint8_t brightness() const;
    const char* brightnessOptionLabel() const;
    const char* brightnessOptionLabel(uint8_t index) const;
    uint8_t brightnessOptionCount() const;
    void cycleBrightnessOption(int delta);
    void applyBrightness() const;

private:
    SystemSettings() = default;

    void load();
    bool save() const;
    void clampIndices();

    uint8_t sleep_option_index_ = 3;
    uint8_t brightness_option_index_ = 2;
};
