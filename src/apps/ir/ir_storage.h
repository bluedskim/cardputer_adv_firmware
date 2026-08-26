#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "config.h"

struct IrButton {
    std::string name;
    std::string protocol;
    uint32_t address = 0;
    uint32_t command = 0;
    uint8_t bits = 32;
};

struct IrDevice {
    std::string name;
    std::vector<IrButton> buttons;
};

class IrStorage {
public:
    bool begin();
    void load();
    bool save() const;
    void clear();

    const std::vector<IrDevice>& devices() const { return devices_; }
    std::vector<IrDevice>& devices() { return devices_; }

    bool addDevice(const std::string& name);
    bool removeDevice(std::size_t index);
    bool renameDevice(std::size_t index, const std::string& name);

    bool addButton(std::size_t device_index, const IrButton& button);
    bool removeButton(std::size_t device_index, std::size_t button_index);
    bool updateButton(std::size_t device_index, std::size_t button_index, const IrButton& button);

private:
    std::vector<IrDevice> devices_;
};
