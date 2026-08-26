#pragma once

#include <M5Cardputer.h>

enum class ArrowKey {
    None,
    Up,
    Down,
    Left,
    Right,
};

// Cardputer prints arrow glyphs on ; , . / — treat them as direction keys directly.
inline ArrowKey detectArrowKey(const Keyboard_Class::KeysState& status) {
    for (const auto key : status.word) {
        switch (key) {
            case ';':
                return ArrowKey::Up;
            case ',':
                return ArrowKey::Left;
            case '.':
                return ArrowKey::Down;
            case '/':
                return ArrowKey::Right;
            default:
                break;
        }
    }

    return ArrowKey::None;
}
