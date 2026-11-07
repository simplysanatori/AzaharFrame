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
class QLineEdit;
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

    QCheckBox* hide_bottom_check;
    /// Per application: the tristate tracker (Use global / on / off).
    ConfigurationShared::CheckState hide_bottom_state = ConfigurationShared::CheckState::Global;
    QWidget* shape_row;
    QComboBox* shape_combo;
    QWidget* mode_row;
    QComboBox* mode_combo;
    QWidget* fov_row;
    QComboBox* fov_scope; ///< per application only: global / this application
    QSpinBox* fov_spin;
    QWidget* depth_row;
    QComboBox* depth_combo;
    QWidget* register_row;
    QComboBox* register_scope; ///< per application only: global / this application
    QSpinBox* register_spin;
    QWidget* eye_row;
    QComboBox* eye_scope; ///< per application only
    QLineEdit* eye_edit;
};
