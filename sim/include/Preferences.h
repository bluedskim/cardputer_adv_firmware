#pragma once

#include <Arduino.h>

#include <cstdint>
#include <cstdlib>
#include <map>
#include <string>

class Preferences {
public:
    bool begin(const char* name, bool read_only = false) {
        (void)read_only;
        namespace_ = name;
        return true;
    }

    void end() {}

    void clear() { storage()[namespace_].clear(); }

    bool isKey(const char* key) {
        const auto ns_it = storage().find(namespace_);
        return ns_it != storage().end() && ns_it->second.count(key) != 0;
    }

    bool putString(const char* key, const char* value) {
        storage()[namespace_][key] = value;
        return true;
    }

    String getString(const char* key, const char* default_value = "") {
        const auto ns_it = storage().find(namespace_);
        if (ns_it == storage().end()) {
            return String(default_value);
        }
        const auto key_it = ns_it->second.find(key);
        if (key_it == ns_it->second.end()) {
            return String(default_value);
        }
        return String(key_it->second.c_str());
    }

    bool putUChar(const char* key, uint8_t value) {
        return putString(key, String(static_cast<unsigned long>(value)).c_str());
    }

    uint8_t getUChar(const char* key, uint8_t default_value = 0) {
        return static_cast<uint8_t>(getUInt(key, default_value));
    }

    bool putUInt(const char* key, uint32_t value) {
        return putString(key, String(static_cast<unsigned long>(value)).c_str());
    }

    uint32_t getUInt(const char* key, uint32_t default_value = 0) {
        const String value = getString(key, "");
        if (value.length() == 0) {
            return default_value;
        }
        return static_cast<uint32_t>(std::strtoul(value.c_str(), nullptr, 10));
    }

private:
    using Storage = std::map<std::string, std::map<std::string, std::string>>;

    // Shared across instances so save()/load() round-trip within a sim run,
    // like NVS does on the device (still not persisted across runs).
    static Storage& storage() {
        static Storage instance;
        return instance;
    }

    std::string namespace_;
};
