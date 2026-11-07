// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <initializer_list>
#include <QCheckBox>
#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QSpinBox>
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

} // namespace

ConfigureFrameVr::ConfigureFrameVr(QWidget* parent) : QGroupBox(parent) {
    setObjectName(QStringLiteral("frame_vr_group"));
    setTitle(tr("VR"));
    auto* layout = new QVBoxLayout(this);

    hide_bottom_check = new QCheckBox(tr("Hide Bottom Screen"), this);
    hide_bottom_check->setObjectName(QStringLiteral("frame_vr_hide_bottom_check"));
    hide_bottom_check->setToolTip(
        tr("Shows only the top screen in VR. Press Left Bumper + Right Bumper + View to show or "
           "hide the bottom screen. Takes effect when the game starts."));
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
        tr("How large this window looks in VR. Larger shows text and buttons bigger and "
           "fits less of the window on the panel. Flat mode is not affected."),
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

    // Values 0..2 are Settings::values.vr_top_aspect (frame_xr/immersive.h).
    shape_combo = new QComboBox(this);
    shape_combo->addItem(tr("3DS (5:3)"));
    shape_combo->addItem(tr("Widescreen (16:9)"));
    shape_combo->addItem(tr("Ultrawide (21:9)"));
    shape_row = MakeRow(
        this, QStringLiteral("frame_vr_shape_row"), tr("Screen Shape"),
        tr("Widen the top screen in VR for games or mods that render widescreen. Takes effect "
           "when the game starts."),
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

    fov_scope = MakeScopeCombo(this);
    fov_spin = new QSpinBox(this);
    fov_spin->setRange(30, 110);
    fov_spin->setSuffix(QStringLiteral("\u00B0"));
    fov_row = MakeRow(this, QStringLiteral("frame_vr_fov_row"), tr("Immersive Game FOV"),
                      tr("The game camera's vertical field of view in degrees. Change it only if a "
                         "mod changes the game's FOV. Takes effect when the game starts."),
                      {fov_scope, fov_spin});
    layout->addWidget(fov_row);

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
    show_controllers_check->setVisible(Settings::IsConfiguringGlobal()); // AzaharFrame vrvis
    show_stats_check->setVisible(Settings::IsConfiguringGlobal());       // AzaharFrame vrvis
    if (Settings::IsConfiguringGlobal()) {
        register_scope->setVisible(false);
        eye_scope->setVisible(false);
        fov_scope->setVisible(false);
        // A running application that overrides them blocks the global values.
        hide_bottom_check->setEnabled(Settings::values.vr_hide_bottom_screen.UsingGlobal());
        shape_combo->setEnabled(Settings::values.vr_top_aspect.UsingGlobal());
        fov_spin->setEnabled(Settings::values.vr_immersive_game_fov.UsingGlobal());
        mode_combo->setEnabled(Settings::values.vr_immersive_mode.UsingGlobal());
        depth_combo->setEnabled(Settings::values.vr_immersive_positional_game_scaler.UsingGlobal());
        register_spin->setEnabled(Settings::values.vr_si_mode_register_offset.UsingGlobal());
        eye_edit->setEnabled(Settings::values.vr_immersive_eye_indicator.UsingGlobal());
        return;
    }

    ConfigurationShared::SetColoredTristate(
        hide_bottom_check, Settings::values.vr_hide_bottom_screen, hide_bottom_state);
    ConfigurationShared::SetColoredComboBox(
        shape_combo, shape_row, static_cast<int>(Settings::values.vr_top_aspect.GetValue(true)));
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

    hide_bottom_check->setChecked(Settings::values.vr_hide_bottom_screen.GetValue());
    register_spin->setValue(reg.GetValue());
    fov_spin->setValue(Settings::values.vr_immersive_game_fov.GetValue());
    eye_edit->setText(QString::fromStdString(eye.GetValue()));

    if (Settings::IsConfiguringGlobal()) {
        shape_combo->setCurrentIndex(static_cast<int>(Settings::values.vr_top_aspect.GetValue()));
        mode_combo->setCurrentIndex(static_cast<int>(mode.GetValue()));
        face_combo->setCurrentIndex(static_cast<int>(Settings::values.vr_face_buttons.GetValue()));
        motion_combo->setCurrentIndex(
            static_cast<int>(Settings::values.vr_motion_controls.GetValue()));
        background_combo->setCurrentIndex(
            static_cast<int>(Settings::values.vr_background.GetValue()));
        show_controllers_check->setChecked(Settings::values.vr_show_controllers.GetValue());
        show_stats_check->setChecked(Settings::values.vr_show_stats.GetValue());
        window_scale_combo->setCurrentIndex(std::clamp(
            (static_cast<int>(Settings::values.vr_window_scale.GetValue()) - 100 + 25) / 50, 0, 2));
        depth_combo->setCurrentIndex(static_cast<int>(depth.GetValue()));
        return;
    }

    ConfigurationShared::SetPerGameSetting(shape_combo, &Settings::values.vr_top_aspect);
    ConfigurationShared::SetHighlight(shape_row, !Settings::values.vr_top_aspect.UsingGlobal());
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

void ConfigureFrameVr::ApplyConfiguration() {
    if (Settings::IsConfiguringGlobal()) {
        Settings::values.vr_face_buttons.SetValue(static_cast<u32>(face_combo->currentIndex()));
        Settings::values.vr_motion_controls.SetValue(
            static_cast<u32>(motion_combo->currentIndex()));
        Settings::values.vr_background.SetValue(
            static_cast<u32>(background_combo->currentIndex()));
        Settings::values.vr_show_controllers.SetValue(show_controllers_check->isChecked());
        Settings::values.vr_show_stats.SetValue(show_stats_check->isChecked());
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
