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
    // Site UI only — link messages never use these as titles.
    enum class Screen : uint8_t {
        List = 0,
        Running = 1,
        Result = 2,
    };

    enum class Pending : uint8_t {
        None = 0,
        List = 1,
        Run = 2,
    };

    void markDirty() { dirty_ = true; }
    void requestChromeFocus() { pending_chrome_focus_ = true; }
    void requestContentFocus() { pending_content_focus_ = true; }
    void setFooter(const char* msg);
    void clearFooter() { footer_[0] = '\0'; }
    void draw() const;
    void drawFooter() const;
    void drawList() const;
    void drawRunning() const;
    void drawResult() const;
    NavRequest handleInput(const Keyboard_Class::KeysState& status);
    void processBleFrames();
    bool ensureBle(Pending pending);
    void flushPending();
    void sendListReq();
    void sendRunReq(uint8_t index);
    void clearPresets();
    void loadPresetCache();
    void savePresetCache() const;
    void clearResult();
    void setActiveSite(const char* name);
    int listVisibleRows() const;
    void ensureCursorVisible();
    void onConnectFailed();
    void onListFailed(const char* footer_msg);
    void onRunFailed(const char* footer_msg);
    void retryRunOrFail();

    Screen screen_ = Screen::List;
    Pending pending_ = Pending::None;
    uint8_t pending_run_index_ = 0;
    char footer_[40] = {};
    char presets_[config::kHttpMaxPresets][config::kHttpNameMax + 1] = {};
    char active_site_[config::kHttpNameMax + 1] = {};
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
    bool run_got_meta_ = false;
    bool dirty_ = true;
    bool pending_chrome_focus_ = false;
    bool pending_content_focus_ = false;
    bool chrome_focused_ = false;
    bool was_ready_ = false;
    bool connect_failed_ = false;
    uint8_t connect_retries_ = 0;
    uint8_t list_retries_ = 0;
    uint8_t run_retries_ = 0;
    uint32_t connect_started_ms_ = 0;
    uint32_t list_started_ms_ = 0;
    uint32_t run_started_ms_ = 0;
};
