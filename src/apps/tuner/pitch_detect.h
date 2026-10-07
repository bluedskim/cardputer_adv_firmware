#pragma once

#include <cstddef>
#include <cstdint>

namespace pitch {

struct GuitarHit {
    bool ok = false;
    int string_index = -1;  // 0=E2 .. 5=E4
    const char* name = "--";
    float hz = 0.0f;
    float cents = 0.0f;  // vs target; negative = flat
};

// Autocorrelation pitch estimate. Returns false when level/clarity is too low.
bool detectHz(const int16_t* samples, std::size_t count, uint32_t sample_rate, float* out_hz,
              float* out_clarity);

// Map Hz to nearest open Standard guitar string (A4=440).
GuitarHit mapGuitar(float hz);

}  // namespace pitch
