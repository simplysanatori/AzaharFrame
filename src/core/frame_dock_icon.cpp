// Copyright Azahar for Frame contributors
// Licensed under GPLv2 or any later version
// Refer to the misc/licenses/gplv2.txt file included.

// AzaharFrame cpphelpers: see frame_dock_icon.h.

#include "core/frame_dock_icon.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <fcntl.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fmt/format.h>
#include <json.hpp>

namespace fs = std::filesystem;

namespace FrameDockIcon {

namespace {

using ojson = nlohmann::ordered_json;

constexpr const char* kId = "azaharframe"; // window class / Qt desktop file name / icon name
constexpr const char* kAliases[] = {"azahar"}; // the program name, in case the class falls back to it
constexpr const char* kName = "Azahar for Frame";
constexpr int kSizes[] = {64, 128, 256, 512};
constexpr const char* kManifest = "azaharframe.vrmanifest";

bool g_dry = false;

void Say(const std::string& msg) {
    std::printf("%s%s\n", g_dry ? "[dry run] would " : "", msg.c_str());
}

std::string Home() {
    const char* h = std::getenv("HOME");
    return h != nullptr ? h : "";
}

std::string DataHome() {
    const char* x = std::getenv("XDG_DATA_HOME");
    return x != nullptr && *x != '\0' ? x : Home() + "/.local/share";
}

bool Read(const std::string& path, std::string* out) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        return false;
    }
    out->assign((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    return true;
}

/// Writes through a temporary file next to it and a rename.
bool WriteAtomic(const std::string& path, const std::string& data) {
    std::error_code ec;
    fs::create_directories(fs::path(path).parent_path(), ec);
    const std::string tmp = path + ".new" + std::to_string(getpid());
    {
        std::ofstream out(tmp, std::ios::binary);
        out.write(data.data(), static_cast<std::streamsize>(data.size()));
        if (!out) {
            fs::remove(tmp, ec);
            return false;
        }
    }
    fs::rename(tmp, path, ec);
    if (ec) {
        fs::remove(tmp, ec);
        return false;
    }
    return true;
}

bool WriteIfChanged(const std::string& path, const std::string& data) {
    std::string old;
    if (Read(path, &old) && old == data) {
        return false;
    }
    Say(fmt::format("write {} ({} bytes)", path, data.size()));
    if (!g_dry && !WriteAtomic(path, data)) {
        std::fprintf(stderr, "dock icon: could not write %s\n", path.c_str());
    }
    return true;
}

bool VrServerRunning() {
    if (const char* t = std::getenv("AZAHARFRAME_VRSERVER_RUNNING")) {
        return std::strcmp(t, "1") == 0;
    }
    return std::system("pgrep -x vrserver > /dev/null 2>&1") == 0;
}

std::string AppConfigPath() {
    return Home() + "/.config/openvr/config/appconfig.json";
}

bool AppConfigEdit(const std::string& add, const std::string& drop) {
    const std::string path = AppConfigPath();
    std::string text;
    Read(path, &text);
    bool changed = false;
    const std::string out = EditAppConfig(text, add, drop, &changed);
    if (!changed) {
        return false;
    }
    Say(fmt::format("set manifest_paths in {}", path));
    if (!g_dry && !WriteAtomic(path, out)) { // L10: never rewritten in place
        std::fprintf(stderr, "dock icon: could not write %s\n", path.c_str());
    }
    return true;
}

/// Runs vrcmd --appmanifest MANIFEST (LD_LIBRARY_PATH out of its environment), exit status.
int RunVrcmd(const std::string& vrcmd, const std::string& manifest) {
    const pid_t pid = fork();
    if (pid < 0) {
        return -1;
    }
    if (pid == 0) {
        unsetenv("LD_LIBRARY_PATH");
        const int null = open("/dev/null", O_WRONLY);
        if (null >= 0) {
            dup2(null, 1);
            dup2(null, 2);
        }
        execl(vrcmd.c_str(), vrcmd.c_str(), "--appmanifest", manifest.c_str(), static_cast<char*>(nullptr));
        _exit(127);
    }
    int status = 0;
    waitpid(pid, &status, 0);
    return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
}

} // namespace

std::string DesktopText(unsigned long long appid) {
    const unsigned long long gameid = (appid << 32) | 0x02000000ull;
    return fmt::format("[Desktop Entry]\n"
                       "Name={}\n"
                       "Comment=Azahar 3DS emulator in VR, native on the Steam Frame, through its Steam entry\n"
                       "Exec=steam steam://rungameid/{}\n"
                       "Icon={}\n"
                       "StartupWMClass={}\n"
                       "Terminal=false\n"
                       "Type=Application\n"
                       "Categories=Game;Emulator;\n"
                       "NoDisplay=true\n",
                       kName, gameid, kId, kId);
}

std::string ManifestText(const std::string& dest, unsigned long long appid, const std::string& art_src) {
    const std::string launcher = dest + "/azaharframe.sh";
    const std::string art = dest + "/art";
    ojson app = ojson::object();
    app["app_key"] = fmt::format("steam.app.{}", appid);
    app["launch_type"] = "binary";
    app["binary_path_linux"] = launcher;
    app["binary_path_linux_arm"] = launcher;
    app["working_directory"] = dest;
    app["image_path"] = art + "/icon.png";
    app["is_internal"] = true;
    app["strings"] = {{"en_us", {{"name", kName}}}};
    std::error_code ec;
    if (fs::exists(art_src + "/portrait.png", ec)) {
        app["image_path_capsule"] = art + "/portrait.png";
    }
    ojson root = ojson::object();
    root["source"] = "builtin";
    root["applications"] = ojson::array({app});
    return root.dump(2, ' ', true) + "\n";
}

std::string EditAppConfig(const std::string& text, const std::string& add, const std::string& drop,
                          bool* changed) {
    ojson cfg = ojson::parse(text, nullptr, false);
    if (cfg.is_discarded() || !cfg.is_object()) {
        cfg = ojson::object();
    }
    std::vector<std::string> before;
    if (cfg.contains("manifest_paths") && cfg["manifest_paths"].is_array()) {
        for (const auto& p : cfg["manifest_paths"]) {
            if (p.is_string()) {
                before.push_back(p.get<std::string>());
            }
        }
    }
    std::vector<std::string> paths;
    for (const auto& p : before) {
        if (drop.empty() || p != drop) {
            paths.push_back(p);
        }
    }
    if (!add.empty() && std::find(paths.begin(), paths.end(), add) == paths.end()) {
        paths.push_back(add);
    }
    *changed = paths != before;
    if (!*changed) {
        return text;
    }
    cfg["manifest_paths"] = paths;
    return cfg.dump(3, ' ', true);
}

namespace {

int Install(std::string dest, unsigned long long appid, const std::string& art_src) {
    int n = 0;
    for (const int s : kSizes) {
        const std::string src = art_src + "/" + (s == 512 ? std::string("icon.png") : fmt::format("icon-{}.png", s));
        std::string data;
        if (!Read(src, &data)) {
            std::printf("dock icon: %s is missing, skipped\n", src.c_str());
            continue;
        }
        std::vector<std::string> names{kId};
        names.insert(names.end(), std::begin(kAliases), std::end(kAliases));
        for (const auto& name : names) {
            n += WriteIfChanged(fmt::format("{}/icons/hicolor/{}x{}/apps/{}.png", DataHome(), s, s, name), data);
        }
    }
    n += WriteIfChanged(DataHome() + "/applications/" + kId + ".desktop", DesktopText(appid));
    std::vector<std::string> out;
    out.push_back(fmt::format("{} icon/desktop file(s) {} ({}.png and {}.png in hicolor, {}.desktop)", n,
                              g_dry ? "to update" : "updated", kId, kAliases[0], kId));
    const std::string man = dest + "/" + kManifest;
    const bool changed = WriteIfChanged(man, ManifestText(dest, appid, art_src));
    const char* env = std::getenv("AZAHARFRAME_VRCMD");
    const std::string vrcmd = env != nullptr && *env != '\0' ? env : "/opt/steamvr/bin/linuxarm64/vrcmd";
    std::error_code ec;
    if (fs::exists(vrcmd, ec) && VrServerRunning()) {
        Say(fmt::format("run {} --appmanifest {}", vrcmd, man));
        if (!g_dry) {
            const int rc = RunVrcmd(vrcmd, man);
            out.push_back(rc == 0 ? "SteamVR app manifest registered with the running SteamVR"
                                  : "SteamVR app manifest written; vrcmd could not register it (it loads at "
                                    "the next SteamVR start)");
            if (rc != 0) {
                AppConfigEdit(man, "");
            }
        }
    } else {
        const bool added = AppConfigEdit(man, "");
        out.push_back(std::string("SteamVR app manifest ") + (changed ? "written" : "unchanged") +
                      (added ? "; added to SteamVR's appconfig.json" : "") +
                      " (SteamVR is not running: it is read at the next SteamVR start)");
    }
    std::string joined;
    for (const auto& o : out) {
        joined += (joined.empty() ? "" : "; ") + o;
    }
    std::printf("dock icon (steam.app.%llu): %s\n", appid, joined.c_str());
    return 0;
}

int Remove(const std::string& dest) {
    std::error_code ec;
    for (const int s : kSizes) {
        std::vector<std::string> names{kId};
        names.insert(names.end(), std::begin(kAliases), std::end(kAliases));
        for (const auto& name : names) {
            const std::string p = fmt::format("{}/icons/hicolor/{}x{}/apps/{}.png", DataHome(), s, s, name);
            if (fs::exists(p, ec)) {
                Say("remove " + p);
                if (!g_dry) {
                    fs::remove(p, ec);
                }
            }
        }
    }
    const std::string desktop = DataHome() + "/applications/" + kId + ".desktop";
    if (fs::exists(desktop, ec)) {
        Say("remove " + desktop);
        if (!g_dry) {
            fs::remove(desktop, ec);
        }
    }
    const std::string man = dest + "/" + kManifest;
    if (!VrServerRunning()) {
        AppConfigEdit("", man);
    }
    if (fs::exists(man, ec)) {
        Say("remove " + man);
        if (!g_dry) {
            fs::remove(man, ec);
        }
    }
    std::printf("dock icon: files removed (SteamVR forgets the manifest at its next start)\n");
    return 0;
}

} // namespace

int RunCli(const std::vector<std::string>& raw) {
    std::vector<std::string> args;
    for (const auto& a : raw) {
        if (a == "--dry-run") {
            g_dry = true;
        } else {
            args.push_back(a);
        }
    }
    if (args.size() < 2 || (args[0] != "install" && args[0] != "remove")) {
        std::fprintf(stderr, "usage: --frame-dock-icon install DEST --appid N [--art DIR] [--dry-run] | "
                             "remove DEST [--dry-run]\n");
        return 2;
    }
    std::string dest = args[1];
    while (dest.size() > 1 && dest.back() == '/') {
        dest.pop_back();
    }
    std::string art_src = dest + "/art";
    std::string appid_text;
    for (std::size_t i = 2; i + 1 < args.size(); ++i) {
        if (args[i] == "--appid") {
            appid_text = args[i + 1];
        } else if (args[i] == "--art") {
            art_src = args[i + 1];
        }
    }
    if (args[0] == "install") {
        // digits only: it goes into a desktop file, a manifest and a Steam URL
        if (appid_text.empty() || appid_text.size() > 10 ||
            !std::all_of(appid_text.begin(), appid_text.end(), [](char c) { return c >= '0' && c <= '9'; })) {
            std::printf("dock icon: install needs --appid N (the Steam entry's appid)\n");
            return 2;
        }
        return Install(dest, std::strtoull(appid_text.c_str(), nullptr, 10), art_src);
    }
    return Remove(dest);
}

} // namespace FrameDockIcon
