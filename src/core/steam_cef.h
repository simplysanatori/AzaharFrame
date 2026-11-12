// Copyright Azahar for Frame contributors
// Licensed under GPLv2 or any later version
// Refer to the misc/licenses/gplv2.txt file included.

// AzaharFrame cpphelpers: talks to Steam's CEF debugging interface (HTTP /json, then a websocket to
// the SharedJSContext page) to run JavaScript in the Steam client: a shortcut's library art and
// icon, adding and removing the library entry, and where SteamVR shows the app's flat window. The
// C++ port of steam-frame/steam-cef.py; blocking, no Qt, plain sockets. Steam's port is 8080,
// AZAHARFRAME_CEF_PORT names another (for tests). Every string put into the JavaScript is JSON
// encoded, and an app id must be digits.

#pragma once

#include <optional>
#include <string>
#include <vector>
#include "common/common_types.h"

namespace SteamCef {

/// A JSON string literal ("..." with \u escapes for everything outside ASCII), safe to paste into
/// JavaScript.
std::string JsString(const std::string& text);

/// An app id: 1 to 10 digits (what Steam hands out is a 32 bit number).
bool ValidAppId(const std::string& text);

/// Steam's debugging port: AZAHARFRAME_CEF_PORT or 8080.
int Port();

struct Reply {
    bool ok = false;
    std::string json; ///< the value, as JSON ("null" when there is none)
    std::string error;
};

/// Runs `expression` (a promise is awaited) in the page titled `title`.
Reply Evaluate(const std::string& expression, int port, const std::string& title = "SharedJSContext",
               int timeout_s = 30);

// The same websocket and HTTP code is tested against a local stand-in; these are its pieces.
/// One masked client text frame.
std::string WsTextFrame(const std::string& text, const unsigned char mask[4]);

/// What `--art` sends, in order: (JavaScript, or for a dry run the line it prints). `notes` gets
/// the "no FILE, skipped" lines.
struct ArtCall {
    std::string js;       ///< what is sent (can be megabytes)
    std::string dry_line; ///< "[dry run] would call Steam (CEF): ..."
};
std::vector<ArtCall> PlanArt(const std::string& appid, const std::string& dir,
                             std::vector<std::string>* notes);

/// `--art APPID DIR [--dry-run]`: 0 on success.
int Art(const std::string& appid, const std::string& dir, bool dry_run);

// Library entries (install.sh and uninstall.sh)
std::string FindShortcutJs(const std::string& name);
std::string AddShortcutJs(const std::string& name, const std::string& exe, const std::string& dir);
std::string UpdateShortcutJs(const std::string& appid, const std::string& exe,
                             const std::string& dir);
std::string RemoveShortcutJs(const std::string& appid);

/// `--screen theater|dashboard APPID [SECONDS] [--after-vr]`.
int Screen(const std::string& where, const std::string& appid, double seconds, bool after_vr);

/// vrcmd's `--overlays` line state: picture, in the dashboard, visible.
struct OverlayState {
    bool picture = false, in_dashboard = false, visible = false;
};
OverlayState ParseOverlayLine(const std::string& line);
std::optional<std::string> FindOverlayLine(const std::string& listing, const std::string& key);

/// The old steam-cef.py command line: `EXPRESSION`, `--art APPID DIR [--dry-run]`, `--screen ...`.
int RunCli(const std::vector<std::string>& args);
/// `--frame-steam-shortcut find|add|update|remove|remove-named ... [--dry-run]`.
int RunShortcutCli(const std::vector<std::string>& args);

} // namespace SteamCef
