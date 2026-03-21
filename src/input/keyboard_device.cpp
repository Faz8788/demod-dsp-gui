#include "input/keyboard_device.hpp"
#include <cstring>
#include <algorithm>

namespace demod::input {

KeyboardDevice::KeyboardDevice() {
    prev_keys_.fill(0);
}

void KeyboardDevice::poll(std::vector<RawEvent>& events_out) {
    uint64_t now = SDL_GetPerformanceCounter() * 1000000ULL /
                   SDL_GetPerformanceFrequency();

    // ── Keyboard ─────────────────────────────────────────────────────
    int num_keys = 0;
    const uint8_t* keys = SDL_GetKeyboardState(&num_keys);
    int limit = std::min(num_keys, (int)SDL_NUM_SCANCODES);

    for (int i = 0; i < limit; ++i) {
        if (keys[i] && !prev_keys_[i]) {
            events_out.push_back({
                RawEvent::Type::BUTTON_DOWN, i, 1.0f, now
            });
        } else if (!keys[i] && prev_keys_[i]) {
            events_out.push_back({
                RawEvent::Type::BUTTON_UP, i, 0.0f, now
            });
        }
    }
    std::memcpy(prev_keys_.data(), keys, limit);

    // ── Mouse position delta ─────────────────────────────────────────
    int mx, my;
    uint32_t mbuttons = SDL_GetMouseState(&mx, &my);

    int dx = mx - prev_mouse_x_;
    int dy = my - prev_mouse_y_;
    prev_mouse_x_ = mx;
    prev_mouse_y_ = my;

    if (dx != 0) {
        events_out.push_back({
            RawEvent::Type::AXIS_MOVE, 900,
            float(dx) / 10.0f, now  // Normalize roughly
        });
    }
    if (dy != 0) {
        events_out.push_back({
            RawEvent::Type::AXIS_MOVE, 901,
            float(dy) / 10.0f, now
        });
    }

    // ── Mouse buttons — edge detect ─────────────────────────────────
    auto emit_mouse_edge = [&](uint32_t mask, int id) {
        bool now_down = (mbuttons & mask) != 0;
        bool was_down = (prev_mouse_buttons_ & mask) != 0;
        if (now_down && !was_down) {
            events_out.push_back({
                RawEvent::Type::BUTTON_DOWN, id, 1.0f, now
            });
        } else if (!now_down && was_down) {
            events_out.push_back({
                RawEvent::Type::BUTTON_UP, id, 0.0f, now
            });
        }
    };
    emit_mouse_edge(SDL_BUTTON_LMASK,  910);
    emit_mouse_edge(SDL_BUTTON_RMASK,  911);
    emit_mouse_edge(SDL_BUTTON_MMASK,  912);
    prev_mouse_buttons_ = mbuttons;
}

std::vector<Binding> KeyboardDevice::default_bindings() const {
    return {
        // WASD / Arrow navigation
        { SDL_SCANCODE_W,     Action::NAV_UP,    0, 1, false },
        { SDL_SCANCODE_S,     Action::NAV_DOWN,  0, 1, false },
        { SDL_SCANCODE_A,     Action::NAV_LEFT,  0, 1, false },
        { SDL_SCANCODE_D,     Action::NAV_RIGHT, 0, 1, false },
        { SDL_SCANCODE_UP,    Action::NAV_UP,    0, 1, false },
        { SDL_SCANCODE_DOWN,  Action::NAV_DOWN,  0, 1, false },
        { SDL_SCANCODE_LEFT,  Action::NAV_LEFT,  0, 1, false },
        { SDL_SCANCODE_RIGHT, Action::NAV_RIGHT, 0, 1, false },

        // Enter / Escape
        { SDL_SCANCODE_RETURN, Action::NAV_SELECT, 0, 1, false },
        { SDL_SCANCODE_ESCAPE, Action::NAV_BACK,   0, 1, false },

        // Parameter control
        { SDL_SCANCODE_KP_PLUS,  Action::PARAM_INC,  0, 1, false },
        { SDL_SCANCODE_KP_MINUS, Action::PARAM_DEC,  0, 1, false },
        { SDL_SCANCODE_EQUALS,   Action::PARAM_INC,  0, 1, false },
        { SDL_SCANCODE_MINUS,    Action::PARAM_DEC,  0, 1, false },
        { SDL_SCANCODE_BACKSPACE,Action::PARAM_RESET, 0, 1, false },
        { SDL_SCANCODE_R,        Action::PARAM_RANDOMIZE, 0, 1, false },

        // Transport
        { SDL_SCANCODE_SPACE,  Action::TRANSPORT_PLAY, 0, 1, false },
        { SDL_SCANCODE_PERIOD, Action::TRANSPORT_STOP, 0, 1, false },
        { SDL_SCANCODE_B,      Action::BYPASS_TOGGLE,  0, 1, false },
        { SDL_SCANCODE_P,      Action::PANIC,          0, 1, false },

        // Screen navigation
        { SDL_SCANCODE_TAB,     Action::SCREEN_NEXT,   0, 1, false },
        { SDL_SCANCODE_PAGEUP,  Action::NAV_PAGE_UP,   0, 1, false },
        { SDL_SCANCODE_PAGEDOWN,Action::NAV_PAGE_DOWN, 0, 1, false },

        // Sub-tab navigation (within Viz/Settings screens)
        { SDL_SCANCODE_LEFTBRACKET,  Action::NAV_TAB_PREV,  0, 1, false },
        { SDL_SCANCODE_RIGHTBRACKET, Action::NAV_TAB_NEXT,  0, 1, false },

        // Fullscreen
        { SDL_SCANCODE_F11,     Action::FULLSCREEN_TOGGLE, 0, 1, false },
        { SDL_SCANCODE_Q,       Action::QUIT,              0, 1, false },

        // Mouse drag → axis
        { 900, Action::AXIS_X, 0, 0.1f, true },
        { 901, Action::AXIS_Y, 0, 0.1f, true },

        // Mouse left click → select
        { 910, Action::NAV_SELECT, 0, 1, false },
    };
}

} // namespace demod::input
