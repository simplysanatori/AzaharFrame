// Copyright Azahar for Frame contributors
// Licensed under GPLv2 or any later version
// Refer to the misc/licenses/gplv2.txt file included.

// AzaharFrame jsoncrash: pack.json in every bad shape reads as a legacy pack, never throws.

#include <catch2/catch_test_macros.hpp>
#include "video_core/custom_textures/pack_config.h"

using VideoCore::PackConfig::Parse;

TEST_CASE("pack.json without options is a legacy pack (the downloaded-pack shape)", "[jsoncrash]") {
    // What Download writes for a pack that has none: a name and an author. It aborted the app
    // with "cannot use operator[] with a string argument with null".
    const auto c = Parse(R"({"author": "blivveries", "name": "5X Redrawn UI HD"})");
    REQUIRE_FALSE(c.valid);
}

TEST_CASE("pack.json that is not an object, or not JSON", "[jsoncrash]") {
    for (const char* text : {"", "null", "[]", "[1,2]", "42", "\"x\"", "{", "not json",
                             "{\"options\": null}", "{\"options\": []}", "{\"options\": 3}"}) {
        REQUIRE_NOTHROW(Parse(text));
        REQUIRE_FALSE(Parse(text).valid);
    }
}

TEST_CASE("pack.json options of the wrong type keep the legacy defaults", "[jsoncrash]") {
    const auto c = Parse(R"({"options": {"skip_mipmap": "yes", "flip_png_files": null}})");
    REQUIRE(c.valid);
    REQUIRE(c.skip_mipmap);
    REQUIRE(c.flip_png_files);
    REQUIRE_FALSE(c.use_new_hash);
}

TEST_CASE("pack.json good options and textures", "[jsoncrash]") {
    const auto c = Parse(R"({"options": {"skip_mipmap": false, "flip_png_files": false,
        "use_new_hash": true}, "textures": {"4E2DA064574FE0AC": "a/b.png", "1": ["x.png", 5, "y.png"],
        "2": 7, "3": null}})");
    REQUIRE(c.valid);
    REQUIRE_FALSE(c.skip_mipmap);
    REQUIRE_FALSE(c.flip_png_files);
    REQUIRE(c.use_new_hash);
    REQUIRE(c.textures.size() == 4);
    // the keys come in JSON's sorted order
    REQUIRE(c.textures[0].second == std::vector<std::string>{"x.png", "y.png"});
    REQUIRE(c.textures[1].second.empty());
    REQUIRE(c.textures[2].second.empty());
    REQUIRE(c.textures[3].first == "4E2DA064574FE0AC");
    REQUIRE(c.textures[3].second == std::vector<std::string>{"a/b.png"});
}

TEST_CASE("pack.json with textures of the wrong type", "[jsoncrash]") {
    const auto c = Parse(R"({"options": {"use_new_hash": true}, "textures": [1,2]})");
    REQUIRE(c.valid);
    REQUIRE(c.textures.empty());
}
