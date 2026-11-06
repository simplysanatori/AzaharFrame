// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

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
    if (Settings::IsConfiguringGlobal()) {
        register_scope->setVisible(false);
        eye_scope->setVisible(false);
        // A running application that overrides them blocks the global values.
        hide_bottom_check->setEnabled(Settings::values.vr_hide_bottom_screen.UsingGlobal());
        mode_combo->setEnabled(Settings::values.vr_immersive_mode.UsingGlobal());
        depth_combo->setEnabled(Settings::values.vr_immersive_positional_game_scaler.UsingGlobal());
        register_spin->setEnabled(Settings::values.vr_si_mode_register_offset.UsingGlobal());
        eye_edit->setEnabled(Settings::values.vr_immersive_eye_indicator.UsingGlobal());
        return;
    }

    ConfigurationShared::SetColoredTristate(
        hide_bottom_check, Settings::values.vr_hide_bottom_screen, hide_bottom_state);
    ConfigurationShared::SetColoredComboBox(
        mode_combo, mode_row, static_cast<int>(Settings::values.vr_immersive_mode.GetValue(true)));
    ConfigurationShared::SetColoredComboBox(
        depth_combo, depth_row,
        static_cast<int>(Settings::values.vr_immersive_positional_game_scaler.GetValue(true)));

    connect(register_scope, qOverload<int>(&QComboBox::activated), this, [this](int index) {
        register_spin->setEnabled(index == 1);
        ConfigurationShared::SetHighlight(register_row, index == 1);
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
    eye_edit->setText(QString::fromStdString(eye.GetValue()));

    if (Settings::IsConfiguringGlobal()) {
        mode_combo->setCurrentIndex(static_cast<int>(mode.GetValue()));
        depth_combo->setCurrentIndex(static_cast<int>(depth.GetValue()));
        return;
    }

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
    ConfigurationShared::ApplyPerGameSetting(&Settings::values.vr_hide_bottom_screen,
                                             hide_bottom_check, hide_bottom_state);
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
