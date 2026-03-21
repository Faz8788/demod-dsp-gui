// Input manager binding resolution tests
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "input/input_manager.hpp"
#include "mocks/mock_device.hpp"

using namespace demod;
using namespace demod::input;
using Catch::Approx;

TEST_CASE("InputManager starts with no state", "[input]") {
    InputManager mgr;
    CHECK_FALSE(mgr.held(Action::NAV_SELECT));
    CHECK_FALSE(mgr.pressed(Action::NAV_SELECT));
    CHECK(mgr.axis(Action::NAV_SELECT) == Approx(0.0f));
}

TEST_CASE("InputManager button down resolves to held", "[input]") {
    InputManager mgr;
    auto dev = std::make_unique<test::MockDevice>(std::vector<RawEvent>{
        { RawEvent::Type::BUTTON_DOWN, 42, 1.0f, 0 }
    });
    mgr.add_device(std::move(dev));
    mgr.bind("mock", { 42, Action::NAV_SELECT, 0, 1, false });
    mgr.update();

    CHECK(mgr.held(Action::NAV_SELECT));
    CHECK(mgr.pressed(Action::NAV_SELECT));
    CHECK(mgr.axis(Action::NAV_SELECT) == Approx(1.0f));
}

TEST_CASE("InputManager no events = no state change", "[input]") {
    InputManager mgr;
    auto dev = std::make_unique<test::MockDevice>(std::vector<RawEvent>{});
    mgr.add_device(std::move(dev));
    mgr.bind("mock", { 42, Action::NAV_SELECT, 0, 1, false });
    mgr.update();

    CHECK_FALSE(mgr.pressed(Action::NAV_SELECT));
    CHECK_FALSE(mgr.held(Action::NAV_SELECT));
}

TEST_CASE("InputManager axis with deadzone", "[input]") {
    InputManager mgr;
    auto dev = std::make_unique<test::MockDevice>(std::vector<RawEvent>{
        { RawEvent::Type::AXIS_MOVE, 1, 0.1f, 0 }
    });
    mgr.add_device(std::move(dev));
    mgr.bind("mock", { 1, Action::AXIS_X, 0.2f, 1.0f, true });
    mgr.update();

    // 0.1 * 1.0 = 0.1, deadzone 0.2 → clamped to 0
    CHECK(mgr.axis(Action::AXIS_X) == Approx(0.0f));
}

TEST_CASE("InputManager axis above deadzone", "[input]") {
    InputManager mgr;
    auto dev = std::make_unique<test::MockDevice>(std::vector<RawEvent>{
        { RawEvent::Type::AXIS_MOVE, 1, 0.5f, 0 }
    });
    mgr.add_device(std::move(dev));
    mgr.bind("mock", { 1, Action::AXIS_X, 0.2f, 1.0f, true });
    mgr.update();

    CHECK(mgr.axis(Action::AXIS_X) == Approx(0.5f));
}

TEST_CASE("InputManager axis scaling", "[input]") {
    InputManager mgr;
    auto dev = std::make_unique<test::MockDevice>(std::vector<RawEvent>{
        { RawEvent::Type::AXIS_MOVE, 1, 0.5f, 0 }
    });
    mgr.add_device(std::move(dev));
    mgr.bind("mock", { 1, Action::AXIS_X, 0.0f, 2.0f, true });
    mgr.update();

    // 0.5 * 2.0 = 1.0
    CHECK(mgr.axis(Action::AXIS_X) == Approx(1.0f));
}

TEST_CASE("InputManager axis-as-button threshold", "[input]") {
    InputManager mgr;
    auto dev = std::make_unique<test::MockDevice>(std::vector<RawEvent>{
        { RawEvent::Type::AXIS_MOVE, 1, 0.8f, 0 }
    });
    mgr.add_device(std::move(dev));
    mgr.bind("mock", { 1, Action::AXIS_X, 0.0f, 1.0f, true });
    mgr.update();

    // |0.8| > 0.5 → held = true
    CHECK(mgr.held(Action::AXIS_X));
}

TEST_CASE("InputManager axis below held threshold", "[input]") {
    InputManager mgr;
    auto dev = std::make_unique<test::MockDevice>(std::vector<RawEvent>{
        { RawEvent::Type::AXIS_MOVE, 1, 0.3f, 0 }
    });
    mgr.add_device(std::move(dev));
    mgr.bind("mock", { 1, Action::AXIS_X, 0.0f, 1.0f, true });
    mgr.update();

    // |0.3| < 0.5 → held = false, but analog != 0
    CHECK_FALSE(mgr.held(Action::AXIS_X));
    CHECK(mgr.axis(Action::AXIS_X) == Approx(0.3f));
}

TEST_CASE("InputManager multiple bindings to same action", "[input]") {
    InputManager mgr;
    auto dev = std::make_unique<test::MockDevice>(std::vector<RawEvent>{
        { RawEvent::Type::BUTTON_DOWN, 10, 1.0f, 0 },
        { RawEvent::Type::BUTTON_DOWN, 20, 1.0f, 0 }
    });
    mgr.add_device(std::move(dev));
    mgr.bind("mock", { 10, Action::NAV_UP, 0, 1, false });
    mgr.bind("mock", { 20, Action::NAV_UP, 0, 1, false });
    mgr.update();

    CHECK(mgr.held(Action::NAV_UP));
}

TEST_CASE("InputManager device count and introspection", "[input]") {
    InputManager mgr;
    auto dev = std::make_unique<test::MockDevice>(std::vector<RawEvent>{});
    mgr.add_device(std::move(dev));

    CHECK(mgr.device_count() == 1);
    CHECK(mgr.device_name(0) == "Mock");
    CHECK(mgr.device_connected(0) == true);
}

TEST_CASE("InputManager clear_bindings removes all", "[input]") {
    InputManager mgr;
    auto dev = std::make_unique<test::MockDevice>(std::vector<RawEvent>{
        { RawEvent::Type::BUTTON_DOWN, 42, 1.0f, 0 }
    });
    mgr.add_device(std::move(dev));
    mgr.bind("mock", { 42, Action::NAV_SELECT, 0, 1, false });

    mgr.clear_bindings("mock");
    mgr.update();

    CHECK_FALSE(mgr.held(Action::NAV_SELECT));
}

TEST_CASE("InputManager debug_string is non-empty", "[input]") {
    InputManager mgr;
    auto dev = std::make_unique<test::MockDevice>(std::vector<RawEvent>{});
    mgr.add_device(std::move(dev));

    std::string dbg = mgr.debug_string();
    CHECK_FALSE(dbg.empty());
}

TEST_CASE("InputManager MIDI learn captures CC", "[input]") {
    InputManager mgr;
    // Device that returns a MIDI CC event (source_id >= 1000)
    auto dev = std::make_unique<test::MockDevice>(std::vector<RawEvent>{
        { RawEvent::Type::AXIS_MOVE, 1071, 0.5f, 0 }  // CC71 at 0.5
    });
    mgr.add_device(std::move(dev));

    // Start learning for AXIS_X on the mock device
    mgr.start_learn(Action::AXIS_X, "mock");
    CHECK(mgr.learn_active());

    mgr.update();

    // Learn should have completed
    CHECK_FALSE(mgr.learn_active());
    CHECK(mgr.learn_result() == 1071);

    // Subsequent updates should resolve the learned binding
    // (the binding was created for source_id 1071 → AXIS_X)
    // Need to poll again with the same device that now has the binding
    // Note: The binding was auto-created in the first update()
    // But the event was consumed by learn, not by resolve_event
    // So we need a second update to see the binding in action

    // Device returns the same CC again
    mgr.update();

    // The binding should be active now
    CHECK(mgr.axis(Action::AXIS_X) == Approx(0.5f));
}

TEST_CASE("InputManager MIDI learn cancel", "[input]") {
    InputManager mgr;
    mgr.start_learn(Action::AXIS_X, "mock");
    CHECK(mgr.learn_active());

    mgr.cancel_learn();
    CHECK_FALSE(mgr.learn_active());
}
