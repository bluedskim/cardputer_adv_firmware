#include "apps/tuner/pitch_detect.h"

#include <cmath>

namespace pitch {
namespace {

constexpr float kMinHz = 70.0f;
constexpr float kMaxHz = 360.0f;
constexpr float kClarityMin = 0.35f;
constexpr float kRmsMin = 180.0f;
constexpr float kOctaveRatio = 0.88f;
constexpr std::size_t kMaxWindow = 2048;

struct StringDef {
    const char* name;
    float hz;
};

// Standard EADGBE, A4 = 440 Hz.
constexpr StringDef kStrings[] = {
    {"E", 82.4069f}, {"A", 110.0000f}, {"D", 146.8324f},
    {"G", 195.9977f}, {"B", 246.9417f}, {"e", 329.6276f},
};
constexpr int kStringCount = 6;

float centsBetween(const float hz, const float target) {
    if (hz <= 0.0f || target <= 0.0f) {
        return 0.0f;
    }
    return 1200.0f * std::log2(hz / target);
}

int32_t autocorrAt(const int16_t* centered, const std::size_t count, const int tau) {
    int64_t ac = 0;
    const std::size_t n = count - static_cast<std::size_t>(tau);
    for (std::size_t i = 0; i < n; ++i) {
        ac += static_cast<int32_t>(centered[i]) *
              static_cast<int32_t>(centered[i + static_cast<std::size_t>(tau)]);
    }
    return static_cast<int32_t>(ac >> 8);  // keep in int32 range
}

}  // namespace

bool detectHz(const int16_t* samples, const std::size_t count, const uint32_t sample_rate,
              float* out_hz, float* out_clarity) {
    if (samples == nullptr || out_hz == nullptr || count < 256 || count > kMaxWindow ||
        sample_rate == 0) {
        return false;
    }

    int32_t sum = 0;
    for (std::size_t i = 0; i < count; ++i) {
        sum += samples[i];
    }
    const int16_t mean = static_cast<int16_t>(sum / static_cast<int32_t>(count));

    static int16_t centered[kMaxWindow];
    int64_t energy64 = 0;
    for (std::size_t i = 0; i < count; ++i) {
        const int32_t v = static_cast<int32_t>(samples[i]) - mean;
        centered[i] = static_cast<int16_t>(v);
        energy64 += v * v;
    }
    const float energy = static_cast<float>(energy64);
    const float rms = std::sqrt(energy / static_cast<float>(count));
    if (rms < kRmsMin || energy < 1.0f) {
        return false;
    }

    int tau_min = static_cast<int>(static_cast<float>(sample_rate) / kMaxHz);
    int tau_max = static_cast<int>(static_cast<float>(sample_rate) / kMinHz);
    if (tau_min < 2) {
        tau_min = 2;
    }
    if (tau_max >= static_cast<int>(count) - 2) {
        tau_max = static_cast<int>(count) - 2;
    }
    if (tau_min >= tau_max) {
        return false;
    }

    static int32_t ac_buf[512];
    if (tau_max + 1 >= static_cast<int>(sizeof(ac_buf) / sizeof(ac_buf[0]))) {
        return false;
    }
    for (int tau = tau_min - 1; tau <= tau_max + 1; ++tau) {
        if (tau < 1) {
            continue;
        }
        ac_buf[tau] = autocorrAt(centered, count, tau);
    }

    // Length-normalized AC at local maxima. Prefer the shortest lag near the
    // global peak so 2T/3T harmonics do not pull the estimate an octave low.
    float peak_score = -1.0f;
    for (int tau = tau_min; tau <= tau_max; ++tau) {
        if (ac_buf[tau] < ac_buf[tau - 1] || ac_buf[tau] < ac_buf[tau + 1]) {
            continue;
        }
        const float score =
            static_cast<float>(ac_buf[tau]) / static_cast<float>(count - static_cast<std::size_t>(tau));
        if (score > peak_score) {
            peak_score = score;
        }
    }
    if (peak_score < 0.0f) {
        return false;
    }

    int best_tau = -1;
    for (int tau = tau_min; tau <= tau_max; ++tau) {
        if (ac_buf[tau] < ac_buf[tau - 1] || ac_buf[tau] < ac_buf[tau + 1]) {
            continue;
        }
        const float score =
            static_cast<float>(ac_buf[tau]) / static_cast<float>(count - static_cast<std::size_t>(tau));
        if (score >= kOctaveRatio * peak_score) {
            best_tau = tau;
            break;
        }
    }
    if (best_tau < 0) {
        return false;
    }

    const float energy_scaled = energy / 256.0f;
    const float clarity =
        energy_scaled > 0.0f ? static_cast<float>(ac_buf[best_tau]) / energy_scaled : 0.0f;
    if (clarity < kClarityMin) {
        return false;
    }

    float period = static_cast<float>(best_tau);
    if (best_tau > tau_min && best_tau < tau_max) {
        const float ym1 = static_cast<float>(ac_buf[best_tau - 1]);
        const float y0 = static_cast<float>(ac_buf[best_tau]);
        const float yp1 = static_cast<float>(ac_buf[best_tau + 1]);
        const float denom = ym1 - 2.0f * y0 + yp1;
        if (std::fabs(denom) > 1e-3f) {
            period += 0.5f * (ym1 - yp1) / denom;
        }
    }

    if (period < 1.0f) {
        return false;
    }

    *out_hz = static_cast<float>(sample_rate) / period;
    if (out_clarity != nullptr) {
        *out_clarity = clarity;
    }
    if (*out_hz < kMinHz || *out_hz > kMaxHz) {
        return false;
    }
    return true;
}

GuitarHit mapGuitar(const float hz) {
    GuitarHit hit{};
    if (hz < kMinHz || hz > kMaxHz) {
        return hit;
    }

    int best = 0;
    float best_cents = centsBetween(hz, kStrings[0].hz);
    for (int i = 1; i < kStringCount; ++i) {
        const float c = centsBetween(hz, kStrings[i].hz);
        if (std::fabs(c) < std::fabs(best_cents)) {
            best = i;
            best_cents = c;
        }
    }

    // Reject if more than a semi-tone from every open string (noise / fretted note).
    if (std::fabs(best_cents) > 100.0f) {
        return hit;
    }

    hit.ok = true;
    hit.string_index = best;
    hit.name = kStrings[best].name;
    hit.hz = hz;
    hit.cents = best_cents;
    return hit;
}

}  // namespace pitch
