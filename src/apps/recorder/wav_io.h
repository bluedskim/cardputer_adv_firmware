#pragma once

#include <cstddef>
#include <cstdint>

// Minimal 16-bit PCM WAV helpers (44-byte standard header).
namespace WavIo {

constexpr std::size_t kHeaderBytes = 44;

struct Info {
    uint32_t sample_rate = 0;
    uint16_t channels = 0;
    uint16_t bits_per_sample = 0;
    uint32_t data_bytes = 0;
    uint32_t data_offset = 0;
};

// Write a placeholder PCM header (sizes filled with 0). Call finalizeHeader after data.
void writeHeaderPlaceholder(uint8_t* out44, uint32_t sample_rate, uint16_t channels = 1,
                            uint16_t bits_per_sample = 16);

// Patch RIFF/data sizes after streaming PCM. data_bytes is PCM payload length.
void finalizeHeader(uint8_t* header44, uint32_t data_bytes);

// Parse a WAV header (at least 44 bytes; skips unknown chunks to find "data").
// For files we wrote ourselves, data starts at offset 44.
bool parseHeader(const uint8_t* data, std::size_t len, Info* out);

uint32_t durationMs(const Info& info);

}  // namespace WavIo
