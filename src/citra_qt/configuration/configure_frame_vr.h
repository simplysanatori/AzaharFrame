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

    QCheckBox* hide_bottom_check;
    /// Per application: the tristate tracker (Use global / on / off).
    ConfigurationShared::CheckState hide_bottom_state = ConfigurationShared::CheckState::Global;
    /// Global settings only (hidden in a game's properties).
    QWidget* face_row;
    QComboBox* face_combo;
    QWidget* background_row;
    QComboBox* background_combo;
    QWidget* window_scale_row; ///< global only
    QComboBox* window_scale_combo;
    QWidget* curve_row; ///< AzaharFrame curve, global only
    QComboBox* curve_combo;
    QWidget* motion_row;
    QComboBox* motion_combo;
    QCheckBox* show_controllers_check; ///< AzaharFrame vrvis, global only
    QCheckBox* show_stats_check;       ///< AzaharFrame vrvis, global only
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
    QCheckBox* wider_view_check;
    ConfigurationShared::CheckState wider_view_state = ConfigurationShared::CheckState::Global;
    QWidget* squeeze_row;
    QComboBox* squeeze_combo;
    QWidget* depth_row;
    QComboBox* depth_combo;
    QWidget* register_row;
    QComboBox* register_scope; ///< per application only: global / this application
    QSpinBox* register_spin;
    QWidget* eye_row;
    QComboBox* eye_scope; ///< per application only
    QLineEdit* eye_edit;
};
