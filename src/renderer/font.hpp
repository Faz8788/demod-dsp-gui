#pragma once
// ╔══════════════════════════════════════════════════════════════════════╗
// ║  DeMoD DSP GUI — Bitmap Font Renderer                              ║
// ║  Embedded 5×7 pixel font, oscilloscope-typography aesthetic.       ║
// ╚══════════════════════════════════════════════════════════════════════╝

#include "renderer/renderer.hpp"
#include <string>

namespace demod::renderer {

class Font {
public:
    static constexpr int GLYPH_W = 5;
    static constexpr int GLYPH_H = 7;
    static constexpr int SPACING = 1;

    // Draw a single character. Returns x advance.
    static int draw_char(Renderer& r, int x, int y, char ch, Color c,
                          int scale = 1);

    // Draw a string. Returns total width in pixels.
    static int draw_string(Renderer& r, int x, int y, const std::string& text,
                            Color c, int scale = 1);

    // Draw centered string within a given width
    static void draw_centered(Renderer& r, int x, int y, int width,
                               const std::string& text, Color c, int scale = 1);

    // Draw right-aligned string
    static void draw_right(Renderer& r, int right_x, int y,
                            const std::string& text, Color c, int scale = 1);

    // Measure string width in pixels
    static int measure(const std::string& text, int scale = 1);

    // Draw with phosphor glow effect (draws twice: base + offset glow)
    static void draw_glow(Renderer& r, int x, int y, const std::string& text,
                           Color c, Color glow, int scale = 1);

private:
    // 5×7 bitmap font data. Each glyph is 7 bytes (rows), 5 bits wide.
    static const uint8_t FONT_DATA[128][GLYPH_H];
};

} // namespace demod::renderer
