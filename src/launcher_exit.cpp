#include "launcher_exit.h"

#include <esp_ota_ops.h>
#include <esp_partition.h>
#include <esp_system.h>

namespace launcher_exit {

void rebootToLauncher() {
    const esp_partition_t* factory = esp_partition_find_first(
        ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_FACTORY, nullptr);
    if (factory != nullptr) {
        esp_ota_set_boot_partition(factory);
    }
    esp_restart();
}

}  // namespace launcher_exit
