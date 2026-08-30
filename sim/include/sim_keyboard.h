#pragma once

#include <cstdint>
#include <vector>

struct Point2D_t {
    int8_t x = -1;
    int8_t y = -1;

    bool operator==(const Point2D_t& other) const {
        return x == other.x && y == other.y;
    }
};

#define KEY_BACKSPACE 0x2a
#define KEY_TAB 0x2b
#define KEY_ENTER 0x28

class Keyboard_Class {
public:
    struct KeysState {
        bool tab = false;
        bool fn = false;
        bool shift = false;
        bool ctrl = false;
        bool opt = false;
        bool alt = false;
        bool del = false;
        bool enter = false;
        bool space = false;
        bool up = false;
        bool left = false;
        bool down = false;
        bool right = false;
        uint8_t modifiers = 0;
        std::vector<char> word;
        std::vector<uint8_t> hid_keys;
        std::vector<uint8_t> modifier_keys;
    };

    void begin() {}
    void updateKeyList();
    void updateKeysState();
    uint8_t isPressed() const { return static_cast<uint8_t>(key_list_.size()); }
    bool isChange();
    const std::vector<Point2D_t>& keyList() const { return key_list_; }
    KeysState& keysState() { return keys_state_; }

private:
    std::vector<Point2D_t> key_list_;
    KeysState keys_state_;
    uint8_t last_key_size_ = 0;
};

void sim_keyboard_init();
void sim_keyboard_poll();
