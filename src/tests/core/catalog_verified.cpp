// Copyright Azahar for Frame contributors
// Licensed under GPLv2 or any later version
// Refer to the misc/licenses/gplv2.txt file included.

// AzaharFrame fixd: "verified" in both catalogs reaches every list the menu shows (Download Packs,
// Install Mods, the loaded mods), and the lists' order: the version fit first, then the verified
// entries, then the recommended ones, then the most downloaded (unknown counts last), then by name.

#include <filesystem>
#include <string>
#include <vector>
#include <unistd.h>
#include <catch2/catch_test_macros.hpp>
#include "core/custom_tex/download.h"
#include "core/mods/catalog.h"
#include "core/mods/library.h"

namespace fs = std::filesystem;

namespace {

constexpr u64 kTid = 0x0004000000125600ULL; // Majora's Mask 3D (Europe)

struct Temp {
    fs::path root;
    Temp() {
        static int n = 0;
        root = fs::temp_directory_path() /
               ("azf_fixd_" + std::to_string(::getpid()) + "_" + std::to_string(n++));
        fs::remove_all(root);
        fs::create_directories(root / "load");
    }
    ~Temp() {
        std::error_code ec;
        fs::remove_all(root, ec);
    }
    std::string load() const {
        return (root / "load").string();
    }
};

std::vector<std::string> Ids(const std::vector<CustomTex::Download::PackInfo>& list) {
    std::vector<std::string> ids;
    for (const auto& p : list) {
        ids.push_back(p.id);
    }
    return ids;
}

std::vector<std::string> Ids(const std::vector<Mods::AvailableMod>& list) {
    std::vector<std::string> ids;
    for (const auto& m : list) {
        ids.push_back(m.id);
    }
    return ids;
}

Mods::Compat::Verdict Mismatched() {
    Mods::Compat::Verdict v;
    v.shown = true;
    v.kind = Mods::Compat::Mismatch::Region;
    return v;
}

} // namespace

TEST_CASE("fixd: verified in the texture catalog, and verified packs above unknown counts",
          "[fixd][verified]") {
    std::vector<std::string> warnings;
    // Henriko's two packs: verified, no download count (Drive); a popular pack; a recommended one;
    // a "verified" that is not a boolean (left out, not an error).
    const auto c = CustomTex::Download::ParseCatalog(R"({"schema":1,"packs":[
      {"id":"popular","name":"Popular","title":"MM","title_ids":["0004000000125600"],"author":"x",
       "downloads":90000,"source":{"type":"https","url":"https://example.com/p.zip"}},
      {"id":"henriko-1080p","name":"Majora's Mask 3D 1080p","title":"MM","title_ids":["0004000000125600"],
       "author":"Henriko Magnifico","verified":true,"source":{"type":"https","url":"https://example.com/h1.zip"}},
      {"id":"unknown","name":"Aaa Unknown","title":"MM","title_ids":["0004000000125600"],"author":"x",
       "verified":"yes","source":{"type":"https","url":"https://example.com/u.zip"}},
      {"id":"henriko-4k","name":"Majora's Mask 3D 4K","title":"MM","title_ids":["0004000000125600"],
       "author":"Henriko Magnifico","verified":true,"source":{"type":"https","url":"https://example.com/h4.zip"}},
      {"id":"rec","name":"Recommended","title":"MM","title_ids":["0004000000125600"],"author":"x",
       "downloads":5,"recommended":true,"source":{"type":"https","url":"https://example.com/r.zip"}}]})",
                                                     nullptr, &warnings);
    REQUIRE(c.has_value());
    REQUIRE(c->packs.size() == 5);
    CHECK(c->Find("henriko-4k")->verified);
    CHECK(c->Find("henriko-1080p")->verified);
    CHECK_FALSE(c->Find("henriko-4k")->recommended);
    CHECK_FALSE(c->Find("unknown")->verified); // not a boolean
    CHECK_FALSE(c->Find("popular")->verified);

    Temp t;
    CustomTex::Download::Downloader dl(t.load(), *c);
    const auto list = dl.ListForTitle(kTid);
    // verified first (by name between them: neither has a count), then recommended, then the most
    // downloaded, then the unknown counts
    CHECK(Ids(list) == std::vector<std::string>{"henriko-1080p", "henriko-4k", "rec", "popular", "unknown"});
    CHECK(list[0].verified);
    CHECK(list[1].verified);
    CHECK_FALSE(list[2].verified);
}

TEST_CASE("fixd: the pack order puts the version fit before verified", "[fixd][verified]") {
    using CustomTex::Download::ListedBefore;
    using CustomTex::Download::PackInfo;
    PackInfo fits, verified_other, verified, rec, counted, unknown;
    fits.name = "Z fits";
    verified_other.name = "A verified, other version";
    verified_other.verified = true;
    verified_other.compat = Mismatched();
    verified.name = "Y verified";
    verified.verified = true;
    rec.name = "B recommended";
    rec.recommended = true;
    counted.name = "C counted";
    counted.downloads = 10;
    unknown.name = "D unknown";
    // the version fit wins over verified
    CHECK(ListedBefore(fits, verified_other));
    CHECK_FALSE(ListedBefore(verified_other, fits));
    // verified above recommended, above counted, above unknown counts
    CHECK(ListedBefore(verified, rec));
    CHECK(ListedBefore(verified, counted));
    CHECK(ListedBefore(verified, unknown));
    CHECK_FALSE(ListedBefore(rec, verified));
    // the existing order below: recommended, downloads, unknown counts, name
    CHECK(ListedBefore(rec, counted));
    CHECK(ListedBefore(counted, unknown));
    PackInfo more = counted;
    more.name = "E more";
    more.downloads = 20;
    CHECK(ListedBefore(more, counted));
    PackInfo verified2 = verified;
    verified2.name = "X verified";
    CHECK(ListedBefore(verified2, verified)); // by name between two verified ones
    CHECK_FALSE(ListedBefore(verified, verified)); // a strict order
}

TEST_CASE("fixd: verified in the mod catalog, Install Mods and the loaded list", "[fixd][verified]") {
    // "verified" counts for the maintainer's repository and GameBanana only (catalog.cpp)
    const auto c = Mods::ParseCatalog(R"({"schema":1,"mods":[
 {"id":"gb-counted","name":"Counted","title_ids":["0004000000125600"],"author":"x","downloads":5000,
  "source":{"type":"gamebanana","mod":1,"file_pattern":"*.zip"}},
 {"id":"gb-verified","name":"Verified GameBanana","title_ids":["0004000000125600"],"author":"x",
  "verified":true,"source":{"type":"gamebanana","mod":2,"file_pattern":"*.zip"}},
 {"id":"gh-claims","name":"Aaa Claims Verified","title_ids":["0004000000125600"],"author":"x",
  "verified":true,"source":{"type":"github_release","repo":"o/c","asset_pattern":"*.zip"}},
 {"id":"owner-fix","name":"No Bloom","title_ids":["0004000000125600"],"author":"simplysanatori",
  "verified":true,"source":{"type":"owner_repo_path","path":"games/x/patch/0004000000125600/code.ips"}},
 {"id":"rec","name":"Recommended","title_ids":["0004000000125600"],"author":"x","recommended":true,
  "source":{"type":"github_release","repo":"o/r","asset_pattern":"*.zip"}}
]})");
    REQUIRE(c.has_value());
    CHECK(c->Find("gb-verified")->verified);
    CHECK(c->Find("owner-fix")->verified);
    CHECK_FALSE(c->Find("gh-claims")->verified); // a community release is never marked verified
    CHECK_FALSE(c->Find("gb-counted")->verified);
    CHECK_FALSE(c->Find("rec")->verified);

    Temp t;
    Mods::Library lib(t.load() + "/", *c, [](const std::string&, const std::string&, std::size_t) {
        Mods::HttpReply r;
        r.status = 404;
        return r;
    });
    const auto list = lib.ListAvailable(kTid);
    CHECK(Ids(list) == std::vector<std::string>{"owner-fix", "gb-verified", "rec", "gb-counted", "gh-claims"});
    REQUIRE(list.size() == 5);
    CHECK(list[0].verified);
    CHECK(list[1].verified);
    CHECK_FALSE(list[4].verified);

    // the loaded list's groups: verified, then recommended, then by name
    Mods::ModInfo a, b, r, v;
    a.name = "Aaa";
    b.name = "Bbb";
    r.name = "Zzz recommended";
    r.recommended = true;
    v.name = "Zzz verified";
    v.verified = true;
    CHECK(Mods::LoadedBefore(v, r));
    CHECK(Mods::LoadedBefore(r, a));
    CHECK(Mods::LoadedBefore(a, b));
    CHECK_FALSE(Mods::LoadedBefore(a, v));
}

TEST_CASE("fixd: the mod order puts the version fit before verified", "[fixd][verified]") {
    using Mods::AvailableMod;
    using Mods::ListedBefore;
    AvailableMod fits, verified_other, verified, rec, match, counted, unknown;
    fits.name = "Z fits";
    verified_other.name = "A verified, other version";
    verified_other.verified = true;
    verified_other.compat = Mismatched();
    verified.name = "Y verified";
    verified.verified = true;
    rec.name = "B recommended";
    rec.recommended = true;
    match.name = "C matches the game's code";
    match.matches_game = true;
    counted.name = "D counted";
    counted.downloads = 10;
    unknown.name = "E unknown";
    CHECK(ListedBefore(fits, verified_other));
    CHECK(ListedBefore(verified, rec));
    CHECK(ListedBefore(verified, match));
    CHECK(ListedBefore(verified, counted));
    CHECK(ListedBefore(verified, unknown));
    CHECK(ListedBefore(rec, match));
    CHECK(ListedBefore(match, counted));
    CHECK(ListedBefore(counted, unknown));
    CHECK_FALSE(ListedBefore(verified, verified));
}
