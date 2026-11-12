// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame useglobal: Use Global Value in a game's file (citra_qt/configuration/
// frame_per_game_ini.h, QtConfig::WriteGlobalSetting): the key goes, only use_global=true stays,
// the global value is never written there, and the rest of the file is kept.

#include <QDir>
#include <QFile>
#include <QSettings>
#include <QTemporaryDir>
#include <catch2/catch_test_macros.hpp>
#include "citra_qt/configuration/frame_per_game_ini.h"

TEST_CASE("Use Global Value: the key goes from the game's file", "[frame_xr][useglobal]") {
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("0004000000033500.ini"));
    {
        QSettings ini(path, QSettings::IniFormat);
        ini.beginGroup(QStringLiteral("VR"));
        ini.setValue(QStringLiteral("vr_immersive_game_fov/use_global"), false);
        ini.setValue(QStringLiteral("vr_immersive_game_fov"), 95);
        ini.setValue(QStringLiteral("autosave_keep/use_global"), false);
        ini.setValue(QStringLiteral("autosave_keep"), 8);
        ini.endGroup();
        ini.beginGroup(QStringLiteral("Renderer"));
        ini.setValue(QStringLiteral("resolution_factor/use_global"), false);
        ini.setValue(QStringLiteral("resolution_factor"), 4);
        ini.endGroup();
        ini.sync();
    }
    {
        QSettings ini(path, QSettings::IniFormat);
        ini.beginGroup(QStringLiteral("VR"));
        FrameConfig::WriteUsesGlobal(ini, QStringLiteral("vr_immersive_game_fov"));
        ini.endGroup();
        ini.beginGroup(QStringLiteral("Renderer"));
        FrameConfig::WriteUsesGlobal(ini, QStringLiteral("resolution_factor"));
        ini.endGroup();
        ini.sync();
        CHECK(ini.status() == QSettings::NoError);
    }
    QSettings ini(path, QSettings::IniFormat);
    CHECK(!ini.contains(QStringLiteral("VR/vr_immersive_game_fov")));
    CHECK(ini.value(QStringLiteral("VR/vr_immersive_game_fov/use_global")).toBool());
    CHECK(!ini.contains(QStringLiteral("Renderer/resolution_factor")));
    CHECK(ini.value(QStringLiteral("Renderer/resolution_factor/use_global")).toBool());
    // The other row's own value stays
    CHECK(!ini.value(QStringLiteral("VR/autosave_keep/use_global")).toBool());
    CHECK(ini.value(QStringLiteral("VR/autosave_keep")).toInt() == 8);
    // No value line for the cleared keys in the text (not the old one, not a global one)
    QFile f(path);
    REQUIRE(f.open(QIODevice::ReadOnly));
    const QString text = QString::fromUtf8(f.readAll());
    CHECK(!text.contains(QStringLiteral("vr_immersive_game_fov=")));
    CHECK(!text.contains(QStringLiteral("resolution_factor=")));
    CHECK(text.contains(QStringLiteral("autosave_keep=8")));
    // Nothing else left in the folder (QSettings writes through a temporary file and a rename)
    CHECK(QDir(dir.path()).entryList(QDir::Files) ==
          QStringList{QStringLiteral("0004000000033500.ini")});
}
