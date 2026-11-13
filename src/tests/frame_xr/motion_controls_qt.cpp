// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame kiu: the VR group's Motion Controls is never locked. In the global settings it
// edits the global value, also while the running game has its own (a plain hint says so); in a
// game's Properties it edits that game's value, or Use global.

#include <QApplication>
#include <QComboBox>
#include <QDir>
#include <QWidget>
#include <catch2/catch_test_macros.hpp>
#include "citra_qt/configuration/configuration_shared.h"
#include "citra_qt/configuration/configure_frame_vr.h"
#include "common/settings.h"

namespace {

QApplication& App() {
    static int argc = 1;
    static char arg0[] = "frame_qt_tests";
    static char* argv[] = {arg0, nullptr};
    static QApplication* app = nullptr;
    if (!app) {
        app = qobject_cast<QApplication*>(QApplication::instance());
    }
    if (!app) {
        qputenv("QT_QPA_PLATFORM", "offscreen");
        app = new QApplication(argc, argv);
    }
    return *app;
}

QComboBox* MotionCombo(QWidget& w) {
    auto* row = w.findChild<QWidget*>(QStringLiteral("frame_vr_motion_row"));
    return row ? row->findChild<QComboBox*>() : nullptr;
}

// FRAME_QT_SHOTS=<folder>: a picture of the group, for the review.
void Shot(QWidget& w, const char* name) {
    const QByteArray dir = qgetenv("FRAME_QT_SHOTS");
    if (dir.isEmpty()) {
        return;
    }
    w.resize(760, w.sizeHint().height());
    w.grab().save(QDir(QString::fromUtf8(dir)).filePath(QString::fromLatin1(name)));
}

struct KeepMotion {
    Settings::SwitchableSetting<u32, true>& s = Settings::values.vr_motion_controls;
    bool global = s.UsingGlobal();
    u32 global_value = s.GetValue(true);
    u32 game_value = s.GetValue();
    ~KeepMotion() {
        s.SetGlobal(true);
        s.SetValue(global_value);
        s.SetGlobal(global);
        if (!global) {
            s.SetValue(game_value);
        }
        Settings::SetConfiguringGlobal(true);
    }
};

} // namespace

TEST_CASE("VR group: Motion Controls is never locked", "[frame_xr][kiu]") {
    App();
    KeepMotion keep;
    auto& motion = Settings::values.vr_motion_controls;
    SECTION("global settings while the running game has its own value") {
        Settings::SetConfiguringGlobal(true);
        motion.SetGlobal(true);
        motion.SetValue(1); // global: Right Controller
        motion.SetGlobal(false);
        motion.SetValue(0); // the game's own: Off (Kid Icarus: Uprising's starting value)
        ConfigureFrameVr w;
        QComboBox* combo = MotionCombo(w);
        REQUIRE(combo != nullptr);
        CHECK(combo->isEnabled());
        CHECK(combo->count() == 4);
        CHECK(combo->currentIndex() == 1); // the global value, not the game's
        CHECK(combo->toolTip() ==
              QStringLiteral("The running game has its own value. This sets the one for other "
                             "games."));
        Shot(w, "qt-vr-motion-global-game-value.png");
        combo->setCurrentIndex(3); // Headset
        w.ApplyConfiguration();
        CHECK(motion.GetValue(true) == 3u); // the global value changed
        CHECK(!motion.UsingGlobal());       // the game keeps its own
        CHECK(motion.GetValue() == 0u);
    }
    SECTION("global settings, no game value") {
        Settings::SetConfiguringGlobal(true);
        motion.SetGlobal(true);
        motion.SetValue(0);
        ConfigureFrameVr w;
        QComboBox* combo = MotionCombo(w);
        REQUIRE(combo != nullptr);
        CHECK(combo->isEnabled());
        CHECK(combo->toolTip().isEmpty());
        combo->setCurrentIndex(2);
        w.ApplyConfiguration();
        CHECK(motion.UsingGlobal());
        CHECK(motion.GetValue() == 2u);
    }
    SECTION("a game's Properties: its own value or Use global") {
        motion.SetGlobal(true);
        motion.SetValue(1);
        Settings::SetConfiguringGlobal(false);
        motion.SetGlobal(true); // the game follows the global value
        ConfigureFrameVr w;
        QComboBox* combo = MotionCombo(w);
        REQUIRE(combo != nullptr);
        CHECK(combo->isEnabled());
        CHECK(!combo->isHidden());
        CHECK(combo->count() == 4 + ConfigurationShared::USE_GLOBAL_OFFSET);
        CHECK(combo->currentIndex() == ConfigurationShared::USE_GLOBAL_INDEX);
        Shot(w, "qt-vr-motion-per-game-use-global.png");
        combo->setCurrentIndex(ConfigurationShared::USE_GLOBAL_OFFSET + 0); // Off for this game
        w.ApplyConfiguration();
        CHECK(!motion.UsingGlobal());
        CHECK(motion.GetValue() == 0u);
        CHECK(motion.GetValue(true) == 1u); // the global value stays
        {
            ConfigureFrameVr again; // reopened: the game's own value shows
            QComboBox* c2 = MotionCombo(again);
            REQUIRE(c2 != nullptr);
            CHECK(c2->currentIndex() == ConfigurationShared::USE_GLOBAL_OFFSET + 0);
            Shot(again, "qt-vr-motion-per-game-own-value.png");
            c2->setCurrentIndex(ConfigurationShared::USE_GLOBAL_INDEX);
            again.ApplyConfiguration();
        }
        CHECK(motion.UsingGlobal());
        CHECK(motion.GetValue() == 1u);
    }
}
