#pragma once

#include <cstddef>
#include <cstdint>

#include "apps/app_base.h"

class TunerApp final : public AppBase {
public:
    const char* name() const override;
    void onEnter() override;
    void onExit() override;
    void onDisplayWake() override;
    void onBackgroundTick() override;
    bool hasActiveBackgroundWork() const override;
    NavRequest onUpdate(const Keyboard_Class::KeysState* keys, bool header_focused) override;

private:
    void markDirty() { dirty_ = true; }
    bool beginListen();
    void stopListen();
    void tickMic();
    void pushChunk(const int16_t* samples);
    void analyze();
    void paintFooter();
    void draw();

    static constexpr std::size_t kChunkSamples = 512;
    static constexpr std::size_t kBufCount = 3;
    static constexpr std::size_t kWindowSamples = 2048;

    bool listening_ = false;
    bool mic_ok_ = false;
    bool have_pitch_ = false;
    bool in_tune_ = false;
    bool chrome_valid_ = false;
    bool smooth_ready_ = false;

    char note_name_[4] = "--";
    float cents_ = 0.0f;
    float cents_smooth_ = 0.0f;
    int cents_shown_ = 0;
    int needle_px_ = 0;

    int painted_needle_px_ = 0;
    int painted_cents_shown_ = 0;
    bool painted_have_pitch_ = false;
    bool painted_in_tune_ = false;
    bool painted_mic_ok_ = false;
    char painted_note_[4] = "";

    int16_t mic_buf_[kBufCount][kChunkSamples]{};
    std::size_t mic_queued_ = 0;

    int16_t window_[kWindowSamples]{};
    int16_t ordered_[kWindowSamples]{};
    std::size_t window_fill_ = 0;
    std::size_t window_pos_ = 0;
    std::size_t chunks_since_analyze_ = 0;

    bool dirty_ = true;
    bool header_focused_ = false;
};
