// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame: the "VR" group on the Graphics tab (global and per application): CitraVR's
// immersive mode settings, with CitraVR's keys (Settings::values.vr_*). Built in code, no .ui.

#pragma once

#include <QGroupBox>
#include "citra_qt/configuration/configuration_shared.h"

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QSlider;
class QSpinBox;
class QWidget;

class ConfigureFrameVr : public QGroupBox {
    Q_OBJECT

public:
    explicit ConfigureFrameVr(QWidget* parent = nullptr);
    ~ConfigureFrameVr() override;

    void SetConfiguration();
    void ApplyConfiguration();

private:
    void SetupPerGameUI();
    void ShowOffset(float right_deg, float up_deg);
    void SetMatchCameraTips(); ///< AzaharFrame fovonce: each choice's own tooltip

    QCheckBox* hide_bottom_check;
    /// Per application: the tristate tracker (Use global / on / off).
    ConfigurationShared::CheckState hide_bottom_state = ConfigurationShared::CheckState::Global;
    /// AzaharFrame depth, fovonce: Match Game Camera, Off | Set Once | Always, global and per game
    QWidget* match_camera_row;
    QComboBox* match_camera_combo;
    /// Global settings only (hidden in a game's properties).
    QWidget* face_row;
    QComboBox* face_combo;
    QWidget* background_row;
    QComboBox* background_combo;
    QWidget* window_scale_row; ///< global only
    QComboBox* window_scale_combo;
    QWidget* curve_row; ///< AzaharFrame curve, global only
    QComboBox* curve_combo;
    QWidget* screen_layout_row; ///< AzaharFrame layout, global only
    QComboBox* screen_layout_combo;
    QWidget* motion_row;
    QComboBox* motion_combo;
    QCheckBox* show_controllers_check; ///< AzaharFrame vrvis, global only
    QWidget* controllers_in_front_row;     ///< AzaharFrame ctrlfront, global only
    QComboBox* controllers_in_front_combo;
    QCheckBox* show_stats_check;       ///< AzaharFrame vrvis, global only
    QWidget* stats_position_row;       ///< AzaharFrame statspos, global only
    QComboBox* stats_position_combo;
    QWidget* shape_row;
    QComboBox* shape_combo;
    QWidget* mode_row;
    QComboBox* mode_combo;
    QWidget* offset_row; ///< AzaharFrame vrsize, global only: the Immersive Offset and a reset
    QLabel* offset_label;
    QPushButton* offset_reset;
    bool offset_reset_pending = false; ///< Reset pressed: OK sets the offset to 0
    QWidget* fov_row;
    QComboBox* fov_scope; ///< per application only: global / this application
    QSpinBox* fov_spin;
    QWidget* wider_row; ///< AzaharFrame widerview: Off, 1.1x to 1.4x
    QComboBox* wider_combo;
    QWidget* scale_x_row; ///< AzaharFrame kiu, fov100: Horizontal Scale, -100 % to +100 %
    QComboBox* scale_x_combo;
    QWidget* scale_y_row; ///< Vertical Scale
    QComboBox* scale_y_combo;
    QWidget* scale_mode_row; ///< AzaharFrame fovscale: Scale Mode, Stretch or Field of View
    QComboBox* scale_mode_combo;
    QWidget* depth_row;     ///< AzaharFrame natural: World Scale (vr_immersive_world_scale)
    QComboBox* world_scope; ///< per application only: global / this application
    QSlider* world_slider;  ///< Names::kWorldScaleStops' indexes
    QLabel* world_label;
    float world_value = 100.0f; ///< percent: as set, or the stop the slider was moved to
    QCheckBox* follow_check; ///< AzaharFrame natural: Follow Screen Size
    ConfigurationShared::CheckState follow_state = ConfigurationShared::CheckState::Global;
    QCheckBox* clarity_check; ///< AzaharFrame flash: Motion Clarity
    ConfigurationShared::CheckState clarity_state = ConfigurationShared::CheckState::Global;
    QWidget* immersive_depth_row; ///< AzaharFrame natural: the Full modes' 3D Depth
    QComboBox* immersive_depth_scope; ///< per application only: global / this application
    QSpinBox* immersive_depth_spin;
    QWidget* register_row;
    QComboBox* register_scope; ///< per application only: global / this application
    QSpinBox* register_spin;
    QWidget* eye_row;
    QComboBox* eye_scope; ///< per application only
    QLineEdit* eye_edit;
};
