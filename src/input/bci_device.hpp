#pragma once
// ╔══════════════════════════════════════════════════════════════════════╗
// ║  DeMoD DSP GUI — OpenBCI Device (LSL Stream Consumer)              ║
// ║                                                                    ║
// ║  Consumes EEG data via Lab Streaming Layer (LSL) and maps          ║
// ║  frequency band powers to input axes:                              ║
// ║    AXIS_X ← Alpha band power (8–12 Hz)  — relaxation/focus        ║
// ║    AXIS_Y ← Beta band power  (12–30 Hz) — active thinking         ║
// ║    AXIS_Z ← Theta band power (4–8 Hz)   — meditation/drowsiness   ║
// ║    AXIS_W ← Gamma band power (30–100 Hz) — higher cognition       ║
// ║                                                                    ║
// ║  Requires: liblsl (Lab Streaming Layer) at runtime.                ║
// ║  Without LSL, this device reports disconnected and is a no-op.     ║
// ╚══════════════════════════════════════════════════════════════════════╝

#include "input/device.hpp"
#include <atomic>
#include <thread>
#include <mutex>
#include <array>

namespace demod::input {

class BCIDevice : public InputDevice {
public:
    BCIDevice();
    ~BCIDevice() override;

    std::string name()      const override { return "OpenBCI (LSL)"; }
    std::string type_tag()  const override { return "bci:lsl"; }
    bool        connected() const override { return connected_.load(); }

    bool open()  override;
    void close() override;
    void poll(std::vector<RawEvent>& events_out) override;

    std::vector<Binding> default_bindings() const override;

    // BCI-specific: configure band power thresholds
    void set_alpha_range(float low, float high) { alpha_range_ = {low, high}; }
    void set_beta_range(float low, float high)  { beta_range_  = {low, high}; }

private:
    std::atomic<bool> connected_{false};
    std::atomic<bool> running_{false};
    std::thread       reader_thread_;
    std::mutex        data_mutex_;

    // Band power values (normalized 0.0–1.0)
    std::array<float, 4> band_powers_{}; // alpha, beta, theta, gamma
    std::array<float, 2> alpha_range_{0.0f, 50.0f};
    std::array<float, 2> beta_range_{0.0f, 30.0f};

    void reader_loop();
};

} // namespace demod::input
