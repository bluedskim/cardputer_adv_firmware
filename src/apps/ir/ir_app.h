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
    NavRequest onUpdate(const Keyboard_Class::KeysState* keys) override;

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

    enum class ListAction : uint8_t {
        None = 0,
        New,
        Learn,
        Rename,
        RenameDevice,
        Delete,
        DeleteDevice,
    };

    enum class PendingDelete {
        None,
        Device,
        Button,
    };

    void draw() const;
    NavRequest handleInput(const Keyboard_Class::KeysState& status);
    void enterDeviceList();
    void enterDeviceDetail();
    void beginTextInput(InputPurpose purpose, const std::string& seed = "");
    void commitTextInput();
    void beginLearn();
    void beginConfirmDelete(PendingDelete kind);
    void confirmDelete();
    void pollLearn();
    void sendSelectedButton();
    void activateSelection();
    void goBack();
    void clampSelection();
    void ensureCursorVisible();
    int visibleRowCount() const;
    std::size_t listRowCount() const;
    bool isActionRow(std::size_t index, ListAction* out = nullptr) const;
    std::string screenTitle() const;
    bool consumeTextChar(char key);
    void markDirty() { dirty_ = true; }
    void ensureSender();
    void ensureReceiver();
    static const char* actionLabel(ListAction action);

    IrStorage storage_;
    Screen screen_ = Screen::DeviceList;
    InputPurpose input_purpose_ = InputPurpose::NewDevice;
    std::size_t selected_device_ = 0;
    std::size_t selected_button_ = 0;
    std::size_t cursor_ = 0;
    std::size_t scroll_ = 0;
    std::string input_buffer_;
    std::string status_message_;
    uint32_t status_until_ms_ = 0;
    bool status_visible_ = false;
    bool sender_ready_ = false;
    bool receiver_ready_ = false;
    bool dirty_ = true;
    PendingDelete pending_delete_ = PendingDelete::None;
    bool input_return_list_ = false;
};
