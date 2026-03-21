// Renderer drawing primitives tests (no SDL init needed)
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "renderer/renderer.hpp"
#include <cstring>
#include <algorithm>

using namespace demod;
using namespace demod::renderer;
using Catch::Approx;

// Helper: construct a Renderer without calling init() — framebuffer works
// but SDL window/texture are not created. Drawing primitives operate on fb_.

TEST_CASE("Renderer default resolution", "[renderer]") {
    Renderer r;
    // Constructor initializes framebuffer at default resolution
    CHECK(r.fb_w() == 384);
    CHECK(r.fb_h() == 240);
    CHECK(r.resolution_index() == 1);
}

TEST_CASE("Renderer clear fills with color", "[renderer]") {
    Renderer r;
    r.clear({ 10, 20, 30 });

    const uint32_t* fb = r.framebuffer();
    uint32_t expected = Color(10, 20, 30).to_argb();
    for (int i = 0; i < r.fb_w() * r.fb_h(); ++i)
        CHECK(fb[i] == expected);
}

TEST_CASE("Renderer pixel sets correct location", "[renderer]") {
    Renderer r;
    r.clear(palette::BLACK);
    r.pixel(10, 20, palette::CYAN);

    const uint32_t* fb = r.framebuffer();
    uint32_t expected = palette::CYAN.to_argb();
    CHECK(fb[20 * r.fb_w() + 10] == expected);

    // Adjacent pixels should be black
    CHECK(fb[20 * r.fb_w() + 11] == palette::BLACK.to_argb());
}

TEST_CASE("Renderer pixel out of bounds is no-op", "[renderer]") {
    Renderer r;
    r.clear(palette::BLACK);

    // Should not crash or corrupt
    r.pixel(-1, 0, palette::CYAN);
    r.pixel(0, -1, palette::CYAN);
    r.pixel(r.fb_w(), 0, palette::CYAN);
    r.pixel(0, r.fb_h(), palette::CYAN);

    // All pixels should still be black
    const uint32_t* fb = r.framebuffer();
    for (int i = 0; i < r.fb_w() * r.fb_h(); ++i)
        CHECK(fb[i] == palette::BLACK.to_argb());
}

TEST_CASE("Renderer hline draws horizontal line", "[renderer]") {
    Renderer r;
    r.clear(palette::BLACK);
    r.hline(10, 50, 100, palette::RED);

    const uint32_t* fb = r.framebuffer();
    uint32_t red = palette::RED.to_argb();

    for (int x = 10; x <= 50; ++x)
        CHECK(fb[100 * r.fb_w() + x] == red);

    // Outside the line
    CHECK(fb[100 * r.fb_w() + 9] == palette::BLACK.to_argb());
    CHECK(fb[100 * r.fb_w() + 51] == palette::BLACK.to_argb());
}

TEST_CASE("Renderer vline draws vertical line", "[renderer]") {
    Renderer r;
    r.clear(palette::BLACK);
    r.vline(50, 10, 60, palette::GREEN);

    const uint32_t* fb = r.framebuffer();
    uint32_t green = palette::GREEN.to_argb();

    for (int y = 10; y <= 60; ++y)
        CHECK(fb[y * r.fb_w() + 50] == green);

    CHECK(fb[9 * r.fb_w() + 50] == palette::BLACK.to_argb());
    CHECK(fb[61 * r.fb_w() + 50] == palette::BLACK.to_argb());
}

TEST_CASE("Renderer rect draws border", "[renderer]") {
    Renderer r;
    r.clear(palette::BLACK);
    r.rect(10, 10, 20, 20, palette::CYAN);

    const uint32_t* fb = r.framebuffer();
    uint32_t cyan = palette::CYAN.to_argb();

    // Corners
    CHECK(fb[10 * r.fb_w() + 10] == cyan);
    CHECK(fb[10 * r.fb_w() + 29] == cyan);
    CHECK(fb[29 * r.fb_w() + 10] == cyan);
    CHECK(fb[29 * r.fb_w() + 29] == cyan);

    // Inside should be black
    CHECK(fb[15 * r.fb_w() + 15] == palette::BLACK.to_argb());
}

TEST_CASE("Renderer rect_fill fills area", "[renderer]") {
    Renderer r;
    r.clear(palette::BLACK);
    r.rect_fill(5, 5, 10, 10, palette::VIOLET);

    const uint32_t* fb = r.framebuffer();
    uint32_t violet = palette::VIOLET.to_argb();

    // All pixels inside rect
    for (int y = 5; y < 15; ++y)
        for (int x = 5; x < 15; ++x)
            CHECK(fb[y * r.fb_w() + x] == violet);

    // Outside
    CHECK(fb[4 * r.fb_w() + 5] == palette::BLACK.to_argb());
    CHECK(fb[5 * r.fb_w() + 4] == palette::BLACK.to_argb());
}

TEST_CASE("Color to_argb packs correctly", "[renderer]") {
    Color c(255, 128, 0, 64);
    uint32_t argb = c.to_argb();
    CHECK((argb >> 24) == 64);   // A
    CHECK(((argb >> 16) & 0xFF) == 255); // R
    CHECK(((argb >> 8) & 0xFF) == 128);  // G
    CHECK((argb & 0xFF) == 0);   // B
}

TEST_CASE("Color lerp interpolates", "[renderer]") {
    Color a(0, 0, 0);
    Color b(100, 200, 50);

    Color mid = a.lerp(b, 0.5f);
    CHECK(mid.r == 50);
    CHECK(mid.g == 100);
    CHECK(mid.b == 25);

    Color start = a.lerp(b, 0.0f);
    CHECK(start.r == 0);
    CHECK(start.g == 0);
    CHECK(start.b == 0);

    Color end = a.lerp(b, 1.0f);
    CHECK(end.r == 100);
    CHECK(end.g == 200);
    CHECK(end.b == 50);
}

TEST_CASE("Color with_alpha sets alpha", "[renderer]") {
    Color c(100, 150, 200, 255);
    Color half = c.with_alpha(128);
    CHECK(half.r == 100);
    CHECK(half.g == 150);
    CHECK(half.b == 200);
    CHECK(half.a == 128);
}
