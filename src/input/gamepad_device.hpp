#pragma once
#include "input/device.hpp"
#include <SDL2/SDL.h>

namespace demod::input {

class GamepadDevice : public InputDevice {
public:
    explicit GamepadDevice(int sdl_index);
    ~GamepadDevice() override;

    std::string name()      const override;
    std::string type_tag()  const override;
    bool        connected() const override;

    bool open()  override;
    void close() override;

    void poll(std::vector<RawEvent>& events_out) override;
    void haptic(float low, float high, uint32_t ms) override;
    void set_led(uint8_t r, uint8_t g, uint8_t b) override;

    std::vector<Binding> default_bindings() const override;

private:
    int                  sdl_index_;
    SDL_GameController*  controller_ = nullptr;
    SDL_Haptic*          haptic_     = nullptr;
    bool                 connected_  = false;

    // Axis state cache for delta detection
    float axes_[SDL_CONTROLLER_AXIS_MAX] = {};

    // Button state cache for edge detection
    uint8_t buttons_[SDL_CONTROLLER_BUTTON_MAX] = {};

    static constexpr float AXIS_SCALE = 1.0f / 32767.0f;
};

} // namespace demod::input
