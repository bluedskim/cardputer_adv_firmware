#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

// Cardputer-Adv microSD helper (SPI). Sim stub always fails mount.
namespace SdFs {

bool begin();
void end();
bool isMounted();

// Ensure /rec exists. Requires a successful begin().
bool ensureRecDir();

struct WavEntry {
    std::string name;  // basename, e.g. REC_0001.wav
    std::string path;  // full path, e.g. /rec/REC_0001.wav
    uint32_t size_bytes = 0;
    uint32_t duration_ms = 0;  // 0 if unknown
};

// Lists *.wav under /rec (sorted by name). Caps at config::kRecorderMaxFiles.
bool listWavFiles(std::vector<WavEntry>* out);

// Next free sequential name: /rec/REC_XXXX.wav
bool nextRecordingPath(char* path_buf, std::size_t path_len, char* name_buf,
                       std::size_t name_len);

bool removeFile(const char* path);

}  // namespace SdFs
