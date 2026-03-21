// ╔══════════════════════════════════════════════════════════════════════╗
// ║  DeMoDOOM — FX Chain Screen                                        ║
// ╚══════════════════════════════════════════════════════════════════════╝

#include "ui/fx_chain_screen.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>

namespace demod::ui {

FXChainScreen::FXChainScreen() { init_demo_slots(); }

void FXChainScreen::init_demo_slots() {
    const char* names[] = {
        "NOISE GATE",  "COMP/LIMITER", "EQ: PARAMETRIC",
        "OVERDRIVE",   "DISTORTION",   "CHORUS",
        "PHASER",      "DELAY",        "REVERB",
        "CABINET SIM", "STEREO WIDTH", "MASTER LIMITER"
    };
    slots_.resize(MAX_FX_SLOTS);
    for (int i = 0; i < MAX_FX_SLOTS; ++i) {
        slots_[i].id       = i;
        slots_[i].name     = names[i];
        slots_[i].color    = FX_COLORS[i];
        slots_[i].loaded   = (i < 9);  // First 9 "loaded" in demo
        slots_[i].bypassed = (i == 4 || i == 6);
        slots_[i].wet_mix  = (i < 3) ? 1.0f : 0.5f + 0.05f * i;
    }
}

int FXChainScreen::visible_slots(int fb_h) const {
    return std::max(3, (fb_h - 50) / 28);
}

void FXChainScreen::update(const input::InputManager& input, float dt) {
    blink_t_ += dt;
    int count = (int)slots_.size();
    if (count == 0) return;

    // Navigation
    if (input.pressed(Action::NAV_DOWN))
        focused_ = std::min(focused_ + 1, count - 1);
    if (input.pressed(Action::NAV_UP))
        focused_ = std::max(focused_ - 1, 0);

    // Wet/dry with L/R
    if (input.held(Action::NAV_RIGHT) || input.held(Action::PARAM_INC)) {
        slots_[focused_].wet_mix = std::min(1.0f, slots_[focused_].wet_mix + dt);
    }
    if (input.held(Action::NAV_LEFT) || input.held(Action::PARAM_DEC)) {
        slots_[focused_].wet_mix = std::max(0.0f, slots_[focused_].wet_mix - dt);
    }

    // Bypass
    if (input.pressed(Action::BYPASS_TOGGLE)) {
        slots_[focused_].bypassed = !slots_[focused_].bypassed;
    }

    // Reorder mode toggle
    if (input.pressed(Action::PARAM_RANDOMIZE)) { // R key
        reorder_mode_ = !reorder_mode_;
        if (reorder_mode_) {
            drag_from_ = focused_;
        } else if (drag_from_ >= 0 && drag_from_ != focused_) {
            // Swap slots
            std::swap(slots_[drag_from_], slots_[focused_]);
            drag_from_ = -1;
        }
    }

    // Scroll
    int vis = 8; // Will be set properly in draw
    if (focused_ < scroll_) scroll_ = focused_;
    if (focused_ >= scroll_ + vis) scroll_ = focused_ - vis + 1;
}

void FXChainScreen::draw(renderer::Renderer& r) {
    using namespace demod::palette;
    using namespace demod::renderer;

    int W = r.fb_w(), H = r.fb_h();

    // ── Header ───────────────────────────────────────────────────────
    r.rect_fill(0, 0, W, 14, DARK_GRAY);
    r.hline(0, W-1, 14, CYAN_DARK);
    Font::draw_glow(r, 4, 3, "FX CHAIN", CYAN, GLOW_CYAN);

    if (reorder_mode_) {
        float pulse = 0.5f + 0.5f * std::sin(blink_t_ * 6);
        Color rc = ORANGE.lerp(YELLOW, pulse);
        Font::draw_right(r, W-4, 3, "[REORDER]", rc);
    }

    // ── Signal flow: IN → slots → OUT ────────────────────────────────
    int slot_x  = 8;
    int slot_w  = W - 16;
    int slot_h  = 22;
    int gap     = 4;
    int start_y = 20;

    // INPUT label
    Font::draw_centered(r, slot_x, start_y, slot_w, ">> INPUT >>", CYAN_DARK);
    start_y += 10;

    int vis = visible_slots(H);
    int count = (int)slots_.size();

    for (int vi = 0; vi < vis; ++vi) {
        int idx = scroll_ + vi;
        if (idx >= count) break;

        int sy = start_y + vi * (slot_h + gap);
        bool is_focus = (idx == focused_);

        FXSlotWidget widget;
        widget.x = slot_x;
        widget.y = sy;
        widget.w = slot_w;
        widget.h = slot_h;
        widget.slot = &slots_[idx];
        widget.focused = is_focus;
        widget.dragging = reorder_mode_ && idx == drag_from_;
        widget.chain_index = idx;
        widget.total = count;
        widget.draw(r);

        // Reorder: draw drag indicator
        if (reorder_mode_ && is_focus && drag_from_ >= 0) {
            float p = 0.5f + 0.5f * std::sin(blink_t_ * 8);
            Color dc = ORANGE.with_alpha(uint8_t(100 * p));
            r.rect(slot_x-1, sy-1, slot_w+2, slot_h+2, dc);
        }

        // Flow arrow between slots
        if (idx < count - 1) {
            int arrow_y = sy + slot_h + gap/2;
            int ax = W/2;
            r.vline(ax, sy+slot_h, arrow_y+1, MID_GRAY);
            r.pixel(ax-1, arrow_y, MID_GRAY);
            r.pixel(ax+1, arrow_y, MID_GRAY);
        }
    }

    // OUTPUT label
    int out_y = start_y + vis * (slot_h + gap);
    if (out_y < H - 20) {
        Font::draw_centered(r, slot_x, out_y, slot_w, "<< OUTPUT <<", CYAN_DARK);
    }

    // ── Scroll bar ───────────────────────────────────────────────────
    if (count > vis) {
        int sb_x = W - 4;
        int sb_h = H - 40;
        int sb_y = 18;
        r.vline(sb_x, sb_y, sb_y + sb_h, DARK_GRAY);

        int thumb_h = std::max(4, sb_h * vis / count);
        int thumb_y = sb_y + (sb_h - thumb_h) * scroll_ / std::max(1, count - vis);
        r.rect_fill(sb_x-1, thumb_y, 3, thumb_h, CYAN_MID);
    }

    // ── Summary bar ──────────────────────────────────────────────────
    int sum_y = H - 12;
    r.rect_fill(0, sum_y, W, 12, {15,15,18});
    r.hline(0, W-1, sum_y, MENU_BORDER);

    int active = 0, bypassed = 0;
    for (const auto& s : slots_) {
        if (s.loaded && !s.bypassed) ++active;
        if (s.loaded && s.bypassed)  ++bypassed;
    }
    char buf[64];
    snprintf(buf, sizeof(buf), "Active:%d  Bypassed:%d  Total:%d",
             active, bypassed, count);
    Font::draw_string(r, 4, sum_y+2, buf, MID_GRAY);
}

} // namespace demod::ui
