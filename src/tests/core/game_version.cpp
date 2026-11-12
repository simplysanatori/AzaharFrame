// Copyright Azahar for Frame contributors
// Licensed under GPLv2 or any later version
// Refer to the misc/licenses/gplv2.txt file included.

// AzaharFrame versions: raw version detection (NCSD card info, CIA and TMD parsing on synthetic
// headers, the order and "higher wins"), the table and its online override, the wording when
// there is no entry.

#include <cstring>
#include <catch2/catch_test_macros.hpp>
#include "core/cheats/sharkive.h"

using namespace Cheats::Sharkive;

namespace {

void Put16Le(std::vector<u8>& v, std::size_t at, u16 x) {
    v[at] = x & 0xFF;
    v[at + 1] = x >> 8;
}
void Put32Le(std::vector<u8>& v, std::size_t at, u32 x) {
    for (int i = 0; i < 4; ++i) {
        v[at + i] = (x >> (8 * i)) & 0xFF;
    }
}
void PutBe(std::vector<u8>& v, std::size_t at, u64 x, int bytes) {
    for (int i = 0; i < bytes; ++i) {
        v[at + i] = (x >> (8 * (bytes - 1 - i))) & 0xFF;
    }
}

std::vector<u8> Ncsd(u16 title_version, u16 revision, bool cver = true) {
    std::vector<u8> h(0x400, 0);
    std::memcpy(h.data() + 0x100, "NCSD", 4);
    Put16Le(h, 0x310, title_version);
    Put16Le(h, 0x312, revision);
    if (cver) { // the system update partition's title ID and version: never read
        Put16Le(h, 0x328, 6160);
        h[0x320] = 0x02;
    }
    return h;
}

/// A TMD with an RSA-2048/SHA-256 signature (type 0x10004).
std::vector<u8> Tmd(u64 title_id, u16 version) {
    std::vector<u8> t(0x140 + 0x9E + 0x40, 0);
    PutBe(t, 0, 0x10004, 4);
    PutBe(t, 0x140 + 0x4C, title_id, 8);
    PutBe(t, 0x140 + 0x9C, version, 2);
    return t;
}

/// A CIA: header, certificates, ticket, then the TMD (every section 0x40-aligned).
std::vector<u8> Cia(const std::vector<u8>& tmd) {
    const u32 cert = 0x100, tik = 0x350;
    std::vector<u8> c(0x2040 + 0x100 + 0x380 + tmd.size() + 0x40, 0);
    Put32Le(c, 0, 0x2020);
    Put32Le(c, 8, cert);
    Put32Le(c, 12, tik);
    Put32Le(c, 16, static_cast<u32>(tmd.size()));
    // cert at 0x2040, ticket at align(0x2140), tmd at align(0x2140 + 0x350)
    const std::size_t tmd_at = (0x2040 + cert + tik + 0x3F) / 0x40 * 0x40;
    std::memcpy(c.data() + tmd_at, tmd.data(), tmd.size());
    return c;
}

FileVersion FromBytes(const std::vector<u8>& file) {
    return ReadFileVersionFrom(std::span<const u8>(file.data(), std::min<std::size_t>(file.size(), 0x400)),
                               [&](u64 off, u8* out, u64 size) {
                                   if (off + size > file.size()) {
                                       return false;
                                   }
                                   std::memcpy(out, file.data() + off, size);
                                   return true;
                               });
}

constexpr u64 kMk7 = 0x0004000000030800ULL;
constexpr u64 kEchoesEu = 0x00040000001B4100ULL;
constexpr u64 kOther = 0x0004000000055D00ULL;

} // namespace

TEST_CASE("Versions: NCSD card info", "[versions]") {
    const auto card = ParseNcsdCardInfo(Ncsd(2112, 1));
    REQUIRE(card.has_value());
    REQUIRE(card->title_version == 2112);
    REQUIRE(card->revision == 1);
    // the CVer partition's version (0x328) is not the game's
    REQUIRE(ParseNcsdCardInfo(Ncsd(0, 0, true))->title_version == 0);
    auto bad = Ncsd(1, 1);
    bad[0x100] = 'X';
    REQUIRE_FALSE(ParseNcsdCardInfo(bad).has_value());
    bad = Ncsd(1, 1);
    bad.resize(0x200);
    REQUIRE_FALSE(ParseNcsdCardInfo(bad).has_value());
    // an encrypted dump: the NCCH is unreadable, the NCSD header is plaintext all the same
    auto enc = Ncsd(48, 1);
    for (std::size_t i = 0x1000; i < enc.size(); ++i) {
        enc[i] = 0xAA;
    }
    REQUIRE(FromBytes(enc).card_version == 48);
}

TEST_CASE("Versions: TMD and CIA", "[versions]") {
    const auto tmd = Tmd(0x0004000E00030800ULL, 3072);
    const auto info = ParseTmd(tmd);
    REQUIRE(info.has_value());
    REQUIRE(info->version == 3072);
    REQUIRE(info->title_id == 0x0004000E00030800ULL);
    for (const u32 type : {0x10000u, 0x10001u, 0x10003u, 0x10005u}) {
        auto t = tmd;
        t.resize(0x400, 0);
        PutBe(t, 0, type, 4);
        const std::size_t sig = type == 0x10000 || type == 0x10003 ? 0x200 : type == 0x10005 ? 0x3C : 0x100;
        const std::size_t body = (4 + sig + 0x3F) / 0x40 * 0x40;
        PutBe(t, body + 0x9C, 1040, 2);
        PutBe(t, body + 0x4C, 0x0004000000030800ULL, 8);
        REQUIRE(ParseTmd(t)->version == 1040);
    }
    auto unknown = tmd;
    PutBe(unknown, 0, 0x12345, 4);
    REQUIRE_FALSE(ParseTmd(unknown).has_value());
    REQUIRE_FALSE(ParseTmd(std::span<const u8>(tmd.data(), 0x100)).has_value());

    // a CIA with the update built in: the TMD in the file gives the version
    const auto cia = Cia(Tmd(kMk7, 3072));
    REQUIRE(FromBytes(cia).cia_tmd == 3072);
    REQUIRE_FALSE(FromBytes(cia).card_version.has_value());
    // a CIA of a system title (firmware) is never a game's version
    REQUIRE_FALSE(FromBytes(Cia(Tmd(0x000400DB00017302ULL, 2069))).cia_tmd.has_value());
    REQUIRE_FALSE(FromBytes(Cia(Tmd(0x0004013000001702ULL, 100))).cia_tmd.has_value());
    // an update's own CIA counts
    REQUIRE(FromBytes(Cia(Tmd(0x0004000E00030800ULL, 1040))).cia_tmd == 1040);
    // neither: a .3dsx, an empty file
    REQUIRE(FromBytes(std::vector<u8>(0x400, 0)).cia_tmd == std::nullopt);
    REQUIRE(ReadFileVersion("/nonexistent/none.3ds").card_version == std::nullopt);
}

TEST_CASE("Versions: raw order and the higher wins", "[versions]") {
    ClearOnlineVersions();
    const FileVersion none;
    auto r = SelectInstalledVersion(kOther, std::nullopt, std::nullopt, none, std::nullopt);
    REQUIRE_FALSE(r.raw.has_value());
    REQUIRE(r.source == VersionSource::Unknown);
    REQUIRE(r.Display().empty());

    // update, then base
    r = SelectInstalledVersion(kOther, 0x0410, 0x0400, none, 0x0002);
    REQUIRE(r.raw == 0x0410);
    REQUIRE(r.source == VersionSource::UpdateTmd);
    r = SelectInstalledVersion(kOther, std::nullopt, 0, none, 0x0002);
    REQUIRE(r.source == VersionSource::BaseTmd);
    REQUIRE(r.version == Version{1, 0});

    // a cartridge: card info, not the remaster (the remaster is the last resort)
    FileVersion card;
    card.card_version = 2112;
    card.card_revision = 1;
    r = SelectInstalledVersion(kMk7, std::nullopt, std::nullopt, card, 0x0002);
    REQUIRE(r.raw == 2112);
    REQUIRE(r.source == VersionSource::CardInfo);
    REQUIRE(r.revision == 1);
    // the higher of the cartridge's and the installed one: an older installed update loses
    r = SelectInstalledVersion(kMk7, 1040, std::nullopt, card, std::nullopt);
    REQUIRE(r.raw == 2112);
    REQUIRE(r.source == VersionSource::CardInfo);
    // a newer installed update wins; a tie goes to the installed one
    r = SelectInstalledVersion(kMk7, 3072, std::nullopt, card, std::nullopt);
    REQUIRE(r.raw == 3072);
    REQUIRE(r.source == VersionSource::UpdateTmd);
    r = SelectInstalledVersion(kMk7, 2112, std::nullopt, card, std::nullopt);
    REQUIRE(r.source == VersionSource::UpdateTmd);
    // a CIA with a newer update built in beats an older installed update
    FileVersion cia;
    cia.cia_tmd = 3072;
    r = SelectInstalledVersion(kMk7, 1040, std::nullopt, cia, std::nullopt);
    REQUIRE(r.raw == 3072);
    REQUIRE(r.source == VersionSource::CiaTmd);

    // the remaster: only with nothing else; a revision, never a version
    r = SelectInstalledVersion(kOther, std::nullopt, std::nullopt, none, 0x0002);
    REQUIRE(r.source == VersionSource::Ncch);
    REQUIRE(r.revision == 1);
    REQUIRE_FALSE(r.raw.has_value());
    REQUIRE_FALSE(r.version.has_value());
    r = SelectInstalledVersion(kOther, std::nullopt, std::nullopt, none, 4);
    REQUIRE(r.revision == 2);
    REQUIRE(SelectInstalledVersion(kOther, std::nullopt, std::nullopt, none, 0).source ==
            VersionSource::Unknown);
    REQUIRE(SelectInstalledVersion(kOther, std::nullopt, std::nullopt, none, 3).source ==
            VersionSource::Unknown);
    // card info of 0 (a cartridge as released) is a raw version too: no remaster needed
    FileVersion zero;
    zero.card_version = 0;
    zero.card_revision = 0;
    r = SelectInstalledVersion(kOther, std::nullopt, std::nullopt, zero, 0x0002);
    REQUIRE(r.source == VersionSource::CardInfo);
    REQUIRE(r.version == Version{1, 0});
    REQUIRE(r.Display() == "1.0");
}

TEST_CASE("Versions: the table", "[versions]") {
    ClearOnlineVersions();
    // the built-in entries
    REQUIRE(GameVersionOf(kEchoesEu, 1024) == Version{1, 1});
    REQUIRE(GameVersionOf(0x0004000E001B4100ULL, 1024) == Version{1, 1}); // the update's own ID
    REQUIRE(GameVersionOf(kMk7, 1040) == Version{1, 1});
    REQUIRE(GameVersionOf(kMk7, 3072) == Version{1, 2});
    REQUIRE(GameVersionOf(0x0004000E00030800ULL, 3072) == Version{1, 2});
    REQUIRE_FALSE(GameVersionOf(0x0004000000030700ULL, 3072).has_value()); // Europe: not checked
    // never decoded: 1024 is 1.0.0 in the bits, and another title's 1024 says nothing
    REQUIRE(FromTitleVersion(1024) == Version{1, 0});
    REQUIRE_FALSE(GameVersionOf(kOther, 1024).has_value());
    REQUIRE_FALSE(GameVersionOf(kOther, 5216).has_value());
    REQUIRE(GameVersionOf(kOther, 0) == Version{1, 0}); // the game as released
    REQUIRE(DecodedTriple(3072) == "3.0.0");

    // online: extends and overrides, keeps the built-in entries under it
    const char* online = R"({"schema": 1, "mods": [], "versions": [
        {"title_ids": ["00055D00", "0004000000055E00"], "raw": 5216, "shown": "1.5", "source": "test"},
        {"title_ids": ["001B4100"], "raw": 1024, "shown": "1.2", "source": "test override"},
        {"title_ids": ["zz"], "raw": 1, "shown": "1.0", "source": "bad id"},
        {"title_ids": ["00055D00"], "raw": 7, "shown": "banana", "source": "bad shown"},
        {"title_ids": ["00055D00"], "raw": 8, "shown": "1.0"}]})";
    std::vector<std::string> warnings;
    REQUIRE(VersionTable::Parse(online, nullptr, &warnings).has_value());
    REQUIRE(warnings.size() == 3);
    REQUIRE(SetOnlineVersions(online));
    REQUIRE(GameVersionOf(kOther, 5216) == Version{1, 5});
    REQUIRE(GameVersionOf(0x0004000000055E00ULL, 5216) == Version{1, 5});
    REQUIRE(GameVersionOf(kEchoesEu, 1024) == Version{1, 2});   // overridden
    REQUIRE(GameVersionOf(kMk7, 1040) == Version{1, 1});        // built in, still there
    REQUIRE(LookupVersion(kOther, 5216)->source == "test");
    REQUIRE_FALSE(SetOnlineVersions("{\"mods\": []}")); // no section: nothing changes
    REQUIRE(GameVersionOf(kOther, 5216) == Version{1, 5});
    REQUIRE_FALSE(SetOnlineVersions("not json"));
    ClearOnlineVersions();
    REQUIRE_FALSE(GameVersionOf(kOther, 5216).has_value());

    // every built-in entry has a source and a plain version
    for (const auto& e : BuiltInVersionTable().Entries()) {
        REQUIRE_FALSE(e.source.empty());
        REQUIRE(e.shown.major >= 1);
    }
}

TEST_CASE("Versions: no entry says nothing misleading", "[versions]") {
    ClearOnlineVersions();
    // an update the table does not know
    auto r = SelectInstalledVersion(kOther, 5216, std::nullopt, {}, std::nullopt);
    REQUIRE_FALSE(r.version.has_value());
    REQUIRE(r.unmapped);
    REQUIRE(r.Display() == "Update Installed");
    REQUIRE(r.State() == "has an update installed");
    REQUIRE(r.LogText().find("raw 5216 (5.6.0)") != std::string::npos);
    // a cartridge at revision 0 is the game as released, whatever its card info says: never "Rev 0"
    FileVersion first;
    first.card_version = 32;
    first.card_revision = 0;
    r = SelectInstalledVersion(kOther, std::nullopt, std::nullopt, first, std::nullopt);
    REQUIRE(r.version == Version{1, 0});
    REQUIRE_FALSE(r.unmapped);
    REQUIRE(r.Display() == "1.0");
    // ... but an installed update still decides
    r = SelectInstalledVersion(kOther, 5216, std::nullopt, first, std::nullopt);
    REQUIRE(r.unmapped);
    REQUIRE(r.Display() == "Update Installed");
    // a cartridge revision the table does not know
    FileVersion card;
    card.card_version = 48;
    card.card_revision = 1;
    r = SelectInstalledVersion(0x0004000000054000ULL, std::nullopt, std::nullopt, card, std::nullopt);
    REQUIRE(r.unmapped);
    REQUIRE(r.Display() == "Rev 1");
    REQUIRE(r.State() == "is Rev 1");
    REQUIRE(r.LogText().find("raw 48 (0.3.0)") != std::string::npos);
    // raw 0 with a later revision: not "1.0"
    card.card_version = 0;
    r = SelectInstalledVersion(kOther, std::nullopt, std::nullopt, card, std::nullopt);
    REQUIRE_FALSE(r.version.has_value());
    REQUIRE(r.Display() == "Rev 1");
    // a table entry gives the version
    r = SelectInstalledVersion(kEchoesEu, 1024, std::nullopt, {}, std::nullopt);
    REQUIRE(r.version == Version{1, 1});
    REQUIRE(r.Display() == "1.1");
    REQUIRE(r.State().empty());
    REQUIRE(r.LogText().find("raw 1024 (1.0.0)") != std::string::npos);
    REQUIRE(r.LogText().find("shows 1.1") != std::string::npos);
    // a CIA with a higher unmapped TMD
    FileVersion cia;
    cia.cia_tmd = 4096;
    REQUIRE(SelectInstalledVersion(kOther, std::nullopt, std::nullopt, cia, std::nullopt).Display() ==
            "Update Installed");
}

TEST_CASE("Versions: cheats match through the table only", "[versions]") {
    ClearOnlineVersions();
    auto entries = Parse("[All Karts, Game v1.0]\n00000000 00000001\n\n"
                         "[Manual Mode, Game v1.1 USA]\n00000000 00000002\n\n[C]\n00000000 00000003\n");
    // MK7 with 1040 (in the table): v1.1 matches, v1.0 does not
    const auto known = SelectInstalledVersion(kMk7, 1040, std::nullopt, {}, std::nullopt);
    REQUIRE(MarkOtherVersions(entries, known) == 1);
    REQUIRE(entries[0].comments == "Version note: for v1.0, installed v1.1\n");
    REQUIRE(entries[1].comments.empty());
    // an unmapped raw version: no tag is confirmed, an untagged cheat is fine
    entries = Parse("[A v1.0]\n00000000 00000001\n\n[B v1.5]\n00000000 00000002\n\n[C]\n00000000 00000003\n");
    const auto other = SelectInstalledVersion(kOther, 5216, std::nullopt, {}, std::nullopt);
    REQUIRE_FALSE(VersionMatches(Version{1, 5}, other));
    REQUIRE(VersionMatches(std::nullopt, other));
    REQUIRE(MarkOtherVersions(entries, other) == 2);
    REQUIRE(entries[0].comments == "Version note: for v1.0, this game has an update installed\n");
    REQUIRE(entries[2].comments.empty());
    // nothing known at all: everything matches
    REQUIRE(VersionMatches(Version{1, 1}, SelectInstalledVersion(kOther, std::nullopt, std::nullopt, {}, std::nullopt)));
}
