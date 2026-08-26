#pragma once

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>

#include <SDL.h>

class String {
public:
    String() = default;
    String(const char* value) : data_(value != nullptr ? value : "") {}
    String(int value) : data_(std::to_string(value)) {}
    String(unsigned int value) : data_(std::to_string(value)) {}
    String(long value) : data_(std::to_string(value)) {}
    String(unsigned long value) : data_(std::to_string(value)) {}

    const char* c_str() const { return data_.c_str(); }
    operator const char*() const { return c_str(); }
    size_t length() const { return data_.length(); }

    String operator+(const String& other) const { return String((data_ + other.data_).c_str()); }
    String operator+(const char* other) const { return String((data_ + (other != nullptr ? other : "")).c_str()); }
    String operator+(int other) const { return *this + String(other); }

private:
    std::string data_;
};

inline String operator+(const char* lhs, const String& rhs) {
    return String(lhs) + rhs;
}

inline uint32_t millis() {
    return static_cast<uint32_t>(SDL_GetTicks());
}

inline void delay(uint32_t ms) {
    SDL_Delay(ms);
}

class SerialSim {
public:
    template <typename T>
    void begin(T) {}

    template <typename... Args>
    void printf(const char* fmt, Args... args) {
        std::fprintf(stdout, fmt, args...);
    }

    template <typename T>
    void println(const T& value) {
        std::printf("%s\n", value);
    }

    void println() { std::printf("\n"); }
};

inline SerialSim Serial;

#ifndef HIGH
#define HIGH 0x1
#endif
#ifndef LOW
#define LOW 0x0
#endif
#ifndef INPUT
#define INPUT 0x0
#endif
#ifndef OUTPUT
#define OUTPUT 0x1
#endif

inline void pinMode(int, int) {}
inline void digitalWrite(int, int) {}
inline int digitalRead(int) { return LOW; }
