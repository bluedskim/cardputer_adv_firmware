#include <Arduino.h>
#define SDL_MAIN_HANDLED
#include <SDL2/SDL.h>
#include <M5GFX.h>

#include <memory>

#include "app_shell.h"
#include "apps/settings/settings_app.h"
#include "apps/stopwatch/stopwatch_app.h"
#include "apps/timer/timer_app.h"
#include "apps/torch/torch_app.h"

namespace {

AppShell g_shell;

void setup() {
    Serial.begin(115200);
    M5Cardputer.begin(true);
    std::printf("cardputer_adv_firmware desktop simulator\n");
    std::printf("Arrow: ; , . / or PC arrow keys | B blink | ` G0\n");

    g_shell.addApp(std::unique_ptr<AppBase>(new TorchApp()));
    g_shell.addApp(std::unique_ptr<AppBase>(new TimerApp()));
    g_shell.addApp(std::unique_ptr<AppBase>(new StopwatchApp()));
    g_shell.addApp(std::unique_ptr<AppBase>(new SettingsApp()));
    g_shell.begin();
}

void loop() {
    g_shell.update();
    delay(5);
}

int user_func(bool* running) {
    setup();
    while (*running) {
        loop();
    }
    return 0;
}

}  // namespace

int main(int, char**) {
#if defined(__APPLE__)
    SDL_SetHint(SDL_HINT_MAC_BACKGROUND_APP, "0");
#endif
    return lgfx::Panel_sdl::main(user_func, 16);
}
