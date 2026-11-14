// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <array>
#include <cmath>
#include <functional>
#include <initializer_list>
#include <utility>
#include <vector>
#include <QCheckBox>
#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSlider>
#include <QSpinBox>
#include <QStringList>
#include <QVBoxLayout>
#include "citra_qt/configuration/configuration_shared.h"
#include "citra_qt/configuration/configure_frame_vr.h"
#include "common/settings.h"
#include "frame_xr/immersive_names.h" // AzaharFrame kiu

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
// 60, 75, 90 degrees; frame_xr/input/screen_rig.h kCurveStepsDeg).
// AzaharFrame kiu: Horizontal and Vertical Scale (Settings::values.vr_immersive_scale_x and _y)
// are signed percent, -30 to +30 in 5 % steps (frame_xr/immersive_layout.h ScalePercent). The
// combo's item n is -30 + n x 5 %, so item 6 is 0 %.
constexpr s32 kScaleStep = 5;
constexpr s32 kScaleMax = 30;
int ScaleIndex(s32 percent) {
    const s32 p = std::clamp(percent, -kScaleMax, kScaleMax) + kScaleMax;
    return static_cast<int>((p + kScaleStep / 2) / kScaleStep);
}
s32 ScaleOfIndex(s32 index) {
    return std::clamp(index, 0, 2 * kScaleMax / kScaleStep) * kScaleStep - kScaleMax;
}
QComboBox* MakeScaleCombo(QWidget* parent) {
    auto* combo = new QComboBox(parent);
    for (s32 p = -kScaleMax; p <= kScaleMax; p += kScaleStep) {
        combo->addItem(p > 0 ? QStringLiteral("+%1%").arg(p) : QStringLiteral("%1%").arg(p));
    }
    return combo;
}

int CurveIndex(u32 deg) {
    static constexpr int kSteps[] = {0, 15, 30, 45, 60, 75, 90};
    int best = deg == 0 ? 0 : 1;
    for (int i = 1; i < 7; i++) {
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

    // Values 0..3 are Settings::values.vr_motion_controls (frame_xr/input/motion.h). AzaharFrame
    // kiu: global and per game, as the rows of the VR group (a game's Properties have Use global).
    motion_combo = new QComboBox(this);
    motion_combo->addItem(tr("Off"));
    motion_combo->addItem(tr("Right Controller"));
    motion_combo->addItem(tr("Left Controller"));
    motion_combo->addItem(tr("Headset"));
    motion_row = MakeRow(
        this, QStringLiteral("frame_vr_motion_row"), tr("Motion Controls"),
        tr("Uses a controller or your head as the 3DS's gyroscope and accelerometer, for games "
           "that aim by moving the 3DS. Hold a controller like a 3DS, with its beam pointing "
           "into the screen, or turn and tilt your head. Going from Off to another choice takes "
           "effect the next time VR starts."),
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
        tr("Shows FPS, speed and frame times in VR. The Stats button above this window "
           "switches it too."));
    layout->addWidget(show_stats_check);
    // AzaharFrame statspos: global only. Values 0..1 are Settings::values.vr_stats_position
    // (frame_xr/vis/stats_place.h).
    stats_position_combo = new QComboBox(this);
    stats_position_combo->addItem(tr("In Front"));
    stats_position_combo->addItem(tr("On Wrist"));
    stats_position_row = MakeRow(this, QStringLiteral("frame_vr_stats_position_row"),
                                 tr("Stats Position"),
                                 tr("Where the stats show: in front of you, or on your left wrist."),
                                 {stats_position_combo});
    layout->addWidget(stats_position_row);

    // AzaharFrame curve: global only. The items are the steps of Settings::values.vr_screen_curve
    // (frame_xr/input/screen_rig.h kCurveStepsDeg); another value in the file shows as the
    // nearest one.
    curve_combo = new QComboBox(this);
    curve_combo->addItem(tr("Off"), 0);
    curve_combo->addItem(tr("15°"), 15);
    curve_combo->addItem(tr("30°"), 30);
    curve_combo->addItem(tr("45°"), 45);
    curve_combo->addItem(tr("60°"), 60);
    curve_combo->addItem(tr("75°"), 75);
    curve_combo->addItem(tr("90°"), 90);
    curve_row = MakeRow(this, QStringLiteral("frame_vr_curve_row"), tr("Screen Curve"),
                        tr("Not in immersive mode. In VR: grip a screen, D-pad up or down."),
                        {curve_combo});
    layout->addWidget(curve_row);

    // AzaharFrame layout: global only.
    screen_layout_combo = new QComboBox(this);
    screen_layout_combo->addItem(tr("Stacked"), 0);
    screen_layout_combo->addItem(tr("Side by Side"), 1);
    screen_layout_row =
        MakeRow(this, QStringLiteral("frame_vr_screen_layout_row"), tr("Screen Layout"),
                tr("Where the smaller screen sits: below the main one, or to its right. In VR: "
                   "hold the left stick click."),
                {screen_layout_combo});
    layout->addWidget(screen_layout_row);

    // Values 0..6 are Settings::values.vr_top_aspect (frame_xr/immersive.h).
    shape_combo = new QComboBox(this);
    shape_combo->addItem(tr("3DS (5:3)"));
    shape_combo->addItem(tr("Widescreen (16:9)"));
    shape_combo->addItem(tr("Ultrawide (21:9)"));
    shape_combo->addItem(tr("Standard (4:3)"));
    shape_combo->addItem(tr("3:2"));
    shape_combo->addItem(tr("16:10"));
    shape_combo->addItem(tr("Automatic")); // AzaharFrame fixes7: 6, frame_xr/auto_shape.h
    shape_row = MakeRow(
        this, QStringLiteral("frame_vr_shape_row"), tr("Screen Shape"),
        tr("The shape of the top screen in VR, for games or mods that change the aspect. "
           "Automatic follows the shape the game's camera draws with."),
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
           "the immersive screen with a grip and press D-pad left or right."),
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

    // AzaharFrame depth, fovonce: values 0..2 are Settings::values.vr_immersive_match_camera_mode
    // (Off, Set Once, Always), live.
    match_camera_combo = new QComboBox(this);
    match_camera_combo->setObjectName(QStringLiteral("frame_vr_match_camera_combo"));
    match_camera_combo->addItem(tr("Off"));
    match_camera_combo->addItem(tr("Once"));
    match_camera_combo->addItem(tr("Always"));
    match_camera_row = MakeRow(this, QStringLiteral("frame_vr_match_camera_row"),
                               tr("Detect Game FOV"),
                               tr("Reads the game's field of view so the world lines up with "
                                  "your head."),
                               {match_camera_combo});
    layout->addWidget(match_camera_row);

    // AzaharFrame widerview: values 0..4 are Settings::values.vr_immersive_wider_view_level
    // (frame_xr/immersive_layout.h).
    wider_combo = new QComboBox(this);
    wider_combo->addItem(tr("Off"));
    for (const char* step : {"1.1x", "1.2x", "1.3x", "1.4x"}) {
        wider_combo->addItem(QString::fromLatin1(step));
    }
    // AzaharFrame kiu: Render Margin (was Wider View); the names are in frame_xr/immersive_names.h.
    namespace Names = FrameXr::Immersive::Names;
    wider_row = MakeRow(this, QStringLiteral("frame_vr_wider_row"), tr(Names::kRenderMargin),
                        tr(Names::kRenderMarginTip), {wider_combo});
    layout->addWidget(wider_row);

    // AzaharFrame kiu: Horizontal and Vertical Scale (ScaleIndex).
    scale_x_combo = MakeScaleCombo(this);
    scale_x_row = MakeRow(this, QStringLiteral("frame_vr_scale_x_row"),
                          tr(Names::kHorizontalScale), tr(Names::kHorizontalScaleTip),
                          {scale_x_combo});
    layout->addWidget(scale_x_row);
    scale_y_combo = MakeScaleCombo(this);
    scale_y_row = MakeRow(this, QStringLiteral("frame_vr_scale_y_row"), tr(Names::kVerticalScale),
                          tr(Names::kVerticalScaleTip), {scale_y_combo});
    layout->addWidget(scale_y_row);

    // AzaharFrame natural: World Scale (Settings::values.vr_immersive_world_scale, percent): a
    // slider over Names::kWorldScaleStops with its value beside it ("Automatic" at 100 %); a
    // value between stops (the grips set any) stays as it is unless the slider is moved. Follow
    // Screen Size; the Full modes' 3D Depth (vr_immersive_depth). Names: frame_xr/immersive_names.h.
    world_scope = MakeScopeCombo(this);
    world_slider = new QSlider(Qt::Horizontal, this);
    world_slider->setRange(0, static_cast<int>(Names::kWorldScaleStops.size()) - 1);
    world_slider->setPageStep(6); // a doubling
    world_slider->setMinimumWidth(160);
    world_label = new QLabel(this);
    world_label->setMinimumWidth(70);
    connect(world_slider, &QSlider::valueChanged, this, [this](int i) {
        world_value = Names::kWorldScaleStops[static_cast<std::size_t>(
            std::clamp(i, 0, static_cast<int>(Names::kWorldScaleStops.size()) - 1))];
        world_label->setText(QString::fromStdString(Names::WorldScaleText(world_value)));
    });
    depth_row = MakeRow(this, QStringLiteral("frame_vr_depth_row"),
                        QString::fromUtf8(Names::kWorldScale),
                        QString::fromUtf8(Names::kWorldScaleWindowTip),
                        {world_scope, world_slider, world_label});
    layout->addWidget(depth_row);

    follow_check = new QCheckBox(QString::fromUtf8(Names::kFollowScreen), this);
    follow_check->setObjectName(QStringLiteral("frame_vr_follow_screen_check"));
    follow_check->setToolTip(QString::fromUtf8(Names::kFollowScreenWindowTip));
    layout->addWidget(follow_check);

    immersive_depth_scope = MakeScopeCombo(this);
    immersive_depth_spin = new QSpinBox(this);
    immersive_depth_spin->setRange(0, 100);
    immersive_depth_spin->setSuffix(QStringLiteral("%"));
    immersive_depth_row = MakeRow(this, QStringLiteral("frame_vr_immersive_depth_row"),
                                  QString::fromUtf8(Names::kImmersiveDepth),
                                  QString::fromUtf8(Names::kImmersiveDepthTip),
                                  {immersive_depth_scope, immersive_depth_spin});
    layout->addWidget(immersive_depth_row);

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
    // Face Buttons is not per game (AzaharFrame kiu: Motion Controls is).
    face_row->setVisible(Settings::IsConfiguringGlobal());
    window_scale_row->setVisible(Settings::IsConfiguringGlobal());
    background_row->setVisible(Settings::IsConfiguringGlobal());
    curve_row->setVisible(Settings::IsConfiguringGlobal()); // AzaharFrame curve
    screen_layout_row->setVisible(Settings::IsConfiguringGlobal()); // AzaharFrame layout
    offset_row->setVisible(Settings::IsConfiguringGlobal()); // AzaharFrame vrsize
    show_controllers_check->setVisible(Settings::IsConfiguringGlobal()); // AzaharFrame vrvis
    show_stats_check->setVisible(Settings::IsConfiguringGlobal());       // AzaharFrame vrvis
    stats_position_row->setVisible(Settings::IsConfiguringGlobal()); // AzaharFrame statspos
    if (Settings::IsConfiguringGlobal()) {
        register_scope->setVisible(false);
        eye_scope->setVisible(false);
        fov_scope->setVisible(false);
        immersive_depth_scope->setVisible(false); // AzaharFrame natural
        world_scope->setVisible(false);
        // AzaharFrame kiu: never locked. While the running game has its own value, a row still
        // edits the global value (ApplyConfiguration), the game keeps its own, and a plain hint
        // says so.
        const auto hint = [this](QWidget* widget, bool using_global) {
            if (!using_global) {
                widget->setToolTip(
                    tr("The running game has its own value. This sets the one for other games."));
            }
        };
        hint(hide_bottom_check, Settings::values.vr_hide_bottom_screen.UsingGlobal());
        hint(shape_combo, Settings::values.vr_top_aspect.UsingGlobal());
        hint(fov_spin, Settings::values.vr_immersive_game_fov.UsingGlobal());
        hint(match_camera_combo, Settings::values.vr_immersive_match_camera_mode.UsingGlobal());
        hint(wider_combo, Settings::values.vr_immersive_wider_view_level.UsingGlobal());
        hint(scale_x_combo, Settings::values.vr_immersive_scale_x.UsingGlobal());
        hint(scale_y_combo, Settings::values.vr_immersive_scale_y.UsingGlobal());
        hint(mode_combo, Settings::values.vr_immersive_mode.UsingGlobal());
        hint(register_spin, Settings::values.vr_si_mode_register_offset.UsingGlobal());
        hint(eye_edit, Settings::values.vr_immersive_eye_indicator.UsingGlobal());
        hint(motion_combo, Settings::values.vr_motion_controls.UsingGlobal());
        hint(world_slider, Settings::values.vr_immersive_world_scale.UsingGlobal()); // natural
        hint(follow_check, Settings::values.vr_immersive_follow_screen.UsingGlobal());
        hint(immersive_depth_spin, Settings::values.vr_immersive_depth.UsingGlobal());
        return;
    }

    ConfigurationShared::SetColoredTristate(
        hide_bottom_check, Settings::values.vr_hide_bottom_screen, hide_bottom_state);
    ConfigurationShared::SetColoredComboBox(
        match_camera_combo, match_camera_row,
        static_cast<int>(std::min<u32>(
            Settings::values.vr_immersive_match_camera_mode.GetValue(true), 2))); // fovonce
    ConfigurationShared::SetColoredComboBox(
        shape_combo, shape_row, static_cast<int>(Settings::values.vr_top_aspect.GetValue(true)));
    ConfigurationShared::SetColoredComboBox(
        wider_combo, wider_row,
        static_cast<int>(Settings::values.vr_immersive_wider_view_level.GetValue(true)));
    ConfigurationShared::SetColoredComboBox(
        scale_x_combo, scale_x_row,
        ScaleIndex(Settings::values.vr_immersive_scale_x.GetValue(true)));
    ConfigurationShared::SetColoredComboBox(
        scale_y_combo, scale_y_row,
        ScaleIndex(Settings::values.vr_immersive_scale_y.GetValue(true)));
    ConfigurationShared::SetColoredComboBox(
        mode_combo, mode_row, static_cast<int>(Settings::values.vr_immersive_mode.GetValue(true)));
    ConfigurationShared::SetColoredComboBox( // AzaharFrame kiu
        motion_combo, motion_row,
        static_cast<int>(Settings::values.vr_motion_controls.GetValue(true)));
    ConfigurationShared::SetColoredTristate(
        follow_check, Settings::values.vr_immersive_follow_screen, follow_state);
    connect(world_scope, qOverload<int>(&QComboBox::activated), this, [this](int index) {
        world_slider->setEnabled(index == 1);
        ConfigurationShared::SetHighlight(depth_row, index == 1);
    });

    connect(immersive_depth_scope, qOverload<int>(&QComboBox::activated), this, [this](int index) {
        immersive_depth_spin->setEnabled(index == 1);
        ConfigurationShared::SetHighlight(immersive_depth_row, index == 1);
    });
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
    const auto& world = Settings::values.vr_immersive_world_scale; // AzaharFrame natural
    const auto& reg = Settings::values.vr_si_mode_register_offset;
    const auto& eye = Settings::values.vr_immersive_eye_indicator;

    // AzaharFrame: the global window shows the global values, also while a running game
    // overrides some (those rows are disabled). GetValue() alone gave the game's own value
    // there, and if the game stopped while the window was open (the in-game menu in VR can stop
    // it), OK would then have saved that value as the global one.
    const bool global = Settings::IsConfiguringGlobal();
    hide_bottom_check->setChecked(Settings::values.vr_hide_bottom_screen.GetValue(global));
    follow_check->setChecked(Settings::values.vr_immersive_follow_screen.GetValue(global));
    SetMatchCameraTips(); // AzaharFrame fovonce
    register_spin->setValue(reg.GetValue(global));
    fov_spin->setValue(Settings::values.vr_immersive_game_fov.GetValue(global));
    immersive_depth_spin->setValue(
        static_cast<int>(Settings::values.vr_immersive_depth.GetValue(global)));
    {
        // AzaharFrame natural: the value as it is, the slider at its nearest stop.
        const float value = FrameXr::Immersive::Names::ClampWorldScale(world.GetValue(global));
        world_slider->blockSignals(true);
        world_slider->setValue(FrameXr::Immersive::Names::WorldScaleStop(value));
        world_slider->blockSignals(false);
        world_value = value;
        world_label->setText(
            QString::fromStdString(FrameXr::Immersive::Names::WorldScaleText(value)));
    }
    eye_edit->setText(QString::fromStdString(eye.GetValue(global)));

    if (global) {
        shape_combo->setCurrentIndex(
            static_cast<int>(Settings::values.vr_top_aspect.GetValue(true)));
        scale_x_combo->setCurrentIndex(
            ScaleIndex(Settings::values.vr_immersive_scale_x.GetValue(true)));
        scale_y_combo->setCurrentIndex(
            ScaleIndex(Settings::values.vr_immersive_scale_y.GetValue(true)));
        wider_combo->setCurrentIndex(
            static_cast<int>(Settings::values.vr_immersive_wider_view_level.GetValue(true)));
        mode_combo->setCurrentIndex(static_cast<int>(mode.GetValue(true)));
        match_camera_combo->setCurrentIndex(static_cast<int>(
            std::min<u32>(Settings::values.vr_immersive_match_camera_mode.GetValue(true), 2)));
        face_combo->setCurrentIndex(static_cast<int>(Settings::values.vr_face_buttons.GetValue()));
        motion_combo->setCurrentIndex( // AzaharFrame kiu: the global value, not a game's
            static_cast<int>(Settings::values.vr_motion_controls.GetValue(true)));
        background_combo->setCurrentIndex(
            static_cast<int>(Settings::values.vr_background.GetValue()));
        show_controllers_check->setChecked(Settings::values.vr_show_controllers.GetValue());
        show_stats_check->setChecked(Settings::values.vr_show_stats.GetValue());
        stats_position_combo->setCurrentIndex(
            static_cast<int>(Settings::values.vr_stats_position.GetValue())); // statspos
        window_scale_combo->setCurrentIndex(std::clamp(
            (static_cast<int>(Settings::values.vr_window_scale.GetValue()) - 100 + 25) / 50, 0, 2));
        curve_combo->setCurrentIndex(CurveIndex(Settings::values.vr_screen_curve.GetValue()));
        screen_layout_combo->setCurrentIndex(
            std::min<int>(static_cast<int>(Settings::values.vr_screen_layout.GetValue()), 1));
        offset_reset_pending = false;
        ShowOffset(Settings::values.vr_immersive_offset_x.GetValue(),
                   Settings::values.vr_immersive_offset_y.GetValue());
        return;
    }

    ConfigurationShared::SetPerGameSetting(shape_combo, &Settings::values.vr_top_aspect);
    ConfigurationShared::SetHighlight(shape_row, !Settings::values.vr_top_aspect.UsingGlobal());
    for (auto [combo, setting] :
         {std::pair{scale_x_combo, &Settings::values.vr_immersive_scale_x},
          std::pair{scale_y_combo, &Settings::values.vr_immersive_scale_y}}) {
        combo->setCurrentIndex(setting->UsingGlobal()
                                   ? ConfigurationShared::USE_GLOBAL_INDEX
                                   : ScaleIndex(setting->GetValue()) +
                                         ConfigurationShared::USE_GLOBAL_OFFSET);
    }
    ConfigurationShared::SetPerGameSetting(wider_combo,
                                           &Settings::values.vr_immersive_wider_view_level);
    ConfigurationShared::SetHighlight(
        wider_row, !Settings::values.vr_immersive_wider_view_level.UsingGlobal());
    ConfigurationShared::SetHighlight(scale_x_row,
                                      !Settings::values.vr_immersive_scale_x.UsingGlobal());
    ConfigurationShared::SetHighlight(scale_y_row,
                                      !Settings::values.vr_immersive_scale_y.UsingGlobal());
    fov_scope->setCurrentIndex(Settings::values.vr_immersive_game_fov.UsingGlobal() ? 0 : 1);
    {
        const bool own = !Settings::values.vr_immersive_depth.UsingGlobal();
        immersive_depth_scope->setCurrentIndex(own ? 1 : 0);
        immersive_depth_spin->setEnabled(own);
        ConfigurationShared::SetHighlight(immersive_depth_row, own);
    }
    fov_spin->setEnabled(!Settings::values.vr_immersive_game_fov.UsingGlobal());
    ConfigurationShared::SetHighlight(fov_row,
                                      !Settings::values.vr_immersive_game_fov.UsingGlobal());
    ConfigurationShared::SetPerGameSetting(mode_combo, &mode);
    ConfigurationShared::SetHighlight(mode_row, !mode.UsingGlobal());
    ConfigurationShared::SetPerGameSetting(match_camera_combo,
                                           &Settings::values.vr_immersive_match_camera_mode);
    ConfigurationShared::SetHighlight(
        match_camera_row, !Settings::values.vr_immersive_match_camera_mode.UsingGlobal());
    world_scope->setCurrentIndex(world.UsingGlobal() ? 0 : 1);
    world_slider->setEnabled(!world.UsingGlobal());
    ConfigurationShared::SetHighlight(depth_row, !world.UsingGlobal());
    // AzaharFrame kiu: Motion Controls, the game's own value or Use global
    ConfigurationShared::SetPerGameSetting(motion_combo, &Settings::values.vr_motion_controls);
    ConfigurationShared::SetHighlight(motion_row,
                                      !Settings::values.vr_motion_controls.UsingGlobal());

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

// AzaharFrame fovonce: each choice's tooltip (shown for the item pointed at in the open list, and
// for the box itself while it is picked); "Use global configuration" shows the global choice's.
void ConfigureFrameVr::SetMatchCameraTips() {
    const std::array<QString, 3> tips{
        tr("Uses Immersive Game FOV as set."),
        tr("Measures it once while you play and saves it. Choose again to remeasure."),
        tr("Keeps measuring and eases into changes."),
    };
    QComboBox* box = match_camera_combo;
    const bool per_game = !Settings::IsConfiguringGlobal();
    const int first = per_game ? ConfigurationShared::USE_GLOBAL_OFFSET : 0;
    for (int i = 0; i < static_cast<int>(tips.size()); i++) {
        box->setItemData(first + i, tips[static_cast<std::size_t>(i)], Qt::ToolTipRole);
    }
    if (per_game) {
        const auto global = std::min<std::size_t>(
            Settings::values.vr_immersive_match_camera_mode.GetValue(true), 2);
        box->setItemData(ConfigurationShared::USE_GLOBAL_INDEX, tips[global], Qt::ToolTipRole);
    }
    const auto follow = [box] {
        if (box->isEnabled()) { // a locked box keeps the lock's tooltip
            box->setToolTip(box->currentData(Qt::ToolTipRole).toString());
        }
    };
    follow();
    if (!box->property("frame_tips").toBool()) {
        box->setProperty("frame_tips", true);
        connect(box, qOverload<int>(&QComboBox::currentIndexChanged), box, follow);
    }
}

void ConfigureFrameVr::ApplyConfiguration() {
    if (Settings::IsConfiguringGlobal()) {
        if (offset_reset_pending) { // AzaharFrame vrsize
            offset_reset_pending = false;
            Settings::values.vr_immersive_offset_x.SetValue(0.0f);
            Settings::values.vr_immersive_offset_y.SetValue(0.0f);
        }
        Settings::values.vr_face_buttons.SetValue(static_cast<u32>(face_combo->currentIndex()));
        Settings::values.vr_background.SetValue(
            static_cast<u32>(background_combo->currentIndex()));
        Settings::values.vr_show_controllers.SetValue(show_controllers_check->isChecked());
        Settings::values.vr_show_stats.SetValue(show_stats_check->isChecked());
        Settings::values.vr_stats_position.SetValue(
            static_cast<u32>(stats_position_combo->currentIndex())); // AzaharFrame statspos
        // AzaharFrame curve: kept as it is while the item shown is still its nearest one.
        if (CurveIndex(Settings::values.vr_screen_curve.GetValue()) != curve_combo->currentIndex()) {
            Settings::values.vr_screen_curve.SetValue(
                static_cast<u32>(curve_combo->currentData().toInt()));
        }
        Settings::values.vr_screen_layout.SetValue(
            static_cast<u32>(screen_layout_combo->currentIndex())); // AzaharFrame layout
        // Kept as it is when the item shown is still the nearest one (a value typed into the file).
        const int percent = window_scale_combo->currentData().toInt();
        if (std::clamp((static_cast<int>(Settings::values.vr_window_scale.GetValue()) - 75) / 50,
                       0, 2) != window_scale_combo->currentIndex()) {
            Settings::values.vr_window_scale.SetValue(static_cast<u32>(percent));
        }
    }
    // AzaharFrame kiu: in the global settings every row edits the global value, also while the
    // running game has its own: ApplyPerGameSetting writes only a setting that uses the global
    // value there (SetValue would change the game's), so each is switched to it for the writes
    // and back after, and the game keeps its value.
    std::vector<std::function<void()>> game_values;
    const auto as_global = [&game_values](auto& setting) {
        if (Settings::IsConfiguringGlobal() && !setting.UsingGlobal()) {
            setting.SetGlobal(true);
            game_values.push_back([&setting] { setting.SetGlobal(false); });
        }
    };
    as_global(Settings::values.vr_hide_bottom_screen);
    as_global(Settings::values.vr_immersive_match_camera_mode);
    as_global(Settings::values.vr_top_aspect);
    as_global(Settings::values.vr_motion_controls);
    as_global(Settings::values.vr_immersive_game_fov);
    as_global(Settings::values.vr_immersive_wider_view_level);
    as_global(Settings::values.vr_immersive_scale_x);
    as_global(Settings::values.vr_immersive_scale_y);
    as_global(Settings::values.vr_immersive_mode);
    as_global(Settings::values.vr_immersive_positional_game_scaler);
    as_global(Settings::values.vr_immersive_world_scale); // AzaharFrame natural
    as_global(Settings::values.vr_immersive_follow_screen);
    as_global(Settings::values.vr_immersive_depth);
    as_global(Settings::values.vr_si_mode_register_offset);
    as_global(Settings::values.vr_immersive_eye_indicator);
    ConfigurationShared::ApplyPerGameSetting(&Settings::values.vr_hide_bottom_screen,
                                             hide_bottom_check, hide_bottom_state);
    {
        // AzaharFrame fovonce: Set Once newly chosen for this game: it measures its camera again.
        auto& match = Settings::values.vr_immersive_match_camera_mode;
        const u32 before = match.GetValue();
        ConfigurationShared::ApplyPerGameSetting(&match, match_camera_combo);
        if (!Settings::IsConfiguringGlobal() && before != match.GetValue() &&
            match.GetValue() == static_cast<u32>(Settings::MatchGameCamera::SetOnce)) {
            Settings::values.vr_immersive_fov_measured.SetGlobal(true);
        }
    }
    ConfigurationShared::ApplyPerGameSetting(&Settings::values.vr_top_aspect, shape_combo);
    ConfigurationShared::ApplyPerGameSetting(&Settings::values.vr_motion_controls,
                                             motion_combo); // AzaharFrame kiu
    ConfigurationShared::ApplyPerGameSetting(&Settings::values.vr_immersive_game_fov, fov_scope,
                                             [this](s32) { return fov_spin->value(); });
    ConfigurationShared::ApplyPerGameSetting(&Settings::values.vr_immersive_wider_view_level,
                                             wider_combo);
    ConfigurationShared::ApplyPerGameSetting(&Settings::values.vr_immersive_scale_x, scale_x_combo,
                                             [](s32 index) { return ScaleOfIndex(index); });
    ConfigurationShared::ApplyPerGameSetting(&Settings::values.vr_immersive_scale_y, scale_y_combo,
                                             [](s32 index) { return ScaleOfIndex(index); });
    ConfigurationShared::ApplyPerGameSetting(&Settings::values.vr_immersive_mode, mode_combo);
    ConfigurationShared::ApplyPerGameSetting(&Settings::values.vr_immersive_world_scale,
                                             world_scope, [this](s32) { return world_value; });
    ConfigurationShared::ApplyPerGameSetting(&Settings::values.vr_immersive_follow_screen,
                                             follow_check, follow_state);
    ConfigurationShared::ApplyPerGameSetting(
        &Settings::values.vr_immersive_depth, immersive_depth_scope,
        [this](s32) { return static_cast<u32>(immersive_depth_spin->value()); });
    ConfigurationShared::ApplyPerGameSetting(&Settings::values.vr_si_mode_register_offset,
                                             register_scope,
                                             [this](s32) { return register_spin->value(); });
    ConfigurationShared::ApplyPerGameSetting(
        &Settings::values.vr_immersive_eye_indicator, eye_scope,
        [this](s32) { return eye_edit->text().trimmed().toStdString(); });
    for (const auto& back : game_values) {
        back(); // AzaharFrame kiu: the running game's own values again
    }
}
