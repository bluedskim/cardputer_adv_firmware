#pragma once

#include <M5Cardputer.h>

#include <algorithm>
#include <vector>

struct KeyboardInputTracker {
    bool nav_left_down = false;
    bool nav_right_down = false;
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

inline bool isAppSwitchKey(const char key) {
    return key == ',' || key == '<' || key == '/' || key == '?';
}

inline void resetKeyboardTracker(KeyboardInputTracker& tracker) {
    tracker.nav_left_down = false;
    tracker.nav_right_down = false;
    tracker.enter_down = false;
    tracker.del_down = false;
    tracker.held_chars.clear();
}

inline ArrowKey pollNavEdge(const Keyboard_Class::KeysState& status, KeyboardInputTracker& tracker) {
    const bool left = keysContain(status, ',', '<');
    const bool right = keysContain(status, '/', '?');

    if (status.enter) {
        tracker.nav_left_down = left;
        tracker.nav_right_down = right;
        return ArrowKey::None;
    }

    ArrowKey nav = ArrowKey::None;
    if (left && !tracker.nav_left_down) {
        nav = ArrowKey::Left;
    } else if (right && !tracker.nav_right_down) {
        nav = ArrowKey::Right;
    }

    tracker.nav_left_down = left;
    tracker.nav_right_down = right;
    return nav;
}

struct KeyEvent {
    bool active = false;
    Keyboard_Class::KeysState status;
};

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
        if (isAppSwitchKey(ch)) {
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
