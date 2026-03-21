#include "input/bci_device.hpp"
#include <cmath>
#include <cstdio>
#include <algorithm>

// ── LSL is optional at compile time ──────────────────────────────────
// To enable: install liblsl-dev, uncomment the include, and link -llsl.
// #include <lsl_cpp.h>
// #define HAVE_LSL 1

namespace demod::input {

BCIDevice::BCIDevice() {
    band_powers_.fill(0.0f);
}

BCIDevice::~BCIDevice() {
    close();
}

bool BCIDevice::open() {
#ifdef HAVE_LSL
    running_ = true;
    reader_thread_ = std::thread(&BCIDevice::reader_loop, this);
    return true;
#else
    fprintf(stderr, "[BCI] liblsl not available — BCI device disabled.\n");
    fprintf(stderr, "[BCI] Install liblsl and rebuild with -DHAVE_LSL=1.\n");
    return false;
#endif
}

void BCIDevice::close() {
    running_ = false;
    if (reader_thread_.joinable()) {
        reader_thread_.join();
    }
    connected_ = false;
}

void BCIDevice::reader_loop() {
#ifdef HAVE_LSL
    try {
        // Resolve an EEG stream on the network
        fprintf(stderr, "[BCI] Searching for LSL EEG stream...\n");
        auto results = lsl::resolve_stream("type", "EEG", 1, 5.0);

        if (results.empty()) {
            fprintf(stderr, "[BCI] No EEG stream found.\n");
            return;
        }

        lsl::stream_inlet inlet(results[0]);
        int n_channels = inlet.info().channel_count();
        double srate   = inlet.info().nominal_srate();
        std::vector<float> sample(n_channels);

        connected_ = true;
        fprintf(stderr, "[BCI] Connected: %s (%d ch, %.0f Hz)\n",
                inlet.info().name().c_str(), n_channels, srate);

        // Simple band power estimation via sliding DFT bins
        // For production: use proper FFT with Hanning window
        constexpr int WINDOW = 256;
        std::vector<std::vector<float>> buffer(n_channels,
                                                std::vector<float>(WINDOW, 0));
        int buf_pos = 0;

        while (running_) {
            double timestamp = inlet.pull_sample(sample, 0.1);
            if (timestamp == 0.0) continue;

            // Store sample (use channel 0 for now)
            buffer[0][buf_pos % WINDOW] = sample[0];
            buf_pos++;

            if (buf_pos % 32 == 0) { // Update every ~32 samples
                // Compute simple band powers via DFT
                float alpha = 0, beta = 0, theta = 0, gamma = 0;
                for (int k = 0; k < WINDOW / 2; ++k) {
                    float freq = k * srate / WINDOW;
                    float power = 0;
                    float cos_sum = 0, sin_sum = 0;
                    for (int n = 0; n < WINDOW; ++n) {
                        float angle = 2.0f * M_PI * k * n / WINDOW;
                        cos_sum += buffer[0][n] * std::cos(angle);
                        sin_sum += buffer[0][n] * std::sin(angle);
                    }
                    power = (cos_sum * cos_sum + sin_sum * sin_sum) / WINDOW;

                    if (freq >= 8  && freq < 12)  alpha += power;
                    if (freq >= 12 && freq < 30)  beta  += power;
                    if (freq >= 4  && freq < 8)   theta += power;
                    if (freq >= 30 && freq < 100)  gamma += power;
                }

                // Normalize to 0.0–1.0 range using configured ranges
                auto norm = [](float val, float lo, float hi) -> float {
                    if (hi <= lo) return 0;
                    return std::clamp((val - lo) / (hi - lo), 0.0f, 1.0f);
                };

                std::lock_guard<std::mutex> lock(data_mutex_);
                band_powers_[0] = norm(alpha, alpha_range_[0], alpha_range_[1]);
                band_powers_[1] = norm(beta,  beta_range_[0],  beta_range_[1]);
                band_powers_[2] = norm(theta, 0, 20.0f);
                band_powers_[3] = norm(gamma, 0, 10.0f);
            }
        }

    } catch (const std::exception& e) {
        fprintf(stderr, "[BCI] LSL error: %s\n", e.what());
    }
#endif
    connected_ = false;
}

void BCIDevice::poll(std::vector<RawEvent>& events_out) {
    if (!connected_) return;

    uint64_t now = 0; // Use SDL clock in real impl

    std::lock_guard<std::mutex> lock(data_mutex_);

    // Emit band powers as axis events
    // source_id 2000–2003: alpha, beta, theta, gamma
    for (int i = 0; i < 4; ++i) {
        events_out.push_back({
            RawEvent::Type::AXIS_MOVE,
            2000 + i,
            band_powers_[i],
            now
        });
    }
}

std::vector<Binding> BCIDevice::default_bindings() const {
    return {
        // Alpha → primary axis X (relaxation controls cutoff, etc.)
        { 2000, Action::AXIS_X, 0.05f, 1.0f, true },
        // Beta → primary axis Y (active thinking controls resonance)
        { 2001, Action::AXIS_Y, 0.05f, 1.0f, true },
        // Theta → axis Z
        { 2002, Action::AXIS_Z, 0.05f, 1.0f, true },
        // Gamma → axis W
        { 2003, Action::AXIS_W, 0.05f, 1.0f, true },
    };
}

} // namespace demod::input
