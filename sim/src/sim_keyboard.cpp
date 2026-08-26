#include "sim_keyboard.h"

#include <algorithm>
#include <set>

#include <SDL.h>

namespace {

struct ActiveKey {
    char ch = 0;
    bool special = false;
    bool del = false;
    bool enter = false;
};

std::set<char> g_pressed_chars;
bool g_del = false;
bool g_enter = false;
bool g_shift = false;

char mapScanCode(SDL_Scancode scancode, bool shift) {
    switch (scancode) {
        case SDL_SCANCODE_COMMA:
        case SDL_SCANCODE_LEFT:
            return ',';
        case SDL_SCANCODE_PERIOD:
        case SDL_SCANCODE_DOWN:
            return '.';
        case SDL_SCANCODE_SEMICOLON:
        case SDL_SCANCODE_UP:
            return ';';
        case SDL_SCANCODE_SLASH:
        case SDL_SCANCODE_RIGHT:
            return '/';
        case SDL_SCANCODE_SPACE:
            return ' ';
        case SDL_SCANCODE_MINUS:
            return shift ? '_' : '-';
        case SDL_SCANCODE_EQUALS:
            return shift ? '+' : '=';
        case SDL_SCANCODE_LEFTBRACKET:
            return shift ? '{' : '[';
        case SDL_SCANCODE_RIGHTBRACKET:
            return shift ? '}' : ']';
        case SDL_SCANCODE_BACKSLASH:
            return shift ? '|' : '\\';
        case SDL_SCANCODE_APOSTROPHE:
            return shift ? '"' : '\'';
        case SDL_SCANCODE_GRAVE:
            return shift ? '~' : '`';
        default:
            break;
    }

    if (scancode >= SDL_SCANCODE_A && scancode <= SDL_SCANCODE_Z) {
        char ch = static_cast<char>('a' + (scancode - SDL_SCANCODE_A));
        return shift ? static_cast<char>(ch - 'a' + 'A') : ch;
    }
    if (scancode >= SDL_SCANCODE_0 && scancode <= SDL_SCANCODE_9) {
        if (!shift) {
            return static_cast<char>('0' + (scancode - SDL_SCANCODE_0));
        }
        static const char shifted[] = ")!@#$%^&*(";
        return shifted[scancode - SDL_SCANCODE_0];
    }

    return 0;
}

}  // namespace

void sim_keyboard_poll() {
    SDL_PumpEvents();

    g_pressed_chars.clear();
    g_del = false;
    g_enter = false;
    g_shift = (SDL_GetModState() & KMOD_SHIFT) != 0;

    const Uint8* state = SDL_GetKeyboardState(nullptr);
    if (state == nullptr) {
        return;
    }

    if (state[SDL_SCANCODE_BACKSPACE]) {
        g_del = true;
    }
    if (state[SDL_SCANCODE_RETURN] || state[SDL_SCANCODE_KP_ENTER]) {
        g_enter = true;
    }

    for (int scancode = SDL_SCANCODE_A; scancode < SDL_NUM_SCANCODES; ++scancode) {
        if (!state[scancode]) {
            continue;
        }
        if (scancode == SDL_SCANCODE_BACKSPACE || scancode == SDL_SCANCODE_RETURN ||
            scancode == SDL_SCANCODE_KP_ENTER) {
            continue;
        }

        const char ch = mapScanCode(static_cast<SDL_Scancode>(scancode), g_shift);
        if (ch != 0) {
            g_pressed_chars.insert(ch);
        }
    }
}

void Keyboard_Class::updateKeyList() {
    sim_keyboard_poll();
    key_list_.clear();
    key_list_.reserve(g_pressed_chars.size() + g_del + g_enter);
    int index = 0;
    for (char /*unused*/ _ : g_pressed_chars) {
        (void)_;
        key_list_.push_back(Point2D_t{static_cast<int8_t>(index++), 0});
    }
    if (g_del) {
        key_list_.push_back(Point2D_t{static_cast<int8_t>(index++), 0});
    }
    if (g_enter) {
        key_list_.push_back(Point2D_t{static_cast<int8_t>(index++), 0});
    }
}

bool Keyboard_Class::isChange() {
    const uint8_t current_size = static_cast<uint8_t>(key_list_.size());
    if (last_key_size_ != current_size) {
        last_key_size_ = current_size;
        return true;
    }
    return false;
}

void Keyboard_Class::updateKeysState() {
    keys_state_ = KeysState{};
    keys_state_.shift = g_shift;
    keys_state_.del = g_del;
    keys_state_.enter = g_enter;

    for (char ch : g_pressed_chars) {
        if (ch == ' ') {
            keys_state_.space = true;
        }
        keys_state_.word.push_back(ch);
    }
}
