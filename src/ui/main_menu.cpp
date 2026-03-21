// ╔══════════════════════════════════════════════════════════════════════╗
// ║  DeMoDOOM — Main Menu Implementation                               ║
// ╚══════════════════════════════════════════════════════════════════════╝

#include "ui/main_menu.hpp"
#include <cmath>
#include <algorithm>

namespace demod::ui {

MainMenu::MainMenu() = default;

void MainMenu::skip_to_next_enabled(int dir) {
    int n = (int)entries_.size();
    if (n == 0) return;
    for (int attempts = 0; attempts < n; ++attempts) {
        focused_ = (focused_ + dir + n) % n;
        if (entries_[focused_].enabled && !entries_[focused_].separator)
            return;
    }
}

void MainMenu::update(const input::InputManager& input, float dt) {
    anim_t_   += dt;
    logo_bob_ += dt * 2.5f;
    wants_close_ = false;

    if (skip_first_frame_) {
        skip_first_frame_ = false;
        return;
    }

    if (input.pressed(Action::NAV_DOWN))  skip_to_next_enabled(+1);
    if (input.pressed(Action::NAV_UP))    skip_to_next_enabled(-1);

    if (input.pressed(Action::NAV_SELECT)) {
        if (focused_ < (int)entries_.size() &&
            entries_[focused_].enabled && !entries_[focused_].separator) {
            entries_[focused_].action();
        }
    }
    if (input.pressed(Action::NAV_BACK) || input.pressed(Action::MENU_OPEN)) {
        wants_close_ = true;
    }
}

void MainMenu::draw(renderer::Renderer& r) {
    using namespace demod::palette;
    using namespace demod::renderer;

    int W = r.fb_w(), H = r.fb_h();

    r.dim_screen(180);

    // ── Panel sizing ─────────────────────────────────────────────────
    // Count visible rows (entries + separators are shorter)
    int row_count = 0;
    for (const auto& e : entries_) row_count += e.separator ? 0 : 1;

    int panel_w = std::min(260, W - 16);
    int panel_h = 42 + row_count * 14 + 26; // logo + entries + desc + footer
    int px = (W - panel_w) / 2;
    int py = (H - panel_h) / 2;

    // Slide-in animation
    float fade = std::min(1.0f, anim_t_ * 4.0f);
    py += int((1.0f - fade) * 10);

    // ── Panel chrome ─────────────────────────────────────────────────
    r.rect_fill(px-1, py-1, panel_w+2, panel_h+2, MENU_BORDER);
    r.rect_fill(px, py, panel_w, panel_h, MENU_BG);
    r.rect(px, py, panel_w, panel_h, CYAN_DARK);

    // Corner accents
    for (int d = 0; d < 3; ++d) {
        r.pixel(px+1+d, py+1, CYAN_DARK);
        r.pixel(px+1, py+1+d, CYAN_DARK);
        r.pixel(px+panel_w-2-d, py+1, VIOLET_DARK);
        r.pixel(px+panel_w-2, py+1+d, VIOLET_DARK);
    }

    // ── Logo ─────────────────────────────────────────────────────────
    draw_logo(r, W/2, py + 8 + int(std::sin(logo_bob_) * 1.5f));

    // Dotted separator
    int sep_y = py + 32;
    for (int i = px+8; i < px+panel_w-8; i += 3)
        r.pixel(i, sep_y, CYAN_DARK);

    // ── Menu entries ─────────────────────────────────────────────────
    int ey = sep_y + 6;
    for (int i = 0; i < (int)entries_.size(); ++i) {
        const auto& e = entries_[i];

        if (e.separator) {
            // Thin divider line
            ey += 3;
            r.hline(px+16, px+panel_w-16, ey, MENU_BORDER);
            ey += 5;
            continue;
        }

        bool sel = (i == focused_);

        if (sel) {
            // Highlight bar with pulsing border
            float pulse = 0.7f + 0.3f * std::sin(anim_t_ * 5.0f);
            Color hl = MENU_HL.lerp(CYAN_DARK, pulse * 0.3f);
            r.rect_fill(px+4, ey-1, panel_w-8, 13, hl);
            r.vline(px+4, ey-1, ey+11, CYAN);

            // Animated arrow
            int phase = int(anim_t_ * 3) % 2;
            Font::draw_string(r, px+7+phase, ey+2, ">", CYAN);
        }

        // Label
        Color tc = !e.enabled ? MID_GRAY : sel ? WHITE : LIGHT_GRAY;
        Font::draw_centered(r, px, ey+2, panel_w, e.label, tc);

        ey += 14;
    }

    // ── Description of focused entry ─────────────────────────────────
    ey += 4;
    r.hline(px+16, px+panel_w-16, ey, MENU_BORDER);
    ey += 4;

    if (focused_ >= 0 && focused_ < (int)entries_.size()) {
        const auto& desc = entries_[focused_].desc;
        if (!desc.empty()) {
            // Word-wrap the description into the panel width
            int max_chars = (panel_w - 16) / 6; // ~6px per char at scale 1
            if ((int)desc.size() <= max_chars) {
                Font::draw_centered(r, px, ey, panel_w, desc, MID_GRAY);
            } else {
                // Simple two-line split at nearest space
                int split = max_chars;
                while (split > 0 && desc[split] != ' ') --split;
                if (split == 0) split = max_chars;
                Font::draw_centered(r, px, ey, panel_w,
                    desc.substr(0, split), MID_GRAY);
                if (split < (int)desc.size()) {
                    Font::draw_centered(r, px, ey+9, panel_w,
                        desc.substr(split+1), MID_GRAY);
                }
            }
        }
    }

    // ── Footer ───────────────────────────────────────────────────────
    Font::draw_centered(r, px, py+panel_h-10, panel_w, "v0.2.0  |  DeMoD LLC", MID_GRAY);

    // ── Bottom hint bar ──────────────────────────────────────────────
    int hint_y = py + panel_h + 4;
    if (hint_y < H - 2) {
        std::string hint = has_session_
            ? "Esc:Close  W/S:Navigate  Enter:Select"
            : "Welcome!  Select a screen or press HELP to get started.";
        int hw = Font::measure(hint);
        Font::draw_string(r, (W-hw)/2, hint_y, hint, CYAN_DARK);
    }
}

void MainMenu::draw_logo(renderer::Renderer& r, int cx, int y) {
    using namespace demod::palette;
    using namespace demod::renderer;

    std::string logo = "DeMoDOOM";
    int tw = Font::measure(logo, 2);
    int lx = cx - tw/2;

    // Glow layers
    Font::draw_string(r, lx-1, y-1, logo, CYAN.with_alpha(40), 2);
    Font::draw_string(r, lx+1, y+1, logo, VIOLET.with_alpha(30), 2);

    // Two-tone text
    int half = (int)logo.size() / 2;
    std::string left_half  = logo.substr(0, half);
    std::string right_half = logo.substr(half);
    int lw = Font::measure(left_half, 2);
    Font::draw_string(r, lx, y, left_half, CYAN, 2);
    Font::draw_string(r, lx + lw, y, right_half, VIOLET_LIGHT, 2);

    // Underline accent
    r.gradient_h(lx, y + 16, tw, 1, CYAN, VIOLET);
}

} // namespace demod::ui
