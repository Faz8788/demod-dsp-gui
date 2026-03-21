// ╔══════════════════════════════════════════════════════════════════════╗
// ║  DeMoD DSP GUI — Input Manager Implementation                      ║
// ╚══════════════════════════════════════════════════════════════════════╝

#include "input/input_manager.hpp"
#include "input/gamepad_device.hpp"
#include <SDL2/SDL.h>
#include <algorithm>
#include <cmath>
#include <sstream>
#include <cstdio>

namespace demod::input {

InputManager::InputManager() {
    state_.fill({});
    prev_state_.fill({});
    event_buf_.reserve(64);
}

InputManager::~InputManager() {
    for (auto& entry : devices_) {
        entry.device->close();
    }
}

void InputManager::add_device(std::unique_ptr<InputDevice> device) {
    device->open();
    auto bindings = device->default_bindings();
    devices_.push_back({ std::move(device), std::move(bindings) });
}

void InputManager::remove_device(const std::string& type_tag) {
    devices_.erase(
        std::remove_if(devices_.begin(), devices_.end(),
            [&](const DeviceEntry& e) {
                if (e.device->type_tag() == type_tag) {
                    e.device->close();
                    return true;
                }
                return false;
            }),
        devices_.end()
    );
}

void InputManager::bind(const std::string& device_tag, Binding binding) {
    for (auto& entry : devices_) {
        if (entry.device->type_tag() == device_tag) {
            entry.bindings.push_back(binding);
            return;
        }
    }
}

void InputManager::clear_bindings(const std::string& device_tag) {
    for (auto& entry : devices_) {
        if (entry.device->type_tag() == device_tag) {
            entry.bindings.clear();
            return;
        }
    }
}

void InputManager::load_default_bindings() {
    for (auto& entry : devices_) {
        entry.bindings = entry.device->default_bindings();
    }
}

void InputManager::process_sdl_event(const SDL_Event& event) {
    // Keyboard events are forwarded to KeyboardDevice via its own poll,
    // but we handle gamepad hotplug here for discovery.
    if (event.type == SDL_CONTROLLERDEVICEADDED) {
        int idx = event.cdevice.which;
        if (SDL_IsGameController(idx)) {
            auto gp = std::make_unique<GamepadDevice>(idx);
            fprintf(stderr, "[INPUT] Gamepad connected: %s\n",
                    gp->name().c_str());
            add_device(std::move(gp));
        }
    } else if (event.type == SDL_CONTROLLERDEVICEREMOVED) {
        SDL_JoystickID removed_id = event.cdevice.which;
        for (auto& entry : devices_) {
            if (entry.device->type_tag().rfind("gamepad:", 0) != 0)
                continue;
            auto* gp = static_cast<GamepadDevice*>(entry.device.get());
            if (gp->joystick_instance_id() == removed_id) {
                fprintf(stderr, "[INPUT] Gamepad disconnected: %s\n",
                        gp->name().c_str());
                gp->close();
                break;
            }
        }
    }
}

void InputManager::update() {
    // Save previous state for edge detection
    prev_state_ = state_;

    // Reset transient fields (pressed/released are per-frame)
    for (auto& s : state_) {
        s.pressed  = false;
        s.released = false;
    }

    // Poll each device
    for (auto& entry : devices_) {
        if (!entry.device->connected()) continue;

        event_buf_.clear();
        entry.device->poll(event_buf_);

        for (const auto& raw : event_buf_) {
            resolve_event(raw, entry.bindings);
        }
    }

    // Detect edges
    for (size_t i = 0; i < state_.size(); ++i) {
        auto& cur  = state_[i];
        auto& prev = prev_state_[i];

        if (cur.held && !prev.held) {
            cur.pressed = true;
        }
        if (!cur.held && prev.held) {
            cur.released = true;
        }
    }
}

void InputManager::resolve_event(const RawEvent& raw,
                                  const std::vector<Binding>& bindings) {
    for (const auto& bind : bindings) {
        if (bind.source_id != raw.source_id) continue;

        auto& s = state_[size_t(bind.action)];

        if (bind.is_axis) {
            // Apply deadzone and scale
            float v = raw.value * bind.scale;
            if (std::fabs(v) < bind.deadzone) v = 0.0f;
            s.analog = v;
            s.held   = (std::fabs(v) > 0.5f); // Axis-as-button threshold
        } else {
            // Digital button
            if (raw.type == RawEvent::Type::BUTTON_DOWN) {
                s.held   = true;
                s.analog = 1.0f;
            } else if (raw.type == RawEvent::Type::BUTTON_UP) {
                s.held   = false;
                s.analog = 0.0f;
            }
        }
    }
}

const ActionState& InputManager::action(Action a) const {
    return state_[size_t(a)];
}

float InputManager::axis(Action a) const {
    return state_[size_t(a)].analog;
}

bool InputManager::pressed(Action a) const {
    return state_[size_t(a)].pressed;
}

bool InputManager::held(Action a) const {
    return state_[size_t(a)].held;
}

int InputManager::device_count() const {
    return (int)devices_.size();
}

std::string InputManager::device_name(int index) const {
    if (index < 0 || index >= (int)devices_.size()) return "";
    return devices_[index].device->name();
}

bool InputManager::device_connected(int index) const {
    if (index < 0 || index >= (int)devices_.size()) return false;
    return devices_[index].device->connected();
}

std::string InputManager::debug_string() const {
    std::ostringstream ss;
    ss << "INPUT [" << devices_.size() << " devices]\n";
    for (const auto& entry : devices_) {
        ss << "  " << entry.device->type_tag()
           << " [" << (entry.device->connected() ? "OK" : "DC") << "] "
           << entry.bindings.size() << " bindings\n";
    }

    // Show active actions
    for (size_t i = 1; i < size_t(Action::ACTION_COUNT); ++i) {
        const auto& s = state_[i];
        if (s.held || std::fabs(s.analog) > 0.01f) {
            ss << "  ACTION " << i << ": "
               << (s.held ? "HELD" : "    ") << " "
               << "analog=" << s.analog << "\n";
        }
    }
    return ss.str();
}

} // namespace demod::input
