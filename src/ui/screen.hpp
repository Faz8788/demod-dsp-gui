#pragma once
// ╔══════════════════════════════════════════════════════════════════════╗
// ║  DeMoDOOM — Screen Base Class                                      ║
// ╚══════════════════════════════════════════════════════════════════════╝

#include "renderer/renderer.hpp"
#include "renderer/font.hpp"
#include "renderer/widgets.hpp"
#include "input/input_manager.hpp"
#include <string>

namespace demod::ui {

class Screen {
public:
    virtual ~Screen() = default;
    virtual std::string name() const = 0;

    // Called every frame with delta time
    virtual void update(const input::InputManager& input, float dt) = 0;

    // Draw screen contents
    virtual void draw(renderer::Renderer& r) = 0;

    // Called when this screen becomes active / inactive
    virtual void on_enter() {}
    virtual void on_exit()  {}

    // Does this screen want to draw over the previous screen?
    // (used for menus / overlays that dim the background)
    virtual bool is_overlay() const { return false; }

    // Screen-level help text for the bottom bar
    virtual std::string help_text() const { return ""; }
};

} // namespace demod::ui
