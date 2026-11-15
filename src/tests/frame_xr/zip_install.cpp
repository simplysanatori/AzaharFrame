// Copyright Azahar for Frame contributors
// Licensed under GPLv2 or any later version
// Refer to the misc/licenses/gplv2.txt file included.

// AzaharFrame zipinstall: Install from Zip (core/mods/zip_install.h): what an archive is, which title
// it is for, and installing small synthetic archives (stored zips written here, a 7z through bsdtar)
// into the texture pack library and the mod library. No network.

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <map>
#include <sstream>
#include <unistd.h>
#include <catch2/catch_test_macros.hpp>
#include "core/custom_tex/download.h"
#include "core/custom_tex/packs.h"
#include "core/mods/archive.h"
#include "core/mods/library.h"
#include "core/mods/zip_install.h"

namespace fs = std::filesystem;
using namespace Mods::Zip;
using V = std::vector<std::string>;

namespace {

constexpr u64 kOot = 0x0004000000033500ULL; // the running game in these tests
constexpr u64 kOotEu = 0x0004000000033600ULL;
const std::string kOotText = "0004000000033500";

struct Temp {
    fs::path root;
    Temp() {
        static int n = 0;
        root = fs::temp_directory_path() /
               ("azf_zipinst_" + std::to_string(::getpid()) + "_" + std::to_string(n++));
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

void Le16(std::string& s, unsigned v) {
    s += static_cast<char>(v & 255);
    s += static_cast<char>((v >> 8) & 255);
}
void Le32(std::string& s, unsigned v) {
    Le16(s, v & 0xffff);
    Le16(s, v >> 16);
}

/// A zip of stored members; `modes` gives some of them a Unix mode (a link: 0120777).
std::string StoredZip(const std::vector<std::pair<std::string, std::string>>& members,
                      const std::map<std::string, unsigned>& modes = {}) {
    std::string out, central;
    for (const auto& [name, data] : members) {
        const unsigned offset = static_cast<unsigned>(out.size());
        const unsigned crc = Mods::Crc32(data);
        const unsigned size = static_cast<unsigned>(data.size());
        out += "PK\x03\x04";
        Le16(out, 20), Le16(out, 0), Le16(out, 0), Le16(out, 0), Le16(out, 0x21);
        Le32(out, crc), Le32(out, size), Le32(out, size);
        Le16(out, static_cast<unsigned>(name.size())), Le16(out, 0);
        out += name + data;
        const auto mode = modes.find(name);
        central += "PK\x01\x02";
        Le16(central, mode != modes.end() ? (3u << 8) | 20u : 20u), Le16(central, 20), Le16(central, 0),
            Le16(central, 0), Le16(central, 0), Le16(central, 0x21);
        Le32(central, crc), Le32(central, size), Le32(central, size);
        Le16(central, static_cast<unsigned>(name.size())), Le16(central, 0), Le16(central, 0),
            Le16(central, 0);
        Le16(central, 0), Le32(central, mode != modes.end() ? mode->second << 16 : 0u),
            Le32(central, offset);
        central += name;
    }
    const unsigned cd_offset = static_cast<unsigned>(out.size());
    out += central;
    out += "PK\x05\x06";
    Le16(out, 0), Le16(out, 0), Le16(out, static_cast<unsigned>(members.size())),
        Le16(out, static_cast<unsigned>(members.size()));
    Le32(out, static_cast<unsigned>(central.size())), Le32(out, cd_offset), Le16(out, 0);
    return out;
}

void WriteFile(const fs::path& p, const std::string& data) {
    fs::create_directories(p.parent_path());
    std::ofstream(p, std::ios::binary) << data;
}

std::string ReadFile(const fs::path& p) {
    std::ifstream in(p, std::ios::binary);
    std::stringstream s;
    s << in.rdbuf();
    return s.str();
}

std::string Zip(const Temp& t, const std::string& name,
                const std::vector<std::pair<std::string, std::string>>& members,
                const std::map<std::string, unsigned>& modes = {}) {
    const std::string path = (t.root / "Downloads" / name).string();
    WriteFile(path, StoredZip(members, modes));
    return path;
}

Mods::Library MakeLibrary(const Temp& t) {
    return Mods::Library(t.load() + "/", Mods::Catalog{},
                         [](const std::string&, const std::string&, std::size_t) {
                             Mods::HttpReply r;
                             r.error = "no network in tests";
                             return r;
                         });
}

} // namespace

TEST_CASE("zipinstall: title ID folders", "[frame_xr][zipinstall]") {
    CHECK(ParseTitleFolder("0004000000033500") == kOot);
    CHECK(ParseTitleFolder("00040000000335aB") == 0x00040000000335ABULL);
    CHECK(ParseTitleFolder("0004000000033500x") == 0);
    CHECK(ParseTitleFolder("1234567890abcdef") == 0); // not a 3DS title
    CHECK(ParseTitleFolder("000400000003350g") == 0);
    CHECK(ParseTitleFolder("") == 0);
}

TEST_CASE("zipinstall: what an archive is", "[frame_xr][zipinstall]") {
    SECTION("texture packs") {
        auto d = Detect({"readme.txt", "Pack/tex1_8x8_ABCDEF0123456789_1.png"});
        CHECK(d.kind == Kind::TexturePack);
        CHECK(d.title_ids.empty());
        d = Detect({"load/textures/0004000000033500/tex1_8x8_ABCDEF0123456789_1.DDS"});
        CHECK(d.kind == Kind::TexturePack);
        CHECK(d.title_ids == std::vector<u64>{kOot});
        d = Detect({"Pack/pack.json"});
        CHECK(d.kind == Kind::TexturePack);
        d = Detect({"USA/0004000000033500/tex1_1x1_A_1.ktx", "EUR/0004000000033600/tex1_1x1_A_1.ktx"});
        CHECK(d.kind == Kind::TexturePack);
        CHECK(d.title_ids == std::vector<u64>{kOot, kOotEu});
    }
    SECTION("mods") {
        auto d = Detect({"code.ips"});
        CHECK(d.kind == Kind::Mod);
        d = Detect({"My Mod/romfs/actor/a.szs", "My Mod/readme.md"});
        CHECK(d.kind == Kind::Mod);
        CHECK(d.title_ids.empty());
        d = Detect({"luma/titles/0004000000033500/exefs/code.bin"});
        CHECK(d.kind == Kind::Mod);
        CHECK(d.title_ids == std::vector<u64>{kOot});
        d = Detect({"North-America/load/mods/0004000000033500/code.ips",
                    "Europe/load/mods/0004000000033600/code.ips"});
        CHECK(d.title_ids == std::vector<u64>{kOot, kOotEu});
        // a texture file inside romfs is part of the mod, not a pack
        d = Detect({"romfs/tex1_8x8_ABCDEF0123456789_1.png", "romfs/b.bin"});
        CHECK(d.kind == Kind::Mod);
        // a title ID-like name inside romfs is not the title
        d = Detect({"romfs/0004000000033600/a.bin"});
        CHECK(d.title_ids.empty());
    }
    SECTION("refused") {
        auto d = Detect({"readme.txt", "image.png", "notes/tex.png"});
        CHECK(d.kind == Kind::Unknown);
        CHECK(d.reason == "Not a Texture Pack or Mod");
        d = Detect({"__MACOSX/romfs/._a.bin", "__MACOSX/._tex1_8x8_A_1.png"});
        CHECK(d.kind == Kind::Unknown);
        d = Detect({"Pack/tex1_8x8_ABCDEF0123456789_1.png", "Mod/romfs/a.bin"});
        CHECK(d.kind == Kind::Unknown);
        CHECK(d.reason == "Texture Pack and Mod in One Zip");
        CHECK(Detect({}).kind == Kind::Unknown);
        CHECK(Detect({}).reason == "Empty Zip");
        CHECK(Detect({"__MACOSX/._a"}).reason == "Empty Zip");
    }
}

TEST_CASE("zipinstall: which title an archive is for", "[frame_xr][zipinstall]") {
    // no title ID folder: the running game, or ask when there is none
    CHECK(ResolveTitle({}, kOot).title_id == kOot);
    CHECK(ResolveTitle({}, 0).title_id == 0);
    CHECK(ResolveTitle({}, 0).candidates.empty());
    // one folder: that title, whatever runs
    CHECK(ResolveTitle({kOotEu}, kOot).title_id == kOotEu);
    CHECK(ResolveTitle({kOotEu}, 0).title_id == kOotEu);
    // several: the running game's when it is one of them, else ask
    CHECK(ResolveTitle({kOot, kOotEu}, kOot).title_id == kOot);
    CHECK(ResolveTitle({kOot, kOotEu}, kOotEu).title_id == kOotEu);
    const auto ask = ResolveTitle({kOot, kOotEu}, 0x0004000000125500ULL);
    CHECK(ask.title_id == 0);
    CHECK(ask.candidates == std::vector<u64>{kOot, kOotEu});

    const V files{"USA/0004000000033500/tex1_1x1_A_1.png", "EUR/0004000000033600/tex1_1x1_A_1.png",
                  "readme.txt"};
    const Detected d = Detect(files);
    CHECK(ContentRoot(files, d, kOot) == "USA/0004000000033500");
    CHECK(ContentRoot(files, d, kOotEu) == "EUR/0004000000033600");
    CHECK(ContentRoot({"Pack/tex1_1x1_A_1.png"}, Detect({"Pack/tex1_1x1_A_1.png"}), kOot).empty());
}

TEST_CASE("zipinstall: names", "[frame_xr][zipinstall]") {
    CHECK(NameFromArchive("/home/u/Downloads/Ocarina 4K.zip") == "Ocarina 4K");
    CHECK(NameFromArchive("x/ReFramed_v2.7Z") == "ReFramed_v2");
    CHECK(NameFromArchive("x/.zip") == ".zip");
    CHECK(ModId("Ocarina ReFramed (Single Screen)") == "zip-ocarina-reframed-single-screen");
    CHECK(ModId("___") == "zip-mod");
    CHECK(ModId(std::string(200, 'a')).size() == 64);
    CHECK(ShortError("The download is not a zip, 7z or rar archive") == "Not a Zip or 7z File");
    CHECK(ShortError("Not enough free space: 3.2 GB needed, 1.1 GB free") == "Not Enough Space");
    CHECK(ShortError(kNotPackOrMod) == "Not a Texture Pack or Mod");
    CHECK(ShortError("7z needs bsdtar, which wasn't found") == "Unsupported Archive");
}

TEST_CASE("zipinstall: a texture pack from a zip", "[frame_xr][zipinstall]") {
    Temp t;
    const fs::path packs = fs::path(t.load()) / "texture-packs" / kOotText;
    const std::string zip =
        Zip(t, "OoT 4K.zip",
            {{"OoT 4K/load/textures/0004000000033500/pack.json", R"({"name": "OoT HD", "author": "Me"})"},
             {"OoT 4K/load/textures/0004000000033500/a/tex1_8x8_ABCDEF0123456789_1.png", "png"},
             {"OoT 4K/load/textures/0004000000033600/a/tex1_8x8_ABCDEF0123456789_1.png", "eu"},
             {"OoT 4K/readme.txt", "hi"}});
    Job job;
    REQUIRE(Open(zip, t.load(), job).empty());
    REQUIRE(job.detected.kind == Kind::TexturePack);
    REQUIRE(ResolveTitle(job.detected.title_ids, kOot).title_id == kOot);
    auto out = InstallTextures(job, kOot, t.load(), false);
    REQUIRE(out.result == Outcome::Result::Ok);
    CHECK(out.installed == "OoT HD");
    CHECK(ReadFile(packs / "OoT HD" / "a" / "tex1_8x8_ABCDEF0123456789_1.png") == "png");
    CHECK_FALSE(fs::exists(packs / "OoT HD" / "readme.txt"));
    CHECK(CustomTex::Download::MarkerId((packs / "OoT HD").string()) == std::optional<std::string>("zip:OoT HD"));
    Close(job);
    CHECK_FALSE(fs::exists(fs::path(t.load()) / "zip-install"));

    // the pack list shows it
    CustomTex::Paths paths;
    paths.load_dir = t.load();
    const auto listed = CustomTex::ListPacks(paths, kOot);
    REQUIRE(listed.size() == 1);
    CHECK(listed[0].name == "OoT HD");
    CHECK(listed[0].managed);

    // again: a question first, then replaced
    REQUIRE(Open(zip, t.load(), job).empty());
    out = InstallTextures(job, kOot, t.load(), false);
    CHECK(out.result == Outcome::Result::Conflict);
    CHECK(out.conflict_name == "OoT HD");
    CHECK(ReadFile(packs / "OoT HD" / "a" / "tex1_8x8_ABCDEF0123456789_1.png") == "png");
    WriteFile(packs / "OoT HD" / "old.png", "old");
    out = InstallTextures(job, kOot, t.load(), true);
    REQUIRE(out.result == Outcome::Result::Ok);
    CHECK_FALSE(fs::exists(packs / "OoT HD" / "old.png"));
    Close(job);

    // a deletable pack, as Download Packs' are
    CustomTex::Download::Downloader dl(t.load(), {});
    CHECK(dl.Delete(kOot, "OoT HD").ok);
    CHECK_FALSE(fs::exists(packs / "OoT HD"));
}

TEST_CASE("zipinstall: loose textures go to the running game, the player's folder is kept",
          "[frame_xr][zipinstall]") {
    Temp t;
    const fs::path packs = fs::path(t.load()) / "texture-packs" / kOotText;
    WriteFile(packs / "Zelda Pack" / "mine.png", "mine");
    const std::string zip = Zip(t, "Zelda Pack.zip", {{"tex1_8x8_ABCDEF0123456789_1.png", "png"}});
    Job job;
    REQUIRE(Open(zip, t.load(), job).empty());
    REQUIRE(job.detected.title_ids.empty());
    const auto out = InstallTextures(job, ResolveTitle(job.detected.title_ids, kOot).title_id, t.load(), false);
    REQUIRE(out.result == Outcome::Result::Ok);
    CHECK(out.installed == "Zelda Pack (2)");
    CHECK(ReadFile(packs / "Zelda Pack" / "mine.png") == "mine");
    CHECK(fs::exists(packs / "Zelda Pack (2)" / "tex1_8x8_ABCDEF0123456789_1.png"));
    Close(job);
}

TEST_CASE("zipinstall: a mod from a zip", "[frame_xr][zipinstall]") {
    Temp t;
    auto library = MakeLibrary(t);
    const fs::path mods = fs::path(t.load()) / "mods" / kOotText;
    const std::string zip =
        Zip(t, "OoT Restored.zip",
            {{"README.txt", "r"},
             {"North-America/load/mods/0004000000033500/code.ips", "PATCH-NA"},
             {"North-America/load/mods/0004000000033500/romfs/menu/a.ctxb", "ctxb"},
             {"Europe/load/mods/0004000000033600/code.ips", "PATCH-EU"}});
    Job job;
    REQUIRE(Open(zip, t.load(), job).empty());
    REQUIRE(job.detected.kind == Kind::Mod);
    REQUIRE(job.detected.title_ids == std::vector<u64>{kOot, kOotEu});
    auto out = InstallMod(job, kOot, library, false);
    REQUIRE(out.result == Outcome::Result::Ok);
    CHECK(out.installed == "zip-oot-restored");
    CHECK(ReadFile(mods / "code.ips") == "PATCH-NA");
    CHECK(ReadFile(mods / "romfs" / "menu" / "a.ctxb") == "ctxb");

    // listed as a managed mod that can be switched off and removed
    auto loaded = library.ListLoaded(kOot);
    REQUIRE(loaded.size() == 1);
    CHECK(loaded[0].id == "zip-oot-restored");
    CHECK(loaded[0].name == "OoT Restored");
    CHECK(loaded[0].managed);
    CHECK(loaded[0].source == "zip");
    CHECK(loaded[0].state == Mods::ModState::Active);
    REQUIRE(library.Deactivate(kOot, "zip-oot-restored").ok);
    CHECK_FALSE(fs::exists(mods / "code.ips"));
    REQUIRE(library.Install(kOot, "zip-oot-restored").result == Mods::InstallResult::Ok); // switch on
    CHECK(ReadFile(mods / "code.ips") == "PATCH-NA");

    // the same zip again: asks, then replaces
    out = InstallMod(job, kOot, library, false);
    CHECK(out.result == Outcome::Result::Conflict);
    CHECK(out.conflict_id == "zip-oot-restored");
    REQUIRE(InstallMod(job, kOot, library, true).result == Outcome::Result::Ok);
    CHECK(library.ListLoaded(kOot).size() == 1);
    Close(job);

    // another mod with a code patch: a question, then Install(id, true) replaces from the library
    const std::string other = Zip(t, "Other Patch.zip", {{"code.ips", "PATCH-OTHER"}});
    REQUIRE(Open(other, t.load(), job).empty());
    out = InstallMod(job, kOot, library, false);
    CHECK(out.result == Outcome::Result::Conflict);
    CHECK(out.conflict_id == "zip-oot-restored");
    CHECK(out.installed == "zip-other-patch");
    CHECK(ReadFile(mods / "code.ips") == "PATCH-NA");
    Close(job);
    REQUIRE(library.Install(kOot, "zip-other-patch", true).result == Mods::InstallResult::Ok);
    CHECK(ReadFile(mods / "code.ips") == "PATCH-OTHER");
    CHECK_FALSE(fs::exists(mods / "romfs" / "menu" / "a.ctxb"));

    REQUIRE(library.Remove(kOot, "zip-other-patch").ok);
    REQUIRE(library.Remove(kOot, "zip-oot-restored").ok);
    CHECK(library.ListLoaded(kOot).empty());

    // the player's own code.ips: blocked, nothing written
    WriteFile(mods / "code.ips", "PATCH-MINE");
    REQUIRE(Open(other, t.load(), job).empty());
    CHECK(InstallMod(job, kOot, library, false).result == Outcome::Result::Blocked);
    CHECK(ReadFile(mods / "code.ips") == "PATCH-MINE");
    Close(job);

    // a code.ips that is not a patch
    const std::string bad = Zip(t, "Bad.zip", {{"code.ips", "nope!"}});
    REQUIRE(Open(bad, t.load(), job).empty());
    CHECK(InstallMod(job, kOot, library, false).error == "Not a valid code patch");
    Close(job);
}

TEST_CASE("zipinstall: what cannot be installed", "[frame_xr][zipinstall]") {
    Temp t;
    Job job;
    const std::string junk = (t.root / "Downloads" / "junk.zip").string();
    WriteFile(junk, "not an archive");
    CHECK(ShortError(Open(junk, t.load(), job)) == "Not a Zip or 7z File");
    CHECK_FALSE(fs::exists(fs::path(t.load()) / "zip-install"));
    CHECK(ShortError(Open((t.root / "missing.zip").string(), t.load(), job)) == "Can't Read the File");

    const std::string photos = Zip(t, "photos.zip", {{"a.jpg", "jpg"}, {"b/readme.txt", "x"}});
    REQUIRE(Open(photos, t.load(), job).empty());
    CHECK(job.detected.kind == Kind::Unknown);
    CHECK(job.detected.reason == "Not a Texture Pack or Mod");
    auto library = MakeLibrary(t);
    CHECK(InstallTextures(job, kOot, t.load(), false).result == Outcome::Result::Error);
    CHECK(InstallMod(job, kOot, library, false).result == Outcome::Result::Error);
    Close(job);
    CHECK_FALSE(fs::exists(fs::path(t.load()) / "texture-packs"));
    CHECK_FALSE(fs::exists(fs::path(t.load()) / "mods"));
}

TEST_CASE("zipinstall: a 7z through bsdtar", "[frame_xr][zipinstall]") {
    const std::string tool = Mods::FindBsdtar();
    if (tool.empty()) {
        SKIP("no bsdtar");
    }
    Temp t;
    const fs::path src = t.root / "src";
    WriteFile(src / "Mod" / "0004000000033500" / "romfs" / "a.bin", "a");
    WriteFile(src / "Mod" / "0004000000033500" / "exefs" / "code.bps", "BPS1xyz");
    fs::create_directories(t.root / "Downloads");
    const std::string archive = (t.root / "Downloads" / "Seven.7z").string();
    const std::string cmd = tool + " --format 7zip -cf '" + archive + "' -C '" + src.string() + "' Mod";
    if (std::system(cmd.c_str()) != 0) {
        SKIP("bsdtar cannot write 7z here");
    }
    Job job;
    REQUIRE(Open(archive, t.load(), job).empty());
    REQUIRE(job.detected.kind == Kind::Mod);
    auto library = MakeLibrary(t);
    REQUIRE(InstallMod(job, kOot, library, false).result == Outcome::Result::Ok);
    const fs::path mods = fs::path(t.load()) / "mods" / kOotText;
    CHECK(ReadFile(mods / "romfs" / "a.bin") == "a");
    CHECK(ReadFile(mods / "exefs" / "code.bps") == "BPS1xyz");
    Close(job);
}

// ---- Release gate: zip slip and wrong layouts ----

namespace {

/// Nothing installed and nothing left in the work folder.
void CheckNothingLeft(const Temp& t) {
    const fs::path load = t.load();
    CHECK_FALSE(fs::exists(load / "texture-packs"));
    CHECK_FALSE(fs::exists(load / "mods"));
    CHECK_FALSE(fs::exists(load / "mod-library"));
    CHECK_FALSE(fs::exists(load / "zip-install"));
}

/// No file named `name` anywhere under the temp root but inside Downloads (where the archives are).
bool Escaped(const Temp& t, const std::string& name) {
    std::error_code ec;
    for (fs::recursive_directory_iterator it(t.root, ec), end; !ec && it != end; it.increment(ec)) {
        if (it->path().filename() == name &&
            it->path().parent_path() != t.root / "Downloads") {
            return true;
        }
    }
    return fs::exists(t.root.parent_path() / name) || fs::exists("/tmp/" + name);
}

} // namespace

TEST_CASE("zipinstall: zip slip is refused (zip)", "[frame_xr][zipinstall]") {
    // the built-in reader
    for (const char* bad : {"../zs-evil.txt", "/tmp/zs-evil.txt", "a/../../zs-evil.txt", "C:/zs-evil.txt"}) {
        const auto r = Mods::ExtractZip(StoredZip({{"romfs/a.bin", "a"}, {bad, "x"}}));
        CHECK(r.error == "The archive holds an unsafe file path");
        CHECK(r.files.empty());
    }
    const auto link = Mods::ExtractZip(StoredZip({{"romfs/a.bin", "a"}, {"romfs/l", "/etc"}},
                                                 {{"romfs/l", 0120777}}));
    CHECK(link.error == "The archive holds links or special files");

    // through Install from Zip (bsdtar when there is one, else the built-in reader)
    Temp t;
    Job job;
    for (const char* bad : {"../../../zs-evil.txt", "/tmp/zs-evil.txt", "romfs/../../../../zs-evil.txt"}) {
        const std::string zip = Zip(t, "slip.zip", {{"romfs/a.bin", "a"}, {bad, "x"}});
        const std::string error = Open(zip, t.load(), job);
        INFO(bad);
        CHECK(ShortError(error) == "Unsafe Archive");
        CHECK_FALSE(Escaped(t, "zs-evil.txt"));
        CheckNothingLeft(t);
    }
    const std::string linked =
        Zip(t, "link.zip", {{"romfs/a.bin", "a"}, {"romfs/l", "../../../../../etc"}}, {{"romfs/l", 0120777}});
    CHECK(ShortError(Open(linked, t.load(), job)) == "Unsafe Archive");
    CheckNothingLeft(t);
}

TEST_CASE("zipinstall: zip slip is refused (7z)", "[frame_xr][zipinstall]") {
    const std::string tool = Mods::FindBsdtar();
    if (tool.empty()) {
        SKIP("no bsdtar");
    }
    Temp t;
    Job job;
    const fs::path src = t.root / "src";
    fs::create_directories(t.root / "Downloads");
    // a link pointing outside
    WriteFile(src / "Mod" / "romfs" / "a.bin", "a");
    fs::create_symlink("/etc", src / "Mod" / "romfs" / "outside");
    const std::string linked = (t.root / "Downloads" / "link.7z").string();
    std::string cmd = tool + " --format 7zip -cf '" + linked + "' -C '" + src.string() + "' Mod";
    if (std::system(cmd.c_str()) != 0) {
        SKIP("bsdtar cannot write 7z here");
    }
    CHECK(ShortError(Open(linked, t.load(), job)) == "Unsafe Archive");
    CheckNothingLeft(t);
    // a ".." path (written with -P and a rename, when this bsdtar allows it)
    fs::remove(src / "Mod" / "romfs" / "outside");
    WriteFile(src / "evil" / "zs-evil7.txt", "x");
    const std::string slip = (t.root / "Downloads" / "slip.7z").string();
    cmd = tool + " --format 7zip -P -s ',^evil,../../../evil,' -cf '" + slip + "' -C '" + src.string() +
          "' Mod evil 2>/dev/null";
    const std::string list = (t.root / "list.txt").string();
    if (std::system(cmd.c_str()) == 0 &&
        std::system((tool + " -tf '" + slip + "' > '" + list + "'").c_str()) == 0 &&
        ReadFile(list).find("../") != std::string::npos) {
        fs::remove_all(src / "evil");
        CHECK(ShortError(Open(slip, t.load(), job)) == "Unsafe Archive");
        CHECK_FALSE(Escaped(t, "zs-evil7.txt"));
        CheckNothingLeft(t);
    } else {
        WARN("bsdtar would not write a \"..\" path into a 7z; that case is covered by the zip test");
    }
}

TEST_CASE("zipinstall: wrong layouts are refused, nothing left behind", "[frame_xr][zipinstall]") {
    Temp t;
    Job job;
    auto library = MakeLibrary(t);
    const auto refused = [&](const std::string& zip) {
        const std::string error = Open(zip, t.load(), job);
        std::string why = error.empty() ? job.detected.reason : ShortError(error);
        INFO(error);
        if (error.empty()) {
            CHECK(job.detected.kind == Kind::Unknown);
            CHECK(InstallTextures(job, kOot, t.load(), false).result == Outcome::Result::Error);
            CHECK(InstallMod(job, kOot, library, false).result == Outcome::Result::Error);
        }
        Close(job);
        CheckNothingLeft(t);
        return why;
    };

    SECTION("an empty archive") {
        const std::string why = refused(Zip(t, "empty.zip", {}));
        CHECK(why == "Empty Zip");
    }
    SECTION("a zip inside the zip") {
        const std::string inner = StoredZip({{"tex1_8x8_ABCDEF0123456789_1.png", "png"}});
        CHECK(refused(Zip(t, "nested.zip", {{"Pack/OoT 4K.zip", inner}, {"readme.txt", "r"}})) ==
              "Zip Inside a Zip");
    }
    SECTION("a mod with romfs and a texture folder") {
        CHECK(refused(Zip(t, "both.zip", {{"Mod/romfs/a.bin", "a"},
                                          {"Textures/0004000000033500/tex1_8x8_ABCDEF0123456789_1.png",
                                           "png"}})) == "Texture Pack and Mod in One Zip");
    }
    SECTION("a texture pack with no title ID and no game") {
        const std::string zip = Zip(t, "loose.zip", {{"tex1_8x8_ABCDEF0123456789_1.png", "png"}});
        REQUIRE(Open(zip, t.load(), job).empty());
        REQUIRE(job.detected.kind == Kind::TexturePack);
        const TitleChoice c = ResolveTitle(job.detected.title_ids, 0);
        CHECK(c.title_id == 0);
        CHECK(c.candidates.empty()); // nothing to choose from: the player must pick the game
        const Outcome out = InstallTextures(job, c.title_id, t.load(), false);
        CHECK(out.result == Outcome::Result::Error);
        CHECK(ShortError(out.error) == "Choose a Game First");
        Close(job);
        CheckNothingLeft(t);
    }
    SECTION("an archive over the free space") {
        const std::string zip = Zip(t, "big.zip", {{"romfs/a.bin", std::string(4096, 'a')}});
        const std::string error = Open(zip, t.load(), job, nullptr, {}, u64{1024});
        CHECK(ShortError(error) == "Not Enough Space");
        CheckNothingLeft(t);
        // fits as an archive but not unpacked (deflated zeros, through bsdtar)
        const std::string tool = Mods::FindBsdtar();
        if (!tool.empty()) {
            WriteFile(t.root / "src" / "romfs" / "zeros.bin", std::string(8u << 20, '\0'));
            const std::string packed = (t.root / "Downloads" / "zeros.zip").string();
            const std::string cmd =
                tool + " --format zip -cf '" + packed + "' -C '" + (t.root / "src").string() + "' romfs";
            if (std::system(cmd.c_str()) == 0) {
                const u64 size = fs::file_size(packed);
                const u64 room = CustomTex::Download::RequiredSpace(size, size) + 1024;
                CHECK(ShortError(Open(packed, t.load(), job, nullptr, {}, room)) == "Not Enough Space");
                CheckNothingLeft(t);
            }
        }
    }
}

// ---- Download Packs: when a pack was updated, and the verified badge (AzaharFrame zipinstall) ----

TEST_CASE("texdl: update dates are parsed and shown", "[frame_xr][texdl]") {
    using namespace CustomTex::Download;
    CHECK(ParseIsoDate("2026-09-04") == std::optional<std::string>("2026-09-04"));
    CHECK(ParseIsoDate("2025-02-28T12:34:56Z") == std::optional<std::string>("2025-02-28")); // published_at
    CHECK(ParseIsoDate("2024-02-29") == std::optional<std::string>("2024-02-29"));
    CHECK_FALSE(ParseIsoDate("2025-02-29"));
    CHECK_FALSE(ParseIsoDate("2026-13-01"));
    CHECK_FALSE(ParseIsoDate("2026-09-31"));
    CHECK_FALSE(ParseIsoDate("26-09-04"));
    CHECK_FALSE(ParseIsoDate("2026/09/04"));
    CHECK_FALSE(ParseIsoDate("2026-09-04x"));
    CHECK_FALSE(ParseIsoDate(""));

    // Drive's folder listing: this year's dates without the year, older ones month first
    const std::string today = "2026-11-11";
    CHECK(ParseDriveDate("Sep 4", today) == std::optional<std::string>("2026-09-04"));
    CHECK(ParseDriveDate(" Jan 15 ", today) == std::optional<std::string>("2026-01-15"));
    CHECK(ParseDriveDate("3/10/25", today) == std::optional<std::string>("2025-03-10"));
    CHECK(ParseDriveDate("12/31/2024", today) == std::optional<std::string>("2024-12-31"));
    CHECK(ParseDriveDate("3:14 PM", today) == std::optional<std::string>("2026-11-11"));
    CHECK_FALSE(ParseDriveDate("13/1/25", today));
    CHECK_FALSE(ParseDriveDate("Sept 4", today));
    CHECK_FALSE(ParseDriveDate("Sep 31", today));
    CHECK_FALSE(ParseDriveDate("3/10/225", today));
    CHECK_FALSE(ParseDriveDate("", today));
    CHECK_FALSE(ParseDriveDate("Sep 4", "today"));

    CHECK(UpdatedText("2026-09-04") == "Updated Sep 2026");
    CHECK(UpdatedText("2021-05-06") == "Updated May 2021");
    CHECK(UpdatedText("2025-12-01") == "Updated Dec 2025");
    CHECK(UpdatedText("").empty());
    CHECK(UpdatedText("soon").empty());
    CHECK(ParseIsoDate(TodayIso()));

    // the listing's "last modified" column
    const auto items = ParseDriveFolder(
        R"(<div class="flip-entry" id="entry-1w1Ez_V2ky56Kc-Gz18cH3VnpraxiOD0Q"><a href="https://drive.google.com/file/d/x/view"><div class="flip-entry-title">OOT3D 4K.zip</div></a><div class="flip-entry-last-modified"><div>Sep 4</div></div></div>)"
        R"(<div class="flip-entry" id="entry-1wwEdr86qgwV4Erp2W_phcDu73QlBWgzo"><a href="https://drive.google.com/file/d/y/view"><div class="flip-entry-title">Old.zip</div></a><div class="flip-entry-last-modified"><div>3/10/25</div></div></div>)");
    REQUIRE(items.size() == 2);
    CHECK(items[0].modified == "Sep 4");
    CHECK(items[1].modified == "3/10/25");
}

TEST_CASE("texdl: the catalog's updated and verified fields", "[frame_xr][texdl]") {
    using namespace CustomTex::Download;
    const Catalog cat = DefaultCatalog();
    const Entry* henriko = cat.Find("oot3d-4k-henriko");
    REQUIRE(henriko);
    CHECK(henriko->verified);
    CHECK(henriko->updated == "2026-09-04");
    const Entry* dw = cat.Find("mm3d-hd-deathwrench");
    REQUIRE(dw);
    CHECK_FALSE(dw->verified);
    CHECK(dw->updated == "2021-05"); // catalog-refresh keeps months
    int verified = 0;
    for (const Entry& e : cat.packs) {
        CHECK(ParseIsoDate(e.updated.size() == 7 ? e.updated + "-01" : e.updated)); // every entry has a date (a month too)
        verified += e.verified ? 1 : 0;
        if (e.author == "Henriko Magnifico") {
            CHECK(e.verified); // every Henriko pack is verified; others may be too
        }
    }
    CHECK(verified >= 8);

    std::vector<std::string> warnings;
    const auto parsed = ParseCatalog(R"({"schema": 1, "packs": [
        {"id": "a", "name": "A", "title_ids": ["0004000000033500"], "updated": "2026-02-30",
         "verified": "yes", "source": {"type": "https", "url": "https://example.com/a.zip"}},
        {"id": "b", "name": "B", "title_ids": ["0004000000033500"], "updated": "2025-03-10",
         "verified": true, "source": {"type": "https", "url": "https://example.com/b.zip"}}]})",
                                     nullptr, &warnings);
    REQUIRE(parsed);
    REQUIRE(parsed->packs.size() == 2); // a bad date is dropped, not the entry
    CHECK(parsed->packs[0].updated.empty());
    CHECK_FALSE(parsed->packs[0].verified);
    CHECK(parsed->packs[1].updated == "2025-03-10");
    CHECK(parsed->packs[1].verified);
    CHECK(warnings.size() == 1);

    // an installed copy shows the date its source had when it was downloaded
    Temp t;
    Catalog mine;
    mine.packs.push_back(parsed->packs[1]);
    Downloader dl(t.load(), mine);
    auto list = dl.ListForTitle(kOot);
    REQUIRE(list.size() == 1);
    CHECK(list[0].updated == "2025-03-10");
    CHECK(list[0].verified);
    const fs::path folder = fs::path(t.load()) / "texture-packs" / kOotText / "B";
    fs::create_directories(folder);
    REQUIRE(WriteMarker(folder.string(), "b", "https", "2024-01-02"));
    list = dl.ListForTitle(kOot);
    CHECK(list[0].installed);
    CHECK(list[0].updated == "2024-01-02");
}
