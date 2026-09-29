#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "apps/app_base.h"
#include "apps/recorder/wav_io.h"
#include "storage/sd_fs.h"

#ifndef CARDPUTER_SIM
#include <SD.h>
#endif

class RecorderApp final : public AppBase {
public:
    RecorderApp();

    const char* name() const override;
    void onEnter() override;
    void onExit() override;
    void onDisplayWake() override;
    void onBackgroundTick() override;
    bool hasActiveBackgroundWork() const override;
    bool isVoiceRecorder() const override { return true; }
    bool voiceRecorderIsRecording() const override;
    void voiceRecorderHotkey() override;
    NavRequest onUpdate(const Keyboard_Class::KeysState* keys, bool header_focused) override;

private:
    enum class Screen : uint8_t {
        List,
        Record,
        Play,
        ConfirmDelete,
        Message,
    };

    enum class ListAction : uint8_t {
        File,
        NewRecording,
    };

    void draw() const;
    NavRequest handleInput(const Keyboard_Class::KeysState& status);
    void refreshFileList();
    void markDirty() { dirty_ = true; }
    void requestHeaderFocus() { pending_header_focus_ = true; }
    bool isRootScreen() const { return screen_ == Screen::List; }

    std::size_t listRowCount() const;
    ListAction rowAction(std::size_t row) const;
    void activateSelection();
    void goBack();
    void showMessage(const char* text);

    bool beginRecording();
    void stopRecording(bool save);
    void tickRecording();
    bool queueRecordChunk();
    bool writeRecordChunk(std::size_t slot);
    void flushRecordTail();
    bool beginPlayback();
    void stopPlayback();
    void tickPlayback();
    void releaseAudio();

    void formatMmSs(char* buf, std::size_t len, uint32_t ms) const;
    uint32_t recordingElapsedMs() const;
    uint16_t peakLevel(const int16_t* samples, std::size_t count) const;

    Screen screen_ = Screen::List;
    std::vector<SdFs::WavEntry> files_;
    std::size_t cursor_ = 0;
    std::size_t scroll_ = 0;
    std::size_t selected_ = 0;
    bool sd_ok_ = false;
    std::string message_;

    // Record state
    bool recording_ = false;
    uint32_t record_start_ms_ = 0;
    uint32_t record_data_bytes_ = 0;
    uint16_t level_peak_ = 0;
    char record_path_[40]{};
    char record_name_[20]{};
#ifndef CARDPUTER_SIM
    File record_file_;
#endif
    // Three chunks: two stay queued in the mic task, the oldest is written.
    static constexpr std::size_t kRecChunkSamples = 512;
    static constexpr std::size_t kRecBufCount = 3;
    int16_t rec_buf_[kRecBufCount][kRecChunkSamples]{};
    std::size_t rec_queued_ = 0;

    // Play state
    bool playing_ = false;
    uint32_t play_data_bytes_ = 0;
    uint32_t play_bytes_done_ = 0;
    uint32_t play_sample_rate_ = 16000;
    WavIo::Info play_info_{};
#ifndef CARDPUTER_SIM
    File play_file_;
#endif
    static constexpr std::size_t kPlayChunkSamples = 1024;
    static constexpr std::size_t kPlayBufCount = 2;
    int16_t play_buf_[kPlayBufCount][kPlayChunkSamples]{};
    std::size_t play_buf_idx_ = 0;

    bool dirty_ = true;
    bool pending_header_focus_ = false;
    bool header_focused_ = false;
    uint32_t last_draw_sec_ = 0;
};
