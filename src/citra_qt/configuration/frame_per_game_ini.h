// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame useglobal: how a per-game file stores a setting that follows the global value.

#pragma once

#include <optional>
#include <QSettings>
#include <QString>

namespace FrameConfig {

/// In a per-game file (QtConfig::WriteGlobalSetting): the setting's key and its subkeys go and
/// only `use_global=true` stays, so the game's old value is not left behind and the global value
/// is never written there. QSettings saves the file whole (a temporary file, then a rename).
inline void WriteUsesGlobal(QSettings& ini, const QString& name) {
    ini.remove(name);
    ini.setValue(name + QStringLiteral("/use_global"), true);
}

/// AzaharFrame fovonce: Match Game Camera was a switch (`vr_immersive_match_camera`) and is now
/// Off (0), Set Once (1) or Always (2) (`vr_immersive_match_camera_mode`). For a file (its "VR"
/// group open) with no new key, the old switch's choice: on is Always, off is Off. Nothing
/// (std::nullopt) when the file has the new key, or the old switch was never set: the global file
/// marks a value never changed with `/default=true`, a game's file one that follows the global
/// value with `/use_global=true`.
inline std::optional<unsigned> MigratedMatchCamera(const QSettings& ini, bool global) {
    const QString now = QStringLiteral("vr_immersive_match_camera_mode");
    const QString old = QStringLiteral("vr_immersive_match_camera");
    if (ini.contains(now) || ini.contains(now + QStringLiteral("/use_global")) ||
        !ini.contains(old)) {
        return std::nullopt;
    }
    const bool unset = global ? ini.value(old + QStringLiteral("/default"), true).toBool()
                              : ini.value(old + QStringLiteral("/use_global"), true).toBool();
    if (unset) {
        return std::nullopt;
    }
    return ini.value(old, true).toBool() ? 2u : 0u;
}

} // namespace FrameConfig
