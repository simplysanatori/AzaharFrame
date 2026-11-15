// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame fixa: a per-game change from the in-game menu sticks (frame_xr/per_game_keys.h).
// citra_qt.cpp FrameMenuRequest sets the game's value, constructs the game's QtConfig (which reads
// the game's file back into the settings), puts the menu's values back (KeepMenuKeys) and saves.
// Here the file is read and written as QtConfig::ReadGlobalSetting and WriteGlobalSetting do for a
// game's file (the key's "use_global", and its value when the game has its own), with QSettings.

#include <algorithm>
#include <string>
#include <type_traits>
#include <vector>
#include <QFile>
#include <QSettings>
#include <QTemporaryDir>
#include <catch2/catch_test_macros.hpp>
#include "common/settings.h"
#include "frame_xr/menu/tab_menu.h"
#include "frame_xr/per_game.h"
#include "frame_xr/per_game_keys.h"

namespace {

using FrameXr::Menu::Id;

template <typename T>
QVariant ToVariant(const T& value) {
    if constexpr (std::is_same_v<T, bool>) {
        return value;
    } else if constexpr (std::is_enum_v<T>) {
        return QVariant::fromValue(static_cast<qint64>(value));
    } else if constexpr (std::is_floating_point_v<T>) {
        return QVariant::fromValue(static_cast<double>(value));
    } else if constexpr (std::is_signed_v<T>) {
        return QVariant::fromValue(static_cast<qint64>(value));
    } else {
        return QVariant::fromValue(static_cast<quint64>(value));
    }
}

template <typename T>
T FromVariant(const QVariant& v) {
    if constexpr (std::is_same_v<T, bool>) {
        return v.toBool();
    } else if constexpr (std::is_enum_v<T>) {
        return static_cast<T>(v.toLongLong());
    } else if constexpr (std::is_floating_point_v<T>) {
        return static_cast<T>(v.toDouble());
    } else if constexpr (std::is_signed_v<T>) {
        return static_cast<T>(v.toLongLong());
    } else {
        return static_cast<T>(v.toULongLong());
    }
}

/// QtConfig::ReadGlobalSetting for a game's file.
template <typename T, bool R>
void ReadGame(QSettings& ini, Settings::SwitchableSetting<T, R>& s) {
    const QString name = QString::fromStdString(s.GetLabel());
    const bool use_global = ini.value(name + QStringLiteral("/use_global"), true).toBool();
    s.SetGlobal(use_global);
    if (!use_global) {
        s.SetValue(FromVariant<T>(ini.value(name, ToVariant(s.GetDefault()))));
    }
}

/// QtConfig::WriteGlobalSetting for a game's file.
template <typename T, bool R>
void WriteGame(QSettings& ini, const Settings::SwitchableSetting<T, R>& s) {
    const QString name = QString::fromStdString(s.GetLabel());
    if (s.UsingGlobal()) {
        ini.remove(name);
        ini.setValue(name + QStringLiteral("/use_global"), true);
        return;
    }
    ini.setValue(name + QStringLiteral("/use_global"), false);
    ini.setValue(name, ToVariant(s.GetValue()));
}

void ReadAll(QSettings& ini) {
    FrameXr::PerGame::ForEachMenuKey([&ini](auto& s) { ReadGame(ini, s); });
}
void WriteAll(QSettings& ini) {
    FrameXr::PerGame::ForEachMenuKey([&ini](auto& s) { WriteGame(ini, s); });
}

/// A game value other than the global one (as the menu sets it: Per-Game scope), within the
/// setting's range. False when none was found.
template <typename T, bool R>
bool SetOtherGameValue(Settings::SwitchableSetting<T, R>& s) {
    const T global = s.GetValue(true);
    std::vector<T> tries;
    if constexpr (std::is_same_v<T, bool>) {
        tries = {!global};
    } else if constexpr (std::is_enum_v<T>) {
        using U = std::underlying_type_t<T>;
        tries = {static_cast<T>(static_cast<U>(global) + 1), static_cast<T>(static_cast<U>(global) - 1)};
    } else if constexpr (std::is_floating_point_v<T>) {
        tries = {global + T(0.5), global - T(0.5), global + T(5), global * T(2), T(1)};
    } else {
        tries = {static_cast<T>(global + 1), static_cast<T>(global - 1), T(1), T(2)};
    }
    s.SetGlobal(false);
    for (const T& t : tries) {
        s.SetValue(t);
        if (s.GetValue() != global) {
            return true;
        }
    }
    return false;
}

std::vector<std::string> Labels() {
    std::vector<std::string> labels;
    FrameXr::PerGame::ForEachMenuKey([&labels](auto& s) { labels.push_back(s.GetLabel()); });
    return labels;
}

/// Every key follows the global value again when the test ends.
struct AllGlobal {
    ~AllGlobal() {
        FrameXr::PerGame::ForEachMenuKey([](auto& s) { s.SetGlobal(true); });
    }
};

} // namespace

TEST_CASE("Per-Game: every per-game row's key is kept across the read of the game's file",
          "[frame_xr][fixa]") {
    const std::vector<std::string> labels = Labels();
    for (int i = 0; i < FrameXr::Menu::kIdCount; i++) {
        const Id id = static_cast<Id>(i);
        const std::vector<std::string> keys = FrameXr::PerGame::KeysOf(id);
        CHECK(keys.empty() == !FrameXr::Menu::PerGameSetting(id));
        for (const std::string& key : keys) {
            INFO("row " << i << " key " << key);
            CHECK(std::find(labels.begin(), labels.end(), key) != labels.end());
        }
    }
    // The two that reverted in session 651, by name.
    for (const char* key : {"vr_motion_clarity_immersive", "vr_screens_in_projection"}) {
        INFO(key);
        CHECK(std::find(labels.begin(), labels.end(), key) != labels.end());
    }
    // No key twice
    std::vector<std::string> sorted = labels;
    std::sort(sorted.begin(), sorted.end());
    CHECK(std::adjacent_find(sorted.begin(), sorted.end()) == sorted.end());
}

TEST_CASE("Per-Game: a value the menu writes sticks after the game's file is read again",
          "[frame_xr][fixa]") {
    AllGlobal reset;
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("0004000000030800.ini"));
    std::size_t checked = 0;
    FrameXr::PerGame::ForEachMenuKey([&](auto& s) {
        using T = std::remove_cvref_t<decltype(s.GetValue())>;
        INFO("key " << s.GetLabel());
        // The game's file: every key follows the global value (nothing set for this game yet).
        QFile::remove(path);
        FrameXr::PerGame::ForEachMenuKey([](auto& o) { o.SetGlobal(true); });
        {
            QSettings ini(path, QSettings::IniFormat);
            ini.beginGroup(QStringLiteral("VR"));
            WriteAll(ini);
        }
        // The menu, Per-Game scope: the game's own value.
        REQUIRE(SetOtherGameValue(s));
        const T wanted = s.GetValue();
        // FrameMenuRequest: the game's QtConfig reads the file, the keeps put the menu's values
        // back, and the file is saved.
        {
            QSettings ini(path, QSettings::IniFormat);
            ini.beginGroup(QStringLiteral("VR"));
            {
                FrameXr::PerGame::KeepMenuKeys keep;
                ReadAll(ini);
            }
            CHECK(!s.UsingGlobal());
            CHECK(s.GetValue() == wanted);
            WriteAll(ini);
        }
        // The next boot reads the file afresh.
        FrameXr::PerGame::ForEachMenuKey([](auto& o) { o.SetGlobal(true); });
        {
            QSettings ini(path, QSettings::IniFormat);
            ini.beginGroup(QStringLiteral("VR"));
            ReadAll(ini);
        }
        CHECK(!s.UsingGlobal());
        CHECK(s.GetValue() == wanted);
        checked++;
    });
    CHECK(checked == Labels().size());
}

TEST_CASE("Per-Game: without the keep the read puts the file's value back (the session 651 bug)",
          "[frame_xr][fixa]") {
    AllGlobal reset;
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("0004000000033500.ini"));
    auto& s = Settings::values.vr_screens_in_projection;
    s.SetGlobal(true);
    {
        QSettings ini(path, QSettings::IniFormat);
        ini.beginGroup(QStringLiteral("VR"));
        WriteGame(ini, s);
    }
    REQUIRE(SetOtherGameValue(s));
    QSettings ini(path, QSettings::IniFormat);
    ini.beginGroup(QStringLiteral("VR"));
    ReadGame(ini, s); // no keep: the file says "use the global value"
    CHECK(s.UsingGlobal());
}

TEST_CASE("Per-Game: KeepMenuKeys puts the values back once, at Restore or at the end",
          "[frame_xr][fixa]") {
    AllGlobal reset;
    auto& s = Settings::values.vr_motion_clarity_immersive;
    s.SetGlobal(false);
    s.SetValue(true);
    {
        FrameXr::PerGame::KeepMenuKeys keep;
        s.SetGlobal(true);
        keep.Restore();
        CHECK(!s.UsingGlobal());
        CHECK(s.GetValue());
        s.SetGlobal(true); // after Restore: not put back again
    }
    CHECK(s.UsingGlobal());
}

namespace {

/// QtConfig::ReadBasicSetting in the global file: a value marked "\default=true" is read as the
/// build's own default.
template <typename T, bool R>
T ReadGlobalFile(QSettings& ini, const Settings::SwitchableSetting<T, R>& s) {
    const QString name = QString::fromStdString(s.GetLabel());
    if (ini.value(name + QStringLiteral("/default"), false).toBool()) {
        return s.GetDefault();
    }
    return FromVariant<T>(ini.value(name, ToVariant(s.GetDefault())));
}

/// QtConfig::WriteBasicSetting in the global file: the value, marked "\default" when it equals the
/// build's default.
template <typename T, bool R>
void WriteGlobalFile(QSettings& ini, const Settings::SwitchableSetting<T, R>& s, T value) {
    const QString name = QString::fromStdString(s.GetLabel());
    ini.setValue(name + QStringLiteral("/default"), value == s.GetDefault());
    ini.setValue(name, ToVariant(value));
}

} // namespace

// AzaharFrame fixa: Screen Shape starts on Automatic. The global file keeps an unchanged value as
// "\default=true", which each build reads as its own default, so 1.1.1 (3DS 5:3, 0 to 5 only)
// never sees Automatic from it; a game's own value is read as written, in every build.
TEST_CASE("Defaults: Screen Shape is Automatic, rollback-safe", "[fixa]") {
    auto& top = Settings::values.vr_top_aspect;
    CHECK(top.GetDefault() == 6u); // Automatic, the last choice
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    QSettings ini(dir.filePath(QStringLiteral("qt-config.ini")), QSettings::IniFormat);
    // A file an older build wrote with Screen Shape never changed (its 5:3 default): Automatic.
    ini.setValue(QStringLiteral("vr_top_aspect/default"), true);
    ini.setValue(QStringLiteral("vr_top_aspect"), 0);
    CHECK(ReadGlobalFile(ini, top) == 6u);
    // Automatic saved by this build is marked as the default: an older build reads its own.
    WriteGlobalFile(ini, top, 6u);
    CHECK(ini.value(QStringLiteral("vr_top_aspect/default")).toBool());
    // 3DS (5:3) picked in this build is saved as itself, and read back as itself.
    WriteGlobalFile(ini, top, 0u);
    CHECK_FALSE(ini.value(QStringLiteral("vr_top_aspect/default")).toBool());
    CHECK(ReadGlobalFile(ini, top) == 0u);
    // A game's own 5:3 (no "\default" in a game's file) stays 5:3.
    QSettings game(dir.filePath(QStringLiteral("game.ini")), QSettings::IniFormat);
    game.setValue(QStringLiteral("vr_top_aspect/use_global"), false);
    game.setValue(QStringLiteral("vr_top_aspect"), 0);
    const bool was_global = top.UsingGlobal();
    const u32 was = top.GetValue();
    ReadGame(game, top);
    CHECK_FALSE(top.UsingGlobal());
    CHECK(top.GetValue() == 0u);
    top.SetGlobal(was_global);
    if (!was_global) {
        top.SetValue(was);
    }
}
