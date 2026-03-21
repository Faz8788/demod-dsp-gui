#pragma once
// ╔══════════════════════════════════════════════════════════════════════╗
// ║  DeMoD DSP GUI — Input Device Abstraction                          ║
// ║  Any input source implements this interface: gamepad, keyboard,     ║
// ║  BCI headset, MIDI, OSC, custom serial, etc.                       ║
// ╚══════════════════════════════════════════════════════════════════════╝

#include "core/config.hpp"
#include <string>
#include <functional>
#include <vector>
#include <unordered_map>

namespace demod::input {

// ── Raw input event from a device ────────────────────────────────────
struct RawEvent {
    enum class Type { BUTTON_DOWN, BUTTON_UP, AXIS_MOVE } type;
    int      source_id;   // Device-specific button/axis ID
    float    value;        // 0.0/1.0 for buttons, -1.0–1.0 for axes
    uint64_t timestamp_us; // Microsecond timestamp
};

// ── Binding: maps a raw device input to a semantic action ────────────
struct Binding {
    int      source_id;    // Raw button/axis ID on the device
    Action   action;       // Semantic action it maps to
    float    deadzone;     // Axis deadzone (0.0–1.0)
    float    scale;        // Multiplier (negative to invert)
    bool     is_axis;      // true = analog axis, false = digital button
};

// ── Abstract input device ────────────────────────────────────────────
class InputDevice {
public:
    virtual ~InputDevice() = default;

    // Human-readable device name
    virtual std::string name() const = 0;

    // Device type tag for serialization / UI
    virtual std::string type_tag() const = 0;

    // Is this device currently connected and functional?
    virtual bool connected() const = 0;

    // Poll for new events. Called once per frame.
    // Implementations should push events into the provided vector.
    virtual void poll(std::vector<RawEvent>& events_out) = 0;

    // Optional: device-specific setup (e.g., open serial port, calibrate)
    virtual bool open()  { return true; }
    virtual void close() {}

    // Rumble / haptic feedback (0.0–1.0 intensity)
    virtual void haptic(float /*low_freq*/, float /*high_freq*/,
                        uint32_t /*duration_ms*/) {}

    // Get default bindings for this device type
    virtual std::vector<Binding> default_bindings() const = 0;

    // LED / status indicator control (e.g., gamepad LED color)
    virtual void set_led(uint8_t /*r*/, uint8_t /*g*/, uint8_t /*b*/) {}
};

} // namespace demod::input
