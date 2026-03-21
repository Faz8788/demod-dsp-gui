// ╔══════════════════════════════════════════════════════════════════════╗
// ║  DeMoDOOM — Help Screen Implementation                             ║
// ╚══════════════════════════════════════════════════════════════════════╝

#include "ui/help_screen.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>

namespace demod::ui {

using LT = HelpScreen::HelpLine::Type;

HelpScreen::HelpScreen() { build_content(); }

void HelpScreen::build_content() {
    lines_.clear();

    auto H = [&](const std::string& t) { lines_.push_back({LT::HEADING, t, ""}); };
    auto T = [&](const std::string& t) { lines_.push_back({LT::TEXT, t, ""}); };
    auto K = [&](const std::string& l, const std::string& r) {
        lines_.push_back({LT::KEYBIND, l, r});
    };
    auto B = [&]() { lines_.push_back({LT::BLANK, "", ""}); };
    auto D = [&]() { lines_.push_back({LT::DIVIDER, "", ""}); };

    // ── Welcome ──────────────────────────────────────────────────────
    H("WELCOME TO DeMoDOOM");
    B();
    T("DeMoDOOM is a DOOM-style GUI controller");
    T("for Faust DSP effects. It renders in a");
    T("chunky pixel framebuffer with CRT post-");
    T("processing, and supports keyboard, game-");
    T("pad, and brain-computer interface input.");
    B();
    T("Navigate with W/S or Arrow keys. Press");
    T("Enter to select, Escape to go back.");
    D();

    // ── Screens ──────────────────────────────────────────────────────
    H("SCREENS");
    B();
    T("DeMoDOOM has four main screens you can");
    T("switch between using Tab:");
    B();
    T("FX CHAIN   - Manage up to 12 effects in");
    T("             a signal chain. Bypass, re-");
    T("             order, and set wet/dry mix.");
    B();
    T("PARAMETERS - Edit the knobs and sliders");
    T("             of the currently selected");
    T("             effect. PgUp/PgDn to switch");
    T("             which effect you're editing.");
    B();
    T("VISUALIZER - See your audio in 5 modes:");
    T("             Scope, Spectrum, Waterfall,");
    T("             Lissajous, and Phase Meter.");
    T("             Press [ or ] to switch mode.");
    B();
    T("SETTINGS   - Change resolution, toggle");
    T("             CRT effects, and view audio");
    T("             engine status. Press [ or ]");
    T("             to switch settings sections.");
    D();

    // ── Global keys ──────────────────────────────────────────────────
    H("GLOBAL CONTROLS");
    B();
    K("Open/close menu",     "Escape");
    K("Cycle screens",       "Tab");
    K("Debug overlay",       "F3");
    K("Fullscreen",          "F11");
    K("Quit application",    "Q");
    D();

    // ── Navigation ───────────────────────────────────────────────────
    H("NAVIGATION");
    B();
    K("Move up / down",      "W/S  or  Up/Down");
    K("Adjust / move L/R",   "A/D  or  Left/Right");
    K("Select / confirm",    "Enter");
    K("Back / cancel",       "Escape");
    K("Page up / down",      "PgUp / PgDn");
    K("Sub-tab next/prev",   "[  /  ]");
    D();

    // ── FX Chain controls ────────────────────────────────────────────
    H("FX CHAIN CONTROLS");
    B();
    K("Select slot",         "W / S");
    K("Adjust wet/dry",      "A / D  (hold)");
    K("Toggle bypass",       "B");
    K("Enter reorder mode",  "R  (press again to confirm)");
    D();

    // ── Parameter controls ───────────────────────────────────────────
    H("PARAMETER CONTROLS");
    B();
    K("Navigate parameters", "W / S");
    K("Adjust value",        "A / D  or  +/-");
    K("Fine adjust",         "Gamepad right stick");
    K("Reset to default",    "Backspace");
    K("Randomize all",       "R");
    K("Toggle bypass",       "B");
    K("Switch FX slot",      "PgUp / PgDn");
    D();

    // ── Visualizer controls ──────────────────────────────────────────
    H("VISUALIZER CONTROLS");
    B();
    K("Switch viz mode",     "[  /  ]");
    K("Y-axis scale",        "W / S");
    K("Time scale",          "A / D");
    K("Cycle trigger mode",  "Enter");
    K("Freeze display",      "Space");
    K("Reset all",           "Backspace");
    B();
    T("Viz modes: SCOPE shows the waveform,");
    T("SPECTRUM shows frequency content,");
    T("WATERFALL scrolls a spectrogram,");
    T("LISSAJOUS plots stereo phase, and");
    T("PHASE shows stereo correlation.");
    D();

    // ── Gamepad ──────────────────────────────────────────────────────
    H("GAMEPAD CONTROLS");
    B();
    K("Navigate",            "D-pad");
    K("Select / confirm",    "A button");
    K("Back / cancel",       "B button");
    K("Reset parameter",     "X button");
    K("Toggle bypass",       "Y button");
    K("Screen prev / next",  "L / R shoulder");
    K("Analog param adjust", "Left stick");
    K("Fine adjust",         "Right stick");
    K("Axis Z / W assign",   "L / R trigger");
    D();

    // ── Brain-Computer Interface ─────────────────────────────────────
    H("OPENBCI / BCI INPUT");
    B();
    T("If launched with --bci, DeMoDOOM reads");
    T("EEG data via Lab Streaming Layer and");
    T("maps brain frequency bands to axes:");
    B();
    K("Alpha (8-12 Hz)",     "Axis X  (relaxation)");
    K("Beta (12-30 Hz)",     "Axis Y  (focus)");
    K("Theta (4-8 Hz)",      "Axis Z  (meditation)");
    K("Gamma (30-100 Hz)",   "Axis W  (cognition)");
    D();

    // ── Audio ────────────────────────────────────────────────────────
    H("AUDIO ENGINE");
    B();
    T("DeMoDOOM uses PipeWire for native Linux");
    T("audio. The DSP runs in a real-time thread");
    T("separate from the GUI.");
    B();
    T("Load DSP effects as:");
    T("  .dsp  - Faust source (needs libfaust)");
    T("  .cpp  - Faust C++ (compiled at runtime)");
    T("  .so   - Pre-compiled shared library");
    B();
    T("Pass the file as a command-line argument:");
    T("  demodoom my_effect.so");
    D();

    // ── Tips ─────────────────────────────────────────────────────────
    H("TIPS");
    B();
    T("- The title screen music re-randomizes");
    T("  every time you open the menu.");
    T("- Toggle menu music on/off and adjust");
    T("  its volume in Settings > Audio.");
    T("- Change resolution in Settings > Display");
    T("  without restarting.");
    T("- The debug overlay (F3) shows FPS, CPU");
    T("  load, audio stats, and input state.");
    T("- All CRT effects (scanlines, bloom,");
    T("  vignette) can be toggled in Settings.");
    B();
    T("Source: github.com/ALH477");
    T("License: GPL-3.0  |  DeMoD LLC");
}

void HelpScreen::update(const input::InputManager& input, float dt) {
    anim_t_ += dt;
    wants_close_ = false;

    // Scroll
    if (input.pressed(Action::NAV_DOWN) || input.pressed(Action::NAV_RIGHT))
        scroll_ = std::min(scroll_ + 1, std::max(0, (int)lines_.size() - 10));
    if (input.pressed(Action::NAV_UP) || input.pressed(Action::NAV_LEFT))
        scroll_ = std::max(scroll_ - 1, 0);

    // Page scroll
    if (input.pressed(Action::NAV_PAGE_DOWN) || input.pressed(Action::NAV_TAB_NEXT))
        scroll_ = std::min(scroll_ + 10, std::max(0, (int)lines_.size() - 10));
    if (input.pressed(Action::NAV_PAGE_UP) || input.pressed(Action::NAV_TAB_PREV))
        scroll_ = std::max(scroll_ - 10, 0);

    // Close
    if (input.pressed(Action::NAV_BACK) || input.pressed(Action::NAV_SELECT)
        || input.pressed(Action::MENU_OPEN)) {
        wants_close_ = true;
    }
}

void HelpScreen::draw(renderer::Renderer& r) {
    using namespace demod::palette;
    using namespace demod::renderer;

    int W = r.fb_w(), H = r.fb_h();

    r.dim_screen(200);

    // ── Panel ────────────────────────────────────────────────────────
    int margin = 6;
    int panel_x = margin;
    int panel_y = margin;
    int panel_w = W - margin*2;
    int panel_h = H - margin*2;

    r.rect_fill(panel_x, panel_y, panel_w, panel_h, MENU_BG);
    r.rect(panel_x, panel_y, panel_w, panel_h, CYAN_DARK);

    // ── Header ───────────────────────────────────────────────────────
    int hdr_y = panel_y + 2;
    Font::draw_glow(r, panel_x + 4, hdr_y, "HELP", CYAN, GLOW_CYAN, 1);

    // Scroll indicator
    int total = (int)lines_.size();
    int visible = (panel_h - 20) / 10;
    if (total > visible) {
        char pos[24];
        snprintf(pos, sizeof(pos), "%d/%d", scroll_+1, total);
        Font::draw_right(r, panel_x + panel_w - 4, hdr_y, pos, MID_GRAY);
    }

    Font::draw_right(r, panel_x + panel_w - 50, hdr_y, "Esc:Close", CYAN_DARK);

    r.hline(panel_x+1, panel_x+panel_w-2, hdr_y + 10, MENU_BORDER);

    // ── Content ──────────────────────────────────────────────────────
    int content_y = hdr_y + 13;
    int content_h = panel_h - 20;
    int line_h = 10;
    int max_visible = content_h / line_h;

    for (int vi = 0; vi < max_visible; ++vi) {
        int li = scroll_ + vi;
        if (li >= total) break;

        const auto& line = lines_[li];
        int ly = content_y + vi * line_h;

        switch (line.type) {
        case LT::HEADING: {
            // Bold heading with accent underline
            Font::draw_string(r, panel_x + 6, ly, line.left, CYAN);
            int tw = Font::measure(line.left);
            r.hline(panel_x+6, panel_x+6+tw, ly+8, CYAN_DARK);
        } break;

        case LT::TEXT:
            Font::draw_string(r, panel_x + 8, ly, line.left, LIGHT_GRAY);
            break;

        case LT::KEYBIND: {
            // Left-aligned label, right-aligned key
            Font::draw_string(r, panel_x + 8, ly, line.left, OFF_WHITE);
            Font::draw_right(r, panel_x + panel_w - 8, ly, line.right, CYAN);
        } break;

        case LT::BLANK:
            break;

        case LT::DIVIDER:
            for (int dx = panel_x+12; dx < panel_x+panel_w-12; dx += 4)
                r.pixel(dx, ly + 4, MENU_BORDER);
            break;
        }
    }

    // ── Scrollbar ────────────────────────────────────────────────────
    if (total > max_visible) {
        int sb_x = panel_x + panel_w - 3;
        int sb_y = content_y;
        int sb_h = content_h;
        r.vline(sb_x, sb_y, sb_y + sb_h, DARK_GRAY);

        int thumb_h = std::max(4, sb_h * max_visible / total);
        int thumb_y = sb_y + (sb_h - thumb_h) * scroll_ / std::max(1, total - max_visible);
        r.rect_fill(sb_x-1, thumb_y, 3, thumb_h, CYAN_MID);
    }

    // ── Bottom hint ──────────────────────────────────────────────────
    int hint_y = panel_y + panel_h - 9;
    Font::draw_centered(r, panel_x, hint_y, panel_w,
        "W/S:Scroll  PgUp/PgDn:Page  Esc:Close", MID_GRAY);
}

} // namespace demod::ui
