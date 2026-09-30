#pragma once

#include <cstdint>

#include "apps/app_base.h"

class SeismoApp final : public AppBase {
public:
    const char* name() const override;
    void onEnter() override;
    void onExit() override;
    void onDisplayWake() override;
    NavRequest onUpdate(const Keyboard_Class::KeysState* keys, bool header_focused) override;

private:
    bool readShake(int* mg);
    void resetTrace();
    void pushSample(int mg);
    void drawFull();
    void paintLatestColumn();
    void paintFooter(bool force);
    int sampleToY(int mg) const;
    int columns() const;
    int scaleMg() const;

    static constexpr int kTraceColumns = 240;
    static constexpr int kScaleCount = 4;

    bool imu_ok_ = false;
    bool base_ready_ = false;
    bool need_full_ = true;
    bool header_focused_ = false;
    float base_g_ = 1.0f;
    int scale_index_ = 2;
    int cursor_ = 0;
    int prev_y_ = 0;
    int peak_mg_ = 0;
    uint32_t last_sample_ms_ = 0;
    int16_t samples_[kTraceColumns] = {};
    char footer_[32] = {};
};
