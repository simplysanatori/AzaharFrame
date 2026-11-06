// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
//
// AzaharFrame: the Azahar for Frame icon for the Qt windows. The installed layout has it at
// <program folder>/art/icon.png (steam-frame/install.sh); a build tree finds the checkout's
// steam-frame/art/icon.png. Without the file the callers keep Azahar's own icon.
#pragma once

#ifdef ENABLE_FRAME_XR

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QIcon>
#include <QString>

namespace FrameBranding {

/// The path of art/<name>, or an empty string when it is not found.
inline QString ArtFile(const QString& name) {
    const QString env = qEnvironmentVariable("AZAHARFRAME_ART");
    const QDir app_dir(QCoreApplication::applicationDirPath());
    const QString candidates[] = {
        env.isEmpty() ? QString() : QDir(env).filePath(name),
        app_dir.filePath(QStringLiteral("art/") + name),
        app_dir.filePath(QStringLiteral("../../../steam-frame/art/") + name),
    };
    for (const QString& path : candidates) {
        if (!path.isEmpty() && QFileInfo::exists(path)) {
            return path;
        }
    }
    return {};
}

/// The Azahar for Frame icon, or `fallback` when art/icon.png is not found.
inline QIcon AppIcon(const QIcon& fallback) {
    const QString path = ArtFile(QStringLiteral("icon.png"));
    return path.isEmpty() ? fallback : QIcon(path);
}

} // namespace FrameBranding

#endif // ENABLE_FRAME_XR
