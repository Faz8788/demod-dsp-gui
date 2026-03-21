// ╔══════════════════════════════════════════════════════════════════════╗
// ║  DeMoDOOM — Widget Implementations                                 ║
// ╚══════════════════════════════════════════════════════════════════════╝

#include "renderer/widgets.hpp"
#include <cmath>
#include <algorithm>
#include <cstdio>

namespace demod::renderer {

// ── HSlider ──────────────────────────────────────────────────────────
void HSlider::draw(Renderer& r) const {
    Color bg     = palette::DARK_GRAY;
    Color border = focused ? accent : palette::MID_GRAY;
    Color fill_c = bypassed ? palette::MID_GRAY :
                   focused  ? accent : accent.lerp(palette::DARK_GRAY, 0.4f);
    Color text_c = focused ? accent.lerp(palette::WHITE, 0.4f) : palette::LIGHT_GRAY;

    r.rect_chamfer(x, y, w, h, border, bg);
    int fill_w = int((w - 4) * std::clamp(value, 0.0f, 1.0f));
    if (fill_w > 0) {
        Color dark_fill = fill_c.lerp(palette::BLACK, 0.6f);
        r.gradient_h(x+2, y+2, fill_w, h-4, dark_fill, fill_c);
    }
    // Default tick
    int def_x = x + 2 + int((w - 4) * default_value);
    r.vline(def_x, y+1, y+h-2, palette::MID_GRAY);

    Font::draw_string(r, x+3, y+(h-7)/2, label, text_c);
    char buf[16]; snprintf(buf, sizeof(buf), "%.2f", value);
    Font::draw_right(r, x+w-3, y+(h-7)/2, buf, text_c);

    if (focused) {
        r.blend_pixel(x-1, y-1, accent.with_alpha(50));
        r.blend_pixel(x+w, y-1, accent.with_alpha(50));
        r.blend_pixel(x-1, y+h, accent.with_alpha(50));
        r.blend_pixel(x+w, y+h, accent.with_alpha(50));
    }
}

// ── Knob ─────────────────────────────────────────────────────────────
void Knob::draw(Renderer& r) const {
    int cx = x + radius, cy = y + radius;
    Color ring = focused ? accent : palette::MID_GRAY;
    for (int dy = -radius; dy <= radius; ++dy)
        for (int dx = -radius; dx <= radius; ++dx) {
            float dist = std::sqrt(float(dx*dx + dy*dy));
            if (dist <= radius && dist >= radius - 2)
                r.pixel(cx+dx, cy+dy, ring);
            else if (dist < radius - 2)
                r.pixel(cx+dx, cy+dy, palette::BLACK.lerp(palette::DARK_GRAY, dist/(radius-2)*0.5f));
        }
    float angle = 2.356f + 4.712f * value;
    int ix = int(std::cos(angle - 3.14159f) * (radius - 4));
    int iy = int(std::sin(angle - 3.14159f) * (radius - 4));
    r.rect_fill(cx+ix-1, cy+iy-1, 3, 3, focused ? palette::WHITE : accent);
    Font::draw_centered(r, x, y+radius*2+3, radius*2, label,
                        focused ? accent : palette::LIGHT_GRAY);
}

// ── VUMeter ──────────────────────────────────────────────────────────
void VUMeter::draw(Renderer& r) const {
    r.rect(x, y, w, h, palette::MID_GRAY);
    if (vertical) {
        int segs = h-2;
        int fill = int(segs * std::clamp(level, 0.f, 1.f));
        int pk   = int(segs * std::clamp(peak,  0.f, 1.f));
        for (int i = 0; i < segs; ++i) {
            float t = float(i)/segs;
            Color c = t>.85f ? palette::RED : t>.7f ? palette::ORANGE
                    : t>.55f ? palette::YELLOW : palette::GREEN;
            if (i < fill) r.hline(x+1, x+w-2, y+h-2-i, c);
            if (i == pk && pk > 0) r.hline(x+1, x+w-2, y+h-2-i, palette::WHITE);
        }
    } else {
        int segs = w-2;
        int fill = int(segs * std::clamp(level, 0.f, 1.f));
        for (int i = 0; i < fill; ++i) {
            float t = float(i)/segs;
            Color c = t>.85f ? palette::RED : t>.7f ? palette::ORANGE
                    : t>.55f ? palette::YELLOW : palette::GREEN;
            r.vline(x+1+i, y+1, y+h-2, c);
        }
        int pk = x+1+int(segs*std::clamp(peak,0.f,1.f));
        if (peak > 0) r.vline(pk, y+1, y+h-2, palette::WHITE);
    }
}

// ── Scope ────────────────────────────────────────────────────────────
void Scope::draw(Renderer& r) const {
    r.rect_fill(x, y, w, h, {5,5,8});
    r.rect(x, y, w, h, palette::MID_GRAY);
    int cx = x+w/2, cy = y+h/2;
    Color grid = {20,20,25};
    r.hline(x+1, x+w-2, cy, grid);
    r.vline(cx, y+1, y+h-2, grid);
    for (int i=1;i<4;++i) {
        for (int px=x+1;px<x+w-1;px+=4) r.pixel(px, y+(h*i)/4, grid);
        for (int py=y+1;py<y+h-1;py+=4) r.pixel(x+(w*i)/4, py, grid);
    }
    if (!buffer || buffer_len <= 0) {
        Font::draw_centered(r, x, cy-3, w, "NO SIGNAL", palette::MID_GRAY);
        return;
    }
    Color tc = trace_color.a > 0 ? trace_color : palette::CYAN;
    int prev_py = cy;
    for (int px=0; px<w-2; ++px) {
        int idx = (px*buffer_len)/(w-2);
        if (idx >= buffer_len) idx = buffer_len-1;
        int py = cy - int(buffer[idx] * y_scale * (h/2-2));
        py = std::clamp(py, y+1, y+h-2);
        int y0 = std::min(prev_py,py), y1 = std::max(prev_py,py);
        for (int yy=y0;yy<=y1;++yy) r.pixel(x+1+px, yy, tc);
        r.blend_pixel(x+1+px, py-1, tc.with_alpha(40));
        r.blend_pixel(x+1+px, py+1, tc.with_alpha(40));
        prev_py = py;
    }
    if (!label.empty()) Font::draw_string(r, x+3, y+2, label, palette::CYAN_DARK);
}

// ── Toggle ───────────────────────────────────────────────────────────
void Toggle::draw(Renderer& r) const {
    Color border = focused ? palette::CYAN : palette::MID_GRAY;
    r.rect(x, y, 7, 7, border);
    if (value) r.rect_fill(x+2, y+2, 3, 3, focused ? palette::CYAN : palette::CYAN_MID);
    Font::draw_string(r, x+10, y, label, focused ? palette::CYAN_LIGHT : palette::LIGHT_GRAY);
}

// ── StatusBar ────────────────────────────────────────────────────────
void StatusBar::draw(Renderer& r) const {
    int W = r.fb_w(), H = r.fb_h();
    int bar_h = 14;
    int by = H - bar_h;
    r.gradient_v(0, by, W, bar_h, palette::DARK_GRAY, palette::BLACK);
    r.hline(0, W-1, by, palette::CYAN_DARK);
    int ty = by + (bar_h-7)/2;
    Font::draw_string(r, 4, ty, left_text, palette::CYAN);
    Font::draw_centered(r, 0, ty, W, center_text, palette::WHITE);
    char buf[32]; snprintf(buf, sizeof(buf), "CPU:%.0f%%", cpu_load*100);
    Color cc = cpu_load>.8f ? palette::RED : cpu_load>.5f ? palette::YELLOW : palette::GREEN;
    Font::draw_right(r, W-4, ty, buf, cc);
}

// ═══════════════════════════════════════════════════════════════════════
//  GAME MENU PRIMITIVES
// ═══════════════════════════════════════════════════════════════════════

void MenuList::clamp() {
    if (items.empty()) { focused_index = 0; return; }
    focused_index = std::clamp(focused_index, 0, (int)items.size()-1);
}

void MenuList::ensure_visible() {
    if (focused_index < scroll_offset) scroll_offset = focused_index;
    if (focused_index >= scroll_offset + visible_count)
        scroll_offset = focused_index - visible_count + 1;
    scroll_offset = std::max(0, scroll_offset);
}

void MenuList::draw(Renderer& r) const {
    // Title
    if (!title.empty()) {
        Font::draw_glow(r, x+2, y, title, palette::CYAN, palette::GLOW_CYAN);
        r.hline(x, x+w-1, y+9, palette::CYAN_DARK);
    }
    int content_y = title.empty() ? y : y + 12;

    for (int vi = 0; vi < visible_count; ++vi) {
        int idx = scroll_offset + vi;
        if (idx >= (int)items.size()) break;
        const auto& item = items[idx];

        int iy = content_y + vi * row_h;
        bool is_focused = (idx == focused_index);

        if (item.separator) {
            r.hline(x+4, x+w-5, iy + row_h/2, palette::MENU_BORDER);
            continue;
        }

        // Selection highlight
        if (is_focused) {
            r.rect_fill(x, iy, w, row_h-1, palette::MENU_HL);
            r.vline(x, iy, iy+row_h-2, item.accent);
            r.vline(x+1, iy, iy+row_h-2, item.accent.with_alpha(60));
        }

        Color lbl_c = !item.enabled ? palette::MID_GRAY
                     : is_focused   ? palette::WHITE
                     :                palette::LIGHT_GRAY;

        Font::draw_string(r, x + 6, iy + (row_h-7)/2, item.label, lbl_c);

        if (!item.value_text.empty()) {
            Color val_c = is_focused ? item.accent : palette::MID_GRAY;
            Font::draw_right(r, x+w-4, iy + (row_h-7)/2, item.value_text, val_c);
        }

        // Focus arrow
        if (is_focused) {
            Font::draw_string(r, x+1, iy + (row_h-7)/2, ">", item.accent);
        }
    }

    // Scroll indicators
    if (scroll_offset > 0) {
        Font::draw_centered(r, x, content_y - 8, w, "^^^", palette::MID_GRAY);
    }
    if (scroll_offset + visible_count < (int)items.size()) {
        int bot = content_y + visible_count * row_h;
        Font::draw_centered(r, x, bot, w, "vvv", palette::MID_GRAY);
    }
}

// ── FX Chain Slot ────────────────────────────────────────────────────
void FXSlotWidget::draw(Renderer& r) const {
    if (!slot) return;
    Color col = slot->color;
    Color bg  = focused ? palette::MENU_HL : palette::DARK_GRAY;
    Color bdr = focused ? col : palette::MENU_BORDER;

    r.rect_chamfer(x, y, w, h, bdr, bg);

    // Slot number
    char num[4]; snprintf(num, sizeof(num), "%d", chain_index + 1);
    Font::draw_string(r, x+3, y+2, num, col.with_alpha(120));

    if (slot->loaded) {
        // Name
        Color name_c = slot->bypassed ? palette::MID_GRAY : palette::WHITE;
        Font::draw_string(r, x+14, y+2, slot->name, name_c);

        // Bypass indicator
        if (slot->bypassed) {
            Font::draw_right(r, x+w-3, y+2, "BYP", palette::RED);
        }

        // Wet/dry bar
        int bar_y = y + h - 5;
        int bar_w = w - 6;
        r.rect(x+3, bar_y, bar_w, 3, palette::MID_GRAY);
        int fill = int(bar_w * slot->wet_mix);
        r.rect_fill(x+3, bar_y, fill, 3, col.with_alpha(180));
    } else {
        Font::draw_centered(r, x, y+(h-7)/2, w, "[ EMPTY ]", palette::MID_GRAY);
    }

    // Chain flow line
    if (chain_index < total - 1) {
        int arrow_x = x + w + 2;
        int arrow_y = y + h/2;
        r.hline(arrow_x, arrow_x + 4, arrow_y, palette::MID_GRAY);
        r.pixel(arrow_x+3, arrow_y-1, palette::MID_GRAY);
        r.pixel(arrow_x+3, arrow_y+1, palette::MID_GRAY);
    }

    // Focused glow
    if (focused) {
        for (int i = 0; i < w; ++i) {
            r.blend_pixel(x+i, y-1, col.with_alpha(20));
            r.blend_pixel(x+i, y+h, col.with_alpha(20));
        }
    }
}

// ── TabBar ───────────────────────────────────────────────────────────
void TabBar::draw(Renderer& r) const {
    int tx = x;
    for (int i = 0; i < (int)tabs.size(); ++i) {
        int tw = Font::measure(tabs[i]) + 10;
        bool active = (i == active_tab);

        Color bg  = active ? palette::DARK_GRAY  : palette::MENU_BG;
        Color txt = active ? palette::CYAN_LIGHT  : palette::MID_GRAY;
        Color bdr = active ? palette::CYAN        : palette::MENU_BORDER;

        r.rect_fill(tx, y, tw, tab_height(), bg);
        if (active) {
            r.hline(tx, tx+tw-1, y, bdr);
            r.vline(tx, y, y+tab_height()-1, bdr);
            r.vline(tx+tw-1, y, y+tab_height()-1, bdr);
        } else {
            r.hline(tx, tx+tw-1, y+tab_height()-1, palette::MENU_BORDER);
        }
        Font::draw_centered(r, tx, y+2, tw, tabs[i], txt);
        tx += tw + 1;
    }
    // Fill remainder with bottom border
    r.hline(tx, x+w-1, y+tab_height()-1, palette::MENU_BORDER);
}

// ── Breadcrumb ───────────────────────────────────────────────────────
void Breadcrumb::draw(Renderer& r) const {
    int cx = x;
    for (int i = 0; i < (int)path.size(); ++i) {
        bool last = (i == (int)path.size() - 1);
        Color c = last ? palette::CYAN : palette::MID_GRAY;
        cx += Font::draw_string(r, cx, y, path[i], c);
        if (!last) {
            cx += Font::draw_string(r, cx, y, " > ", palette::DARK_GRAY.lerp(palette::MID_GRAY, 0.5f));
        }
    }
}

} // namespace demod::renderer
