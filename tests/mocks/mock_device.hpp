#pragma once
#include "input/device.hpp"
#include <vector>

namespace demod::test {

class MockDevice : public input::InputDevice {
public:
    explicit MockDevice(std::vector<input::RawEvent> events)
        : events_(std::move(events)) {}

    std::string name()      const override { return "Mock"; }
    std::string type_tag()  const override { return "mock"; }
    bool        connected() const override { return true; }

    void poll(std::vector<input::RawEvent>& out) override {
        out = events_;
    }

    std::vector<input::Binding> default_bindings() const override { return {}; }

private:
    std::vector<input::RawEvent> events_;
};

} // namespace demod::test
