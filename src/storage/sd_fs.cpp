#include "storage/sd_fs.h"

#include <algorithm>
#include <cstdio>
#include <cstring>

#include "apps/recorder/wav_io.h"
#include "config.h"

#ifndef CARDPUTER_SIM
#include <SD.h>
#include <SPI.h>
#endif

namespace {

bool g_mounted = false;

bool endsWithIgnoreCase(const char* name, const char* suffix) {
    if (name == nullptr || suffix == nullptr) {
        return false;
    }
    const std::size_t nlen = std::strlen(name);
    const std::size_t slen = std::strlen(suffix);
    if (nlen < slen) {
        return false;
    }
    for (std::size_t i = 0; i < slen; ++i) {
        char a = name[nlen - slen + i];
        char b = suffix[i];
        if (a >= 'A' && a <= 'Z') {
            a = static_cast<char>(a - 'A' + 'a');
        }
        if (b >= 'A' && b <= 'Z') {
            b = static_cast<char>(b - 'A' + 'a');
        }
        if (a != b) {
            return false;
        }
    }
    return true;
}

}  // namespace

namespace SdFs {

bool begin() {
#ifdef CARDPUTER_SIM
    g_mounted = false;
    return false;
#else
    if (g_mounted) {
        return true;
    }
    SPI.begin(config::kSdSpiSck, config::kSdSpiMiso, config::kSdSpiMosi, config::kSdSpiCs);
    if (!SD.begin(config::kSdSpiCs, SPI, 25000000)) {
        g_mounted = false;
        return false;
    }
    if (SD.cardType() == CARD_NONE) {
        SD.end();
        g_mounted = false;
        return false;
    }
    g_mounted = true;
    return true;
#endif
}

void end() {
#ifdef CARDPUTER_SIM
    g_mounted = false;
#else
    if (g_mounted) {
        SD.end();
        g_mounted = false;
    }
#endif
}

bool isMounted() { return g_mounted; }

bool ensureRecDir() {
#ifdef CARDPUTER_SIM
    return false;
#else
    if (!g_mounted) {
        return false;
    }
    if (SD.exists(config::kRecorderDir)) {
        File dir = SD.open(config::kRecorderDir);
        const bool ok = dir && dir.isDirectory();
        if (dir) {
            dir.close();
        }
        return ok;
    }
    return SD.mkdir(config::kRecorderDir);
#endif
}

bool listWavFiles(std::vector<WavEntry>* out) {
    if (out == nullptr) {
        return false;
    }
    out->clear();
#ifdef CARDPUTER_SIM
    return false;
#else
    if (!g_mounted || !ensureRecDir()) {
        return false;
    }
    File root = SD.open(config::kRecorderDir);
    if (!root || !root.isDirectory()) {
        if (root) {
            root.close();
        }
        return false;
    }

    for (File file = root.openNextFile(); file; file = root.openNextFile()) {
        if (file.isDirectory()) {
            continue;
        }
        const char* name = file.name();
        if (name == nullptr) {
            continue;
        }
        // SD may return "REC_0001.wav" or "/rec/REC_0001.wav"
        const char* base = name;
        if (const char* slash = std::strrchr(name, '/')) {
            base = slash + 1;
        }
        if (!endsWithIgnoreCase(base, ".wav")) {
            continue;
        }
        WavEntry entry;
        entry.name = base;
        entry.path = std::string(config::kRecorderDir) + "/" + base;
        entry.size_bytes = static_cast<uint32_t>(file.size());
        entry.duration_ms = 0;
        {
            uint8_t hdr[64];
            File rf = SD.open(entry.path.c_str(), FILE_READ);
            if (rf) {
                const size_t n = rf.read(hdr, sizeof(hdr));
                WavIo::Info info;
                if (WavIo::parseHeader(hdr, n, &info)) {
                    entry.duration_ms = WavIo::durationMs(info);
                }
                rf.close();
            }
        }
        out->push_back(std::move(entry));
        if (out->size() >= config::kRecorderMaxFiles) {
            break;
        }
    }
    root.close();

    std::sort(out->begin(), out->end(),
              [](const WavEntry& a, const WavEntry& b) { return a.name < b.name; });
    return true;
#endif
}

bool nextRecordingPath(char* path_buf, const std::size_t path_len, char* name_buf,
                       const std::size_t name_len) {
    if (path_buf == nullptr || name_buf == nullptr || path_len < 20 || name_len < 16) {
        return false;
    }
#ifdef CARDPUTER_SIM
    return false;
#else
    if (!g_mounted || !ensureRecDir()) {
        return false;
    }
    for (int i = 1; i <= 9999; ++i) {
        std::snprintf(name_buf, name_len, "REC_%04d.wav", i);
        std::snprintf(path_buf, path_len, "%s/%s", config::kRecorderDir, name_buf);
        if (!SD.exists(path_buf)) {
            return true;
        }
    }
    return false;
#endif
}

bool removeFile(const char* path) {
#ifdef CARDPUTER_SIM
    (void)path;
    return false;
#else
    if (!g_mounted || path == nullptr) {
        return false;
    }
    return SD.remove(path);
#endif
}

}  // namespace SdFs
