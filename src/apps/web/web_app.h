#pragma once

#include <cstdint>

#include "apps/app_base.h"
#include "ble/ble_link.h"
#include "config.h"

class WebApp final : public AppBase {
public:
    const char* name() const override;
    void onEnter() override;
    void onExit() override;
    void onDisplayWake() override;
    NavRequest onUpdate(const Keyboard_Class::KeysState* keys, bool chrome_focused) override;
    bool hasActiveBackgroundWork() const override;

private:
    enum class Screen : uint8_t {
        Status = 0,
        List = 1,
        Running = 2,
        Result = 3,
    };

    void markDirty() { dirty_ = true; }
    void requestChromeFocus() { pending_chrome_focus_ = true; }
    void draw() const;
    void drawStatus() const;
    void drawList() const;
    void drawRunning() const;
    void drawResult() const;
    NavRequest handleInput(const Keyboard_Class::KeysState& status);
    void processBleFrames();
    void requestList();
    void runSelected();
    void clearPresets();
    void clearResult();
    int visibleRows() const;
    void ensureCursorVisible();

    Screen screen_ = Screen::Status;
    char status_line_[28] = "BLE…";
    char presets_[config::kHttpMaxPresets][config::kHttpNameMax + 1] = {};
    uint8_t preset_count_ = 0;
    uint8_t cursor_ = 0;
    uint8_t scroll_ = 0;
    uint16_t list_req_id_ = 0;
    uint16_t run_req_id_ = 0;
    char result_body_[config::kHttpBodyMax + 1] = {};
    uint16_t result_len_ = 0;
    uint16_t result_http_status_ = 0;
    uint8_t result_err_ = 0;
    uint16_t result_scroll_ = 0;
    bool awaiting_list_ = false;
    bool awaiting_run_ = false;
    bool list_requested_ = false;
    bool was_link_up_ = false;
    bool dirty_ = true;
    bool pending_chrome_focus_ = false;
    bool chrome_focused_ = false;
    uint32_t connect_started_ms_ = 0;
};
