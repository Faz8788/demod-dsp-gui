#pragma once
// ╔══════════════════════════════════════════════════════════════════════╗
// ║  DeMoD DSP GUI — Input Manager                                     ║
// ║  Aggregates all input devices, resolves bindings to Actions,       ║
// ║  provides unified polling interface to the engine.                 ║
// ╚══════════════════════════════════════════════════════════════════════╝

#include "input/device.hpp"
#include <memory>
#include <array>
#include <vector>

union SDL_Event;

namespace demod::input {

// ── Resolved action state ────────────────────────────────────────────
struct ActionState {
    bool  pressed;       // True on the frame the action was first pressed
    bool  held;          // True while held
    bool  released;      // True on the frame it was released
    float analog;        // Continuous value (axes: -1..1, buttons: 0/1)
};

class InputManager {
public:
    InputManager();
    ~InputManager();

    // Register a new input device. Manager takes ownership.
    void add_device(std::unique_ptr<InputDevice> device);

    // Remove a device by type tag (e.g., "gamepad:0")
    void remove_device(const std::string& type_tag);

    // Add/remove/clear bindings for a specific device
    void bind(const std::string& device_tag, Binding binding);
    void clear_bindings(const std::string& device_tag);
    void load_default_bindings();

    // Process SDL events (keyboard, gamepad hotplug, etc.)
    void process_sdl_event(const SDL_Event& event);

    // Poll all devices and resolve actions. Call once per frame.
    void update();

    // Query action state
    const ActionState& action(Action a) const;
    float axis(Action a) const;    // Shorthand for analog value
    bool  pressed(Action a) const; // Shorthand for pressed edge
    bool  held(Action a) const;

    // Device introspection (safe accessors — no ownership exposure)
    int         device_count() const;
    std::string device_name(int index) const;
    bool        device_connected(int index) const;

    // Human-readable dump (for debug overlay)
    std::string debug_string() const;

    // MIDI learn — watch for next MIDI event and bind it to an action
    void start_learn(Action target_action, const std::string& device_tag = "midi");
    void cancel_learn();
    bool learn_active() const { return learn_active_; }
    // Returns the learned source_id if a learn completed this frame, else -1
    int  learn_result() const { return learn_result_; }

private:
    struct DeviceEntry {
        std::unique_ptr<InputDevice>  device;
        std::vector<Binding>          bindings;
    };

    std::vector<DeviceEntry> devices_;
    std::array<ActionState, size_t(Action::ACTION_COUNT)> state_;
    std::array<ActionState, size_t(Action::ACTION_COUNT)> prev_state_;
    std::vector<RawEvent> event_buf_;  // Reusable per-frame buffer

    // MIDI learn state
    bool        learn_active_ = false;
    Action      learn_action_ = Action::NONE;
    std::string learn_device_tag_;
    int         learn_result_ = -1;

    void resolve_event(const RawEvent& raw, const std::vector<Binding>& bindings);
};

} // namespace demod::input
