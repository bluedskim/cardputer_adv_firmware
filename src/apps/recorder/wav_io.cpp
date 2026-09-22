#include "apps/recorder/wav_io.h"

#include <cstring>

namespace WavIo {

namespace {

void writeLe16(uint8_t* p, const uint16_t v) {
    p[0] = static_cast<uint8_t>(v & 0xff);
    p[1] = static_cast<uint8_t>((v >> 8) & 0xff);
}

void writeLe32(uint8_t* p, const uint32_t v) {
    p[0] = static_cast<uint8_t>(v & 0xff);
    p[1] = static_cast<uint8_t>((v >> 8) & 0xff);
    p[2] = static_cast<uint8_t>((v >> 16) & 0xff);
    p[3] = static_cast<uint8_t>((v >> 24) & 0xff);
}

uint16_t readLe16(const uint8_t* p) {
    return static_cast<uint16_t>(p[0] | (static_cast<uint16_t>(p[1]) << 8));
}

uint32_t readLe32(const uint8_t* p) {
    return static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8) |
           (static_cast<uint32_t>(p[2]) << 16) | (static_cast<uint32_t>(p[3]) << 24);
}

}  // namespace

void writeHeaderPlaceholder(uint8_t* out44, const uint32_t sample_rate, const uint16_t channels,
                            const uint16_t bits_per_sample) {
    if (out44 == nullptr) {
        return;
    }
    std::memset(out44, 0, kHeaderBytes);
    std::memcpy(out44 + 0, "RIFF", 4);
    writeLe32(out44 + 4, 36);  // placeholder: 36 + data_bytes
    std::memcpy(out44 + 8, "WAVE", 4);
    std::memcpy(out44 + 12, "fmt ", 4);
    writeLe32(out44 + 16, 16);  // PCM fmt chunk size
    writeLe16(out44 + 20, 1);   // PCM
    writeLe16(out44 + 22, channels);
    writeLe32(out44 + 24, sample_rate);
    const uint16_t block = static_cast<uint16_t>(channels * (bits_per_sample / 8));
    writeLe32(out44 + 28, sample_rate * block);
    writeLe16(out44 + 32, block);
    writeLe16(out44 + 34, bits_per_sample);
    std::memcpy(out44 + 36, "data", 4);
    writeLe32(out44 + 40, 0);  // placeholder data size
}

void finalizeHeader(uint8_t* header44, const uint32_t data_bytes) {
    if (header44 == nullptr) {
        return;
    }
    writeLe32(header44 + 4, 36 + data_bytes);
    writeLe32(header44 + 40, data_bytes);
}

bool parseHeader(const uint8_t* data, const std::size_t len, Info* out) {
    if (data == nullptr || out == nullptr || len < kHeaderBytes) {
        return false;
    }
    if (std::memcmp(data, "RIFF", 4) != 0 || std::memcmp(data + 8, "WAVE", 4) != 0) {
        return false;
    }

    std::size_t pos = 12;
    uint16_t audio_fmt = 0;
    uint16_t channels = 0;
    uint32_t sample_rate = 0;
    uint16_t bits = 0;
    uint32_t data_bytes = 0;
    uint32_t data_offset = 0;
    bool have_fmt = false;
    bool have_data = false;

    while (pos + 8 <= len) {
        const char* id = reinterpret_cast<const char*>(data + pos);
        const uint32_t chunk_size = readLe32(data + pos + 4);
        const std::size_t payload = pos + 8;
        if (payload > len) {
            break;
        }

        if (std::memcmp(id, "fmt ", 4) == 0) {
            if (chunk_size < 16 || payload + 16 > len) {
                return false;
            }
            audio_fmt = readLe16(data + payload + 0);
            channels = readLe16(data + payload + 2);
            sample_rate = readLe32(data + payload + 4);
            bits = readLe16(data + payload + 14);
            have_fmt = true;
        } else if (std::memcmp(id, "data", 4) == 0) {
            data_bytes = chunk_size;
            data_offset = static_cast<uint32_t>(payload);
            have_data = true;
            break;
        }

        // Chunks are word-aligned.
        const uint32_t advance = 8 + chunk_size + (chunk_size & 1U);
        if (pos + advance < pos) {
            break;
        }
        pos += advance;
    }

    if (!have_fmt || !have_data || audio_fmt != 1 || channels == 0 || channels > 2 ||
        (bits != 8 && bits != 16) || sample_rate == 0) {
        return false;
    }

    out->sample_rate = sample_rate;
    out->channels = channels;
    out->bits_per_sample = bits;
    out->data_bytes = data_bytes;
    out->data_offset = data_offset;
    return true;
}

uint32_t durationMs(const Info& info) {
    if (info.sample_rate == 0 || info.channels == 0 || info.bits_per_sample == 0) {
        return 0;
    }
    const uint32_t bytes_per_sec =
        info.sample_rate * info.channels * (info.bits_per_sample / 8U);
    if (bytes_per_sec == 0) {
        return 0;
    }
    return static_cast<uint32_t>((static_cast<uint64_t>(info.data_bytes) * 1000ULL) /
                                 bytes_per_sec);
}

}  // namespace WavIo
