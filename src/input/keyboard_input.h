#pragma once

#include <M5Cardputer.h>

struct KeyEvent {
    bool active = false;
    Keyboard_Class::KeysState status;
};

inline KeyEvent pollKeyEvent() {
    KeyEvent event;
    if (!M5Cardputer.Keyboard.isChange()) {
        return event;
    }

    if (!M5Cardputer.Keyboard.isPressed()) {
        return event;
    }

    event.active = true;
    event.status = M5Cardputer.Keyboard.keysState();
    return event;
}

enum class ArrowKey {
    None,
    Up,
    Down,
    Left,
    Right,
};

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
