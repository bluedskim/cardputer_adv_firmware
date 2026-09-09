#pragma once

#include <M5Cardputer.h>

#include <algorithm>
#include <vector>

struct KeyboardInputTracker {
    bool arrow_up_down = false;
    bool arrow_down_down = false;
    bool arrow_left_down = false;
    bool arrow_right_down = false;
    bool enter_down = false;
    bool del_down = false;
    bool backspace_down = false;
    std::vector<char> held_chars;
};

enum class ArrowKey {
    None,
    Up,
    Down,
    Left,
    Right,
};

// USB HID usage IDs. Cardputer-Adv Fn+arrows put these in hid_keys and
// leave word[] empty (M5Cardputer Keyboard.cpp returns early on Fn).
constexpr uint8_t kHidArrowRight = 0x4F;
constexpr uint8_t kHidArrowLeft = 0x50;
constexpr uint8_t kHidArrowDown = 0x51;
constexpr uint8_t kHidArrowUp = 0x52;

inline bool keysContain(const Keyboard_Class::KeysState& status, const char key, const char shifted = 0) {
    for (const char ch : status.word) {
        if (ch == key || (shifted != 0 && ch == shifted)) {
            return true;
        }
    }
    return false;
}

inline bool hidContains(const Keyboard_Class::KeysState& status, const uint8_t hid) {
    for (const uint8_t code : status.hid_keys) {
        if (code == hid) {
            return true;
        }
    }
    return false;
}

inline bool arrowHeld(const Keyboard_Class::KeysState& status, ArrowKey arrow) {
    switch (arrow) {
        case ArrowKey::Up:
            return keysContain(status, ';', ':') || hidContains(status, kHidArrowUp);
        case ArrowKey::Down:
            return keysContain(status, '.', '>') || hidContains(status, kHidArrowDown);
        case ArrowKey::Left:
            return keysContain(status, ',', '<') || hidContains(status, kHidArrowLeft);
        case ArrowKey::Right:
            return keysContain(status, '/', '?') || hidContains(status, kHidArrowRight);
        default:
            return false;
    }
}

inline bool isArrowChar(const char key) {
    switch (key) {
        case ';':
        case ':':
        case ',':
        case '<':
        case '.':
        case '>':
        case '/':
        case '?':
            return true;
        default:
            return false;
    }
}

inline ArrowKey detectArrowKey(const Keyboard_Class::KeysState& status) {
    if (arrowHeld(status, ArrowKey::Up)) {
        return ArrowKey::Up;
    }
    if (arrowHeld(status, ArrowKey::Left)) {
        return ArrowKey::Left;
    }
    if (arrowHeld(status, ArrowKey::Down)) {
        return ArrowKey::Down;
    }
    if (arrowHeld(status, ArrowKey::Right)) {
        return ArrowKey::Right;
    }
    return ArrowKey::None;
}

inline void resetKeyboardTracker(KeyboardInputTracker& tracker) {
    tracker.arrow_up_down = false;
    tracker.arrow_down_down = false;
    tracker.arrow_left_down = false;
    tracker.arrow_right_down = false;
    tracker.enter_down = false;
    tracker.del_down = false;
    tracker.backspace_down = false;
    tracker.held_chars.clear();
}

inline ArrowKey pollArrowEdge(const Keyboard_Class::KeysState& status, KeyboardInputTracker& tracker) {
    const bool up = arrowHeld(status, ArrowKey::Up);
    const bool down = arrowHeld(status, ArrowKey::Down);
    const bool left = arrowHeld(status, ArrowKey::Left);
    const bool right = arrowHeld(status, ArrowKey::Right);

    ArrowKey nav = ArrowKey::None;
    if (up && !tracker.arrow_up_down) {
        nav = ArrowKey::Up;
    } else if (down && !tracker.arrow_down_down) {
        nav = ArrowKey::Down;
    } else if (left && !tracker.arrow_left_down) {
        nav = ArrowKey::Left;
    } else if (right && !tracker.arrow_right_down) {
        nav = ArrowKey::Right;
    }

    tracker.arrow_up_down = up;
    tracker.arrow_down_down = down;
    tracker.arrow_left_down = left;
    tracker.arrow_right_down = right;
    return nav;
}

struct KeyEvent {
    bool active = false;
    Keyboard_Class::KeysState status;
};

// Edge-triggered non-arrow keys (and Enter/Del). Arrows are handled via pollArrowEdge.
inline KeyEvent pollAppKeyEdge(const Keyboard_Class::KeysState& status, KeyboardInputTracker& tracker) {
    KeyEvent event;
    event.status = status;

    if (!M5Cardputer.Keyboard.isPressed()) {
        resetKeyboardTracker(tracker);
        return event;
    }

    bool changed = false;

    if (status.enter && !tracker.enter_down) {
        changed = true;
    }
    if (status.del && !tracker.del_down) {
        changed = true;
    }
    // Cardputer key without Fn reports backspace, not del (Fn+key → del).
    if (status.backspace && !tracker.backspace_down) {
        changed = true;
    }

    for (const char ch : status.word) {
        if (isArrowChar(ch)) {
            continue;
        }
        if (std::find(tracker.held_chars.begin(), tracker.held_chars.end(), ch) == tracker.held_chars.end()) {
            changed = true;
            break;
        }
    }

    tracker.enter_down = status.enter;
    tracker.del_down = status.del;
    tracker.backspace_down = status.backspace;
    tracker.held_chars = status.word;

    if (changed) {
        event.active = true;
    }

    return event;
}
