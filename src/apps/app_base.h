#pragma once

#include <M5Cardputer.h>

#include <cstdint>

// Returned by apps to participate in the shared two-level navigation model.
enum class NavRequest {
    None,
    FocusHeader,   // ↑ from top of app content
    FocusContent,  // app wants content focus (e.g. list just loaded)
};

class AppBase {
public:
    virtual ~AppBase() = default;

    virtual const char* name() const = 0;
    virtual void onEnter() = 0;
    virtual void onExit() = 0;
    // keys may be null (no new edge). Still called every frame for timers/UI.
    // When header_focused is true, only the header is active; apps must not
    // highlight content selection until the user presses Down from the header.
    virtual NavRequest onUpdate(const Keyboard_Class::KeysState* keys, bool header_focused) = 0;
    // Called when the backlight wakes from display sleep. Redraw content; do not reset app state.
    virtual void onDisplayWake() {}
    // Called every frame for all apps, including while the display is asleep.
    virtual void onBackgroundTick() {}
    // True while the app needs frequent CPU (e.g. running timer/stopwatch).
    virtual bool hasActiveBackgroundWork() const { return false; }
    // True while the app paints the whole screen; the shell then skips the header.
    virtual bool hidesHeader() const { return false; }

    // Global G0 short-press record hotkey (AppShell routes here).
    virtual bool isVoiceRecorder() const { return false; }
    virtual bool voiceRecorderIsRecording() const { return false; }
    // Start recording, or stop&save if already recording.
    virtual void voiceRecorderHotkey() {}
};
