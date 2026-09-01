#include <Arduino.h>
#define SDL_MAIN_HANDLED
#include <SDL2/SDL.h>
#include <IRremote.hpp>
#include <M5GFX.h>

#include <memory>

#include "app_shell.h"
#include "apps/ir/ir_app.h"
#include "apps/stopwatch/stopwatch_app.h"
#include "apps/timer/timer_app.h"
#include "apps/torch/torch_app.h"

namespace {

AppShell g_shell;
bool g_prev_inject_ir = false;

void setup() {
    Serial.begin(115200);
    M5Cardputer.begin(true);
    std::printf("cardputer_adv_firmware desktop simulator\n");
    std::printf("Arrow: ; , . / or PC arrow keys | B blink | ` G0 | I inject test IR while learning\n");

    g_shell.addApp(std::unique_ptr<AppBase>(new TorchApp()));
    g_shell.addApp(std::unique_ptr<AppBase>(new TimerApp()));
    g_shell.addApp(std::unique_ptr<AppBase>(new StopwatchApp()));
    g_shell.addApp(std::unique_ptr<AppBase>(new IrApp()));
    g_shell.begin();
}

void loop() {
    g_shell.update();

    const auto& keys = M5Cardputer.Keyboard.keysState();
    bool inject_now = false;
    for (const char ch : keys.word) {
        if (ch == 'i' || ch == 'I') {
            inject_now = true;
            break;
        }
    }
    if (IrReceiver.isActive() && inject_now && !g_prev_inject_ir) {
        IrReceiver.injectNec(0x00FF, 0x42);
        std::printf("[sim] injected test IR NEC addr=0x00FF cmd=0x42\n");
    }
    g_prev_inject_ir = inject_now;

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
