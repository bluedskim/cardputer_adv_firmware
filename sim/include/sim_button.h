#pragma once

class SimButton {
public:
    void notifyPressed(bool pressed) { pressed_ = pressed; }

    bool wasClicked() {
        if (!clicked_) {
            return false;
        }
        clicked_ = false;
        return true;
    }

    void update(bool pressed) {
        if (pressed && !prev_pressed_) {
            clicked_ = true;
        }
        prev_pressed_ = pressed;
    }

private:
    bool pressed_ = false;
    bool prev_pressed_ = false;
    bool clicked_ = false;
};
