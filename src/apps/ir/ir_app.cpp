#include "apps/ir/ir_app.h"

#include <M5Cardputer.h>
#include <IRremote.hpp>

#include <algorithm>

#include "config.h"
#include "input/keyboard_input.h"

namespace {

constexpr int kContentTop = config::kContentTop;
constexpr int kLineHeight = 16;
constexpr int kListTop = kContentTop + 22;
constexpr int kFooterReserve = 14;
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

const char* IrApp::actionLabel(ListAction action) {
    switch (action) {
        case ListAction::New:
            return "+ New";
        case ListAction::Learn:
            return "+ Learn";
        case ListAction::Rename:
            return "* Rename";
        case ListAction::RenameDevice:
            return "* Dev name";
        case ListAction::Delete:
            return "x Delete";
        case ListAction::DeleteDevice:
            return "x Remove";
        default:
            return "";
    }
}

IrApp::IrApp() { storage_.begin(); }

IrApp::~IrApp() = default;

const char* IrApp::name() const { return "IR Remote"; }

void IrApp::onEnter() {
    enterDeviceList();
}

void IrApp::ensureSender() {
    if (sender_ready_) {
        return;
    }
    IrSender.begin(config::kIrTxPin);
    sender_ready_ = true;
}

void IrApp::ensureReceiver() {
    if (!receiver_ready_) {
        IrReceiver.begin(config::kIrRxPin, DISABLE_LED_FEEDBACK);
        receiver_ready_ = true;
    }
    IrReceiver.start();
    IrReceiver.resume();
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
        markDirty();
    }

    const bool status_now = !status_message_.empty() && status_until_ms_ > millis();
    if (status_now != status_visible_) {
        status_visible_ = status_now;
        markDirty();
    }

    if (dirty_) {
        draw();
        dirty_ = false;
    }
    return req;
}

void IrApp::clampSelection() {
    if (storage_.devices().empty()) {
        selected_device_ = 0;
        selected_button_ = 0;
        return;
    }
    selected_device_ = std::min(selected_device_, storage_.devices().size() - 1);
    const auto& device = storage_.devices()[selected_device_];
    if (device.buttons.empty()) {
        selected_button_ = 0;
    } else {
        selected_button_ = std::min(selected_button_, device.buttons.size() - 1);
    }
}

int IrApp::visibleRowCount() const {
    const int available = M5Cardputer.Display.height() - kFooterReserve - kListTop;
    return std::max(1, available / kLineHeight);
}

void IrApp::ensureCursorVisible() {
    const std::size_t visible = static_cast<std::size_t>(visibleRowCount());
    if (cursor_ < scroll_) {
        scroll_ = cursor_;
    } else if (cursor_ >= scroll_ + visible) {
        scroll_ = cursor_ + 1 - visible;
    }
}

void IrApp::enterDeviceList() {
    screen_ = Screen::DeviceList;
    clampSelection();
    cursor_ = storage_.devices().empty() ? 0 : selected_device_;
    scroll_ = 0;
    ensureCursorVisible();
    markDirty();
}

void IrApp::enterDeviceDetail() {
    screen_ = Screen::DeviceDetail;
    clampSelection();
    cursor_ = selected_button_;
    scroll_ = 0;
    ensureCursorVisible();
    markDirty();
}

std::size_t IrApp::listRowCount() const {
    if (screen_ == Screen::DeviceList) {
        const std::size_t n = storage_.devices().size();
        return n + 1;  // devices + New
    }
    if (screen_ == Screen::DeviceDetail && selected_device_ < storage_.devices().size()) {
        const std::size_t n = storage_.devices()[selected_device_].buttons.size();
        // Learn + Dev name + (Rename/Delete button) + Remove device
        return n + 3 + (n > 0 ? 2 : 0);
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
        return false;
    }

    if (screen_ == Screen::DeviceDetail && selected_device_ < storage_.devices().size()) {
        const std::size_t n = storage_.devices()[selected_device_].buttons.size();
        std::size_t i = n;
        if (index == i++) {
            if (out) {
                *out = ListAction::Learn;
            }
            return true;
        }
        if (index == i++) {
            if (out) {
                *out = ListAction::RenameDevice;
            }
            return true;
        }
        if (n > 0 && index == i++) {
            if (out) {
                *out = ListAction::Rename;
            }
            return true;
        }
        if (n > 0 && index == i++) {
            if (out) {
                *out = ListAction::Delete;
            }
            return true;
        }
        if (index == i) {
            if (out) {
                *out = ListAction::DeleteDevice;
            }
            return true;
        }
    }
    return false;
}

void IrApp::beginTextInput(const InputPurpose purpose, const std::string& seed) {
    input_return_list_ = (screen_ == Screen::DeviceList);
    screen_ = Screen::TextInput;
    input_purpose_ = purpose;
    input_buffer_ = seed;
    markDirty();
}

void IrApp::commitTextInput() {
    if (input_buffer_.empty()) {
        status_message_ = "Name required";
        status_until_ms_ = millis() + kStatusDurationMs;
        markDirty();
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
            if (input_return_list_) {
                enterDeviceList();
            } else {
                enterDeviceDetail();
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
    ensureReceiver();
    markDirty();
}

void IrApp::beginConfirmDelete(const PendingDelete kind) {
    pending_delete_ = kind;
    screen_ = Screen::ConfirmDelete;
    markDirty();
}

void IrApp::confirmDelete() {
    if (pending_delete_ == PendingDelete::Device) {
        if (storage_.removeDevice(selected_device_)) {
            status_message_ = "Device deleted";
            status_until_ms_ = millis() + kStatusDurationMs;
        }
        pending_delete_ = PendingDelete::None;
        selected_device_ = 0;
        enterDeviceList();
        return;
    }

    if (pending_delete_ == PendingDelete::Button) {
        if (storage_.removeButton(selected_device_, selected_button_)) {
            status_message_ = "Button deleted";
            status_until_ms_ = millis() + kStatusDurationMs;
        }
        pending_delete_ = PendingDelete::None;
        enterDeviceDetail();
        return;
    }

    pending_delete_ = PendingDelete::None;
    goBack();
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

void IrApp::sendSelectedButton() {
    if (selected_device_ >= storage_.devices().size()) {
        return;
    }
    ensureSender();

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
                beginTextInput(InputPurpose::NewDevice);
                break;
            case ListAction::Learn:
                input_buffer_ = "Button";
                beginLearn();
                break;
            case ListAction::Rename:
                if (selected_device_ < storage_.devices().size()) {
                    const auto& device = storage_.devices()[selected_device_];
                    if (selected_button_ < device.buttons.size()) {
                        beginTextInput(InputPurpose::RenameButton, device.buttons[selected_button_].name);
                    }
                }
                break;
            case ListAction::RenameDevice:
                if (selected_device_ < storage_.devices().size()) {
                    beginTextInput(InputPurpose::RenameDevice, storage_.devices()[selected_device_].name);
                }
                break;
            case ListAction::Delete:
                beginConfirmDelete(PendingDelete::Button);
                break;
            case ListAction::DeleteDevice:
                beginConfirmDelete(PendingDelete::Device);
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
        markDirty();
    }
}

void IrApp::goBack() {
    if (screen_ == Screen::ConfirmDelete) {
        const PendingDelete kind = pending_delete_;
        pending_delete_ = PendingDelete::None;
        if (kind == PendingDelete::Device) {
            enterDeviceList();
        } else {
            enterDeviceDetail();
        }
        return;
    }
    pending_delete_ = PendingDelete::None;
    if (screen_ == Screen::TextInput) {
        if (input_purpose_ == InputPurpose::RenameButton || input_purpose_ == InputPurpose::NewButton) {
            enterDeviceDetail();
        } else {
            enterDeviceList();
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
        case Screen::ConfirmDelete:
            return "Confirm";
    }
    return "IR";
}

NavRequest IrApp::handleInput(const Keyboard_Class::KeysState& status) {
    if (screen_ == Screen::ConfirmDelete) {
        if (status.enter) {
            confirmDelete();
            return NavRequest::None;
        }
        if (status.del || detectArrowKey(status) == ArrowKey::Left) {
            goBack();
            return NavRequest::None;
        }
        return NavRequest::None;
    }

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
            ensureCursorVisible();
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
            ensureCursorVisible();
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

    int y = kListTop;

    if (screen_ == Screen::ConfirmDelete) {
        display.setTextSize(2);
        display.setTextColor(TFT_ORANGE, TFT_BLACK);
        display.setCursor(8, y);
        if (pending_delete_ == PendingDelete::Device && selected_device_ < storage_.devices().size()) {
            display.printf("Del %s?", storage_.devices()[selected_device_].name.c_str());
        } else if (pending_delete_ == PendingDelete::Button && selected_device_ < storage_.devices().size()) {
            const auto& device = storage_.devices()[selected_device_];
            if (selected_button_ < device.buttons.size()) {
                display.printf("Del %s?", device.buttons[selected_button_].name.c_str());
            }
        } else {
            display.print("Delete?");
        }
        display.setTextSize(1);
        display.setTextColor(TFT_DARKGREY, TFT_BLACK);
        display.setCursor(8, y + 36);
        display.print("Ent yes  < no");
        return;
    }

    if (screen_ == Screen::DeviceList || screen_ == Screen::DeviceDetail) {
        const std::size_t rows = listRowCount();
        const std::size_t visible = static_cast<std::size_t>(visibleRowCount());
        const int width = display.width();
        const std::size_t end = std::min(rows, scroll_ + visible);
        for (std::size_t i = scroll_; i < end; ++i) {
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
                display.printf("%c %s", selected ? '>' : ' ', actionLabel(action));
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
