#pragma once
// ╔══════════════════════════════════════════════════════════════════════╗
// ║  DeMoDOOM — UI Widgets (resolution-independent)                    ║
// ╚══════════════════════════════════════════════════════════════════════╝

#include "renderer/renderer.hpp"
#include "renderer/font.hpp"
#include "core/config.hpp"
#include <string>
#include <vector>
#include <functional>

namespace demod::renderer {

// ── Horizontal Slider ────────────────────────────────────────────────
struct HSlider {
    int x, y, w, h;
    std::string label;
    float value, default_value;
    bool  focused, bypassed;
    Color accent = palette::CYAN;
    void draw(Renderer& r) const;
};

// ── Rotary Knob ──────────────────────────────────────────────────────
struct Knob {
    int x, y, radius;
    std::string label;
    float value;
    bool  focused;
    Color accent = palette::CYAN;
    void draw(Renderer& r) const;
};

// ── VU Meter ─────────────────────────────────────────────────────────
struct VUMeter {
    int x, y, w, h;
    float level, peak;
    bool vertical;
    void draw(Renderer& r) const;
};

// ── Oscilloscope ─────────────────────────────────────────────────────
struct Scope {
    int x, y, w, h;
    std::string label;
    const float* buffer; int buffer_len;
    float y_scale;
    Color trace_color;
    void draw(Renderer& r) const;
};

// ── Toggle ───────────────────────────────────────────────────────────
struct Toggle {
    int x, y;
    std::string label;
    bool value, focused;
    void draw(Renderer& r) const;
};

// ── Status Bar (DOOM HUD) ────────────────────────────────────────────
struct StatusBar {
    std::string left_text, center_text, right_text;
    float cpu_load;
    void draw(Renderer& r) const;   // Always at bottom
};

// ═══════════════════════════════════════════════════════════════════════
//  GAME MENU PRIMITIVES
// ═══════════════════════════════════════════════════════════════════════

// ── MenuItem: one row in a vertical menu ─────────────────────────────
struct MenuItem {
    std::string label;
    std::string value_text;          // Right-aligned value (or "")
    bool        enabled   = true;
    bool        separator = false;   // Draw as a divider line
    Color       accent    = palette::CYAN;
};

// ── MenuList: vertical navigable list ────────────────────────────────
struct MenuList {
    int x, y, w;
    int row_h         = 14;
    int focused_index = 0;
    int scroll_offset = 0;
    int visible_count = 10;
    std::vector<MenuItem> items;

    std::string title;

    void draw(Renderer& r) const;
    void clamp();
    void ensure_visible();
};

// ── FX Chain Slot: visual block in the signal flow ───────────────────
struct FXSlotWidget {
    int x, y, w, h;
    const FXSlot* slot;
    bool focused;
    bool dragging;
    int  chain_index;   // Position in chain
    int  total;         // Total slots for drawing flow lines

    void draw(Renderer& r) const;
};

// ── Tab bar (horizontal) ─────────────────────────────────────────────
struct TabBar {
    int x, y, w;
    int active_tab = 0;
    std::vector<std::string> tabs;

    void draw(Renderer& r) const;
    int  tab_height() const { return 12; }
};

// ── Breadcrumb path ──────────────────────────────────────────────────
struct Breadcrumb {
    int x, y;
    std::vector<std::string> path;

    void draw(Renderer& r) const;
};

} // namespace demod::renderer
