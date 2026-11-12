// Copyright Azahar for Frame contributors
// Licensed under GPLv2 or any later version
// Refer to the misc/licenses/gplv2.txt file included.

// AzaharFrame cpphelpers: what makes Azahar for Frame's name and icon show in SteamVR's dock and
// dashboard (the C++ port of steam-frame/dock-icon.py):
//   --frame-dock-icon install DEST --appid N [--art DIR] [--dry-run]
//   --frame-dock-icon remove  DEST [--dry-run]
// DEST is the program folder; the icons come from DEST/art or DIR. Writes the icon-theme PNGs and a
// NoDisplay .desktop file for the flat Qt window, and DEST/azaharframe.vrmanifest (is_internal)
// registered with SteamVR: through vrcmd when SteamVR runs, else in ~/.config/openvr/config/
// appconfig.json (written to a temporary file and renamed). All under $HOME; running it again
// changes nothing. Environment: AZAHARFRAME_VRCMD, HOME, XDG_DATA_HOME,
// AZAHARFRAME_VRSERVER_RUNNING (0/1, for tests).

#pragma once

#include <string>
#include <vector>

namespace FrameDockIcon {

std::string DesktopText(unsigned long long appid);
/// The manifest's JSON text (what is written to DEST/azaharframe.vrmanifest).
std::string ManifestText(const std::string& dest, unsigned long long appid, const std::string& art_src);
/// `manifest_paths` of an appconfig.json text after adding `add` and dropping `drop` ("" = none);
/// `changed` says whether it differs. Other keys keep their order.
std::string EditAppConfig(const std::string& text, const std::string& add, const std::string& drop,
                          bool* changed);

int RunCli(const std::vector<std::string>& args);

} // namespace FrameDockIcon
