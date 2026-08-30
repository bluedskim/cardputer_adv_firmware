#include "sim_keyboard.h"

#include <SDL.h>

#include <map>
#include <mutex>
#include <set>

namespace {

struct KeySnapshot {
    std::set<char> pressed_chars;
    bool del = false;
    bool enter = false;
    bool shift = false;
};

std::mutex g_key_mutex;
KeySnapshot g_keys;
std::map<SDL_Scancode, char> g_down_by_scancode;
bool g_watch_registered = false;

bool g_del = false;
bool g_enter = false;
bool g_shift = false;
std::set<char> g_pressed_chars;

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
        case SDL_SCANCODE_KP_MINUS:
            return shift ? '_' : '-';
        case SDL_SCANCODE_EQUALS:
        case SDL_SCANCODE_KP_PLUS:
            return shift ? '+' : (scancode == SDL_SCANCODE_KP_PLUS ? '+' : '=');
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

void rebuildPressedChars() {
    g_keys.pressed_chars.clear();
    for (const auto& entry : g_down_by_scancode) {
        g_keys.pressed_chars.insert(entry.second);
    }
}

void applyKeyEvent(const SDL_KeyboardEvent& key, bool down) {
    const bool shift = (key.keysym.mod & KMOD_SHIFT) != 0;
    const SDL_Scancode scancode = key.keysym.scancode;

    if (scancode == SDL_SCANCODE_BACKSPACE) {
        g_keys.del = down;
        return;
    }
    if (scancode == SDL_SCANCODE_RETURN || scancode == SDL_SCANCODE_KP_ENTER) {
        g_keys.enter = down;
        return;
    }
    if (scancode == SDL_SCANCODE_LSHIFT || scancode == SDL_SCANCODE_RSHIFT) {
        g_keys.shift = down;
        return;
    }

    if (down) {
        const char ch = mapScanCode(scancode, shift);
        if (ch == 0) {
            return;
        }
        g_down_by_scancode[scancode] = ch;
    } else {
        g_down_by_scancode.erase(scancode);
    }
    rebuildPressedChars();
}

int keyboardEventWatch(void*, SDL_Event* event) {
    if (event->type != SDL_KEYDOWN && event->type != SDL_KEYUP) {
        return 1;
    }

    // Ignore auto-repeat KEYDOWNs so edge detection stays one-shot.
    if (event->type == SDL_KEYDOWN && event->key.repeat) {
        return 1;
    }

    std::lock_guard<std::mutex> lock(g_key_mutex);
    applyKeyEvent(event->key, event->type == SDL_KEYDOWN);
    return 1;
}

KeySnapshot snapshotKeys() {
    std::lock_guard<std::mutex> lock(g_key_mutex);
    return g_keys;
}

}  // namespace

void sim_keyboard_init() {
    if (g_watch_registered) {
        return;
    }

    SDL_AddEventWatch(keyboardEventWatch, nullptr);
    g_watch_registered = true;
}

void sim_keyboard_poll() {
    const KeySnapshot keys = snapshotKeys();
    g_del = keys.del;
    g_enter = keys.enter;
    g_shift = keys.shift;
    g_pressed_chars = keys.pressed_chars;
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
        switch (ch) {
            case ';':
            case ':':
                keys_state_.up = true;
                keys_state_.hid_keys.push_back(0x52);
                break;
            case ',':
            case '<':
                keys_state_.left = true;
                keys_state_.hid_keys.push_back(0x50);
                break;
            case '.':
            case '>':
                keys_state_.down = true;
                keys_state_.hid_keys.push_back(0x51);
                break;
            case '/':
            case '?':
                keys_state_.right = true;
                keys_state_.hid_keys.push_back(0x4F);
                break;
            default:
                break;
        }
    }
}
