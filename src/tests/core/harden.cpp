// Copyright Azahar for Frame contributors
// Licensed under GPLv2 or any later version
// Refer to the misc/licenses/gplv2.txt file included.

// AzaharFrame harden: the catalog schema check, the catalog fetch cap and its failures, the one retry of a
// bare 200, and the atomic small-file writer.

#include <filesystem>
#include <fstream>
#include <sstream>
#include <catch2/catch_test_macros.hpp>
#include "common/atomic_file.h"
#include "core/custom_tex/download.h"
#include "core/mods/catalog_schema.h"
#include "core/mods/crash_guard.h"
#include "core/mods/library.h"

namespace fs = std::filesystem;

namespace {

std::string Dir(const char* name) {
    const std::string dir = std::string("/tmp/azf_harden_test_") + name + "/";
    std::error_code ec;
    fs::remove_all(dir, ec);
    fs::create_directories(dir);
    return dir;
}

std::string Slurp(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    std::stringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

const char* kModsV1 = R"({"schema":1,"mods":[{"id":"only-one","name":"Only One","title_ids":["0004000000033500"],
  "author":"A","licence":"MIT","source":{"type":"owner_repo_path","path":"x/code.ips"}}]})";

} // namespace

TEST_CASE("Harden: the mod catalog's schema is checked", "[harden]") {
    std::string error;
    CHECK(Mods::ParseCatalog(kModsV1, &error).has_value());
    CHECK(Mods::ParseCatalog(R"({"mods":[]})", &error).has_value()); // no schema counts as 1
    const auto newer = Mods::ParseCatalog(R"({"schema":2,"mods":[]})", &error);
    CHECK_FALSE(newer.has_value());
    CHECK(Mods::SchemaTooNew(error));
    CHECK(error.find("2") != std::string::npos);
    error.clear();
    CHECK_FALSE(Mods::ParseCatalog(R"({"schema":"one","mods":[]})", &error).has_value());
    CHECK_FALSE(Mods::SchemaTooNew(error)); // a broken file, not a newer one
    CHECK_FALSE(error.empty());
    CHECK(Mods::DefaultCatalog().schema <= Mods::kSupportedSchema);
}

TEST_CASE("Harden: the texture catalog's schema is checked", "[harden]") {
    namespace Dl = CustomTex::Download;
    std::string error;
    CHECK(Dl::ParseCatalog(R"({"schema":1,"packs":[]})", &error).has_value());
    CHECK_FALSE(Dl::ParseCatalog(R"({"schema":7,"packs":[]})", &error).has_value());
    CHECK(Mods::SchemaTooNew(error));
    error.clear();
    CHECK_FALSE(Dl::ParseCatalog(R"({"schema":1.5,"packs":[]})", &error).has_value());
    CHECK_FALSE(Mods::SchemaTooNew(error));
}

TEST_CASE("Harden: a newer live mod catalog puts the built-in one back, once", "[harden]") {
    using namespace Mods;
    int fetches = 0;
    std::size_t cap = 0;
    std::string body = R"({"schema":2,"mods":[{"id":"future","name":"F"}]})";
    int status = 200;
    std::string net_error;
    auto mine = ParseCatalog(kModsV1);
    REQUIRE(mine);
    Library lib(Dir("schema"), *mine, [&](const std::string&, const std::string&, std::size_t max) {
        ++fetches;
        cap = max;
        HttpReply r;
        r.status = net_error.empty() ? status : 0;
        r.body = body;
        r.error = net_error;
        return r;
    });
    REQUIRE(lib.GetCatalog().mods.size() == 1);
    std::string error;
    CHECK_FALSE(lib.RefreshCatalog(&error));
    CHECK(SchemaTooNew(error));
    CHECK(cap >= 16u * 1024 * 1024);
    // the compiled-in copy, not the half-read live one
    CHECK(lib.GetCatalog().mods.size() == DefaultCatalog().mods.size());
    CHECK(lib.GetCatalog().Find("future") == nullptr);
    // not asked for again this run
    error.clear();
    CHECK_FALSE(lib.RefreshCatalog(&error));
    CHECK(SchemaTooNew(error));
    CHECK(fetches == 1);
}

TEST_CASE("Harden: a failed or cut mod catalog fetch keeps the list", "[harden]") {
    using namespace Mods;
    std::string body;
    int status = 200;
    std::string net_error;
    auto mine = ParseCatalog(kModsV1);
    REQUIRE(mine);
    Library lib(Dir("trunc"), *mine, [&](const std::string&, const std::string&, std::size_t) {
        HttpReply r;
        r.status = net_error.empty() ? status : 0;
        r.body = body;
        r.error = net_error;
        return r;
    });
    std::string error;
    SECTION("past the size cap") {
        net_error = "Download too large";
        CHECK_FALSE(lib.RefreshCatalog(&error));
        CHECK(error == "Download too large");
    }
    SECTION("the body ends in the middle of the JSON") {
        body = kModsV1;
        body.resize(body.size() / 2);
        CHECK_FALSE(lib.RefreshCatalog(&error));
        CHECK_FALSE(error.empty());
    }
    SECTION("a server error") {
        status = 503;
        CHECK_FALSE(lib.RefreshCatalog(&error));
    }
    SECTION("an empty list") {
        body = R"({"schema":1,"mods":[]})";
        CHECK_FALSE(lib.RefreshCatalog(&error));
    }
    CHECK(lib.GetCatalog().mods.size() == 1);
    CHECK(lib.GetCatalog().Find("only-one") != nullptr);
    // and a good one still replaces it afterwards
    net_error.clear();
    status = 200;
    body = R"({"schema":1,"mods":[{"id":"fresh","name":"Fresh","title_ids":["0004000000033500"],"author":"A",
      "licence":"MIT","source":{"type":"owner_repo_path","path":"x/code.ips"}}]})";
    CHECK(lib.RefreshCatalog(&error));
    CHECK(lib.GetCatalog().Find("fresh") != nullptr);
}

TEST_CASE("Harden: the texture catalog fetch keeps the list on every failure", "[harden]") {
    namespace Dl = CustomTex::Download;
    const char* mine = R"({"schema":1,"packs":[{"id":"mine","name":"Mine","title_ids":["0004000000033500"],
      "source":{"type":"https","url":"https://raw.githubusercontent.com/o/r/main/p.zip"}}]})";
    auto cat = Dl::ParseCatalog(mine);
    REQUIRE(cat);
    Dl::Downloader dl(Dir("texcat"), *cat);
    const auto count = [&] { return dl.ListForTitle(0x0004000000033500ull).size(); };
    REQUIRE(count() == 1);
    dl.ApplyFetchedCatalog(0, "Download too large", "");
    dl.ApplyFetchedCatalog(200, "", std::string(mine).substr(0, 60)); // cut off
    dl.ApplyFetchedCatalog(500, "", mine);
    dl.ApplyFetchedCatalog(200, "", R"({"schema":1,"packs":[]})");
    CHECK(count() == 1);
    // a newer format: the compiled-in list is back
    dl.ApplyFetchedCatalog(200, "", R"({"schema":2,"packs":[{"id":"future","name":"F"}]})");
    for (const auto& pack : dl.ListForTitle(0x0004000000033500ull)) {
        CHECK(pack.id != "mine");
        CHECK(pack.id != "future");
    }
    CHECK(Dl::kMaxCatalogBytes >= 8u * 1024 * 1024);
}

TEST_CASE("Harden: a bare 200 with no body is retried once", "[harden]") {
    using namespace Mods;
    const std::string dir = Dir("retry");
    auto parsed = ParseCatalog(R"({"schema":1,"mods":[{"id":"p","name":"P","title_ids":["0004000000033500"],
      "author":"A","licence":"MIT","source":{"type":"https","url":"https://raw.githubusercontent.com/o/r/main/p.zip"},
      "install":{"rules":[{"from":"*","to":"romfs/"}]}}]})");
    REQUIRE(parsed);
    int calls = 0;
    int empty_answers = 2; // how many times the host answers a bare 200
    Library lib(dir, *parsed, [](const std::string&, const std::string&, std::size_t) {
        HttpReply r;
        r.status = 404;
        return r;
    });
    lib.SetEmptyReplyRetryDelay(std::chrono::milliseconds(1));
    lib.SetFreeSpace([](const std::string&, u64* out) {
        *out = 1ull << 40;
        return true;
    });
    lib.SetFileTransport([&](const std::string&, const std::string&, const std::string& dest,
                             const std::atomic_bool*, const Multipart::Progress&, u64) {
        ++calls;
        Multipart::FileReply r;
        r.status = 200;
        if (calls > empty_answers) {
            std::ofstream(dest, std::ios::binary | std::ios::trunc) << "data";
            r.total = 4;
        } else {
            std::ofstream(dest, std::ios::binary | std::ios::trunc); // 200, nothing in the file
        }
        return r;
    });
    SECTION("the second answer has the file") {
        empty_answers = 1;
        const auto out = lib.Install(0x0004000000033500ull, "p");
        INFO(out.error);
        CHECK(calls == 2);
        CHECK(out.error.find("refused the download") == std::string::npos);
    }
    SECTION("the second answer is empty too: it is reported, after one retry per try") {
        empty_answers = 99;
        const auto out = lib.Install(0x0004000000033500ull, "p");
        CHECK(calls == 4); // the part is tried twice (multipart), each try asks twice
        CHECK(out.result == InstallResult::Error);
        CHECK(out.error.find("refused the download (200)") != std::string::npos);
    }
}

TEST_CASE("Harden: atomic small-file writes", "[harden]") {
    const std::string dir = Dir("atomic");
    const std::string path = dir + "state.json";
    REQUIRE(Common::WriteFileAtomic(path, "one"));
    CHECK(Slurp(path) == "one");
    REQUIRE(Common::WriteFileAtomic(path, std::string(100000, 'x')));
    CHECK(Slurp(path).size() == 100000);
    REQUIRE(Common::WriteFileAtomic(path, ""));
    CHECK(fs::exists(path));
    CHECK(Slurp(path).empty());
    // no temporary file is left behind
    int files = 0;
    for ([[maybe_unused]] const auto& e : fs::directory_iterator(dir)) {
        ++files;
    }
    CHECK(files == 1);
    // a folder that is not there: false, and the old file is untouched
    REQUIRE(Common::WriteFileAtomic(path, "keep"));
    CHECK_FALSE(Common::WriteFileAtomic(dir + "missing/state.json", "x"));
    CHECK(Slurp(path) == "keep");
    // a left-over temporary file from an interrupted write does not matter
    std::ofstream(path + ".tmp") << "half";
    REQUIRE(Common::WriteFileAtomic(path, "next"));
    CHECK(Slurp(path) == "next");
    CHECK_FALSE(fs::exists(path + ".tmp"));
    CHECK(Common::SyncDirectory(dir));
}

TEST_CASE("Harden: the crash-undo marker is written atomically", "[harden]") {
    using Mods::CrashGuard::Guard;
    using Mods::CrashGuard::Item;
    const std::string dir = Dir("guard");
    Guard g(dir);
    g.NoteOn(0x0004000000033500ull, Item{Item::Kind::Mod, "no-bloom", "No Bloom"});
    for ([[maybe_unused]] const auto& e : fs::recursive_directory_iterator(dir)) {
        CHECK(e.path().extension() != ".tmp");
    }
    Guard next(dir);
    CHECK(next.New(0x0004000000033500ull).size() == 1);
}
