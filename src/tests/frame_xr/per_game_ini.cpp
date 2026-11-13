// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame useglobal: Use Global Value in a game's file (citra_qt/configuration/
// frame_per_game_ini.h, QtConfig::WriteGlobalSetting): the key goes, only use_global=true stays,
// the global value is never written there, and the rest of the file is kept.

#include <initializer_list>
#include <utility>
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

// AzaharFrame fovonce: Match Game Camera's old switch becomes Off | Set Once | Always: on is
// Always, off is Off; nothing to convert when the file has the new key or never set the switch.
TEST_CASE("Match Game Camera: the old switch converts", "[frame_xr][fovonce]") {
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    const auto migrated = [&dir](const QString& name, bool global,
                                 const std::initializer_list<std::pair<const char*, QVariant>>&
                                     values) {
        const QString path = dir.filePath(name);
        {
            QSettings ini(path, QSettings::IniFormat);
            ini.beginGroup(QStringLiteral("VR"));
            for (const auto& [key, value] : values) {
                ini.setValue(QString::fromLatin1(key), value);
            }
            ini.endGroup();
            ini.sync();
        }
        QSettings ini(path, QSettings::IniFormat);
        ini.beginGroup(QStringLiteral("VR"));
        return FrameConfig::MigratedMatchCamera(ini, global);
    };
    // The global file: a value the player set
    CHECK(migrated(QStringLiteral("g1.ini"), true,
                   {{"vr_immersive_match_camera/default", false},
                    {"vr_immersive_match_camera", false}}) == 0u);
    CHECK(migrated(QStringLiteral("g2.ini"), true,
                   {{"vr_immersive_match_camera/default", false},
                    {"vr_immersive_match_camera", true}}) == 2u);
    // Never changed: the new default
    CHECK(!migrated(QStringLiteral("g3.ini"), true,
                    {{"vr_immersive_match_camera/default", true},
                     {"vr_immersive_match_camera", true}}));
    // Already converted
    CHECK(!migrated(QStringLiteral("g4.ini"), true,
                    {{"vr_immersive_match_camera/default", false},
                     {"vr_immersive_match_camera", false},
                     {"vr_immersive_match_camera_mode/default", false},
                     {"vr_immersive_match_camera_mode", 1}}));
    // No switch at all (a new user)
    CHECK(!migrated(QStringLiteral("g5.ini"), true, {{"vr_immersive_game_fov", 60}}));
    // A game's file: its own value
    CHECK(migrated(QStringLiteral("0004000000125600.ini"), false,
                   {{"vr_immersive_match_camera/use_global", false},
                    {"vr_immersive_match_camera", false}}) == 0u);
    CHECK(migrated(QStringLiteral("0004000000053F00.ini"), false,
                   {{"vr_immersive_match_camera/use_global", false},
                    {"vr_immersive_match_camera", true}}) == 2u);
    // Following the global value, or already converted (also to "use global")
    CHECK(!migrated(QStringLiteral("0004000000030800.ini"), false,
                    {{"vr_immersive_match_camera/use_global", true}}));
    CHECK(!migrated(QStringLiteral("0004000000033500.ini"), false,
                    {{"vr_immersive_match_camera/use_global", false},
                     {"vr_immersive_match_camera", true},
                     {"vr_immersive_match_camera_mode/use_global", true}}));
    // A file as Azahar writes it (subkeys with a backslash)
    {
        const QString path = dir.filePath(QStringLiteral("0004000000086300.ini"));
        QFile f(path);
        REQUIRE(f.open(QIODevice::WriteOnly));
        f.write("[VR]\nvr_immersive_match_camera\\use_global=false\n"
                "vr_immersive_match_camera=true\nvr_immersive_game_fov\\use_global=true\n");
        f.close();
        QSettings ini(path, QSettings::IniFormat);
        ini.beginGroup(QStringLiteral("VR"));
        CHECK(FrameConfig::MigratedMatchCamera(ini, false) == 2u);
    }
}
