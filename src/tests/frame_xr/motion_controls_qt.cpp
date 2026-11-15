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
    Keep<s32, true> k2x{v.vr_immersive_render_margin_x}; // AzaharFrame fovaxis
    Keep<s32, true> k2y{v.vr_immersive_render_margin_y};
    Keep<s32, true> k3{v.vr_immersive_scale_x};
    Keep<s32, true> k4{v.vr_immersive_game_fov};
    Keep<bool, false> k5{v.vr_hide_bottom_screen};
    Settings::SetConfiguringGlobal(true);
    Values(v.vr_immersive_mode, 0u, 2u);
    Values(v.vr_immersive_wider_view_level, 4u, 1u);
    // AzaharFrame fovaxis: both axes follow Render Margin (the game's own 1.1x, the global 1.4x)
    Values(v.vr_immersive_render_margin_x, -1, -1);
    Values(v.vr_immersive_render_margin_y, -1, -1);
    v.vr_immersive_render_margin_x.SetGlobal(true);
    v.vr_immersive_render_margin_y.SetGlobal(true);
    Values(v.vr_immersive_scale_x, 0, -10);
    Values(v.vr_immersive_game_fov, 60, 55);
    Values(v.vr_hide_bottom_screen, false, true);
    {
        ConfigureFrameVr w;
        auto* mode = In<QComboBox>(w, "frame_vr_mode_row");
        auto* wider = In<QComboBox>(w, "frame_vr_margin_x_row"); // AzaharFrame fovaxis
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
        CHECK(scale->currentIndex() == 20); // 0 % (AzaharFrame fov100: -100 % is item 0)
        CHECK(fov->value() == 60);
        CHECK(!hide->isChecked());
        Shot(w, "qt-vr-global-game-values.png");
        mode->setCurrentIndex(3);
        wider->setCurrentIndex(2);
        scale->setCurrentIndex(22); // +10 %
        fov->setValue(70);
        hide->setChecked(true);
        w.ApplyConfiguration();
    }
    // The global values changed, the game's stayed
    CHECK(v.vr_immersive_mode.GetValue(true) == 3u);
    // AzaharFrame fovaxis: Render Margin: Horizontal's own global value; the old key as it was
    CHECK(v.vr_immersive_render_margin_x.GetValue(true) == 2);
    CHECK(v.vr_immersive_render_margin_y.GetValue(true) == -1);
    CHECK(v.vr_immersive_wider_view_level.GetValue(true) == 4u);
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
        CHECK(scale->count() == 41 + ConfigurationShared::USE_GLOBAL_OFFSET);
        CHECK(scale->currentIndex() == 18 + ConfigurationShared::USE_GLOBAL_OFFSET); // -10 %
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

// AzaharFrame fov100: Horizontal and Vertical Scale offer -100 % to +100 % in 5 % steps (41
// items, 0 % in the middle); a game's -100 % and a global +100 % show and are applied as such,
// and a value of the old range (-30 to +30 %) keeps its item's meaning.
TEST_CASE("VR group: the scales from -100 % to +100 %", "[frame_xr][fov100]") {
    App();
    auto& v = Settings::values;
    Keep<s32, true> kx{v.vr_immersive_scale_x};
    Keep<s32, true> ky{v.vr_immersive_scale_y};
    // The setting's own range (what any reader clamps to).
    CHECK(v.vr_immersive_scale_x.GetDefault() == 0);
    Settings::SetConfiguringGlobal(true);
    Values(v.vr_immersive_scale_x, 100, -100);
    Values(v.vr_immersive_scale_y, -100, 100);
    {
        ConfigureFrameVr w;
        for (const char* row : {"frame_vr_scale_x_row", "frame_vr_scale_y_row"}) {
            auto* combo = In<QComboBox>(w, row);
            REQUIRE(combo);
            CHECK(combo->count() == 41);
            CHECK(combo->itemText(0) == QStringLiteral("-100%"));
            CHECK(combo->itemText(14) == QStringLiteral("-30%"));
            CHECK(combo->itemText(20) == QStringLiteral("0%"));
            CHECK(combo->itemText(26) == QStringLiteral("+30%"));
            CHECK(combo->itemText(40) == QStringLiteral("+100%"));
            CHECK(combo->isEnabled());
        }
        auto* x = In<QComboBox>(w, "frame_vr_scale_x_row");
        auto* y = In<QComboBox>(w, "frame_vr_scale_y_row");
        CHECK(x->currentIndex() == 40); // the global values
        CHECK(y->currentIndex() == 0);
        Shot(w, "qt-vr-scales-global-ends.png");
        x->setCurrentIndex(0);
        y->setCurrentIndex(40);
        w.ApplyConfiguration();
    }
    CHECK(v.vr_immersive_scale_x.GetValue(true) == -100);
    CHECK(v.vr_immersive_scale_y.GetValue(true) == 100);
    CHECK(v.vr_immersive_scale_x.GetValue() == -100); // the game's own stayed
    CHECK(v.vr_immersive_scale_y.GetValue() == 100);
    Settings::SetConfiguringGlobal(false);
    {
        ConfigureFrameVr w;
        auto* x = In<QComboBox>(w, "frame_vr_scale_x_row");
        auto* y = In<QComboBox>(w, "frame_vr_scale_y_row");
        REQUIRE(x);
        REQUIRE(y);
        CHECK(x->count() == 41 + ConfigurationShared::USE_GLOBAL_OFFSET);
        CHECK(x->currentIndex() == 0 + ConfigurationShared::USE_GLOBAL_OFFSET);  // -100 %
        CHECK(y->currentIndex() == 40 + ConfigurationShared::USE_GLOBAL_OFFSET); // +100 %
        Shot(w, "qt-vr-scales-per-game-ends.png");
        x->setCurrentIndex(14 + ConfigurationShared::USE_GLOBAL_OFFSET); // -30 %, the old end
        y->setCurrentIndex(26 + ConfigurationShared::USE_GLOBAL_OFFSET); // +30 %
        w.ApplyConfiguration();
    }
    CHECK(v.vr_immersive_scale_x.GetValue() == -30);
    CHECK(v.vr_immersive_scale_y.GetValue() == 30);
    Settings::SetConfiguringGlobal(true);
}

// AzaharFrame fovaxis: Render Margin's two rows in a game's Properties (each its own value or
// Use global, the old key followed while untouched; shown with Stretch only, their values kept
// while hidden) and "May Warp" on a scale's row.
TEST_CASE("VR group: Render Margin per axis and the May Warp note", "[frame_xr][fovaxis]") {
    App();
    auto& v = Settings::values;
    Keep<u32, true> k1{v.vr_immersive_wider_view_level};
    Keep<s32, true> k2{v.vr_immersive_render_margin_x};
    Keep<s32, true> k3{v.vr_immersive_render_margin_y};
    Keep<s32, true> k4{v.vr_immersive_scale_x};
    Keep<s32, true> k5{v.vr_immersive_scale_y};
    Keep<u32, true> k6{v.vr_immersive_scale_mode};
    // The game's own Render Margin 1.2x (as an older build saved it), the axes following it.
    Values(v.vr_immersive_wider_view_level, 4u, 2u);
    Values(v.vr_immersive_render_margin_x, -1, -1);
    Values(v.vr_immersive_render_margin_y, -1, -1);
    v.vr_immersive_render_margin_x.SetGlobal(true);
    v.vr_immersive_render_margin_y.SetGlobal(true);
    Values(v.vr_immersive_scale_x, 0, 0);
    Values(v.vr_immersive_scale_y, 0, 0);
    Values(v.vr_immersive_scale_mode, 1u, 1u);
    v.vr_immersive_scale_x.SetGlobal(true);
    v.vr_immersive_scale_y.SetGlobal(true);
    v.vr_immersive_scale_mode.SetGlobal(true);
    Settings::SetConfiguringGlobal(false);
    {
        ConfigureFrameVr w;
        auto* mx = In<QComboBox>(w, "frame_vr_margin_x_row");
        auto* my = In<QComboBox>(w, "frame_vr_margin_y_row");
        auto* sx = In<QComboBox>(w, "frame_vr_scale_x_row");
        auto* warp_x = w.findChild<QWidget*>(QStringLiteral("frame_vr_scale_x_warp"));
        auto* warp_y = w.findChild<QWidget*>(QStringLiteral("frame_vr_scale_y_warp"));
        REQUIRE(mx);
        REQUIRE(my);
        REQUIRE(sx);
        REQUIRE(warp_x);
        REQUIRE(warp_y);
        CHECK(mx->count() == 5 + ConfigurationShared::USE_GLOBAL_OFFSET);
        // Both show the game's own 1.2x (its Render Margin decides them).
        CHECK(mx->currentIndex() == 2 + ConfigurationShared::USE_GLOBAL_OFFSET);
        CHECK(my->currentIndex() == 2 + ConfigurationShared::USE_GLOBAL_OFFSET);
        CHECK(warp_x->isHidden());
        CHECK(warp_y->isHidden());
        // Field of View: both margin rows are hidden (Render Margin is Stretch's), no note.
        auto* row_x = w.findChild<QWidget*>(QStringLiteral("frame_vr_margin_x_row"));
        auto* row_y = w.findChild<QWidget*>(QStringLiteral("frame_vr_margin_y_row"));
        REQUIRE(row_x);
        REQUIRE(row_y);
        CHECK(row_x->isHidden());
        CHECK(row_y->isHidden());
        CHECK(!w.findChild<QWidget*>(QStringLiteral("frame_vr_margin_x_note")));
        // +70 % is past Field of View's 1.6x (the scales alone): the note shows, live.
        sx->setCurrentIndex(34 + ConfigurationShared::USE_GLOBAL_OFFSET); // +70 %
        CHECK(!warp_x->isHidden());
        CHECK(warp_y->isHidden());
        // Stretch: the margin rows show, under Scale Mode, with the values they had; May Warp
        // goes: the caps take +70 % across with 0 % up and down (the game's frame draws 1.13x
        // across, stretched 1.5).
        auto* mode = In<QComboBox>(w, "frame_vr_scale_mode_row");
        REQUIRE(mode);
        mode->setCurrentIndex(0 + ConfigurationShared::USE_GLOBAL_OFFSET); // Stretch
        CHECK(!row_x->isHidden());
        CHECK(!row_y->isHidden());
        CHECK(mx->currentIndex() == 2 + ConfigurationShared::USE_GLOBAL_OFFSET);
        CHECK(warp_x->isHidden());
        // Under Scale Mode, so a change of mode moves nothing above it.
        QLayout* box = row_x->parentWidget()->layout();
        REQUIRE(box);
        CHECK(box->indexOf(row_x) == box->indexOf(mode->parentWidget()) + 1);
        CHECK(box->indexOf(row_y) == box->indexOf(row_x) + 1);
        mx->setCurrentIndex(ConfigurationShared::USE_GLOBAL_INDEX); // Horizontal: Use global
        mode->setCurrentIndex(ConfigurationShared::USE_GLOBAL_INDEX); // back to the global FoV
        CHECK(row_x->isHidden());
        CHECK(row_y->isHidden());
        CHECK(mx->currentIndex() == ConfigurationShared::USE_GLOBAL_INDEX); // kept while hidden
        CHECK(my->currentIndex() == 2 + ConfigurationShared::USE_GLOBAL_OFFSET);
        CHECK(!warp_x->isHidden());
        Shot(w, "qt-vr-margin-axes-may-warp.png");
        // Horizontal back to Use global: the game's Render Margin goes, Vertical keeps 1.2x.
        w.ApplyConfiguration();
    }
    CHECK(v.vr_immersive_wider_view_level.UsingGlobal());
    CHECK(v.vr_immersive_render_margin_x.UsingGlobal());
    CHECK(!v.vr_immersive_render_margin_y.UsingGlobal());
    CHECK(v.vr_immersive_render_margin_y.GetValue() == 2);
    CHECK(v.vr_immersive_scale_x.GetValue() == 70);
    {
        // Shown again: Horizontal on Use global (1.4x), Vertical its own 1.2x.
        ConfigureFrameVr w;
        auto* mx = In<QComboBox>(w, "frame_vr_margin_x_row");
        auto* my = In<QComboBox>(w, "frame_vr_margin_y_row");
        REQUIRE(mx);
        REQUIRE(my);
        CHECK(mx->currentIndex() == ConfigurationShared::USE_GLOBAL_INDEX);
        CHECK(my->currentIndex() == 2 + ConfigurationShared::USE_GLOBAL_OFFSET);
        // Untouched rows write nothing.
        w.ApplyConfiguration();
    }
    CHECK(v.vr_immersive_render_margin_x.UsingGlobal());
    CHECK(v.vr_immersive_render_margin_y.GetValue() == 2);
    Settings::SetConfiguringGlobal(true);
}
