#include "input/gamepad_device.hpp"
#include <cmath>

namespace demod::input {

GamepadDevice::GamepadDevice(int sdl_index) : sdl_index_(sdl_index) {}

GamepadDevice::~GamepadDevice() { close(); }

std::string GamepadDevice::name() const {
    if (controller_) {
        const char* n = SDL_GameControllerName(controller_);
        if (n) return n;
    }
    return "Gamepad " + std::to_string(sdl_index_);
}

std::string GamepadDevice::type_tag() const {
    return "gamepad:" + std::to_string(sdl_index_);
}

bool GamepadDevice::connected() const { return connected_; }

bool GamepadDevice::open() {
    if (!SDL_IsGameController(sdl_index_)) return false;

    controller_ = SDL_GameControllerOpen(sdl_index_);
    if (!controller_) return false;

    connected_ = true;

    // Try to open haptic on the underlying joystick
    SDL_Joystick* js = SDL_GameControllerGetJoystick(controller_);
    if (SDL_JoystickIsHaptic(js)) {
        haptic_ = SDL_HapticOpenFromJoystick(js);
        if (haptic_) {
            SDL_HapticRumbleInit(haptic_);
        }
    }

    return true;
}

void GamepadDevice::close() {
    if (haptic_) {
        SDL_HapticClose(haptic_);
        haptic_ = nullptr;
    }
    if (controller_) {
        SDL_GameControllerClose(controller_);
        controller_ = nullptr;
    }
    connected_ = false;
}

void GamepadDevice::poll(std::vector<RawEvent>& events_out) {
    if (!controller_ || !connected_) return;

    uint64_t now = SDL_GetPerformanceCounter() * 1000000ULL /
                   SDL_GetPerformanceFrequency();

    // Poll axes
    for (int i = 0; i < SDL_CONTROLLER_AXIS_MAX; ++i) {
        float raw = SDL_GameControllerGetAxis(controller_,
                        SDL_GameControllerAxis(i)) * AXIS_SCALE;

        if (std::fabs(raw - axes_[i]) > 0.001f) {
            axes_[i] = raw;
            events_out.push_back({
                RawEvent::Type::AXIS_MOVE,
                1000 + i,   // Offset axis IDs above button range
                raw,
                now
            });
        }
    }

    // Poll buttons — track state for press/release edges
    for (int i = 0; i < SDL_CONTROLLER_BUTTON_MAX; ++i) {
        uint8_t state = SDL_GameControllerGetButton(controller_,
                            SDL_GameControllerButton(i));
        if (state && !buttons_[i]) {
            events_out.push_back({
                RawEvent::Type::BUTTON_DOWN, i, 1.0f, now
            });
        } else if (!state && buttons_[i]) {
            events_out.push_back({
                RawEvent::Type::BUTTON_UP, i, 0.0f, now
            });
        }
        buttons_[i] = state;
    }
}

void GamepadDevice::haptic(float low, float high, uint32_t ms) {
    if (!haptic_) return;
    float intensity = (low + high) * 0.5f;
    SDL_HapticRumblePlay(haptic_, intensity, ms);
}

void GamepadDevice::set_led(uint8_t r, uint8_t g, uint8_t b) {
#if SDL_VERSION_ATLEAST(2, 0, 14)
    if (controller_) {
        SDL_GameControllerSetLED(controller_, r, g, b);
    }
#else
    (void)r; (void)g; (void)b;
#endif
}

std::vector<Binding> GamepadDevice::default_bindings() const {
    return {
        // D-pad navigation
        { SDL_CONTROLLER_BUTTON_DPAD_UP,    Action::NAV_UP,     0, 1, false },
        { SDL_CONTROLLER_BUTTON_DPAD_DOWN,  Action::NAV_DOWN,   0, 1, false },
        { SDL_CONTROLLER_BUTTON_DPAD_LEFT,  Action::NAV_LEFT,   0, 1, false },
        { SDL_CONTROLLER_BUTTON_DPAD_RIGHT, Action::NAV_RIGHT,  0, 1, false },

        // Face buttons
        { SDL_CONTROLLER_BUTTON_A,          Action::NAV_SELECT, 0, 1, false },
        { SDL_CONTROLLER_BUTTON_B,          Action::NAV_BACK,   0, 1, false },
        { SDL_CONTROLLER_BUTTON_X,          Action::PARAM_RESET,0, 1, false },
        { SDL_CONTROLLER_BUTTON_Y,          Action::BYPASS_TOGGLE, 0, 1, false },

        // Shoulders
        { SDL_CONTROLLER_BUTTON_LEFTSHOULDER,  Action::SCREEN_PREV, 0, 1, false },
        { SDL_CONTROLLER_BUTTON_RIGHTSHOULDER, Action::SCREEN_NEXT, 0, 1, false },

        // Start/Back
        { SDL_CONTROLLER_BUTTON_START,      Action::TRANSPORT_PLAY, 0, 1, false },
        { SDL_CONTROLLER_BUTTON_BACK,       Action::TRANSPORT_STOP, 0, 1, false },

        // Left stick → parameter control axes
        { 1000 + SDL_CONTROLLER_AXIS_LEFTX,  Action::AXIS_X, 0.15f, 1.0f, true },
        { 1000 + SDL_CONTROLLER_AXIS_LEFTY,  Action::AXIS_Y, 0.15f, -1.0f, true },

        // Right stick → fine parameter control
        { 1000 + SDL_CONTROLLER_AXIS_RIGHTX, Action::PARAM_INC_FINE, 0.2f, 1.0f, true },
        { 1000 + SDL_CONTROLLER_AXIS_RIGHTY, Action::PARAM_DEC_FINE, 0.2f, 1.0f, true },

        // Triggers → assignable axes
        { 1000 + SDL_CONTROLLER_AXIS_TRIGGERLEFT,  Action::AXIS_Z, 0.05f, 1.0f, true },
        { 1000 + SDL_CONTROLLER_AXIS_TRIGGERRIGHT,  Action::AXIS_W, 0.05f, 1.0f, true },
    };
}

} // namespace demod::input
