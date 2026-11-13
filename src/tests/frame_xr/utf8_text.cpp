// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// Game titles and other text with accents in the VR UI: "Pokémon X" shows its é, never "?".

#include <string>
#include <vector>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include "frame_xr/menu/md3_fonts.h"
#include "frame_xr/ui/ui_canvas.h"
#include "frame_xr/ui/ui_ttf.h"

using namespace frame_xr::ui;

namespace {

// The pixels of `text` drawn alone on a canvas (alpha of each pixel, summed per row would be
// enough, but all of them is simplest to compare).
std::vector<uint32_t> Render(int font, const std::string& text, float* width = nullptr) {
    InitFonts();
    Canvas cv;
    cv.Init(1400, 80);
    cv.Clear(rgb(0, 0, 0, 0.0f));
    cv.Text(10, 55, font, text.c_str(), rgb(255, 255, 255));
    if (width) {
        *width = TextWidth(font, text.c_str());
    }
    return cv.px;
}

} // namespace

TEST_CASE("UI text: é is drawn, not '?'", "[frame_xr][utf8]") {
    for (const int font : {kFontLarge, kFontSmall}) {
        float we = 0, wq = 0, wplain = 0;
        const auto e_acute = Render(font, "\xC3\xA9", &we); // é
        const auto question = Render(font, "?", &wq);
        const auto plain_e = Render(font, "e", &wplain);
        CHECK(e_acute != question);
        CHECK(e_acute != plain_e); // the accent is there
        CHECK(e_acute != Render(font, "")); // and something is drawn
        CHECK(we > 0.0f);
        CHECK(we == Catch::Approx(wplain).margin(wplain * 0.25f)); // about an e wide
        // The title of the report: Pokémon X, with "Pokemon X" and "Pok?mon X" both different.
        const auto title = Render(font, "Autosave Off for Pok\xC3\xA9mon X");
        CHECK(title != Render(font, "Autosave Off for Pok?mon X"));
        CHECK(title != Render(font, "Autosave Off for Pokemon X"));
    }
}

TEST_CASE("UI text: Latin-1 and Latin Extended-A have glyphs", "[frame_xr][utf8]") {
    TtfFont face;
    REQUIRE(face.LoadBase64(FrameXr::Menu::Fonts::kSans700));
    for (char32_t cp = 0xC0; cp <= 0x17F; cp++) {
        INFO("U+" << std::hex << static_cast<unsigned>(cp));
        if (cp == 0xD7 || cp == 0xF7) {
            continue; // x and divide are still ordinary glyphs, but not letters: checked below
        }
        // The face lacks a few rare ones (Esperanto's Ĉ, Ŋ, ſ ...): those fold to a plain letter.
        CHECK((face.HasGlyph(cp) || FoldedText(cp) != "?"));
    }
    for (const char32_t cp : {char32_t{0xD7}, char32_t{0xF7}, char32_t{0xA1}, char32_t{0xBF},
                              char32_t{0xB0}, char32_t{0x2019}, char32_t{0x201C},
                              char32_t{0x201D}, char32_t{0x2013}, char32_t{0x2014},
                              char32_t{0x2026}}) {
        INFO("U+" << std::hex << static_cast<unsigned>(cp));
        CHECK(face.HasGlyph(cp));
    }
    // And every one of them draws something other than the "?" box on the canvas.
    const auto question = Render(kFontSmall, "?");
    for (char32_t cp = 0xC0; cp <= 0x17F; cp++) {
        INFO("U+" << std::hex << static_cast<unsigned>(cp));
        CHECK(Render(kFontSmall, Utf8Of(cp)) != question);
    }
    for (const char* punct : {"\xE2\x80\x99", "\xE2\x80\x9C", "\xE2\x80\x9D", "\xE2\x80\x93",
                              "\xE2\x80\x94", "\xE2\x80\xA6"}) {
        CHECK(Render(kFontLarge, punct) != question);
    }
}

TEST_CASE("UI text: characters no face has fold to something plain", "[frame_xr][utf8]") {
    CHECK(FoldedText(0x2019) == "'");
    CHECK(FoldedText(0x201D) == "\"");
    CHECK(FoldedText(0x2014) == "-");
    CHECK(FoldedText(0x2026) == "...");
    CHECK(FoldedText(0xE9) == "e");
    CHECK(FoldedText(0xDC) == "U");
    CHECK(FoldedText(0xF1) == "n");
    CHECK(FoldedText(0x4E2D) == "?");
    // A CJK character is neither in the atlas nor the face: "?" (the same as typing one).
    CHECK(Render(kFontSmall, "\xE4\xB8\xAD") == Render(kFontSmall, "?"));
}

TEST_CASE("UI text: decoding", "[frame_xr][utf8]") {
    const std::string s = "P\xC3\xA9\xE2\x80\x99z";
    std::size_t i = 0;
    CHECK(NextUtf8(s, i) == U'P');
    CHECK(NextUtf8(s, i) == 0xE9);
    CHECK(NextUtf8(s, i) == 0x2019);
    CHECK(NextUtf8(s, i) == U'z');
    CHECK(i == s.size());
    // A raw Latin-1 é (a title that never became UTF-8) still reads as é.
    const std::string raw = "Pok\xE9mon";
    i = 3;
    CHECK(NextUtf8(raw, i) == 0xE9);
    CHECK(i == 4);
    CHECK(Render(kFontSmall, raw) == Render(kFontSmall, "Pok\xC3\xA9mon"));
    // A cut-off sequence at the end does not read past it.
    const std::string cut = "a\xE2\x80";
    i = 1;
    CHECK(NextUtf8(cut, i) == 0xE2);
    CHECK(i == 2);
}

TEST_CASE("UI text: the TrueType face and the atlas agree on width", "[frame_xr][utf8]") {
    InitFonts();
    float with = 0, without = 0;
    Render(kFontLarge, "Pok\xC3\xA9mon X", &with);
    Render(kFontLarge, "Pokemon X", &without);
    CHECK(with > 0.0f);
    CHECK(with == Catch::Approx(without).margin(without * 0.05f));
    // Wrapping and measuring never split the é.
    const auto lines = WrapText(kFontSmall, "Pok\xC3\xA9mon X Special Pok\xC3\xA9mon Edition", 200, 3);
    for (const auto& l : lines) {
        CHECK(TextWidth(kFontSmall, l.c_str()) <= 200.0f);
    }
}
