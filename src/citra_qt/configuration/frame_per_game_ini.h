// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame useglobal: how a per-game file stores a setting that follows the global value.

#pragma once

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

} // namespace FrameConfig
