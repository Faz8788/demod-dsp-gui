// Terminal VT100 parser tests
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "terminal/terminal.hpp"
#include <cstring>

using namespace demod::terminal;
using Catch::Approx;

// Helper: feed a string to a terminal and check cells
static void feed(Terminal& t, const char* s) {
    t.feed_bytes(s, strlen(s));
}

TEST_CASE("Terminal defaults", "[terminal]") {
    Terminal t;
    CHECK(t.rows() == 24);
    CHECK(t.cols() == 80);
    CHECK(t.cursor_row() == 0);
    CHECK(t.cursor_col() == 0);
}

TEST_CASE("indexed_to_rgb standard colors 0-7", "[terminal]") {
    uint8_t r, g, b;

    // Black
    Terminal::indexed_to_rgb(0, r, g, b);
    CHECK(r == 10); CHECK(g == 10); CHECK(b == 18);

    // Red
    Terminal::indexed_to_rgb(1, r, g, b);
    CHECK(r == 200); CHECK(g == 50); CHECK(b == 50);

    // Green
    Terminal::indexed_to_rgb(2, r, g, b);
    CHECK(r == 50); CHECK(g == 200); CHECK(b == 50);

    // White
    Terminal::indexed_to_rgb(7, r, g, b);
    CHECK(r == 200); CHECK(g == 200); CHECK(b == 200);
}

TEST_CASE("indexed_to_rgb bright colors 8-15", "[terminal]") {
    uint8_t r, g, b;

    Terminal::indexed_to_rgb(8, r, g, b);
    CHECK(r == 100); CHECK(g == 100); CHECK(b == 100);

    Terminal::indexed_to_rgb(15, r, g, b);
    CHECK(r == 255); CHECK(g == 255); CHECK(b == 255);
}

TEST_CASE("indexed_to_rgb 6x6x6 color cube", "[terminal]") {
    uint8_t r, g, b;

    // Index 16 = (0,0,0) in color cube
    Terminal::indexed_to_rgb(16, r, g, b);
    CHECK(r == 0); CHECK(g == 0); CHECK(b == 0);

    // Index 231 = (5,5,5) = maximum
    Terminal::indexed_to_rgb(231, r, g, b);
    CHECK(r == 255); CHECK(g == 255); CHECK(b == 255);
}

TEST_CASE("indexed_to_rgb grayscale ramp", "[terminal]") {
    uint8_t r, g, b;

    Terminal::indexed_to_rgb(232, r, g, b);
    CHECK(r == 8); CHECK(g == 8); CHECK(b == 8);

    Terminal::indexed_to_rgb(255, r, g, b);
    CHECK(r == 238); CHECK(g == 238); CHECK(b == 238);
}

TEST_CASE("feed printable ASCII", "[terminal]") {
    Terminal t;
    t.init_grid(5, 10);

    feed(t, "AB");

    auto& c0 = t.get_cell(0, 0);
    auto& c1 = t.get_cell(0, 1);
    CHECK(c0.ch == 'A');
    CHECK(c1.ch == 'B');
    CHECK(t.cursor_col() == 2);
}

TEST_CASE("feed newline advances row", "[terminal]") {
    Terminal t;
    t.init_grid(5, 10);

    t.feed_bytes("H", 1);
    CHECK(t.get_cell(0, 0).ch == 'H');

    t.feed_bytes("\n", 1);
    CHECK(t.cursor_row() == 1);

    // Debug: write directly to grid
    t.feed_bytes("X", 1);
    CHECK(t.cursor_col() == 1);
    CHECK(t.get_cell(1, 0).ch == 'X');
}

TEST_CASE("feed carriage return resets column", "[terminal]") {
    Terminal t;
    t.init_grid(5, 10);

    feed(t, "AB\rC");

    CHECK(t.get_cell(0, 0).ch == 'C');
    CHECK(t.cursor_col() == 1);
}

TEST_CASE("feed tab advances to next stop", "[terminal]") {
    Terminal t;
    t.init_grid(5, 20);

    feed(t, "A\tB");

    CHECK(t.get_cell(0, 0).ch == 'A');
    CHECK(t.get_cell(0, 8).ch == 'B');
    CHECK(t.cursor_col() == 9);
}

TEST_CASE("feed backspace moves cursor back", "[terminal]") {
    Terminal t;
    t.init_grid(5, 10);

    feed(t, "ABC\b\bD");

    CHECK(t.get_cell(0, 0).ch == 'A');
    CHECK(t.get_cell(0, 1).ch == 'D');
    CHECK(t.get_cell(0, 2).ch == 'C');
}

TEST_CASE("CSI cursor movement ESC[A/B/C/D", "[terminal]") {
    Terminal t;
    t.init_grid(10, 20);

    feed(t, "AB\e[C\e[C");
    CHECK(t.cursor_col() == 4);

    feed(t, "\e[A");
    CHECK(t.cursor_row() == 0); // Can't go above 0
}

TEST_CASE("CSI cursor position ESC[row;colH", "[terminal]") {
    Terminal t;
    t.init_grid(10, 20);

    feed(t, "\e[5;10HX");

    CHECK(t.cursor_row() == 4);
    CHECK(t.cursor_col() == 10);
    CHECK(t.get_cell(4, 9).ch == 'X');
}

TEST_CASE("CSI erase display ESC[2J", "[terminal]") {
    Terminal t;
    t.init_grid(5, 10);

    feed(t, "ABC\e[2J");

    CHECK(t.get_cell(0, 0).ch == ' ');
    CHECK(t.get_cell(0, 1).ch == ' ');
    CHECK(t.get_cell(0, 2).ch == ' ');
    CHECK(t.cursor_row() == 0);
    CHECK(t.cursor_col() == 0);
}

TEST_CASE("CSI erase to EOL ESC[K", "[terminal]") {
    Terminal t;
    t.init_grid(5, 10);

    feed(t, "ABCD\e[1;2H\e[K");

    CHECK(t.get_cell(0, 0).ch == 'A');
    CHECK(t.get_cell(0, 1).ch == ' ');
    CHECK(t.get_cell(0, 2).ch == ' ');
    CHECK(t.get_cell(0, 3).ch == ' ');
}

TEST_CASE("SGR reset ESC[0m", "[terminal]") {
    Terminal t;
    t.init_grid(5, 10);

    feed(t, "\e[1;31mA\e[0mB");

    CHECK(t.get_cell(0, 0).ch == 'A');
    CHECK(t.get_cell(0, 0).bold == true);
    CHECK(t.get_cell(0, 1).ch == 'B');
    CHECK(t.get_cell(0, 1).bold == false);
}

TEST_CASE("SGR standard foreground colors", "[terminal]") {
    Terminal t;
    t.init_grid(5, 10);

    feed(t, "\e[31mA");

    // Color 1 = red (200, 50, 50)
    auto& c = t.get_cell(0, 0);
    CHECK(c.fg_r == 200);
    CHECK(c.fg_g == 50);
    CHECK(c.fg_b == 50);
}

TEST_CASE("SGR bright foreground colors", "[terminal]") {
    Terminal t;
    t.init_grid(5, 10);

    feed(t, "\e[91mA");

    // Bright red = color 9 = (255, 80, 80)
    auto& c = t.get_cell(0, 0);
    CHECK(c.fg_r == 255);
    CHECK(c.fg_g == 80);
    CHECK(c.fg_b == 80);
}

TEST_CASE("SGR truecolor foreground ESC[38;2;R;G;Bm", "[terminal]") {
    Terminal t;
    t.init_grid(5, 10);

    feed(t, "\e[38;2;100;200;50mA");

    auto& c = t.get_cell(0, 0);
    CHECK(c.fg_r == 100);
    CHECK(c.fg_g == 200);
    CHECK(c.fg_b == 50);
}

TEST_CASE("SGR truecolor background ESC[48;2;R;G;Bm", "[terminal]") {
    Terminal t;
    t.init_grid(5, 10);

    feed(t, "\e[48;2;10;20;30mA");

    auto& c = t.get_cell(0, 0);
    CHECK(c.bg_r == 10);
    CHECK(c.bg_g == 20);
    CHECK(c.bg_b == 30);
}

TEST_CASE("SGR 256-color foreground ESC[38;5;Nm", "[terminal]") {
    Terminal t;
    t.init_grid(5, 10);

    feed(t, "\e[38;5;42mA");

    // Color 42 is in the 6x6x6 cube
    uint8_t r, g, b;
    Terminal::indexed_to_rgb(42, r, g, b);
    CHECK(t.get_cell(0, 0).fg_r == r);
    CHECK(t.get_cell(0, 0).fg_g == g);
    CHECK(t.get_cell(0, 0).fg_b == b);
}

TEST_CASE("SGR bold and underline", "[terminal]") {
    Terminal t;
    t.init_grid(5, 10);

    feed(t, "\e[1;4mA");

    auto& c = t.get_cell(0, 0);
    CHECK(c.bold == true);
    CHECK(c.underline == true);
}

TEST_CASE("private mode sequences don't crash", "[terminal]") {
    Terminal t;
    t.init_grid(5, 10);

    // These are the sequences OpenCode sends that caused the crash
    feed(t, "\e[?25h");   // Show cursor
    feed(t, "\e[?1049h"); // Alt screen
    feed(t, "\e[?1049l"); // Exit alt screen
    feed(t, "\e[?25l");   // Hide cursor

    // Should not crash, grid should be intact
    feed(t, "OK");
    CHECK(t.get_cell(0, 0).ch == 'O');
    CHECK(t.get_cell(0, 1).ch == 'K');
}

TEST_CASE("newline scrolls grid at bottom", "[terminal]") {
    Terminal t;
    t.init_grid(3, 5); // 3 rows, 5 cols

    // Feed line by line to avoid parsing ambiguity
    t.feed_bytes("AAA\n", 4);
    t.feed_bytes("BBB\n", 4);
    t.feed_bytes("CCC\n", 4);
    t.feed_bytes("DDD", 3);

    // After 4 newlines (3 visible rows), "AAA" should be scrolled off
    CHECK(t.get_cell(0, 0).ch == 'B');
    CHECK(t.get_cell(1, 0).ch == 'C');
    CHECK(t.get_cell(2, 0).ch == 'D');
}

TEST_CASE("ESC c resets terminal", "[terminal]") {
    Terminal t;
    t.init_grid(5, 10);

    feed(t, "ABC\ec");

    CHECK(t.get_cell(0, 0).ch == ' ');
    CHECK(t.cursor_row() == 0);
    CHECK(t.cursor_col() == 0);
}
