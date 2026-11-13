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

// AzaharFrame kiu: Narrow Screen and Narrow Height become Horizontal and Vertical Scale, once:
// Narrow Screen 10 % is -10 % (1 to 3 were 10 % steps); nothing when the file has the new key or
// never set the old one (a new default of 0).
TEST_CASE("Narrow Screen and Narrow Height convert to the scales", "[frame_xr][kiu]") {
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    const QString old_x = QStringLiteral("vr_immersive_squeeze");
    const QString now_x = QStringLiteral("vr_immersive_scale_x");
    const QString old_y = QStringLiteral("vr_immersive_squeeze_y");
    const QString now_y = QStringLiteral("vr_immersive_scale_y");
    const auto migrated = [&dir](const QString& name, bool global, const QString& old,
                                 const QString& now,
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
        return FrameConfig::MigratedScale(ini, global, old, now);
    };
    // A game's own values: Ocarina of Time 3D's old step 1 (10 %), Mario Kart 7's 30 and 15
    CHECK(migrated(QStringLiteral("0004000000033500.ini"), false, old_x, now_x,
                   {{"vr_immersive_squeeze/use_global", false}, {"vr_immersive_squeeze", 1}}) ==
          -10);
    CHECK(migrated(QStringLiteral("0004000000030800.ini"), false, old_x, now_x,
                   {{"vr_immersive_squeeze/use_global", false},
                    {"vr_immersive_squeeze", 30},
                    {"vr_immersive_squeeze_y/use_global", false},
                    {"vr_immersive_squeeze_y", 15}}) == -30);
    CHECK(migrated(QStringLiteral("0004000000030800.ini"), false, old_y, now_y, {}) == -15);
    CHECK(migrated(QStringLiteral("0004000000125600.ini"), false, old_x, now_x,
                   {{"vr_immersive_squeeze/use_global", false}, {"vr_immersive_squeeze", 0}}) ==
          0);
    CHECK(migrated(QStringLiteral("0004000000125500.ini"), false, old_x, now_x,
                   {{"vr_immersive_squeeze/use_global", false}, {"vr_immersive_squeeze", 99}}) ==
          -30);
    // Following the global value: nothing of its own
    CHECK(!migrated(QStringLiteral("0004000000054100.ini"), false, old_x, now_x,
                    {{"vr_immersive_squeeze/use_global", true}}));
    // Converted already (its own value, or following the global one): the old key is ignored
    CHECK(!migrated(QStringLiteral("0004000000086300.ini"), false, old_x, now_x,
                    {{"vr_immersive_squeeze/use_global", false},
                     {"vr_immersive_squeeze", 20},
                     {"vr_immersive_scale_x/use_global", false},
                     {"vr_immersive_scale_x", 5}}));
    CHECK(!migrated(QStringLiteral("0004000000086400.ini"), false, old_x, now_x,
                    {{"vr_immersive_squeeze/use_global", false},
                     {"vr_immersive_squeeze", 20},
                     {"vr_immersive_scale_x/use_global", true}}));
    // The global file: a value the player set, or never changed (the new default, 0)
    CHECK(migrated(QStringLiteral("g1.ini"), true, old_x, now_x,
                   {{"vr_immersive_squeeze/default", false}, {"vr_immersive_squeeze", 20}}) ==
          -20);
    CHECK(migrated(QStringLiteral("g2.ini"), true, old_x, now_x,
                   {{"vr_immersive_squeeze/default", false}, {"vr_immersive_squeeze", 0}}) == 0);
    CHECK(!migrated(QStringLiteral("g3.ini"), true, old_x, now_x,
                    {{"vr_immersive_squeeze/default", true}, {"vr_immersive_squeeze", 10}}));
    CHECK(!migrated(QStringLiteral("g4.ini"), true, old_x, now_x,
                    {{"vr_immersive_squeeze/default", false},
                     {"vr_immersive_squeeze", 20},
                     {"vr_immersive_scale_x/default", true},
                     {"vr_immersive_scale_x", 0}}));
    CHECK(!migrated(QStringLiteral("g5.ini"), true, old_x, now_x, {{"vr_immersive_game_fov", 60}}));
    // A game's file as the app writes it (a u32 QVariant, subkeys with a backslash)
    {
        const QString path = dir.filePath(QStringLiteral("0004000000030900.ini"));
        QFile f(path);
        REQUIRE(f.open(QIODevice::WriteOnly));
        f.write("[VR]\nvr_immersive_squeeze=@Variant(\\0\\0\\0\\x84\\0\\0\\0\\0\\0\\0\\0\\x1e)\n"
                "vr_immersive_squeeze\\use_global=false\n"
                "vr_immersive_squeeze_y=@Variant(\\0\\0\\0\\x84\\0\\0\\0\\0\\0\\0\\0\\xf)\n"
                "vr_immersive_squeeze_y\\use_global=false\n");
        f.close();
        QSettings ini(path, QSettings::IniFormat);
        ini.beginGroup(QStringLiteral("VR"));
        CHECK(FrameConfig::MigratedScale(ini, false, old_x, now_x) == -30);
        CHECK(FrameConfig::MigratedScale(ini, false, old_y, now_y) == -15);
    }
}
