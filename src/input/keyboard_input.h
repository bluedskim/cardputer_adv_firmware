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
    std::vector<char> held_chars;
};

enum class ArrowKey {
    None,
    Up,
    Down,
    Left,
    Right,
};

inline bool keysContain(const Keyboard_Class::KeysState& status, const char key, const char shifted = 0) {
    for (const char ch : status.word) {
        if (ch == key || (shifted != 0 && ch == shifted)) {
            return true;
        }
    }
    return false;
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
    for (const auto key : status.word) {
        switch (key) {
            case ';':
            case ':':
                return ArrowKey::Up;
            case ',':
            case '<':
                return ArrowKey::Left;
            case '.':
            case '>':
                return ArrowKey::Down;
            case '/':
            case '?':
                return ArrowKey::Right;
            default:
                break;
        }
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
    tracker.held_chars.clear();
}

inline ArrowKey pollArrowEdge(const Keyboard_Class::KeysState& status, KeyboardInputTracker& tracker) {
    const bool up = keysContain(status, ';', ':');
    const bool down = keysContain(status, '.', '>');
    const bool left = keysContain(status, ',', '<');
    const bool right = keysContain(status, '/', '?');

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
    tracker.held_chars = status.word;

    if (changed) {
        event.active = true;
    }

    return event;
}
