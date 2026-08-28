#include <M5Cardputer.h>

#include <memory>

#include "app_shell.h"
#include "apps/ir/ir_app.h"
#include "apps/torch/torch_app.h"
#include "config.h"

namespace {

AppShell g_shell;

}  // namespace

void setup() {
    auto cfg = M5.config();
    M5Cardputer.begin(cfg, true);
    Serial.begin(115200);
    Serial.printf("cardputer_adv_firmware %s\n", config::kFirmwareVersion);

    g_shell.addApp(std::unique_ptr<AppBase>(new TorchApp()));
    g_shell.addApp(std::unique_ptr<AppBase>(new IrApp()));
    g_shell.begin();
}

void loop() {
    g_shell.update();
    delay(5);
}
