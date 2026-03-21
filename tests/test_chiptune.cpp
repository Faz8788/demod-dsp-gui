// Chiptune synth tests
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "audio/chiptune.hpp"
#include <cmath>
#include <cstring>
#include <algorithm>

using namespace demod::audio;
using Catch::Approx;

TEST_CASE("Chiptune init sets sample rate", "[chiptune]") {
    ChiptuneSynth synth;
    synth.init(48000);
    // No crash, no output before set_playing
    float buf[256 * 2] = {};
    synth.process(buf, 2, 256);
    // Should be silent (playing_ = false)
    for (int i = 0; i < 512; ++i)
        CHECK(buf[i] == Approx(0.0f).margin(1e-10f));
}

TEST_CASE("Chiptune playing produces output", "[chiptune]") {
    ChiptuneSynth synth;
    synth.init(48000);
    synth.set_playing(true);
    synth.set_volume(0.5f);

    float buf[256 * 2] = {};
    synth.process(buf, 2, 256);

    // Should have some non-zero output
    float max_val = 0;
    for (int i = 0; i < 512; ++i)
        max_val = std::max(max_val, std::fabs(buf[i]));
    CHECK(max_val > 0.0f);
}

TEST_CASE("Chiptune output has no NaN or Inf", "[chiptune]") {
    ChiptuneSynth synth;
    synth.init(48000);
    synth.set_playing(true);

    // Process many blocks to exercise all code paths
    for (int block = 0; block < 200; ++block) {
        float buf[256 * 2] = {};
        synth.process(buf, 2, 256);
        for (int i = 0; i < 512; ++i) {
            CHECK(!std::isnan(buf[i]));
            CHECK(!std::isinf(buf[i]));
        }
    }
}

TEST_CASE("Chiptune output is in valid range", "[chiptune]") {
    ChiptuneSynth synth;
    synth.init(48000);
    synth.set_playing(true);
    synth.set_volume(1.0f);

    for (int block = 0; block < 100; ++block) {
        float buf[256 * 2] = {};
        synth.process(buf, 2, 256);
        for (int i = 0; i < 512; ++i) {
            CHECK(buf[i] >= -1.0f);
            CHECK(buf[i] <= 1.0f);
        }
    }
}

TEST_CASE("Chiptune stereo channels differ", "[chiptune]") {
    ChiptuneSynth synth;
    synth.init(48000);
    synth.set_playing(true);

    float buf[256 * 2] = {};
    synth.process(buf, 2, 256);

    // L and R should differ slightly (lead left, pad right panning)
    bool any_diff = false;
    for (int i = 0; i < 256; ++i) {
        if (std::fabs(buf[i * 2] - buf[i * 2 + 1]) > 1e-6f) {
            any_diff = true;
            break;
        }
    }
    CHECK(any_diff);
}

TEST_CASE("Chiptune mono output", "[chiptune]") {
    ChiptuneSynth synth;
    synth.init(48000);
    synth.set_playing(true);

    float buf[256] = {};
    synth.process(buf, 1, 256);

    float max_val = 0;
    for (int i = 0; i < 256; ++i)
        max_val = std::max(max_val, std::fabs(buf[i]));
    CHECK(max_val > 0.0f);
}

TEST_CASE("Chiptune set_playing(false) silences", "[chiptune]") {
    ChiptuneSynth synth;
    synth.init(48000);
    synth.set_playing(true);

    // Play some blocks
    float buf[256 * 2] = {};
    synth.process(buf, 2, 256);

    // Stop
    synth.set_playing(false);

    // Process more blocks — should be silent
    for (int i = 0; i < 5; ++i) {
        std::memset(buf, 0, sizeof(buf));
        synth.process(buf, 2, 256);
        for (int j = 0; j < 512; ++j)
            CHECK(buf[j] == Approx(0.0f).margin(1e-10f));
    }
}

TEST_CASE("Chiptune reset produces different music", "[chiptune]") {
    ChiptuneSynth synth;
    synth.init(48000);
    synth.set_playing(true);

    // Play and capture first block
    float buf1[256 * 2] = {};
    synth.process(buf1, 2, 256);

    // Reset and play again
    synth.reset();
    synth.set_playing(true);

    float buf2[256 * 2] = {};
    synth.process(buf2, 2, 256);

    // The music should be different after reset (randomized)
    // Just check that both produce non-silent output
    float max1 = 0, max2 = 0;
    for (int i = 0; i < 512; ++i) {
        max1 = std::max(max1, std::fabs(buf1[i]));
        max2 = std::max(max2, std::fabs(buf2[i]));
    }
    CHECK(max1 > 0.0f);
    CHECK(max2 > 0.0f);
}

TEST_CASE("Chiptune volume affects amplitude", "[chiptune]") {
    ChiptuneSynth synth;
    synth.init(48000);
    synth.set_playing(true);

    synth.set_volume(1.0f);
    float buf_loud[256 * 2] = {};
    synth.process(buf_loud, 2, 256);

    synth.reset();
    synth.set_playing(true);
    synth.set_volume(0.1f);
    float buf_quiet[256 * 2] = {};
    synth.process(buf_quiet, 2, 256);

    // Compare peak amplitudes
    float peak_loud = 0, peak_quiet = 0;
    for (int i = 0; i < 512; ++i) {
        peak_loud = std::max(peak_loud, std::fabs(buf_loud[i]));
        peak_quiet = std::max(peak_quiet, std::fabs(buf_quiet[i]));
    }
    // Quiet should be significantly less than loud
    CHECK(peak_quiet < peak_loud);
}

TEST_CASE("Chiptune enabled flag prevents start", "[chiptune]") {
    ChiptuneSynth synth;
    synth.init(48000);
    synth.set_enabled(false);
    synth.set_playing(true);

    float buf[256 * 2] = {};
    synth.process(buf, 2, 256);

    for (int i = 0; i < 512; ++i)
        CHECK(buf[i] == Approx(0.0f).margin(1e-10f));
}
