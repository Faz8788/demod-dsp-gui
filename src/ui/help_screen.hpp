#pragma once
// ╔══════════════════════════════════════════════════════════════════════╗
// ║  DeMoDOOM — Help Screen Overlay                                    ║
// ║  Multi-page scrollable reference for all controls and features.    ║
// ╚══════════════════════════════════════════════════════════════════════╝

#include "ui/screen.hpp"
#include <vector>
#include <string>

namespace demod::ui {

class HelpScreen : public Screen {
public:
    HelpScreen();

    std::string name() const override { return "HELP"; }
    bool is_overlay() const override { return true; }
    std::string help_text() const override { return "W/S:Scroll  [/]:Page  Esc:Close"; }

    void update(const input::InputManager& input, float dt) override;
    void draw(renderer::Renderer& r) override;

    void on_enter() override { scroll_ = 0; anim_t_ = 0; }

    bool wants_close() const { return wants_close_; }
    void reset_close() { wants_close_ = false; }

    // Public so implementation can use the type alias
    struct HelpLine {
        enum class Type { HEADING, TEXT, KEYBIND, BLANK, DIVIDER } type;
        std::string left;    // Label or heading text
        std::string right;   // Value (for keybinds)
    };

private:

    std::vector<HelpLine> lines_;
    int   scroll_      = 0;
    float anim_t_      = 0;
    bool  wants_close_ = false;

    void build_content();
};

} // namespace demod::ui
