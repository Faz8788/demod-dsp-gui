// Faust bridge parameter management tests (no libfaust needed)
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "audio/faust_bridge.hpp"
#include "core/config.hpp"
#include <cmath>
#include <algorithm>

using namespace demod;
using namespace demod::audio;
using Catch::Approx;

TEST_CASE("FaustBridge default state is unloaded", "[faust]") {
    FaustBridge bridge;
    CHECK_FALSE(bridge.loaded());
    CHECK(bridge.num_inputs() == 0);
    CHECK(bridge.num_outputs() == 0);
    CHECK(bridge.num_params() == 0);
    CHECK(bridge.dsp_name().empty());
}

TEST_CASE("FaustBridge unload on empty is safe", "[faust]") {
    FaustBridge bridge;
    bridge.unload();
    CHECK_FALSE(bridge.loaded());
}

TEST_CASE("FaustBridge process with no DSP produces silence", "[faust]") {
    FaustBridge bridge;
    float out[256 * 2] = {};
    for (int i = 0; i < 512; ++i) out[i] = 0.5f;

    bridge.process_interleaved(out, 256);

    for (int i = 0; i < 512; ++i)
        CHECK(out[i] == Approx(0.0f));
}

TEST_CASE("FaustBridge get_param out of range returns 0", "[faust]") {
    FaustBridge bridge;
    CHECK(bridge.get_param(-1) == Approx(0.0f));
    CHECK(bridge.get_param(0) == Approx(0.0f));
    CHECK(bridge.get_param(100) == Approx(0.0f));
}

TEST_CASE("FaustBridge set_param out of range is safe", "[faust]") {
    FaustBridge bridge;
    bridge.set_param(-1, 0.5f);
    bridge.set_param(0, 0.5f);
    bridge.set_param(100, 0.5f);
    SUCCEED();
}

TEST_CASE("FaustParamUI path construction", "[faust]") {
    FaustParamUI ui;

    float cutoff_zone = 1000.0f;
    ui.openHorizontalBox("synth");
    ui.openVerticalBox("filter");
    ui.addHorizontalSlider("cutoff", &cutoff_zone, 1000, 1000, 20000, 1);
    ui.closeBox();
    ui.closeBox();

    CHECK(ui.params().size() == 1);
    CHECK(ui.params()[0].label == "cutoff");
    CHECK(ui.params()[0].path == "/synth/filter/cutoff");
    CHECK(ui.params()[0].min == Approx(1000.0f));
    CHECK(ui.params()[0].max == Approx(20000.0f));
    CHECK(ui.params()[0].init == Approx(1000.0f));
    CHECK(ui.params()[0].step == Approx(1.0f));
    CHECK(ui.params()[0].type == ParamDescriptor::Type::SLIDER);
}

TEST_CASE("FaustParamUI multiple params", "[faust]") {
    FaustParamUI ui;
    float gain_z = 1.0f, mix_z = 0.5f, bypass_z = 0.0f;

    ui.openHorizontalBox("fx");
    ui.addHorizontalSlider("gain", &gain_z, 1, 0, 10, 0.1f);
    ui.addHorizontalSlider("mix", &mix_z, 0.5f, 0, 1, 0.01f);
    ui.addButton("bypass", &bypass_z);
    ui.closeBox();

    CHECK(ui.params().size() == 3);
    CHECK(ui.params()[0].label == "gain");
    CHECK(ui.params()[1].label == "mix");
    CHECK(ui.params()[2].label == "bypass");
    CHECK(ui.params()[2].type == ParamDescriptor::Type::BUTTON);
    CHECK(ui.params()[0].index == 0);
    CHECK(ui.params()[1].index == 1);
    CHECK(ui.params()[2].index == 2);
}

TEST_CASE("FaustParamUI nested boxes build correct paths", "[faust]") {
    FaustParamUI ui;
    float z = 0.5f;
    ui.openHorizontalBox("a");
    ui.openVerticalBox("b");
    ui.openHorizontalBox("c");
    ui.addHorizontalSlider("param", &z, 0.5f, 0, 1, 0.01f);
    ui.closeBox();
    ui.closeBox();
    ui.closeBox();

    CHECK(ui.params()[0].path == "/a/b/c/param");
}

TEST_CASE("FaustParamUI zones are writable", "[faust]") {
    FaustParamUI ui;
    float x_z = 0.5f, y_z = 0.3f;
    ui.openHorizontalBox("test");
    ui.addHorizontalSlider("x", &x_z, 0.5f, 0, 1, 0.01f);
    ui.addHorizontalSlider("y", &y_z, 0.3f, 0, 1, 0.01f);
    ui.closeBox();

    auto zones = ui.zones();
    CHECK(zones.size() == 2);

    *zones[0] = 0.75f;
    CHECK(*zones[0] == Approx(0.75f));

    *zones[1] = 0.123f;
    CHECK(*zones[1] == Approx(0.123f));
}

TEST_CASE("FaustParamUI addButton creates button type", "[faust]") {
    FaustParamUI ui;
    float z = 0.0f;
    ui.addButton("toggle", &z);

    CHECK(ui.params().size() == 1);
    CHECK(ui.params()[0].type == ParamDescriptor::Type::BUTTON);
    CHECK(ui.params()[0].path == "/toggle");
}

TEST_CASE("FaustParamUI addCheckButton", "[faust]") {
    FaustParamUI ui;
    float z = 0.0f;
    ui.addCheckButton("check", &z);

    CHECK(ui.params().size() == 1);
    CHECK(ui.params()[0].type == ParamDescriptor::Type::CHECKBOX);
}

TEST_CASE("FaustParamUI addNumEntry", "[faust]") {
    FaustParamUI ui;
    float z = 5.0f;
    ui.addNumEntry("steps", &z, 5, 1, 16, 1);

    CHECK(ui.params().size() == 1);
    CHECK(ui.params()[0].type == ParamDescriptor::Type::NENTRY);
    CHECK(ui.params()[0].init == Approx(5.0f));
    CHECK(ui.params()[0].max == Approx(16.0f));
}
