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

private:
    SystemSettings() = default;

    void load();
    bool save() const;
    void clampIndex();

    uint8_t sleep_option_index_ = 2;
};
