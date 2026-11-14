// Copyright Azahar for Frame contributors
// Licensed under GPLv2 or any later version
// Refer to the misc/licenses/gplv2.txt file included.

// AzaharFrame cpphelpers: see frame_tools.h.

#include "citra_qt/frame_tools.h"

#include <cstring>
#include <string>
#include <vector>
#include "core/homebrew/emerald3ds_build.h"
#include <QImage>
#include "core/frame_controller_models.h"
#include "core/frame_dock_icon.h"
#include "core/frame_weapon_models.h" // AzaharFrame kiuguns
#include "core/steam_cef.h"

namespace FrameTools {

std::optional<int> Run(int argc, char* argv[]) {
    if (argc < 2 || std::strncmp(argv[1], "--frame-", 8) != 0) {
        return std::nullopt;
    }
    const std::string mode = argv[1];
    std::vector<std::string> args;
    for (int i = 2; i < argc; ++i) {
        args.emplace_back(argv[i]);
    }
    if (mode == "--frame-emerald-build") {
        return Homebrew::Emerald3ds::Build::RunCli(args);
    }
    if (mode == "--frame-cef") { // steam-cef.py's command line
        return SteamCef::RunCli(args);
    }
    if (mode == "--frame-steam-art") { // APPID DIR [--dry-run]
        args.insert(args.begin(), "--art");
        return SteamCef::RunCli(args);
    }
    if (mode == "--frame-controller-models") { // OUTDIR [SteamVR rendermodels dir]
        return FrameControllerModels::Run(args, [](const std::string& path, FrameControllerModels::Image* out) {
            QImage image(QString::fromStdString(path));
            if (image.isNull()) {
                return false;
            }
            image = image.convertToFormat(QImage::Format_RGB888);
            out->width = image.width();
            out->height = image.height();
            out->rgb.resize(static_cast<std::size_t>(image.width()) * image.height() * 3);
            for (int y = 0; y < image.height(); ++y) { // tight rows
                std::memcpy(out->rgb.data() + static_cast<std::size_t>(y) * image.width() * 3, image.constScanLine(y),
                            static_cast<std::size_t>(image.width()) * 3);
            }
            return true;
        });
    }
    if (mode == "--frame-weapon-models") { // AzaharFrame kiuguns: GAME [OUTDIR]
        return FrameWeapons::RunCli(args);
    }
    if (mode == "--frame-dock-icon") {
        return FrameDockIcon::RunCli(args);
    }
    if (mode == "--frame-steam-shortcut") {
        return SteamCef::RunShortcutCli(args);
    }
    return std::nullopt;
}

} // namespace FrameTools
