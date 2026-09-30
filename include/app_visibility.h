#pragma once

#include <cstdint>

// Which apps appear in the header Left/Right switcher. Settings is never hidden.
class AppVisibility {
public:
    static AppVisibility& instance();

    bool begin();

    // Hideable apps only (registration order, Settings excluded).
    uint8_t hideableCount() const;
    const char* hideableName(uint8_t index) const;

    bool isHidden(const char* app_name) const;
    bool canHide(const char* app_name) const;
    void setHidden(const char* app_name, bool hidden);
    void toggleHidden(uint8_t hideable_index);

private:
    AppVisibility() = default;

    int bitForName(const char* app_name) const;
    void load();
    bool save() const;

    uint16_t hidden_mask_ = 0;
};
