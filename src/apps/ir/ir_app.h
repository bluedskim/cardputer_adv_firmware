#pragma once

#include <cstdint>
#include <string>

#include "apps/app_base.h"
#include "apps/ir/ir_storage.h"

class IrApp final : public AppBase {
public:
    IrApp();
    ~IrApp() override;

    const char* name() const override;
    void onEnter() override;
    void onExit() override;
    void onUpdate(const Keyboard_Class::KeysState* keys) override;

private:
    enum class Screen {
        DeviceList,
        DeviceDetail,
        TextInput,
        Learn,
        ConfirmDelete,
    };

    enum class InputPurpose {
        NewDevice,
        RenameDevice,
        NewButton,
        RenameButton,
    };

    void draw() const;
    void handleInput(const Keyboard_Class::KeysState& status);
    void enterDeviceList();
    void enterDeviceDetail();
    void beginTextInput(InputPurpose purpose, const std::string& seed = "");
    void commitTextInput();
    void beginLearn();
    void pollLearn();
    void sendSelectedButton() const;
    void deleteSelected();
    void moveSelection(int delta);
    bool consumeTextChar(char key);
    std::string screenTitle() const;

    IrStorage storage_;
    Screen screen_ = Screen::DeviceList;
    InputPurpose input_purpose_ = InputPurpose::NewDevice;
    std::size_t selected_device_ = 0;
    std::size_t selected_button_ = 0;
    std::string input_buffer_;
    std::string status_message_;
    uint32_t status_until_ms_ = 0;
    bool receiver_ready_ = false;
};
