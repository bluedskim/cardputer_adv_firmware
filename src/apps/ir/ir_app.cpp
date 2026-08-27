#include "apps/ir/ir_app.h"

#include <M5Cardputer.h>
#include <IRremote.hpp>

#include <algorithm>

#include "config.h"
#include "input/keyboard_input.h"

namespace {

constexpr int kContentTop = config::kContentTop;
constexpr int kLineHeight = 16;
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

NavRequest IrApp::onUpdate(const Keyboard_Class::KeysState* keys) {
    if (screen_ == Screen::Learn) {
        pollLearn();
    }

    NavRequest req = NavRequest::None;
    if (keys != nullptr) {
        req = handleInput(*keys);
    }
    draw();
    return req;
}

void IrApp::enterDeviceList() {
    screen_ = Screen::DeviceList;
    selected_device_ = 0;
    cursor_ = 0;
    if (!storage_.devices().empty()) {
        selected_device_ = std::min(selected_device_, storage_.devices().size() - 1);
        cursor_ = selected_device_;
    }
}

void IrApp::enterDeviceDetail() {
    screen_ = Screen::DeviceDetail;
    selected_button_ = 0;
    cursor_ = 0;
    if (selected_device_ < storage_.devices().size()) {
        const auto& device = storage_.devices()[selected_device_];
        if (!device.buttons.empty()) {
            selected_button_ = std::min(selected_button_, device.buttons.size() - 1);
            cursor_ = selected_button_;
        }
    }
}

std::size_t IrApp::listRowCount() const {
    if (screen_ == Screen::DeviceList) {
        // devices + New (+ Delete if any device)
        const std::size_t n = storage_.devices().size();
        return n + 1 + (n > 0 ? 1 : 0);
    }
    if (screen_ == Screen::DeviceDetail && selected_device_ < storage_.devices().size()) {
        const std::size_t n = storage_.devices()[selected_device_].buttons.size();
        // buttons + Learn + Rename + Delete
        return n + 3;
    }
    return 0;
}

bool IrApp::isActionRow(const std::size_t index, ListAction* out) const {
    if (screen_ == Screen::DeviceList) {
        const std::size_t n = storage_.devices().size();
        if (index == n) {
            if (out) {
                *out = ListAction::New;
            }
            return true;
        }
        if (n > 0 && index == n + 1) {
            if (out) {
                *out = ListAction::Delete;
            }
            return true;
        }
        return false;
    }

    if (screen_ == Screen::DeviceDetail && selected_device_ < storage_.devices().size()) {
        const std::size_t n = storage_.devices()[selected_device_].buttons.size();
        if (index == n) {
            if (out) {
                *out = ListAction::Learn;
            }
            return true;
        }
        if (index == n + 1) {
            if (out) {
                *out = ListAction::Rename;
            }
            return true;
        }
        if (index == n + 2) {
            if (out) {
                *out = ListAction::Delete;
            }
            return true;
        }
    }
    return false;
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
                enterDeviceList();
            }
            break;
        case InputPurpose::RenameDevice:
            if (storage_.renameDevice(selected_device_, input_buffer_)) {
                status_message_ = "Device renamed";
            } else {
                status_message_ = "Rename failed";
            }
            enterDeviceDetail();
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
                    } else {
                        status_message_ = "Rename failed";
                    }
                }
            }
            enterDeviceDetail();
            break;
    }

    status_until_ms_ = millis() + kStatusDurationMs;
}

void IrApp::beginLearn() {
    screen_ = Screen::Learn;
    status_message_ = "Point remote & press";
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

void IrApp::activateSelection() {
    ListAction action = ListAction::None;
    if (isActionRow(cursor_, &action)) {
        switch (action) {
            case ListAction::New:
                if (screen_ == Screen::DeviceList) {
                    beginTextInput(InputPurpose::NewDevice);
                }
                break;
            case ListAction::Learn:
                input_buffer_ = "Button";
                beginLearn();
                break;
            case ListAction::Rename:
                if (screen_ == Screen::DeviceDetail && selected_device_ < storage_.devices().size()) {
                    const auto& device = storage_.devices()[selected_device_];
                    if (!device.buttons.empty() && selected_button_ < device.buttons.size()) {
                        beginTextInput(InputPurpose::RenameButton, device.buttons[selected_button_].name);
                    } else {
                        beginTextInput(InputPurpose::RenameDevice, device.name);
                    }
                }
                break;
            case ListAction::Delete:
                if (screen_ == Screen::DeviceList) {
                    if (storage_.removeDevice(selected_device_)) {
                        status_message_ = "Device deleted";
                        status_until_ms_ = millis() + kStatusDurationMs;
                    }
                    enterDeviceList();
                } else if (screen_ == Screen::DeviceDetail) {
                    if (selected_device_ < storage_.devices().size()) {
                        const auto& device = storage_.devices()[selected_device_];
                        if (!device.buttons.empty()) {
                            if (storage_.removeButton(selected_device_, selected_button_)) {
                                status_message_ = "Button deleted";
                                status_until_ms_ = millis() + kStatusDurationMs;
                            }
                            enterDeviceDetail();
                        } else if (storage_.removeDevice(selected_device_)) {
                            status_message_ = "Device deleted";
                            status_until_ms_ = millis() + kStatusDurationMs;
                            enterDeviceList();
                        }
                    }
                }
                break;
            default:
                break;
        }
        return;
    }

    if (screen_ == Screen::DeviceList) {
        selected_device_ = cursor_;
        enterDeviceDetail();
        return;
    }

    if (screen_ == Screen::DeviceDetail) {
        selected_button_ = cursor_;
        sendSelectedButton();
        status_message_ = "IR sent";
        status_until_ms_ = millis() + kStatusDurationMs;
    }
}

void IrApp::goBack() {
    if (screen_ == Screen::TextInput) {
        if (input_purpose_ == InputPurpose::NewDevice) {
            enterDeviceList();
        } else {
            enterDeviceDetail();
        }
        return;
    }
    if (screen_ == Screen::Learn) {
        enterDeviceDetail();
        return;
    }
    if (screen_ == Screen::DeviceDetail) {
        enterDeviceList();
    }
}

bool IrApp::consumeTextChar(const char key) {
    if (key >= 32 && key <= 126 && !isArrowChar(key) &&
        input_buffer_.size() < config::kMaxNameLength) {
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
    }
    return "IR";
}

NavRequest IrApp::handleInput(const Keyboard_Class::KeysState& status) {
    if (screen_ == Screen::TextInput) {
        if (status.enter) {
            commitTextInput();
            return NavRequest::None;
        }
        if (status.del) {
            if (!input_buffer_.empty()) {
                input_buffer_.pop_back();
            }
            return NavRequest::None;
        }
        if (detectArrowKey(status) == ArrowKey::Left) {
            goBack();
            return NavRequest::None;
        }
        for (const char key : status.word) {
            consumeTextChar(key);
        }
        return NavRequest::None;
    }

    if (screen_ == Screen::Learn) {
        if (status.del || detectArrowKey(status) == ArrowKey::Left) {
            goBack();
        }
        return NavRequest::None;
    }

    if (status.del || detectArrowKey(status) == ArrowKey::Left) {
        if (screen_ == Screen::DeviceDetail) {
            goBack();
            return NavRequest::None;
        }
        // Root list: Left does nothing; Up-from-top goes to chrome.
    }

    if (status.enter) {
        activateSelection();
        return NavRequest::None;
    }

    switch (detectArrowKey(status)) {
        case ArrowKey::Up: {
            if (cursor_ == 0) {
                return NavRequest::FocusChrome;
            }
            --cursor_;
            if (screen_ == Screen::DeviceList && cursor_ < storage_.devices().size()) {
                selected_device_ = cursor_;
            }
            if (screen_ == Screen::DeviceDetail && selected_device_ < storage_.devices().size()) {
                const std::size_t n = storage_.devices()[selected_device_].buttons.size();
                if (cursor_ < n) {
                    selected_button_ = cursor_;
                }
            }
            return NavRequest::None;
        }
        case ArrowKey::Down: {
            const std::size_t rows = listRowCount();
            if (rows == 0) {
                return NavRequest::None;
            }
            if (cursor_ + 1 < rows) {
                ++cursor_;
            }
            if (screen_ == Screen::DeviceList && cursor_ < storage_.devices().size()) {
                selected_device_ = cursor_;
            }
            if (screen_ == Screen::DeviceDetail && selected_device_ < storage_.devices().size()) {
                const std::size_t n = storage_.devices()[selected_device_].buttons.size();
                if (cursor_ < n) {
                    selected_button_ = cursor_;
                }
            }
            return NavRequest::None;
        }
        default:
            break;
    }

    return NavRequest::None;
}

void IrApp::draw() const {
    auto& display = M5Cardputer.Display;
    display.fillRect(0, kContentTop, display.width(), display.height() - kContentTop, TFT_BLACK);
    display.setTextSize(2);
    display.setTextColor(TFT_CYAN, TFT_BLACK);
    display.setCursor(4, kContentTop + 2);
    display.println(screenTitle().c_str());

    int y = kContentTop + 22;

    if (screen_ == Screen::DeviceList || screen_ == Screen::DeviceDetail) {
        const std::size_t rows = listRowCount();
        const int width = display.width();
        for (std::size_t i = 0; i < rows; ++i) {
            const bool selected = i == cursor_;
            const uint16_t fg = selected ? TFT_YELLOW : TFT_WHITE;
            const uint16_t bg = selected ? TFT_NAVY : TFT_BLACK;
            if (selected) {
                display.fillRect(0, y, width, kLineHeight, bg);
            }
            display.setTextColor(fg, bg);
            display.setTextSize(2);
            display.setCursor(8, y);

            ListAction action = ListAction::None;
            if (isActionRow(i, &action)) {
                const char* label = "";
                switch (action) {
                    case ListAction::New:
                        label = "+ New";
                        break;
                    case ListAction::Learn:
                        label = "+ Learn";
                        break;
                    case ListAction::Rename:
                        label = "* Rename";
                        break;
                    case ListAction::Delete:
                        label = "x Delete";
                        break;
                    default:
                        break;
                }
                display.printf("%c %s", selected ? '>' : ' ', label);
            } else if (screen_ == Screen::DeviceList) {
                display.printf("%c %s (%u)",
                               selected ? '>' : ' ',
                               storage_.devices()[i].name.c_str(),
                               static_cast<unsigned>(storage_.devices()[i].buttons.size()));
            } else if (selected_device_ < storage_.devices().size()) {
                display.printf("%c %s",
                               selected ? '>' : ' ',
                               storage_.devices()[selected_device_].buttons[i].name.c_str());
            }

            y += kLineHeight;
            if (y > display.height() - 24) {
                break;
            }
        }

        display.setTextSize(1);
        display.setTextColor(TFT_DARKGREY, TFT_BLACK);
        display.setCursor(4, display.height() - 12);
        display.print("^v move  Ent ok  < back");
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
        display.println("Ent save  Del erase  < back");
    } else if (screen_ == Screen::Learn) {
        display.setTextSize(2);
        display.setTextColor(TFT_ORANGE, TFT_BLACK);
        display.setCursor(8, y);
        display.println(status_message_.c_str());
        display.setTextSize(1);
        display.setCursor(8, y + 28);
        display.setTextColor(TFT_DARKGREY, TFT_BLACK);
        display.println("< or Del cancel");
    }

    if (!status_message_.empty() && status_until_ms_ > millis()) {
        display.fillRect(0, display.height() - 18, display.width(), 18, TFT_NAVY);
        display.setTextSize(1);
        display.setTextColor(TFT_WHITE, TFT_NAVY);
        display.setCursor(4, display.height() - 14);
        display.print(status_message_.c_str());
    }
}
