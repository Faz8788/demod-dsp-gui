// Editor syntax highlighting tests
#include <catch2/catch_test_macros.hpp>
#include "ui/editor_screen.hpp"
#include <string>

using namespace demod::ui;
using TokenType = EditorScreen::TokenType;
using Token = EditorScreen::Token;

TEST_CASE("classify_word keywords", "[editor]") {
    CHECK(EditorScreen::classify_word("import") == TokenType::KEYWORD);
    CHECK(EditorScreen::classify_word("process") == TokenType::KEYWORD);
    CHECK(EditorScreen::classify_word("seq") == TokenType::KEYWORD);
    CHECK(EditorScreen::classify_word("par") == TokenType::KEYWORD);
    CHECK(EditorScreen::classify_word("sum") == TokenType::KEYWORD);
    CHECK(EditorScreen::classify_word("prod") == TokenType::KEYWORD);
    CHECK(EditorScreen::classify_word("with") == TokenType::KEYWORD);
    CHECK(EditorScreen::classify_word("letrec") == TokenType::KEYWORD);
}

TEST_CASE("classify_word UI elements", "[editor]") {
    CHECK(EditorScreen::classify_word("hslider") == TokenType::UI_ELEMENT);
    CHECK(EditorScreen::classify_word("vslider") == TokenType::UI_ELEMENT);
    CHECK(EditorScreen::classify_word("button") == TokenType::UI_ELEMENT);
    CHECK(EditorScreen::classify_word("checkbox") == TokenType::UI_ELEMENT);
    CHECK(EditorScreen::classify_word("nentry") == TokenType::UI_ELEMENT);
    CHECK(EditorScreen::classify_word("hbargraph") == TokenType::UI_ELEMENT);
    CHECK(EditorScreen::classify_word("vbargraph") == TokenType::UI_ELEMENT);
}

TEST_CASE("classify_word numbers", "[editor]") {
    CHECK(EditorScreen::classify_word("0") == TokenType::NUMBER);
    CHECK(EditorScreen::classify_word("42") == TokenType::NUMBER);
    CHECK(EditorScreen::classify_word("3.14") == TokenType::NUMBER);
    CHECK(EditorScreen::classify_word("1000") == TokenType::NUMBER);
}

TEST_CASE("classify_word operators", "[editor]") {
    CHECK(EditorScreen::classify_word("=") == TokenType::OPERATOR);
    CHECK(EditorScreen::classify_word("+") == TokenType::OPERATOR);
    CHECK(EditorScreen::classify_word("-") == TokenType::OPERATOR); // - alone is OPERATOR
    CHECK(EditorScreen::classify_word("*") == TokenType::OPERATOR);
    CHECK(EditorScreen::classify_word("/") == TokenType::OPERATOR);
    CHECK(EditorScreen::classify_word("<:") == TokenType::OPERATOR);
    CHECK(EditorScreen::classify_word(":>") == TokenType::OPERATOR);
    CHECK(EditorScreen::classify_word("~") == TokenType::OPERATOR);
    CHECK(EditorScreen::classify_word("(") == TokenType::OPERATOR);
    CHECK(EditorScreen::classify_word(")") == TokenType::OPERATOR);
}

TEST_CASE("classify_word normal identifiers", "[editor]") {
    CHECK(EditorScreen::classify_word("myFunc") == TokenType::NORMAL);
    CHECK(EditorScreen::classify_word("foo.bar") == TokenType::NORMAL);
    CHECK(EditorScreen::classify_word("_private") == TokenType::NORMAL);
    CHECK(EditorScreen::classify_word("reverb") == TokenType::NORMAL);
}

TEST_CASE("classify_word empty string", "[editor]") {
    CHECK(EditorScreen::classify_word("") == TokenType::NORMAL);
}

TEST_CASE("tokenize_line empty", "[editor]") {
    EditorScreen ed;
    auto tokens = ed.tokenize_line("");
    CHECK(tokens.empty());
}

TEST_CASE("tokenize_line comment only", "[editor]") {
    EditorScreen ed;
    auto tokens = ed.tokenize_line("// this is a comment");

    REQUIRE(tokens.size() == 1);
    CHECK(tokens[0].type == TokenType::COMMENT);
    CHECK(tokens[0].start == 0);
}

TEST_CASE("tokenize_line words with comment", "[editor]") {
    EditorScreen ed;
    auto tokens = ed.tokenize_line("x = 1 // comment");

    REQUIRE(tokens.size() >= 4);
    // First tokens: x, =, 1
    CHECK(tokens[0].type == TokenType::NORMAL);   // x
    CHECK(tokens[1].type == TokenType::OPERATOR);  // =
    CHECK(tokens[2].type == TokenType::NUMBER);    // 1
    // Last token is the comment
    CHECK(tokens.back().type == TokenType::COMMENT);
}

TEST_CASE("tokenize_line keyword and UI element", "[editor]") {
    EditorScreen ed;
    auto tokens = ed.tokenize_line("process = vslider(\"gain\",0.5,0,1,0.01)");

    CHECK(tokens[0].type == TokenType::KEYWORD);    // process
    CHECK(tokens[1].type == TokenType::OPERATOR);    // =
    CHECK(tokens[2].type == TokenType::UI_ELEMENT);  // vslider
}

TEST_CASE("tokenize_line string literal", "[editor]") {
    EditorScreen ed;
    auto tokens = ed.tokenize_line("\"hello world\"");

    REQUIRE(tokens.size() == 1);
    CHECK(tokens[0].type == TokenType::STRING);
}

TEST_CASE("tokenize_line multi-char operators", "[editor]") {
    EditorScreen ed;
    auto tokens = ed.tokenize_line("a <: b :> c");

    // Find <: and :> operators
    std::string line = "a <: b :> c";
    bool found_lt_colon = false, found_colon_gt = false;
    for (const auto& t : tokens) {
        if (t.type == TokenType::OPERATOR) {
            std::string op = line.substr(t.start, t.len);
            if (op == "<:") found_lt_colon = true;
            if (op == ":>") found_colon_gt = true;
        }
    }
    CHECK(found_lt_colon);
    CHECK(found_colon_gt);
}

TEST_CASE("tokenize_line mixed Faust expression", "[editor]") {
    EditorScreen ed;
    auto tokens = ed.tokenize_line("import(\"stdfaust.lib\");");

    CHECK(tokens[0].type == TokenType::KEYWORD);  // import
    CHECK(tokens[1].type == TokenType::OPERATOR);  // (
    CHECK(tokens[2].type == TokenType::STRING);    // "stdfaust.lib"
}

TEST_CASE("tokenize_line whitespace only", "[editor]") {
    EditorScreen ed;
    auto tokens = ed.tokenize_line("   ");
    CHECK(tokens.empty());
}
