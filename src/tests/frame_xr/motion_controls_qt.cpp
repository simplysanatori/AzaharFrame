// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame kiu: no row of the VR group is ever locked. In the global settings a row edits the
// global value, also while the running game has its own (that one stays, and a plain hint says
// so); in a game's Properties it edits that game's value, or Use global.

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QLayout>
#include <QSpinBox>
#include <QWidget>
#include <initializer_list>
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

template <typename T, bool R>
struct Keep {
    Settings::SwitchableSetting<T, R>& s;
    bool global = s.UsingGlobal();
    T global_value = s.GetValue(true);
    T game_value = s.GetValue();
    ~Keep() {
        s.SetGlobal(true);
        s.SetValue(global_value);
        s.SetGlobal(global);
        if (!global) {
            s.SetValue(game_value);
        }
    }
};

template <typename T, bool R>
void Values(Settings::SwitchableSetting<T, R>& s, T global, T game) {
    s.SetGlobal(true);
    s.SetValue(global);
    s.SetGlobal(false);
    s.SetValue(game);
}

template <typename W>
W* In(QWidget& w, const char* row) {
    auto* r = w.findChild<QWidget*>(QString::fromLatin1(row));
    return r ? r->findChild<W*>() : nullptr;
}

const QString kHint =
    QStringLiteral("The running game has its own value. This sets the one for other games.");

} // namespace

// The other rows the same way: Immersive Mode, Render Margin, Horizontal Scale, Immersive Game
// FOV and Hide Bottom Screen (combo boxes with and without a conversion, a spin box, a check box).
TEST_CASE("VR group: no row is locked while the running game has its own value",
          "[frame_xr][kiu]") {
    App();
    auto& v = Settings::values;
    Keep<u32, false> k1{v.vr_immersive_mode};
    Keep<u32, true> k2{v.vr_immersive_wider_view_level};
    Keep<s32, true> k3{v.vr_immersive_scale_x};
    Keep<s32, true> k4{v.vr_immersive_game_fov};
    Keep<bool, false> k5{v.vr_hide_bottom_screen};
    Settings::SetConfiguringGlobal(true);
    Values(v.vr_immersive_mode, 0u, 2u);
    Values(v.vr_immersive_wider_view_level, 4u, 1u);
    Values(v.vr_immersive_scale_x, 0, -10);
    Values(v.vr_immersive_game_fov, 60, 55);
    Values(v.vr_hide_bottom_screen, false, true);
    {
        ConfigureFrameVr w;
        auto* mode = In<QComboBox>(w, "frame_vr_mode_row");
        auto* wider = In<QComboBox>(w, "frame_vr_wider_row");
        auto* scale = In<QComboBox>(w, "frame_vr_scale_x_row");
        auto* fov = In<QSpinBox>(w, "frame_vr_fov_row");
        auto* hide = w.findChild<QCheckBox*>(QStringLiteral("frame_vr_hide_bottom_check"));
        REQUIRE(mode);
        REQUIRE(wider);
        REQUIRE(scale);
        REQUIRE(fov);
        REQUIRE(hide);
        for (QWidget* box : std::initializer_list<QWidget*>{mode, wider, scale, fov, hide}) {
            CHECK(box->isEnabled());
            CHECK(box->toolTip() == kHint);
        }
        // The global values show
        CHECK(mode->currentIndex() == 0);
        CHECK(wider->currentIndex() == 4);
        CHECK(scale->currentIndex() == 6); // 0 %
        CHECK(fov->value() == 60);
        CHECK(!hide->isChecked());
        Shot(w, "qt-vr-global-game-values.png");
        mode->setCurrentIndex(3);
        wider->setCurrentIndex(2);
        scale->setCurrentIndex(8); // +10 %
        fov->setValue(70);
        hide->setChecked(true);
        w.ApplyConfiguration();
    }
    // The global values changed, the game's stayed
    CHECK(v.vr_immersive_mode.GetValue(true) == 3u);
    CHECK(v.vr_immersive_wider_view_level.GetValue(true) == 2u);
    CHECK(v.vr_immersive_scale_x.GetValue(true) == 10);
    CHECK(v.vr_immersive_game_fov.GetValue(true) == 70);
    CHECK(v.vr_hide_bottom_screen.GetValue(true));
    CHECK(!v.vr_immersive_mode.UsingGlobal());
    CHECK(v.vr_immersive_mode.GetValue() == 2u);
    CHECK(v.vr_immersive_wider_view_level.GetValue() == 1u);
    CHECK(v.vr_immersive_scale_x.GetValue() == -10);
    CHECK(v.vr_immersive_game_fov.GetValue() == 55);
    CHECK(v.vr_hide_bottom_screen.GetValue());
    // A game's Properties: each has Use global and the values, and shows the game's own
    Settings::SetConfiguringGlobal(false);
    {
        ConfigureFrameVr w;
        auto* mode = In<QComboBox>(w, "frame_vr_mode_row");
        auto* scale = In<QComboBox>(w, "frame_vr_scale_x_row");
        auto* fov_scope = In<QComboBox>(w, "frame_vr_fov_row");
        REQUIRE(mode);
        REQUIRE(scale);
        REQUIRE(fov_scope);
        CHECK(mode->count() == 4 + ConfigurationShared::USE_GLOBAL_OFFSET);
        CHECK(mode->currentIndex() == 2 + ConfigurationShared::USE_GLOBAL_OFFSET);
        CHECK(scale->count() == 13 + ConfigurationShared::USE_GLOBAL_OFFSET);
        CHECK(scale->currentIndex() == 4 + ConfigurationShared::USE_GLOBAL_OFFSET); // -10 %
        CHECK(fov_scope->currentIndex() == 1); // its own
        Shot(w, "qt-vr-per-game-own-values.png");
        mode->setCurrentIndex(ConfigurationShared::USE_GLOBAL_INDEX);
        w.ApplyConfiguration();
    }
    CHECK(v.vr_immersive_mode.UsingGlobal());
    CHECK(v.vr_immersive_mode.GetValue() == 3u);
    CHECK(!v.vr_immersive_scale_x.UsingGlobal());
    CHECK(v.vr_immersive_scale_x.GetValue() == -10);
    Settings::SetConfiguringGlobal(true);
}

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
        CHECK(combo->toolTip() == kHint);
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

// AzaharFrame fovscale: Scale Mode (Stretch | Field of View) next to the scales: Field of View
// by default, never locked, each choice with its tooltip, a game's own value or Use global.
TEST_CASE("VR group: Scale Mode", "[frame_xr][fovscale]") {
    App();
    auto& v = Settings::values;
    Keep<u32, true> keep{v.vr_immersive_scale_mode};
    CHECK(v.vr_immersive_scale_mode.GetDefault() == 1u);
    Settings::SetConfiguringGlobal(true);
    Values(v.vr_immersive_scale_mode, 1u, 0u); // global Field of View, the game's own Stretch
    {
        ConfigureFrameVr w;
        auto* combo = In<QComboBox>(w, "frame_vr_scale_mode_row");
        REQUIRE(combo);
        CHECK(combo->isEnabled());
        CHECK(combo->count() == 2);
        CHECK(combo->itemText(0) == QStringLiteral("Stretch"));
        CHECK(combo->itemText(1) == QStringLiteral("Field of View"));
        CHECK(!combo->itemData(0, Qt::ToolTipRole).toString().isEmpty());
        CHECK(!combo->itemData(1, Qt::ToolTipRole).toString().isEmpty());
        CHECK(combo->currentIndex() == 1); // the global value
        CHECK(combo->toolTip() == kHint);
        // Next to the scales: the row right after Vertical Scale.
        auto* y_row = w.findChild<QWidget*>(QStringLiteral("frame_vr_scale_y_row"));
        auto* mode_row = w.findChild<QWidget*>(QStringLiteral("frame_vr_scale_mode_row"));
        REQUIRE(y_row);
        REQUIRE(mode_row);
        auto* layout = w.layout();
        REQUIRE(layout);
        CHECK(layout->indexOf(mode_row) == layout->indexOf(y_row) + 1);
        Shot(w, "qt-vr-scale-mode-global.png");
        combo->setCurrentIndex(0);
        w.ApplyConfiguration();
    }
    CHECK(v.vr_immersive_scale_mode.GetValue(true) == 0u); // the global value changed
    CHECK(!v.vr_immersive_scale_mode.UsingGlobal());       // the game keeps its own
    CHECK(v.vr_immersive_scale_mode.GetValue() == 0u);
    Settings::SetConfiguringGlobal(false);
    Values(v.vr_immersive_scale_mode, 1u, 0u);
    {
        ConfigureFrameVr w;
        auto* combo = In<QComboBox>(w, "frame_vr_scale_mode_row");
        REQUIRE(combo);
        CHECK(combo->count() == 2 + ConfigurationShared::USE_GLOBAL_OFFSET);
        CHECK(combo->currentIndex() == 0 + ConfigurationShared::USE_GLOBAL_OFFSET); // Stretch
        Shot(w, "qt-vr-scale-mode-per-game.png");
        combo->setCurrentIndex(ConfigurationShared::USE_GLOBAL_INDEX);
        w.ApplyConfiguration();
    }
    CHECK(v.vr_immersive_scale_mode.UsingGlobal());
    CHECK(v.vr_immersive_scale_mode.GetValue() == 1u);
    Settings::SetConfiguringGlobal(true);
}

// AzaharFrame projdef: Motion Clarity in Immersive Modes, per game, right under Motion Clarity,
// usable while Motion Clarity is not off; its tooltip and the experimental Motion Clarity one.
TEST_CASE("VR group: Motion Clarity in Immersive Modes", "[frame_xr][projdef]") {
    App();
    auto& v = Settings::values;
    Keep<bool, false> k1{v.vr_motion_clarity};
    Keep<bool, false> k2{v.vr_motion_clarity_immersive};
    CHECK_FALSE(v.vr_motion_clarity_immersive.GetDefault());
    Settings::SetConfiguringGlobal(true);
    v.vr_motion_clarity.SetGlobal(true);
    v.vr_motion_clarity_immersive.SetGlobal(true);
    v.vr_motion_clarity.SetValue(true);
    v.vr_motion_clarity_immersive.SetValue(false);
    {
        ConfigureFrameVr w;
        auto* mc = w.findChild<QCheckBox*>(QStringLiteral("frame_vr_motion_clarity_check"));
        auto* box =
            w.findChild<QCheckBox*>(QStringLiteral("frame_vr_motion_clarity_immersive_check"));
        REQUIRE(mc);
        REQUIRE(box);
        CHECK(mc->text() == QStringLiteral("Motion Clarity"));
        CHECK(mc->toolTip() == QStringLiteral("Experimental. A clearer, more solid picture, but it "
                                              "may flicker or double in demanding scenes."));
        CHECK(box->text() == QStringLiteral("Motion Clarity in Immersive Modes"));
        CHECK(box->toolTip() ==
              QStringLiteral("Experimental. May flicker, mostly in demanding games."));
        auto* layout = w.layout();
        REQUIRE(layout);
        CHECK(layout->indexOf(box) == layout->indexOf(mc) + 1);
        CHECK(box->isEnabled());
        CHECK_FALSE(box->isChecked()); // off by default
        Shot(w, "qt-vr-motion-clarity-immersive.png");
        mc->setChecked(false);
        CHECK_FALSE(box->isEnabled()); // nothing to opt into with Motion Clarity off
        mc->setChecked(true);
        CHECK(box->isEnabled());
        box->setChecked(true);
        w.ApplyConfiguration();
    }
    CHECK(v.vr_motion_clarity_immersive.GetValue(true));
    CHECK(v.vr_motion_clarity.GetValue(true)); // its parent kept
    // A game's Properties: its own value, or Use global.
    Settings::SetConfiguringGlobal(false);
    Values(v.vr_motion_clarity_immersive, true, false);
    {
        ConfigureFrameVr w;
        auto* box =
            w.findChild<QCheckBox*>(QStringLiteral("frame_vr_motion_clarity_immersive_check"));
        REQUIRE(box);
        CHECK_FALSE(box->isChecked()); // the game's own Off
        CHECK(box->isEnabled());
        box->click(); // its own On
        CHECK(box->isChecked());
        w.ApplyConfiguration();
    }
    CHECK_FALSE(v.vr_motion_clarity_immersive.UsingGlobal());
    CHECK(v.vr_motion_clarity_immersive.GetValue());
    Settings::SetConfiguringGlobal(true);
}
