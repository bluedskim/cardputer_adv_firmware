#include "apps/ir/ir_app.h"

#include <M5Cardputer.h>
#include <IRremote.hpp>

#include <algorithm>

#include "config.h"
#include "input/keyboard_input.h"

namespace {

constexpr int kContentTop = 24;
constexpr int kLineHeight = 18;
constexpr uint32_t kStatusDurationMs = 1800;

std::string protocolToString(decode_type_t protocol) {
    switch (protocol) {
        case NEC:
            return "NEC";
        case SONY:
            return "SONY";
        case RC5:
            return "RC5";
        case SAMSUNG:
            return "SAMSUNG";
        case LG:
            return "LG";
        default:
            return "RAW";
    }
}

void sendStoredButton(const IrButton& button) {
    if (button.protocol == "NEC") {
        IrSender.sendNEC(static_cast<uint16_t>(button.address), static_cast<uint8_t>(button.command), 1);
        return;
    }

    if (button.protocol == "SONY") {
        IrSender.sendSony(button.address, static_cast<uint8_t>(button.command), 0, 1);
        return;
    }

    IrSender.sendNEC(static_cast<uint16_t>(button.address), static_cast<uint8_t>(button.command), 1);
}

}  // namespace

IrApp::IrApp() { storage_.begin(); }

IrApp::~IrApp() = default;

const char* IrApp::name() const { return "IR Remote"; }

void IrApp::onEnter() {
    if (!receiver_ready_) {
        IrSender.begin(DISABLE_LED_FEEDBACK);
        IrSender.setSendPin(config::kIrTxPin);
        IrReceiver.begin(config::kIrRxPin, ENABLE_LED_FEEDBACK);
        IrReceiver.start();
        receiver_ready_ = true;
    }

    enterDeviceList();
}

void IrApp::onExit() {
    if (receiver_ready_) {
        IrReceiver.stop();
    }
    M5Cardputer.Display.fillScreen(TFT_BLACK);
}

void IrApp::onUpdate(const Keyboard_Class::KeysState* keys) {
    if (screen_ == Screen::Learn) {
        pollLearn();
    }

    if (keys != nullptr) {
        handleInput(*keys);
    }
    draw();
}

void IrApp::enterDeviceList() {
    screen_ = Screen::DeviceList;
    selected_device_ = 0;
    if (!storage_.devices().empty()) {
        selected_device_ = std::min(selected_device_, storage_.devices().size() - 1);
    }
}

void IrApp::enterDeviceDetail() {
    screen_ = Screen::DeviceDetail;
    selected_button_ = 0;
    if (!storage_.devices().empty()) {
        const auto& device = storage_.devices()[selected_device_];
        if (!device.buttons.empty()) {
            selected_button_ = std::min(selected_button_, device.buttons.size() - 1);
        }
    }
}

void IrApp::beginTextInput(const InputPurpose purpose, const std::string& seed) {
    screen_ = Screen::TextInput;
    input_purpose_ = purpose;
    input_buffer_ = seed;
}

void IrApp::commitTextInput() {
    if (input_buffer_.empty()) {
        status_message_ = "Name required";
        status_until_ms_ = millis() + kStatusDurationMs;
        return;
    }

    switch (input_purpose_) {
        case InputPurpose::NewDevice:
            if (storage_.addDevice(input_buffer_)) {
                selected_device_ = storage_.devices().size() - 1;
                status_message_ = "Device added";
                enterDeviceDetail();
            } else {
                status_message_ = "Add device failed";
            }
            break;
        case InputPurpose::RenameDevice:
            if (storage_.renameDevice(selected_device_, input_buffer_)) {
                status_message_ = "Device renamed";
                enterDeviceDetail();
            } else {
                status_message_ = "Rename failed";
            }
            break;
        case InputPurpose::NewButton:
            beginLearn();
            return;
        case InputPurpose::RenameButton:
            if (selected_device_ < storage_.devices().size()) {
                auto& device = storage_.devices()[selected_device_];
                if (selected_button_ < device.buttons.size()) {
                    IrButton button = device.buttons[selected_button_];
                    button.name = input_buffer_;
                    if (storage_.updateButton(selected_device_, selected_button_, button)) {
                        status_message_ = "Button renamed";
                        enterDeviceDetail();
                    } else {
                        status_message_ = "Rename failed";
                    }
                }
            }
            break;
    }

    status_until_ms_ = millis() + kStatusDurationMs;
    if (screen_ == Screen::TextInput) {
        enterDeviceList();
    }
}

void IrApp::beginLearn() {
    screen_ = Screen::Learn;
    status_message_ = "Point remote and press key";
    status_until_ms_ = 0;
    IrReceiver.start();
    IrReceiver.resume();
}

void IrApp::pollLearn() {
    if (!IrReceiver.decode()) {
        return;
    }

    const auto& data = IrReceiver.decodedIRData;
    IrButton button;
    button.name = input_buffer_.empty() ? "Button" : input_buffer_;
    button.protocol = protocolToString(data.protocol);
    button.address = data.address;
    button.command = data.command;
    button.bits = data.numberOfBits;

    if (storage_.addButton(selected_device_, button)) {
        selected_button_ = storage_.devices()[selected_device_].buttons.size() - 1;
        status_message_ = "IR code saved";
    } else {
        status_message_ = "Save failed";
    }

    status_until_ms_ = millis() + kStatusDurationMs;
    IrReceiver.resume();
    enterDeviceDetail();
}

void IrApp::sendSelectedButton() const {
    if (selected_device_ >= storage_.devices().size()) {
        return;
    }

    const auto& device = storage_.devices()[selected_device_];
    if (selected_button_ >= device.buttons.size()) {
        return;
    }

    sendStoredButton(device.buttons[selected_button_]);
}

void IrApp::deleteSelected() {
    if (screen_ == Screen::DeviceList) {
        if (storage_.removeDevice(selected_device_)) {
            status_message_ = "Device deleted";
            if (storage_.devices().empty()) {
                selected_device_ = 0;
            } else {
                selected_device_ = std::min(selected_device_, storage_.devices().size() - 1);
            }
        }
    } else if (screen_ == Screen::DeviceDetail) {
        if (storage_.removeButton(selected_device_, selected_button_)) {
            status_message_ = "Button deleted";
            const auto& device = storage_.devices()[selected_device_];
            if (device.buttons.empty()) {
                selected_button_ = 0;
            } else {
                selected_button_ = std::min(selected_button_, device.buttons.size() - 1);
            }
        }
    }

    status_until_ms_ = millis() + kStatusDurationMs;
}

void IrApp::moveSelection(const int delta) {
    if (screen_ == Screen::DeviceList) {
        if (storage_.devices().empty()) {
            return;
        }
        if (delta < 0) {
            selected_device_ = selected_device_ == 0 ? storage_.devices().size() - 1 : selected_device_ - 1;
        } else {
            selected_device_ = (selected_device_ + 1) % storage_.devices().size();
        }
        return;
    }

    if (screen_ == Screen::DeviceDetail && selected_device_ < storage_.devices().size()) {
        const auto& device = storage_.devices()[selected_device_];
        if (device.buttons.empty()) {
            return;
        }
        if (delta < 0) {
            selected_button_ = selected_button_ == 0 ? device.buttons.size() - 1 : selected_button_ - 1;
        } else {
            selected_button_ = (selected_button_ + 1) % device.buttons.size();
        }
    }
}

bool IrApp::consumeTextChar(const char key) {
    if (key == '\b' || key == 127) {
        if (!input_buffer_.empty()) {
            input_buffer_.pop_back();
        }
        return true;
    }

    if (key >= 32 && key <= 126 && input_buffer_.size() < config::kMaxNameLength) {
        input_buffer_.push_back(key);
        return true;
    }

    return false;
}

std::string IrApp::screenTitle() const {
    switch (screen_) {
        case Screen::DeviceList:
            return "Devices";
        case Screen::DeviceDetail:
            if (selected_device_ < storage_.devices().size()) {
                return storage_.devices()[selected_device_].name;
            }
            return "Device";
        case Screen::TextInput:
            return "Enter name";
        case Screen::Learn:
            return "Learning IR";
        case Screen::ConfirmDelete:
            return "Confirm delete";
    }
    return "IR";
}

void IrApp::handleInput(const Keyboard_Class::KeysState& status) {
    if (status.del) {
        if (screen_ == Screen::TextInput) {
            if (!input_buffer_.empty()) {
                input_buffer_.pop_back();
            }
            return;
        }
        if (screen_ == Screen::DeviceDetail) {
            enterDeviceList();
            return;
        }
        if (screen_ == Screen::Learn) {
            enterDeviceDetail();
            return;
        }
        deleteSelected();
        return;
    }

    if (status.enter) {
        if (screen_ == Screen::DeviceList) {
            if (!storage_.devices().empty()) {
                enterDeviceDetail();
            }
            return;
        }
        if (screen_ == Screen::DeviceDetail) {
            sendSelectedButton();
            status_message_ = "IR sent";
            status_until_ms_ = millis() + kStatusDurationMs;
            return;
        }
        if (screen_ == Screen::TextInput) {
            commitTextInput();
            return;
        }
        return;
    }

    if (screen_ == Screen::DeviceList || screen_ == Screen::DeviceDetail) {
        switch (detectArrowKey(status)) {
            case ArrowKey::Up:
                moveSelection(-1);
                return;
            case ArrowKey::Down:
                moveSelection(1);
                return;
            default:
                break;
        }
    }

    for (const auto key : status.word) {
        if (screen_ == Screen::TextInput) {
            consumeTextChar(key);
            continue;
        }

        if (screen_ == Screen::DeviceList && (key == 'n' || key == 'N')) {
            beginTextInput(InputPurpose::NewDevice);
            continue;
        }

        if (screen_ == Screen::DeviceDetail) {
            if (key == 'n' || key == 'N') {
                beginTextInput(InputPurpose::NewButton);
            } else if (key == 'r' || key == 'R') {
                input_buffer_ = "Button";
                beginLearn();
            } else if (key == 'e' || key == 'E') {
                if (selected_device_ < storage_.devices().size()) {
                    beginTextInput(InputPurpose::RenameDevice, storage_.devices()[selected_device_].name);
                }
            } else if (key == 'm' || key == 'M') {
                if (selected_device_ < storage_.devices().size()) {
                    const auto& device = storage_.devices()[selected_device_];
                    if (selected_button_ < device.buttons.size()) {
                        beginTextInput(InputPurpose::RenameButton, device.buttons[selected_button_].name);
                    }
                }
            } else if (key == 's' || key == 'S') {
                sendSelectedButton();
                status_message_ = "IR sent";
                status_until_ms_ = millis() + kStatusDurationMs;
            }
        }
    }
}

void IrApp::draw() const {
    auto& display = M5Cardputer.Display;
    display.fillRect(0, kContentTop, display.width(), display.height() - kContentTop, TFT_BLACK);
    display.setTextSize(2);
    display.setTextColor(TFT_CYAN, TFT_BLACK);
    display.setCursor(4, kContentTop + 2);
    display.println(screenTitle().c_str());

    int y = kContentTop + 24;

    if (screen_ == Screen::DeviceList) {
        if (storage_.devices().empty()) {
            display.setTextColor(TFT_WHITE, TFT_BLACK);
            display.setCursor(8, y);
            display.println("No devices");
            display.setCursor(8, y + kLineHeight);
            display.println("Press N");
        } else {
            for (std::size_t i = 0; i < storage_.devices().size(); ++i) {
                const bool selected = i == selected_device_;
                display.setTextColor(selected ? TFT_YELLOW : TFT_WHITE, TFT_BLACK);
                display.setCursor(8, y);
                display.printf("%c %s (%u)",
                               selected ? '>' : ' ',
                               storage_.devices()[i].name.c_str(),
                               static_cast<unsigned>(storage_.devices()[i].buttons.size()));
                y += kLineHeight;
                if (y > display.height() - 40) {
                    break;
                }
            }
        }

        display.setTextSize(1);
        display.setTextColor(TFT_DARKGREY, TFT_BLACK);
        display.setCursor(4, display.height() - 28);
        display.println("; . move  Enter open  N new");
    } else if (screen_ == Screen::DeviceDetail) {
        if (selected_device_ < storage_.devices().size()) {
            const auto& device = storage_.devices()[selected_device_];
            if (device.buttons.empty()) {
                display.setTextColor(TFT_WHITE, TFT_BLACK);
                display.setCursor(8, y);
                display.println("No buttons");
                display.setCursor(8, y + kLineHeight);
                display.println("Press R");
            } else {
                for (std::size_t i = 0; i < device.buttons.size(); ++i) {
                    const bool selected = i == selected_button_;
                    const auto& button = device.buttons[i];
                    display.setTextColor(selected ? TFT_YELLOW : TFT_WHITE, TFT_BLACK);
                    display.setCursor(8, y);
                    display.printf("%c %s", selected ? '>' : ' ', button.name.c_str());
                    y += kLineHeight;
                    if (y > display.height() - 48) {
                        break;
                    }
                }
            }
        }

        display.setTextSize(1);
        display.setTextColor(TFT_DARKGREY, TFT_BLACK);
        display.setCursor(4, display.height() - 40);
        display.println("; . move  Enter send  R learn");
        display.setCursor(4, display.height() - 28);
        display.println("M rename  E device  Del back");
    } else if (screen_ == Screen::TextInput) {
        display.setTextSize(2);
        display.setTextColor(TFT_WHITE, TFT_BLACK);
        display.setCursor(8, y);
        display.print("> ");
        display.print(input_buffer_.c_str());
        display.print("_");
        display.setTextSize(1);
        display.setCursor(8, y + 28);
        display.setTextColor(TFT_DARKGREY, TFT_BLACK);
        display.println("Enter save  Del erase");
    } else if (screen_ == Screen::Learn) {
        display.setTextSize(2);
        display.setTextColor(TFT_ORANGE, TFT_BLACK);
        display.setCursor(8, y);
        display.println(status_message_.c_str());
        display.setTextSize(1);
        display.setCursor(8, y + 28);
        display.setTextColor(TFT_DARKGREY, TFT_BLACK);
        display.println("Del cancel  I inject test IR");
    }

    if (!status_message_.empty() && status_until_ms_ > millis()) {
        display.fillRect(0, display.height() - 18, display.width(), 18, TFT_NAVY);
        display.setTextSize(1);
        display.setTextColor(TFT_WHITE, TFT_NAVY);
        display.setCursor(4, display.height() - 14);
        display.print(status_message_.c_str());
    }
}
