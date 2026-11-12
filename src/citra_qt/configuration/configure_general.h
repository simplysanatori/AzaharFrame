// Copyright 2016-2025 Citra Emulator Project / Azahar Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the misc/licenses/gplv2.txt file included.

#pragma once

#include <memory>
#include <QWidget>

class HotkeyRegistry;

class QCheckBox; // AzaharFrame autosave
class QComboBox;
class QGroupBox;
class QSpinBox;

namespace Ui {
class ConfigureGeneral;
}

class ConfigureGeneral : public QWidget {
    Q_OBJECT

public:
    explicit ConfigureGeneral(QWidget* parent = nullptr);
    ~ConfigureGeneral() override;

    void ResetDefaults();
    void ApplyConfiguration();
    void RetranslateUI();
    void SetConfiguration();

    void SetupPerGameUI();

private:
    /// AzaharFrame autosave: the Autosave group (between Emulation and Screenshots). Global: a
    /// checkable group; a game's Properties: the "Use global autosave" choice, then its own values.
    void SetupAutosave();
    void SetAutosaveEnabledState();

    std::unique_ptr<Ui::ConfigureGeneral> ui;
    QGroupBox* autosave_group = nullptr;
    QComboBox* autosave_combo = nullptr; ///< Properties only
    QCheckBox* autosave_check = nullptr; ///< Properties only (the group's own box in global)
    QComboBox* autosave_every = nullptr;
    QSpinBox* autosave_keep = nullptr;
};
