#pragma once
// ╔══════════════════════════════════════════════════════════════════════╗
// ║  DeMoDOOM — Main Menu Overlay                                      ║
// ║  DOOM-style title menu with descriptions + help access             ║
// ╚══════════════════════════════════════════════════════════════════════╝

#include "ui/screen.hpp"
#include <vector>
#include <functional>

namespace demod::ui {

class MainMenu : public Screen {
public:
    struct Entry {
        std::string label;
        std::string desc;           // One-line description shown when focused
        std::function<void()> action;
        bool enabled   = true;
        bool separator = false;     // Draw as a divider, not a selectable item
    };

    MainMenu();

    std::string name() const override { return "MENU"; }
    bool is_overlay() const override { return true; }
    std::string help_text() const override { return "W/S:Navigate  Enter:Select  Esc:Close"; }

    void set_entries(std::vector<Entry> entries) { entries_ = std::move(entries); }

    void update(const input::InputManager& input, float dt) override;
    void draw(renderer::Renderer& r) override;

    void on_enter() override { anim_t_ = 0; focused_ = 0; skip_first_frame_ = true; }

    bool wants_close() const { return wants_close_; }
    void reset_close() { wants_close_ = false; }

    // Track if the user has ever left the menu (for first-launch UX)
    void set_has_session(bool v) { has_session_ = v; }

private:
    std::vector<Entry> entries_;
    int   focused_    = 0;
    float anim_t_     = 0;
    float logo_bob_   = 0;
    bool  wants_close_ = false;
    bool  skip_first_frame_ = false;
    bool  has_session_ = false;    // False until user first leaves menu

    void draw_logo(renderer::Renderer& r, int cx, int y);
    void skip_to_next_enabled(int dir);
};

} // namespace demod::ui
