#pragma once
#include "input/device.hpp"
#include <SDL2/SDL.h>
#include <array>

namespace demod::input {

class KeyboardDevice : public InputDevice {
public:
    KeyboardDevice();

    std::string name()      const override { return "Keyboard + Mouse"; }
    std::string type_tag()  const override { return "keyboard"; }
    bool        connected() const override { return true; }

    void poll(std::vector<RawEvent>& events_out) override;
    std::vector<Binding> default_bindings() const override;

private:
    std::array<uint8_t, SDL_NUM_SCANCODES> prev_keys_{};
    int prev_mouse_x_ = 0, prev_mouse_y_ = 0;
    uint32_t prev_mouse_buttons_ = 0;

    // Keyboard button IDs: we use SDL scancodes directly.
    // Mouse axes: 900 = mouse_x_delta, 901 = mouse_y_delta
    // Mouse buttons: 910 = left, 911 = right, 912 = middle
    // Scroll: 920 = scroll_y
};

} // namespace demod::input
