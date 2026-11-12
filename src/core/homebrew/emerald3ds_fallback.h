// Copyright Azahar for Frame contributors
// Licensed under GPLv2 or any later version
// Refer to the misc/licenses/gplv2.txt file included.

// AzaharFrame emfallback: when a newer Emerald 3DS release changes its build recipe so that the C++
// builder (emerald3ds_build.h) cannot build it (Result::unsupported), and the player approves, the pack
// is built the way the project itself builds it: with the release's own Python builder from its
// Emerald3DS-WebPayload.zip (the package the Windows builder and the website's web builder run), under
// Python 3 inside bubblewrap. The sandbox has no network unless the optional follower sprites are to be
// downloaded, no access to the player's files except the ROM file (read only), the unpacked payload's zip
// (read only) and one writable work folder, and is killed when the program stops or the player cancels.
// What comes out is checked here (the pack's checksums, ABI and ROM, the game files' hashes) before it is
// installed, exactly as for a pack the C++ builder made.
//
// No Qt. Blocking: run off the GUI thread.

#pragma once

#include <map>
#include <string>
#include <vector>
#include "core/homebrew/emerald3ds_build.h"

namespace Homebrew::Emerald3ds::Fallback {

constexpr const char* kBwrapPath = "/usr/bin/bwrap";
constexpr const char* kPythonPath = "/usr/bin/python3";
constexpr int kDefaultTimeoutSeconds = 30 * 60;
/// The process exit code of the headless fallback build when the recipe is unsupported by it too is
/// the builder's own (1); a stop is 4.

// ---------------------------------------------------------------------------------------------
// What the program needs

struct Tools {
    std::string bwrap;
    std::string python;
    std::string missing; ///< "" when both are there; else what is missing, for the prompt
    bool Ok() const {
        return missing.empty();
    }
};

/// Bubblewrap and Python 3.11 or newer at their fixed system paths (nothing is searched in PATH).
Tools Detect(const std::string& bwrap = kBwrapPath, const std::string& python = kPythonPath);
/// "bubblewrap (bwrap)" / "Python 3.11 or newer" / both, as the prompt says it.
std::string MissingText(bool bwrap_missing, bool python_missing);

// ---------------------------------------------------------------------------------------------
// The sandbox

struct Sandbox {
    std::string bwrap = kBwrapPath;
    std::string python = kPythonPath;
    std::string launcher;    ///< host path of the launcher script (see LauncherSource())
    std::string rom;         ///< host path, bound read only
    std::string payload_zip; ///< host path, bound read only
    std::string work;        ///< host path, the only writable place (bound as /out)
    std::string language;    ///< "" = the ROM's own
    bool network = false;
};

/// The whole command line, no shell: bwrap's options, then python3 -I and the launcher's own.
std::vector<std::string> Argv(const Sandbox& sandbox);

/// The fixed launcher (Python) that unpacks the zip into the work folder and runs the release's
/// builder. It is ours, not the release's; the release's code is only imported from the unpacked zip.
const char* LauncherSource();

// ---------------------------------------------------------------------------------------------
// One output line of the launcher

struct Line {
    enum class Kind { Other, Progress, Result, Error } kind = Kind::Other;
    int permille = 0;     ///< Progress
    std::string text;     ///< Progress: the step; Result: the JSON; Error: the message
};
Line ParseLine(const std::string& line);

// ---------------------------------------------------------------------------------------------
// The run

struct Options {
    Build::Options build;                ///< payload_zip, rom, out_dir, language, cache_dir, no_network, progress, cancel
    std::string expected_abi;            ///< 8 hex digits (the manifest's), "" = only what the pack says itself
    std::string expected_3dsx_sha256;    ///< the manifest's hash of the language's Emerald3DS.3dsx, "" = not checked
    std::string expected_rom_sha1;       ///< the ROM's SHA-1 (hex), "" = not checked
    int timeout_seconds = kDefaultTimeoutSeconds;
    Tools tools;                         ///< "" = Detect()
};

/// Runs the release's builder in the sandbox and checks what it wrote into build.out_dir
/// (emerald3ds.pak, Emerald3DS.3dsx, Emerald3DS.smdh). Never leaves the ROM changed (it is read only).
Build::Result Run(const Options& options);

/// The checks of a finished build: "" when emerald3ds.pak verifies (header, index and payload CRCs),
/// carries the ABI (and the ROM's SHA-1 when given) and the .3dsx and .smdh exist (the .3dsx with its
/// expected hash when given); else one line.
std::string ValidateOutput(const std::string& out_dir, const std::string& expected_abi,
                           const std::string& expected_rom_sha1,
                           const std::string& expected_3dsx_sha256);

/// Copies the follower files of `from` (names of 64 hex digits) that `to` does not have yet.
int SeedCache(const std::string& from, const std::string& to);

// ---------------------------------------------------------------------------------------------
// The player's answer, remembered per release version

enum class Choice { Unasked, Allowed, Declined };

class Choices {
public:
    Choice Get(const std::string& tag) const;
    void Set(const std::string& tag, Choice choice); ///< Unasked forgets it
    std::string ToJson() const;
    static Choices FromJson(const std::string& json); ///< damaged text: nothing remembered
    bool operator==(const Choices& other) const {
        return map == other.map;
    }

private:
    std::map<std::string, Choice> map;
};

constexpr const char* kChoicesFile = "fallback-choices.json"; ///< in user/emerald3ds/
Choices LoadChoices(const std::string& path);
bool SaveChoices(const std::string& path, const Choices& choices);

// ---------------------------------------------------------------------------------------------
// The prompt's words (Title Case buttons, short sentences)

std::string PromptTitle(const std::string& tag);                 ///< "Emerald 3DS v0.6.0 Needs Its Own Builder"
std::string PromptText(bool network_used);                       ///< 2 to 3 short sentences
std::string UnavailableText(const std::string& tag, const std::string& missing);
constexpr const char* kButtonBuild = "Build With Emerald 3DS's Builder";
constexpr const char* kButtonNotNow = "Not Now";

} // namespace Homebrew::Emerald3ds::Fallback
