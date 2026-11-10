// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <cmath>
#include <initializer_list>
#include <QCheckBox>
#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QStringList>
#include <QVBoxLayout>
#include "citra_qt/configuration/configuration_shared.h"
#include "citra_qt/configuration/configure_frame_vr.h"
#include "common/settings.h"

namespace {

QWidget* MakeRow(QWidget* parent, const QString& name, const QString& label_text,
                 const QString& tooltip, std::initializer_list<QWidget*> fields) {
    auto* row = new QWidget(parent);
    row->setObjectName(name); // ConfigurationShared::SetHighlight styles by object name
    auto* layout = new QHBoxLayout(row);
    layout->setContentsMargins(0, 0, 0, 0);
    auto* label = new QLabel(label_text, row);
    layout->addWidget(label);
    layout->addStretch();
    for (QWidget* field : fields) {
        field->setParent(row);
        layout->addWidget(field);
    }
    row->setToolTip(tooltip);
    return row;
}

// The "Use global / this application" selector of the fields without a combo box of their own
// (as the Graphics tab's Delay Application Render Thread).
QComboBox* MakeScopeCombo(QWidget* parent) {
    auto* combo = new QComboBox(parent);
    combo->addItem(QObject::tr("Use global"));
    combo->addItem(QObject::tr("Use per-application"));
    return combo;
}

// AzaharFrame curve: the Screen Curve item nearest to `deg` (the combo's order: 0, 15, 30, 45,
// 60 degrees; frame_xr/input/screen_rig.h kCurveStepsDeg).
int CurveIndex(u32 deg) {
    static constexpr int kSteps[] = {0, 15, 30, 45, 60};
    int best = deg == 0 ? 0 : 1;
    for (int i = 1; i < 5; i++) {
        if (std::abs(kSteps[i] - static_cast<int>(deg)) <
            std::abs(kSteps[best] - static_cast<int>(deg))) {
            best = i;
        }
    }
    return best;
}

} // namespace

ConfigureFrameVr::ConfigureFrameVr(QWidget* parent) : QGroupBox(parent) {
    setObjectName(QStringLiteral("frame_vr_group"));
    setTitle(tr("VR"));
    auto* layout = new QVBoxLayout(this);

    hide_bottom_check = new QCheckBox(tr("Hide Bottom Screen"), this);
    hide_bottom_check->setObjectName(QStringLiteral("frame_vr_hide_bottom_check"));
    hide_bottom_check->setToolTip(
        tr("Shows only the top screen in VR. Press Left Bumper + Right Bumper + View to show or "
           "hide the bottom screen. A tap of Left Stick Click shows or hides the other screen. "
           "Takes effect when the game starts."));
    layout->addWidget(hide_bottom_check);

    // Global only. Values 0..1 are Settings::values.vr_face_buttons (frame_xr/input/mapper.h).
    face_combo = new QComboBox(this);
    face_combo->addItem(tr("By Position (Nintendo)"));
    face_combo->addItem(tr("By Label (Xbox)"));
    face_row = MakeRow(
        this, QStringLiteral("frame_vr_face_row"), tr("Face Buttons"),
        tr("How the Frame's A, B, X and Y buttons map to the 3DS. By Position matches the 3DS "
           "layout."),
        {face_combo});
    layout->addWidget(face_row);

    // Global only. Values 0..1 are Settings::values.vr_background (frame_xr/hud.h).
    background_combo = new QComboBox(this);
    background_combo->addItem(tr("Black"));
    background_combo->addItem(tr("Passthrough"));
    background_row = MakeRow(
        this, QStringLiteral("frame_vr_background_row"), tr("Background"),
        tr("What shows behind the screens. Passthrough shows the room, if SteamVR offers it. "
           "The in-game menu has a button for it too."),
        {background_combo});
    layout->addWidget(background_row);

    // Global only. The items are 100, 150 and 200 % of Settings::values.vr_window_scale; another
    // value in the file shows as the nearest one (citra_qt/frame_mirror.cpp).
    window_scale_combo = new QComboBox(this);
    window_scale_combo->addItem(tr("100%"), 100);
    window_scale_combo->addItem(tr("150%"), 150);
    window_scale_combo->addItem(tr("200%"), 200);
    window_scale_row = MakeRow(
        this, QStringLiteral("frame_vr_window_scale_row"), tr("Window Scale"),
        tr("Larger makes text and buttons bigger and shows less of the window at once. Flat "
           "mode is not affected."),
        {window_scale_combo});
    layout->addWidget(window_scale_row);

    // Global only. Values 0..2 are Settings::values.vr_motion_controls (frame_xr/input/motion.h).
    motion_combo = new QComboBox(this);
    motion_combo->addItem(tr("Off"));
    motion_combo->addItem(tr("Right Controller"));
    motion_combo->addItem(tr("Left Controller"));
    motion_row = MakeRow(
        this, QStringLiteral("frame_vr_motion_row"), tr("Motion Controls"),
        tr("Uses a controller as the 3DS's gyroscope and accelerometer, for games that aim by "
           "moving the 3DS. Hold it like a 3DS, with its beam pointing into the screen. Going "
           "from Off to a controller takes effect the next time VR starts."),
        {motion_combo});
    layout->addWidget(motion_row);

    // AzaharFrame vrvis: global only (Settings::values.vr_show_controllers, vr_show_stats).
    show_controllers_check = new QCheckBox(tr("Show Controllers"), this);
    show_controllers_check->setObjectName(QStringLiteral("frame_vr_show_controllers_check"));
    show_controllers_check->setToolTip(
        tr("Shows the Steam Frame controllers in VR. They hide when you put them down."));
    layout->addWidget(show_controllers_check);
    show_stats_check = new QCheckBox(tr("Show Stats"), this);
    show_stats_check->setObjectName(QStringLiteral("frame_vr_show_stats_check"));
    show_stats_check->setToolTip(
        tr("Shows FPS, speed and frame times under the top screen in VR. The Stats button above "
           "this window switches it too."));
    layout->addWidget(show_stats_check);

    // AzaharFrame curve: global only. The items are the steps of Settings::values.vr_screen_curve
    // (frame_xr/input/screen_rig.h kCurveStepsDeg); another value in the file shows as the
    // nearest one.
    curve_combo = new QComboBox(this);
    curve_combo->addItem(tr("Off"), 0);
    curve_combo->addItem(tr("Gentle"), 15);
    curve_combo->addItem(tr("Medium"), 30);
    curve_combo->addItem(tr("Strong"), 45);
    curve_combo->addItem(tr("Max"), 60);
    curve_row = MakeRow(this, QStringLiteral("frame_vr_curve_row"), tr("Screen Curve"),
                        tr("Not in immersive mode. In VR: grip a screen, D-pad up or down."),
                        {curve_combo});
    layout->addWidget(curve_row);

    // Values 0..5 are Settings::values.vr_top_aspect (frame_xr/immersive.h).
    shape_combo = new QComboBox(this);
    shape_combo->addItem(tr("3DS (5:3)"));
    shape_combo->addItem(tr("Widescreen (16:9)"));
    shape_combo->addItem(tr("Ultrawide (21:9)"));
    shape_combo->addItem(tr("Standard (4:3)"));
    shape_combo->addItem(tr("3:2"));
    shape_combo->addItem(tr("16:10"));
    shape_row = MakeRow(
        this, QStringLiteral("frame_vr_shape_row"), tr("Screen Shape"),
        tr("The shape of the top screen in VR, for games or mods that change the aspect."),
        {shape_combo});
    layout->addWidget(shape_row);

    // Values 0..3 are Settings::values.vr_immersive_mode (frame_xr/immersive.h).
    mode_combo = new QComboBox(this);
    mode_combo->addItem(tr("Off"));
    mode_combo->addItem(tr("Wide (3DoF)"));
    mode_combo->addItem(tr("Full (6DoF): Ocarina of Time, Majora's Mask"));
    mode_combo->addItem(tr("Full (6DoF): Other Games"));
    mode_row = MakeRow(
        this, QStringLiteral("frame_vr_mode_row"), tr("Immersive Mode"),
        tr("Draws a wider view of the game around you. Full also turns the game camera with "
           "your head. Many games show graphical glitches. Press Left Bumper + Right Bumper + "
           "View to show the bottom screen."),
        {mode_combo});
    layout->addWidget(mode_row);

    // AzaharFrame vrsize: global only (Settings::values.vr_immersive_offset_x, _y). Grip + D-pad
    // sets it in VR; here it shows, and Reset puts it back.
    offset_label = new QLabel(this);
    offset_reset = new QPushButton(tr("Reset"), this);
    offset_row = MakeRow(
        this, QStringLiteral("frame_vr_offset_row"), tr("Immersive Offset"),
        tr("Moves the immersive screen a little to fit your lenses, in every game. In VR, hold "
           "the immersive screen with a grip and press the D-pad."),
        {offset_label, offset_reset});
    layout->addWidget(offset_row);
    connect(offset_reset, &QPushButton::clicked, this, [this] {
        offset_reset_pending = true;
        ShowOffset(0.0f, 0.0f);
    });

    fov_scope = MakeScopeCombo(this);
    fov_spin = new QSpinBox(this);
    fov_spin->setRange(30, 110);
    fov_spin->setSuffix(QStringLiteral("\u00B0"));
    fov_row = MakeRow(this, QStringLiteral("frame_vr_fov_row"), tr("Immersive Game FOV"),
                      tr("The game camera's vertical field of view in degrees. Change it only if a "
                         "mod changes the game's FOV."),
                      {fov_scope, fov_spin});
    layout->addWidget(fov_row);

    // Settings::values.vr_immersive_wider_view (frame_xr/immersive_layout.h).
    wider_view_check = new QCheckBox(tr("Wider View"), this);
    wider_view_check->setObjectName(QStringLiteral("frame_vr_wider_view_check"));
    wider_view_check->setToolTip(
        tr("Full (6DoF) only. Making the screen bigger also shows more around the game's view. "
           "Some games show black or missing scenery at the edges. Takes effect when the game "
           "starts."));
    layout->addWidget(wider_view_check);

    // Values 0..2 are Settings::values.vr_immersive_squeeze (frame_xr/immersive_layout.h).
    squeeze_combo = new QComboBox(this);
    squeeze_combo->addItem(tr("Off"));
    squeeze_combo->addItem(QStringLiteral("10%"));
    squeeze_combo->addItem(QStringLiteral("20%"));
    squeeze_row = MakeRow(
        this, QStringLiteral("frame_vr_squeeze_row"), tr("Squeeze Width"),
        tr("Full (6DoF) only. Makes the screen narrower so the HUD at the sides moves into "
           "view. The game looks slightly thinner. Takes effect when the game starts."),
        {squeeze_combo});
    layout->addWidget(squeeze_row);

    // Values 0..2 are 10^n (Settings::values.vr_immersive_positional_game_scaler).
    depth_combo = new QComboBox(this);
    depth_combo->addItem(QStringLiteral("1x"));
    depth_combo->addItem(QStringLiteral("10x"));
    depth_combo->addItem(QStringLiteral("100x"));
    depth_row = MakeRow(this, QStringLiteral("frame_vr_depth_row"), tr("Depth Factor Multiplier"),
                        tr("How far the game camera moves with your head, together with the 3D "
                           "depth. Some games need a larger value."),
                        {depth_combo});
    layout->addWidget(depth_row);

    register_scope = MakeScopeCombo(this);
    register_spin = new QSpinBox(this);
    register_spin->setRange(-1, 92);
    register_spin->setSpecialValueText(tr("Auto"));
    register_row = MakeRow(this, QStringLiteral("frame_vr_register_row"),
                           tr("View Matrix Register"),
                           tr("Full (6DoF): Other Games only. Auto finds it while you "
                              "play."),
                           {register_scope, register_spin});
    layout->addWidget(register_row);

    eye_scope = MakeScopeCombo(this);
    eye_edit = new QLineEdit(this);
    eye_edit->setPlaceholderText(tr("Auto (e.g. 87,2)"));
    eye_row = MakeRow(this, QStringLiteral("frame_vr_eye_row"), tr("Eye Indicator Register"),
                      tr("Full (6DoF): Other Games only. Register and index that tell "
                         "the eyes apart. Leave empty to find it automatically."),
                      {eye_scope, eye_edit});
    layout->addWidget(eye_row);

    SetupPerGameUI();
    SetConfiguration();
}

ConfigureFrameVr::~ConfigureFrameVr() = default;

void ConfigureFrameVr::SetupPerGameUI() {
    // Face Buttons and Motion Controls are not per game.
    face_row->setVisible(Settings::IsConfiguringGlobal());
    motion_row->setVisible(Settings::IsConfiguringGlobal());
    window_scale_row->setVisible(Settings::IsConfiguringGlobal());
    background_row->setVisible(Settings::IsConfiguringGlobal());
    curve_row->setVisible(Settings::IsConfiguringGlobal()); // AzaharFrame curve
    offset_row->setVisible(Settings::IsConfiguringGlobal()); // AzaharFrame vrsize
    show_controllers_check->setVisible(Settings::IsConfiguringGlobal()); // AzaharFrame vrvis
    show_stats_check->setVisible(Settings::IsConfiguringGlobal());       // AzaharFrame vrvis
    if (Settings::IsConfiguringGlobal()) {
        register_scope->setVisible(false);
        eye_scope->setVisible(false);
        fov_scope->setVisible(false);
        // A running application that overrides them blocks the global values.
        const auto lock = [this](QWidget* widget, bool using_global) {
            widget->setEnabled(using_global);
            if (!using_global) {
                widget->setToolTip(tr("The running game has its own setting for this. Change it "
                                      "in the game's Properties."));
            }
        };
        lock(hide_bottom_check, Settings::values.vr_hide_bottom_screen.UsingGlobal());
        lock(shape_combo, Settings::values.vr_top_aspect.UsingGlobal());
        lock(fov_spin, Settings::values.vr_immersive_game_fov.UsingGlobal());
        lock(wider_view_check, Settings::values.vr_immersive_wider_view.UsingGlobal());
        lock(squeeze_combo, Settings::values.vr_immersive_squeeze.UsingGlobal());
        lock(mode_combo, Settings::values.vr_immersive_mode.UsingGlobal());
        lock(depth_combo, Settings::values.vr_immersive_positional_game_scaler.UsingGlobal());
        lock(register_spin, Settings::values.vr_si_mode_register_offset.UsingGlobal());
        lock(eye_edit, Settings::values.vr_immersive_eye_indicator.UsingGlobal());
        return;
    }

    ConfigurationShared::SetColoredTristate(
        hide_bottom_check, Settings::values.vr_hide_bottom_screen, hide_bottom_state);
    ConfigurationShared::SetColoredComboBox(
        shape_combo, shape_row, static_cast<int>(Settings::values.vr_top_aspect.GetValue(true)));
    ConfigurationShared::SetColoredTristate(
        wider_view_check, Settings::values.vr_immersive_wider_view, wider_view_state);
    ConfigurationShared::SetColoredComboBox(
        squeeze_combo, squeeze_row,
        static_cast<int>(Settings::values.vr_immersive_squeeze.GetValue(true)));
    ConfigurationShared::SetColoredComboBox(
        mode_combo, mode_row, static_cast<int>(Settings::values.vr_immersive_mode.GetValue(true)));
    ConfigurationShared::SetColoredComboBox(
        depth_combo, depth_row,
        static_cast<int>(Settings::values.vr_immersive_positional_game_scaler.GetValue(true)));

    connect(register_scope, qOverload<int>(&QComboBox::activated), this, [this](int index) {
        register_spin->setEnabled(index == 1);
        ConfigurationShared::SetHighlight(register_row, index == 1);
    });
    connect(fov_scope, qOverload<int>(&QComboBox::activated), this, [this](int index) {
        fov_spin->setEnabled(index == 1);
        ConfigurationShared::SetHighlight(fov_row, index == 1);
    });
    connect(eye_scope, qOverload<int>(&QComboBox::activated), this, [this](int index) {
        eye_edit->setEnabled(index == 1);
        ConfigurationShared::SetHighlight(eye_row, index == 1);
    });
}

void ConfigureFrameVr::SetConfiguration() {
    const auto& mode = Settings::values.vr_immersive_mode;
    const auto& depth = Settings::values.vr_immersive_positional_game_scaler;
    const auto& reg = Settings::values.vr_si_mode_register_offset;
    const auto& eye = Settings::values.vr_immersive_eye_indicator;

    // AzaharFrame: the global window shows the global values, also while a running game
    // overrides some (those rows are disabled). GetValue() alone gave the game's own value
    // there, and if the game stopped while the window was open (the in-game menu in VR can stop
    // it), OK would then have saved that value as the global one.
    const bool global = Settings::IsConfiguringGlobal();
    hide_bottom_check->setChecked(Settings::values.vr_hide_bottom_screen.GetValue(global));
    register_spin->setValue(reg.GetValue(global));
    fov_spin->setValue(Settings::values.vr_immersive_game_fov.GetValue(global));
    wider_view_check->setChecked(Settings::values.vr_immersive_wider_view.GetValue(global));
    eye_edit->setText(QString::fromStdString(eye.GetValue(global)));

    if (global) {
        shape_combo->setCurrentIndex(
            static_cast<int>(Settings::values.vr_top_aspect.GetValue(true)));
        squeeze_combo->setCurrentIndex(
            static_cast<int>(Settings::values.vr_immersive_squeeze.GetValue(true)));
        mode_combo->setCurrentIndex(static_cast<int>(mode.GetValue(true)));
        face_combo->setCurrentIndex(static_cast<int>(Settings::values.vr_face_buttons.GetValue()));
        motion_combo->setCurrentIndex(
            static_cast<int>(Settings::values.vr_motion_controls.GetValue()));
        background_combo->setCurrentIndex(
            static_cast<int>(Settings::values.vr_background.GetValue()));
        show_controllers_check->setChecked(Settings::values.vr_show_controllers.GetValue());
        show_stats_check->setChecked(Settings::values.vr_show_stats.GetValue());
        window_scale_combo->setCurrentIndex(std::clamp(
            (static_cast<int>(Settings::values.vr_window_scale.GetValue()) - 100 + 25) / 50, 0, 2));
        depth_combo->setCurrentIndex(static_cast<int>(depth.GetValue(true)));
        curve_combo->setCurrentIndex(CurveIndex(Settings::values.vr_screen_curve.GetValue()));
        offset_reset_pending = false;
        ShowOffset(Settings::values.vr_immersive_offset_x.GetValue(),
                   Settings::values.vr_immersive_offset_y.GetValue());
        return;
    }

    ConfigurationShared::SetPerGameSetting(shape_combo, &Settings::values.vr_top_aspect);
    ConfigurationShared::SetHighlight(shape_row, !Settings::values.vr_top_aspect.UsingGlobal());
    ConfigurationShared::SetPerGameSetting(squeeze_combo, &Settings::values.vr_immersive_squeeze);
    ConfigurationShared::SetHighlight(squeeze_row,
                                      !Settings::values.vr_immersive_squeeze.UsingGlobal());
    fov_scope->setCurrentIndex(Settings::values.vr_immersive_game_fov.UsingGlobal() ? 0 : 1);
    fov_spin->setEnabled(!Settings::values.vr_immersive_game_fov.UsingGlobal());
    ConfigurationShared::SetHighlight(fov_row,
                                      !Settings::values.vr_immersive_game_fov.UsingGlobal());
    ConfigurationShared::SetPerGameSetting(mode_combo, &mode);
    ConfigurationShared::SetHighlight(mode_row, !mode.UsingGlobal());
    ConfigurationShared::SetPerGameSetting(depth_combo, &depth);
    ConfigurationShared::SetHighlight(depth_row, !depth.UsingGlobal());

    register_scope->setCurrentIndex(reg.UsingGlobal() ? 0 : 1);
    register_spin->setEnabled(!reg.UsingGlobal());
    ConfigurationShared::SetHighlight(register_row, !reg.UsingGlobal());
    eye_scope->setCurrentIndex(eye.UsingGlobal() ? 0 : 1);
    eye_edit->setEnabled(!eye.UsingGlobal());
    ConfigurationShared::SetHighlight(eye_row, !eye.UsingGlobal());
}

void ConfigureFrameVr::ShowOffset(float right_deg, float up_deg) {
    // As the toast in VR: "Right 1.5°, Up 0.5°", or None.
    QStringList parts;
    if (right_deg != 0.0f) {
        parts << (right_deg > 0.0f ? tr("Right %1\u00B0") : tr("Left %1\u00B0"))
                     .arg(QString::number(std::abs(right_deg), 'f', 1));
    }
    if (up_deg != 0.0f) {
        parts << (up_deg > 0.0f ? tr("Up %1\u00B0") : tr("Down %1\u00B0"))
                     .arg(QString::number(std::abs(up_deg), 'f', 1));
    }
    offset_label->setText(parts.isEmpty() ? tr("None") : parts.join(QStringLiteral(", ")));
    offset_reset->setEnabled(!parts.isEmpty());
}

void ConfigureFrameVr::ApplyConfiguration() {
    if (Settings::IsConfiguringGlobal()) {
        if (offset_reset_pending) { // AzaharFrame vrsize
            offset_reset_pending = false;
            Settings::values.vr_immersive_offset_x.SetValue(0.0f);
            Settings::values.vr_immersive_offset_y.SetValue(0.0f);
        }
        Settings::values.vr_face_buttons.SetValue(static_cast<u32>(face_combo->currentIndex()));
        Settings::values.vr_motion_controls.SetValue(
            static_cast<u32>(motion_combo->currentIndex()));
        Settings::values.vr_background.SetValue(
            static_cast<u32>(background_combo->currentIndex()));
        Settings::values.vr_show_controllers.SetValue(show_controllers_check->isChecked());
        Settings::values.vr_show_stats.SetValue(show_stats_check->isChecked());
        // AzaharFrame curve: kept as it is while the item shown is still its nearest one.
        if (CurveIndex(Settings::values.vr_screen_curve.GetValue()) != curve_combo->currentIndex()) {
            Settings::values.vr_screen_curve.SetValue(
                static_cast<u32>(curve_combo->currentData().toInt()));
        }
        // Kept as it is when the item shown is still the nearest one (a value typed into the file).
        const int percent = window_scale_combo->currentData().toInt();
        if (std::clamp((static_cast<int>(Settings::values.vr_window_scale.GetValue()) - 75) / 50,
                       0, 2) != window_scale_combo->currentIndex()) {
            Settings::values.vr_window_scale.SetValue(static_cast<u32>(percent));
        }
    }
    ConfigurationShared::ApplyPerGameSetting(&Settings::values.vr_hide_bottom_screen,
                                             hide_bottom_check, hide_bottom_state);
    ConfigurationShared::ApplyPerGameSetting(&Settings::values.vr_top_aspect, shape_combo);
    ConfigurationShared::ApplyPerGameSetting(&Settings::values.vr_immersive_game_fov, fov_scope,
                                             [this](s32) { return fov_spin->value(); });
    ConfigurationShared::ApplyPerGameSetting(&Settings::values.vr_immersive_wider_view,
                                             wider_view_check, wider_view_state);
    ConfigurationShared::ApplyPerGameSetting(&Settings::values.vr_immersive_squeeze,
                                             squeeze_combo);
    ConfigurationShared::ApplyPerGameSetting(&Settings::values.vr_immersive_mode, mode_combo);
    ConfigurationShared::ApplyPerGameSetting(&Settings::values.vr_immersive_positional_game_scaler,
                                             depth_combo);
    ConfigurationShared::ApplyPerGameSetting(&Settings::values.vr_si_mode_register_offset,
                                             register_scope,
                                             [this](s32) { return register_spin->value(); });
    ConfigurationShared::ApplyPerGameSetting(
        &Settings::values.vr_immersive_eye_indicator, eye_scope,
        [this](s32) { return eye_edit->text().trimmed().toStdString(); });
}
