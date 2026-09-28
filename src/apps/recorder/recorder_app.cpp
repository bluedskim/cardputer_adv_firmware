#include "apps/recorder/recorder_app.h"

#include <M5Cardputer.h>

#include <algorithm>
#include <cstdio>
#include <cstring>

#include "config.h"
#include "input/keyboard_input.h"

namespace {

constexpr int kContentTop = config::kContentTop;
constexpr int kLineHeight = 16;
constexpr int kListTop = kContentTop + 4;

int visibleRows() {
    const int available = M5Cardputer.Display.height() - kListTop;
    return std::max(1, available / kLineHeight);
}

// Display name without trailing .wav (case-insensitive).
void formatStemName(char* out, const std::size_t out_len, const char* name) {
    if (out == nullptr || out_len == 0) {
        return;
    }
    if (name == nullptr) {
        out[0] = '\0';
        return;
    }
    std::snprintf(out, out_len, "%s", name);
    const std::size_t n = std::strlen(out);
    if (n >= 4) {
        char* ext = out + (n - 4);
        if (ext[0] == '.' && (ext[1] == 'w' || ext[1] == 'W') && (ext[2] == 'a' || ext[2] == 'A') &&
            (ext[3] == 'v' || ext[3] == 'V')) {
            *ext = '\0';
        }
    }
}

}  // namespace

RecorderApp::RecorderApp() = default;

const char* RecorderApp::name() const { return "Recorder"; }

void RecorderApp::onEnter() {
    screen_ = Screen::List;
    cursor_ = 0;
    scroll_ = 0;
    message_.clear();
    refreshFileList();
    markDirty();
    requestHeaderFocus();
}

void RecorderApp::onExit() {
    if (recording_) {
        stopRecording(recordingElapsedMs() >= config::kRecorderMinSaveMs);
    }
    if (playing_) {
        stopPlayback();
    }
    releaseAudio();
    SdFs::end();
    sd_ok_ = false;
    M5Cardputer.Display.fillScreen(TFT_BLACK);
}

void RecorderApp::onDisplayWake() { markDirty(); }

void RecorderApp::onBackgroundTick() {
    if (recording_) {
        tickRecording();
    }
    if (playing_) {
        tickPlayback();
    }
}

bool RecorderApp::hasActiveBackgroundWork() const { return recording_ || playing_; }

bool RecorderApp::voiceRecorderIsRecording() const { return recording_; }

void RecorderApp::voiceRecorderHotkey() {
    // Toggle: stop&save if recording, otherwise start (after clearing header intent).
    pending_header_focus_ = false;
    if (recording_) {
        stopRecording(true);
        screen_ = Screen::List;
        refreshFileList();
        for (std::size_t i = 0; i < files_.size(); ++i) {
            if (files_[i].name == record_name_) {
                cursor_ = i;
                break;
            }
        }
        markDirty();
        return;
    }
    if (playing_) {
        stopPlayback();
    }
    if (screen_ == Screen::ConfirmDelete || screen_ == Screen::Message) {
        screen_ = Screen::List;
    }
    if (!sd_ok_) {
        refreshFileList();
        if (!sd_ok_) {
            showMessage("No SD card");
            return;
        }
    }
    beginRecording();
}

NavRequest RecorderApp::onUpdate(const Keyboard_Class::KeysState* keys, const bool header_focused) {
    if (header_focused_ != header_focused) {
        header_focused_ = header_focused;
        markDirty();
    }

    if (recording_) {
        markDirty();  // elapsed + level meter
    } else if (playing_) {
        const uint32_t elapsed_ms =
            play_sample_rate_ > 0
                ? static_cast<uint32_t>((static_cast<uint64_t>(play_bytes_done_) * 1000ULL) /
                                        (play_sample_rate_ * 2ULL))
                : 0;
        const uint32_t sec = elapsed_ms / 1000u;
        if (sec != last_draw_sec_) {
            last_draw_sec_ = sec;
            markDirty();
        }
    }

    NavRequest req = NavRequest::None;
    if (keys != nullptr && !header_focused) {
        req = handleInput(*keys);
    }

    if (dirty_) {
        draw();
        dirty_ = false;
    }
    if (pending_header_focus_) {
        pending_header_focus_ = false;
        return NavRequest::FocusHeader;
    }
    return req;
}

void RecorderApp::refreshFileList() {
    sd_ok_ = SdFs::begin() && SdFs::ensureRecDir();
    files_.clear();
    if (sd_ok_) {
        SdFs::listWavFiles(&files_);
    }
    if (cursor_ >= listRowCount() && listRowCount() > 0) {
        cursor_ = listRowCount() - 1;
    }
    if (scroll_ > cursor_) {
        scroll_ = cursor_;
    }
}

std::size_t RecorderApp::listRowCount() const {
    // Files + "New recording..." when SD is OK; otherwise a single status row.
    if (!sd_ok_) {
        return 1;
    }
    return files_.size() + 1;
}

RecorderApp::ListAction RecorderApp::rowAction(const std::size_t row) const {
    if (!sd_ok_) {
        return ListAction::NewRecording;  // unused
    }
    if (row < files_.size()) {
        return ListAction::File;
    }
    return ListAction::NewRecording;
}

void RecorderApp::showMessage(const char* text) {
    message_ = text != nullptr ? text : "";
    screen_ = Screen::Message;
    markDirty();
}

void RecorderApp::formatMmSs(char* buf, const std::size_t len, const uint32_t ms) const {
    const uint32_t total_sec = ms / 1000u;
    const uint32_t min = total_sec / 60u;
    const uint32_t sec = total_sec % 60u;
    std::snprintf(buf, len, "%02u:%02u", min, sec);
}

uint32_t RecorderApp::recordingElapsedMs() const {
    if (!recording_) {
        return 0;
    }
    return millis() - record_start_ms_;
}

uint16_t RecorderApp::peakLevel(const int16_t* samples, const std::size_t count) const {
    uint16_t peak = 0;
    for (std::size_t i = 0; i < count; ++i) {
        const int32_t v = samples[i] < 0 ? -static_cast<int32_t>(samples[i])
                                         : static_cast<int32_t>(samples[i]);
        if (v > peak) {
            peak = static_cast<uint16_t>(std::min<int32_t>(v, 32767));
        }
    }
    return peak;
}

void RecorderApp::releaseAudio() {
#ifndef CARDPUTER_SIM
    if (M5Cardputer.Mic.isRunning()) {
        M5Cardputer.Mic.end();
    }
    if (M5Cardputer.Speaker.isRunning()) {
        M5Cardputer.Speaker.stop();
        M5Cardputer.Speaker.end();
    }
#endif
}

bool RecorderApp::beginRecording() {
#ifndef CARDPUTER_SIM
    if (!sd_ok_ && !(SdFs::begin() && SdFs::ensureRecDir())) {
        showMessage("No SD card");
        return false;
    }
    sd_ok_ = true;
    if (!SdFs::nextRecordingPath(record_path_, sizeof(record_path_), record_name_,
                                 sizeof(record_name_))) {
        showMessage("Name exhausted");
        return false;
    }

    releaseAudio();
    M5Cardputer.Speaker.end();
    if (!M5Cardputer.Mic.begin()) {
        showMessage("Mic failed");
        return false;
    }

    record_file_ = SD.open(record_path_, FILE_WRITE);
    if (!record_file_) {
        M5Cardputer.Mic.end();
        showMessage("Open failed");
        return false;
    }

    uint8_t header[WavIo::kHeaderBytes];
    WavIo::writeHeaderPlaceholder(header, config::kRecorderSampleRate);
    if (record_file_.write(header, sizeof(header)) != sizeof(header)) {
        record_file_.close();
        SD.remove(record_path_);
        M5Cardputer.Mic.end();
        showMessage("Write failed");
        return false;
    }

    recording_ = true;
    record_start_ms_ = millis();
    record_data_bytes_ = 0;
    level_peak_ = 0;
    last_draw_sec_ = 0;
    screen_ = Screen::Record;
    markDirty();
    return true;
#else
    showMessage("SD unavailable");
    return false;
#endif
}

void RecorderApp::tickRecording() {
#ifndef CARDPUTER_SIM
    if (!recording_ || !record_file_) {
        return;
    }

    if (recordingElapsedMs() >= config::kRecorderMaxDurationSec * 1000u) {
        stopRecording(true);
        showMessage("Max length");
        return;
    }

    if (M5Cardputer.Mic.record(rec_buf_, kRecChunkSamples, config::kRecorderSampleRate)) {
        const size_t bytes = kRecChunkSamples * sizeof(int16_t);
        if (record_file_.write(reinterpret_cast<const uint8_t*>(rec_buf_), bytes) != bytes) {
            stopRecording(record_data_bytes_ > 0);
            showMessage("SD full?");
            return;
        }
        record_data_bytes_ += static_cast<uint32_t>(bytes);
        level_peak_ = peakLevel(rec_buf_, kRecChunkSamples);
    }
#else
    (void)0;
#endif
}

void RecorderApp::stopRecording(const bool save) {
#ifndef CARDPUTER_SIM
    if (!recording_) {
        return;
    }
    recording_ = false;

    // Drain any in-flight Mic buffers (best-effort).
    while (M5Cardputer.Mic.isRecording()) {
        delay(1);
    }
    M5Cardputer.Mic.end();

    if (record_file_) {
        if (save && record_data_bytes_ > 0) {
            uint8_t header[WavIo::kHeaderBytes];
            WavIo::writeHeaderPlaceholder(header, config::kRecorderSampleRate);
            WavIo::finalizeHeader(header, record_data_bytes_);
            record_file_.seek(0);
            record_file_.write(header, sizeof(header));
            record_file_.close();
        } else {
            record_file_.close();
            SD.remove(record_path_);
        }
    }
#else
    (void)save;
    recording_ = false;
#endif
    level_peak_ = 0;
}

bool RecorderApp::beginPlayback() {
#ifndef CARDPUTER_SIM
    if (selected_ >= files_.size()) {
        return false;
    }

    releaseAudio();
    M5Cardputer.Mic.end();

    play_file_ = SD.open(files_[selected_].path.c_str(), FILE_READ);
    if (!play_file_) {
        showMessage("Open failed");
        return false;
    }

    uint8_t header[128];
    const size_t n = play_file_.read(header, sizeof(header));
    if (!WavIo::parseHeader(header, n, &play_info_)) {
        play_file_.close();
        showMessage("Bad WAV");
        return false;
    }
    if (play_info_.bits_per_sample != 16 || play_info_.channels != 1) {
        play_file_.close();
        showMessage("Need mono16");
        return false;
    }

    play_file_.seek(play_info_.data_offset);
    play_data_bytes_ = play_info_.data_bytes;
    play_bytes_done_ = 0;
    play_sample_rate_ = play_info_.sample_rate;
    play_buf_idx_ = 0;

    if (!M5Cardputer.Speaker.begin()) {
        play_file_.close();
        showMessage("Spk failed");
        return false;
    }
    M5Cardputer.Speaker.setVolume(180);

    playing_ = true;
    last_draw_sec_ = 0;
    screen_ = Screen::Play;
    markDirty();
    return true;
#else
    showMessage("SD unavailable");
    return false;
#endif
}

void RecorderApp::tickPlayback() {
#ifndef CARDPUTER_SIM
    if (!playing_ || !play_file_) {
        return;
    }

    if (play_bytes_done_ >= play_data_bytes_ && !M5Cardputer.Speaker.isPlaying()) {
        stopPlayback();
        screen_ = Screen::List;
        refreshFileList();
        markDirty();
        return;
    }

    // Keep two chunks queued when possible.
    while (play_bytes_done_ < play_data_bytes_ && M5Cardputer.Speaker.isPlaying(0) < 2) {
        const uint32_t remain = play_data_bytes_ - play_bytes_done_;
        const size_t want_bytes =
            std::min<uint32_t>(remain, static_cast<uint32_t>(kPlayChunkSamples * sizeof(int16_t)));
        const size_t want_samples = want_bytes / sizeof(int16_t);
        int16_t* buf = play_buf_[play_buf_idx_];
        const size_t got = play_file_.read(reinterpret_cast<uint8_t*>(buf), want_samples * sizeof(int16_t));
        if (got == 0) {
            play_bytes_done_ = play_data_bytes_;
            break;
        }
        const size_t samples = got / sizeof(int16_t);
        M5Cardputer.Speaker.playRaw(buf, samples, play_sample_rate_, false, 1, 0);
        play_bytes_done_ += static_cast<uint32_t>(got);
        play_buf_idx_ = (play_buf_idx_ + 1) % kPlayBufCount;
    }
#else
    (void)0;
#endif
}

void RecorderApp::stopPlayback() {
#ifndef CARDPUTER_SIM
    playing_ = false;
    if (M5Cardputer.Speaker.isRunning()) {
        M5Cardputer.Speaker.stop();
        M5Cardputer.Speaker.end();
    }
    if (play_file_) {
        play_file_.close();
    }
#else
    playing_ = false;
#endif
}

void RecorderApp::activateSelection() {
    if (screen_ == Screen::List) {
        if (!sd_ok_) {
            refreshFileList();
            if (!sd_ok_) {
                showMessage("No SD card");
            } else {
                markDirty();
            }
            return;
        }
        if (rowAction(cursor_) == ListAction::NewRecording) {
            beginRecording();
            return;
        }
        selected_ = cursor_;
        beginPlayback();
        return;
    }

    if (screen_ == Screen::Record) {
        stopRecording(true);
        screen_ = Screen::List;
        refreshFileList();
        // Focus the new file if present.
        for (std::size_t i = 0; i < files_.size(); ++i) {
            if (files_[i].name == record_name_) {
                cursor_ = i;
                break;
            }
        }
        markDirty();
        return;
    }

    if (screen_ == Screen::Play) {
        // Enter does nothing while playing.
        return;
    }

    if (screen_ == Screen::ConfirmDelete) {
        if (selected_ < files_.size()) {
            SdFs::removeFile(files_[selected_].path.c_str());
        }
        screen_ = Screen::List;
        refreshFileList();
        markDirty();
        return;
    }

    if (screen_ == Screen::Message) {
        screen_ = Screen::List;
        refreshFileList();
        markDirty();
    }
}

void RecorderApp::goBack() {
    if (screen_ == Screen::Record) {
        stopRecording(false);
        screen_ = Screen::List;
        refreshFileList();
        markDirty();
        return;
    }
    if (screen_ == Screen::Play) {
        // Del → confirm delete current file (stop playback first).
        stopPlayback();
        screen_ = Screen::ConfirmDelete;
        markDirty();
        return;
    }
    if (screen_ == Screen::ConfirmDelete || screen_ == Screen::Message) {
        screen_ = Screen::List;
        markDirty();
        return;
    }
    if (screen_ == Screen::List && selected_ < files_.size() && cursor_ < files_.size()) {
        // Del on a file → confirm delete
        selected_ = cursor_;
        screen_ = Screen::ConfirmDelete;
        markDirty();
    }
}

NavRequest RecorderApp::handleInput(const Keyboard_Class::KeysState& status) {
    // AppShell already edge-filters keys; do not re-poll with a local tracker
    // (that would swallow the second Enter after starting a recording).
    const ArrowKey arrow = detectArrowKey(status);

    if (arrow == ArrowKey::Up) {
        if (screen_ == Screen::List) {
            if (cursor_ == 0) {
                return NavRequest::FocusHeader;
            }
            --cursor_;
            if (cursor_ < scroll_) {
                scroll_ = cursor_;
            }
            markDirty();
        }
        return NavRequest::None;
    }

    if (arrow == ArrowKey::Down) {
        if (screen_ == Screen::List) {
            const std::size_t rows = listRowCount();
            if (rows == 0) {
                return NavRequest::None;
            }
            if (cursor_ + 1 < rows) {
                ++cursor_;
                const std::size_t visible = static_cast<std::size_t>(visibleRows());
                if (cursor_ >= scroll_ + visible) {
                    scroll_ = cursor_ - visible + 1;
                }
                markDirty();
            }
        }
        return NavRequest::None;
    }

    if (arrow == ArrowKey::Left) {
        if (screen_ == Screen::Play) {
            stopPlayback();
            screen_ = Screen::List;
            markDirty();
            return NavRequest::None;
        }
        if (screen_ != Screen::List) {
            goBack();
            return NavRequest::None;
        }
    }

    if (status.enter) {
        if (screen_ == Screen::Play) {
            return NavRequest::None;
        }
        activateSelection();
        return NavRequest::None;
    }

    if (isBackKey(status)) {
        if (screen_ == Screen::List) {
            if (sd_ok_ && cursor_ < files_.size()) {
                selected_ = cursor_;
                screen_ = Screen::ConfirmDelete;
                markDirty();
            }
        } else if (screen_ == Screen::Play) {
            stopPlayback();
            screen_ = Screen::ConfirmDelete;
            markDirty();
        } else {
            goBack();
        }
        return NavRequest::None;
    }

    return NavRequest::None;
}

void RecorderApp::draw() const {
    auto& display = M5Cardputer.Display;
    display.fillRect(0, kContentTop, display.width(), display.height() - kContentTop, TFT_BLACK);
    display.setTextSize(2);
    display.setTextDatum(top_left);

    if (screen_ == Screen::Message) {
        display.setTextColor(TFT_ORANGE, TFT_BLACK);
        display.setCursor(8, kListTop + 8);
        display.print(message_.c_str());
        display.setTextColor(TFT_DARKGREY, TFT_BLACK);
        display.setCursor(8, kListTop + 8 + kLineHeight);
        display.print("Enter OK");
        return;
    }

    if (screen_ == Screen::ConfirmDelete) {
        display.setTextColor(TFT_ORANGE, TFT_BLACK);
        display.setCursor(8, kListTop + 8);
        if (selected_ < files_.size()) {
            char stem[24];
            formatStemName(stem, sizeof(stem), files_[selected_].name.c_str());
            display.printf("Del %s?", stem);
        } else {
            display.print("Delete?");
        }
        display.setTextColor(TFT_DARKGREY, TFT_BLACK);
        display.setCursor(8, kListTop + 8 + kLineHeight);
        display.print("Enter/Del");
        return;
    }

    if (screen_ == Screen::Record) {
        char time_buf[16];
        formatMmSs(time_buf, sizeof(time_buf), recordingElapsedMs());
        display.setTextColor(TFT_RED, TFT_BLACK);
        display.setCursor(8, kListTop);
        display.print("REC ");
        display.setTextColor(TFT_WHITE, TFT_BLACK);
        display.print(time_buf);

        display.setTextColor(TFT_CYAN, TFT_BLACK);
        display.setCursor(8, kListTop + kLineHeight);
        char stem[24];
        formatStemName(stem, sizeof(stem), record_name_);
        display.print(stem);

        // Level bar
        const int bar_x = 8;
        const int bar_y = kListTop + kLineHeight * 2 + 4;
        const int bar_w = display.width() - 16;
        const int bar_h = 12;
        display.drawRect(bar_x, bar_y, bar_w, bar_h, TFT_DARKGREY);
        const int fill = static_cast<int>((static_cast<uint32_t>(level_peak_) * bar_w) / 32767u);
        if (fill > 0) {
            display.fillRect(bar_x + 1, bar_y + 1, std::min(fill, bar_w - 2), bar_h - 2, TFT_GREEN);
        }

        display.setTextColor(TFT_DARKGREY, TFT_BLACK);
        display.setCursor(8, bar_y + bar_h + 6);
        display.print("G0/Ent stop Del x");
        return;
    }

    if (screen_ == Screen::Play) {
        char time_buf[16];
        char total_buf[16];
        const uint32_t elapsed_ms =
            play_sample_rate_ > 0
                ? static_cast<uint32_t>((static_cast<uint64_t>(play_bytes_done_) * 1000ULL) /
                                        (play_sample_rate_ * 2ULL))
                : 0;
        formatMmSs(time_buf, sizeof(time_buf), elapsed_ms);
        formatMmSs(total_buf, sizeof(total_buf), WavIo::durationMs(play_info_));

        display.setTextColor(TFT_GREEN, TFT_BLACK);
        display.setCursor(8, kListTop);
        display.print("PLAY ");
        display.setTextColor(TFT_WHITE, TFT_BLACK);
        display.printf("%s/%s", time_buf, total_buf);

        display.setTextColor(TFT_CYAN, TFT_BLACK);
        display.setCursor(8, kListTop + kLineHeight);
        if (selected_ < files_.size()) {
            char stem[24];
            formatStemName(stem, sizeof(stem), files_[selected_].name.c_str());
            display.print(stem);
        }

        display.setTextColor(TFT_DARKGREY, TFT_BLACK);
        display.setCursor(8, kListTop + kLineHeight * 2 + 4);
        display.print("Del delete");
        return;
    }

    // List
    display.setTextSize(2);
    if (!sd_ok_) {
        const bool selected = !header_focused_;
        display.setTextColor(selected ? TFT_YELLOW : TFT_ORANGE, selected ? TFT_NAVY : TFT_BLACK);
        if (selected) {
            display.fillRect(0, kListTop, display.width(), kLineHeight, TFT_NAVY);
        }
        display.setCursor(4, kListTop);
        display.print("> No SD — Enter");
        return;
    }

    const std::size_t rows = listRowCount();
    const std::size_t visible = static_cast<std::size_t>(visibleRows());
    const std::size_t end = std::min(rows, scroll_ + visible);
    int y = kListTop;
    for (std::size_t i = scroll_; i < end; ++i) {
        const bool selected = (i == cursor_) && !header_focused_;
        const uint16_t fg = selected ? TFT_YELLOW : TFT_WHITE;
        const uint16_t bg = selected ? TFT_NAVY : TFT_BLACK;
        if (selected) {
            display.fillRect(0, y, display.width(), kLineHeight, bg);
        }
        display.setTextColor(fg, bg);
        display.setCursor(4, y);

        if (rowAction(i) == ListAction::NewRecording) {
            display.printf("%c + New recording", selected ? '>' : ' ');
        } else {
            char dur[16] = "--:--";
            if (files_[i].duration_ms > 0) {
                formatMmSs(dur, sizeof(dur), files_[i].duration_ms);
            }
            char stem[24];
            formatStemName(stem, sizeof(stem), files_[i].name.c_str());
            display.printf("%c %s %s", selected ? '>' : ' ', stem, dur);
        }
        y += kLineHeight;
    }
}
