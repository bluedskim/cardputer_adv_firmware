#pragma once

#include <cstdint>

#define DISABLE_LED_FEEDBACK 0
#define ENABLE_LED_FEEDBACK 1

enum decode_type_t : uint8_t {
    UNKNOWN = 0,
    NEC,
    SONY,
    RC5,
    SAMSUNG,
    LG,
};

struct IRData {
    decode_type_t protocol = NEC;
    uint16_t address = 0;
    uint8_t command = 0;
    uint8_t numberOfBits = 32;
};

struct IRrecvClass {
    void begin(uint8_t, bool) { active_ = true; }
    void start() { active_ = true; }
    void stop() { active_ = false; }
    bool decode() {
        if (!active_ || !pending_) {
            return false;
        }
        pending_ = false;
        return true;
    }
    void resume() {}
    bool isActive() const { return active_; }
    void injectNec(uint16_t address, uint8_t command) {
        decodedIRData.protocol = NEC;
        decodedIRData.address = address;
        decodedIRData.command = command;
        decodedIRData.numberOfBits = 32;
        pending_ = true;
    }

    IRData decodedIRData{};
    bool active_ = false;
    bool pending_ = false;
};

struct IRsendClass {
    void begin(int) {}
    void setSendPin(uint8_t) {}
    void sendNEC(uint16_t address, uint8_t command, int) {
        last_address_ = address;
        last_command_ = command;
    }
    void sendSony(uint32_t address, uint8_t command, int, int) {
        last_address_ = static_cast<uint16_t>(address);
        last_command_ = command;
    }

    uint16_t lastAddress() const { return last_address_; }
    uint8_t lastCommand() const { return last_command_; }

private:
    uint16_t last_address_ = 0;
    uint8_t last_command_ = 0;
};

inline IRsendClass IrSender;
inline IRrecvClass IrReceiver;
