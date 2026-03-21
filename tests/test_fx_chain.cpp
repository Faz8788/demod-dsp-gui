// FX chain processor tests
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "audio/fx_chain.hpp"
#include <cstring>
#include <cmath>

using namespace demod;
using namespace demod::audio;
using Catch::Approx;

TEST_CASE("FXChainProcessor default state all bypassed", "[fxchain]") {
    FXChainProcessor fx;
    for (int i = 0; i < MAX_FX_SLOTS; ++i) {
        CHECK_FALSE(fx.slot_loaded(i));
        CHECK(fx.slot_bypassed(i));  // Bypassed by default
        CHECK(fx.slot_wet_mix(i) == Approx(1.0f));
    }
}

TEST_CASE("FXChainProcessor slot state set/get", "[fxchain]") {
    FXChainProcessor fx;

    fx.set_slot_bypassed(3, false);
    CHECK_FALSE(fx.slot_bypassed(3));

    fx.set_slot_wet_mix(5, 0.75f);
    CHECK(fx.slot_wet_mix(5) == Approx(0.75f));

    fx.set_slot_wet_mix(5, 1.5f); // Should clamp
    CHECK(fx.slot_wet_mix(5) == Approx(1.0f));

    fx.set_slot_wet_mix(5, -0.5f); // Should clamp
    CHECK(fx.slot_wet_mix(5) == Approx(0.0f));
}

TEST_CASE("FXChainProcessor out of range slot is safe", "[fxchain]") {
    FXChainProcessor fx;

    CHECK_FALSE(fx.slot_loaded(-1));
    CHECK_FALSE(fx.slot_loaded(MAX_FX_SLOTS));
    CHECK(fx.slot_bypassed(-1));
    CHECK(fx.slot_wet_mix(-1) == Approx(0.0f));

    fx.set_slot_bypassed(-1, false);
    fx.set_slot_wet_mix(MAX_FX_SLOTS, 0.5f);
    fx.load_slot(-1, "test.dsp");
    fx.unload_slot(MAX_FX_SLOTS);
    // Should not crash
    SUCCEED();
}

TEST_CASE("FXChainProcessor swap slots swaps state", "[fxchain]") {
    FXChainProcessor fx;

    fx.set_slot_bypassed(0, false);
    fx.set_slot_wet_mix(0, 0.3f);
    fx.set_slot_bypassed(1, true);
    fx.set_slot_wet_mix(1, 0.8f);

    fx.swap_slots(0, 1);

    CHECK(fx.slot_bypassed(0));   // Was slot 1's state
    CHECK(fx.slot_wet_mix(0) == Approx(0.8f));
    CHECK_FALSE(fx.slot_bypassed(1)); // Was slot 0's state
    CHECK(fx.slot_wet_mix(1) == Approx(0.3f));
}

TEST_CASE("FXChainProcessor swap same slot is no-op", "[fxchain]") {
    FXChainProcessor fx;

    fx.set_slot_bypassed(3, false);
    fx.set_slot_wet_mix(3, 0.5f);

    fx.swap_slots(3, 3);

    CHECK_FALSE(fx.slot_bypassed(3));
    CHECK(fx.slot_wet_mix(3) == Approx(0.5f));
}

TEST_CASE("FXChainProcessor swap out of range is safe", "[fxchain]") {
    FXChainProcessor fx;
    fx.swap_slots(-1, 0);
    fx.swap_slots(0, MAX_FX_SLOTS);
    fx.swap_slots(-1, MAX_FX_SLOTS);
    SUCCEED();
}

TEST_CASE("FXChainProcessor process_serial with no slots is pass-through", "[fxchain]") {
    FXChainProcessor fx;

    float buf[256 * 2];
    for (int i = 0; i < 512; ++i) buf[i] = 0.5f;

    fx.process_serial(buf, 2, 256);

    // No slots loaded — buffer should be unchanged
    for (int i = 0; i < 512; ++i)
        CHECK(buf[i] == Approx(0.5f));
}

TEST_CASE("FXChainProcessor unload_all clears loaded state", "[fxchain]") {
    FXChainProcessor fx;
    fx.set_slot_bypassed(0, false);
    fx.set_slot_wet_mix(0, 0.5f);

    fx.unload_all();

    for (int i = 0; i < MAX_FX_SLOTS; ++i) {
        CHECK_FALSE(fx.slot_loaded(i));
    }
    // Bypassed state is preserved (unload only clears loaded, not bypass)
    CHECK_FALSE(fx.slot_bypassed(0));
}

TEST_CASE("FXChainProcessor sample rate", "[fxchain]") {
    FXChainProcessor fx;
    CHECK(fx.sample_rate() == AUDIO_RATE);

    fx.set_sample_rate(96000);
    CHECK(fx.sample_rate() == 96000);
}

TEST_CASE("FXChainProcessor param access on unloaded slot", "[fxchain]") {
    FXChainProcessor fx;
    CHECK(fx.slot_num_params(0) == 0);
    CHECK(fx.slot_params(0).empty());
    CHECK(fx.get_slot_param(0, 0) == Approx(0.0f));

    // Should not crash
    fx.set_slot_param(0, 0, 0.5f);
    fx.randomize_slot_params(0);
    fx.reset_slot_params(0);
    SUCCEED();
}
