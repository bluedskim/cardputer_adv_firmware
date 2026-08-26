#include "apps/ir/ir_storage.h"

#include <Preferences.h>

#include <algorithm>

namespace {

std::string truncateName(const std::string& name) {
    if (name.size() <= config::kMaxNameLength) {
        return name;
    }
    return name.substr(0, config::kMaxNameLength);
}

}  // namespace

bool IrStorage::begin() {
    load();
    return true;
}

void IrStorage::load() {
    devices_.clear();

    Preferences prefs;
    if (!prefs.begin(config::kStorageNamespace, true)) {
        return;
    }

    const uint8_t device_count = prefs.getUChar("dev_count", 0);
    for (uint8_t device_index = 0; device_index < device_count; ++device_index) {
        const std::string prefix = "d" + std::to_string(device_index) + "_";

        IrDevice device;
        device.name = prefs.getString((prefix + "name").c_str(), "").c_str();
        if (device.name.empty()) {
            continue;
        }

        const uint8_t button_count = prefs.getUChar((prefix + "btn_count").c_str(), 0);
        for (uint8_t button_index = 0; button_index < button_count; ++button_index) {
            const std::string button_prefix = prefix + "b" + std::to_string(button_index) + "_";

            IrButton button;
            button.name = prefs.getString((button_prefix + "name").c_str(), "").c_str();
            button.protocol = prefs.getString((button_prefix + "proto").c_str(), "NEC").c_str();
            button.address = prefs.getUInt((button_prefix + "addr").c_str(), 0);
            button.command = prefs.getUInt((button_prefix + "cmd").c_str(), 0);
            button.bits = prefs.getUChar((button_prefix + "bits").c_str(), 32);

            if (!button.name.empty()) {
                device.buttons.push_back(button);
            }
        }

        devices_.push_back(std::move(device));
    }

    prefs.end();
}

bool IrStorage::save() const {
    Preferences prefs;
    if (!prefs.begin(config::kStorageNamespace, false)) {
        return false;
    }

    prefs.clear();
    const uint8_t device_count = static_cast<uint8_t>(std::min(devices_.size(), static_cast<std::size_t>(config::kMaxDevices)));
    prefs.putUChar("dev_count", device_count);

    for (uint8_t device_index = 0; device_index < device_count; ++device_index) {
        const auto& device = devices_[device_index];
        const std::string prefix = "d" + std::to_string(device_index) + "_";

        prefs.putString((prefix + "name").c_str(), device.name.c_str());

        const uint8_t button_count =
            static_cast<uint8_t>(std::min(device.buttons.size(), static_cast<std::size_t>(config::kMaxButtonsPerDevice)));
        prefs.putUChar((prefix + "btn_count").c_str(), button_count);

        for (uint8_t button_index = 0; button_index < button_count; ++button_index) {
            const auto& button = device.buttons[button_index];
            const std::string button_prefix = prefix + "b" + std::to_string(button_index) + "_";

            prefs.putString((button_prefix + "name").c_str(), button.name.c_str());
            prefs.putString((button_prefix + "proto").c_str(), button.protocol.c_str());
            prefs.putUInt((button_prefix + "addr").c_str(), button.address);
            prefs.putUInt((button_prefix + "cmd").c_str(), button.command);
            prefs.putUChar((button_prefix + "bits").c_str(), button.bits);
        }
    }

    prefs.end();
    return true;
}

void IrStorage::clear() {
    devices_.clear();
    Preferences prefs;
    if (prefs.begin(config::kStorageNamespace, false)) {
        prefs.clear();
        prefs.end();
    }
}

bool IrStorage::addDevice(const std::string& name) {
    if (devices_.size() >= config::kMaxDevices) {
        return false;
    }

    const std::string trimmed = truncateName(name);
    if (trimmed.empty()) {
        return false;
    }

    devices_.push_back(IrDevice{trimmed, {}});
    return save();
}

bool IrStorage::removeDevice(const std::size_t index) {
    if (index >= devices_.size()) {
        return false;
    }

    devices_.erase(devices_.begin() + static_cast<std::ptrdiff_t>(index));
    return save();
}

bool IrStorage::renameDevice(const std::size_t index, const std::string& name) {
    if (index >= devices_.size()) {
        return false;
    }

    const std::string trimmed = truncateName(name);
    if (trimmed.empty()) {
        return false;
    }

    devices_[index].name = trimmed;
    return save();
}

bool IrStorage::addButton(const std::size_t device_index, const IrButton& button) {
    if (device_index >= devices_.size()) {
        return false;
    }

    auto& device = devices_[device_index];
    if (device.buttons.size() >= config::kMaxButtonsPerDevice) {
        return false;
    }

    IrButton stored = button;
    stored.name = truncateName(button.name);
    if (stored.name.empty()) {
        return false;
    }

    device.buttons.push_back(std::move(stored));
    return save();
}

bool IrStorage::removeButton(const std::size_t device_index, const std::size_t button_index) {
    if (device_index >= devices_.size()) {
        return false;
    }

    auto& buttons = devices_[device_index].buttons;
    if (button_index >= buttons.size()) {
        return false;
    }

    buttons.erase(buttons.begin() + static_cast<std::ptrdiff_t>(button_index));
    return save();
}

bool IrStorage::updateButton(const std::size_t device_index, const std::size_t button_index, const IrButton& button) {
    if (device_index >= devices_.size()) {
        return false;
    }

    auto& buttons = devices_[device_index].buttons;
    if (button_index >= buttons.size()) {
        return false;
    }

    IrButton stored = button;
    stored.name = truncateName(button.name);
    if (stored.name.empty()) {
        return false;
    }

    buttons[button_index] = std::move(stored);
    return save();
}
