// Copyright 2016-2026 Citra Emulator Project / Azahar Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the misc/licenses/gplv2.txt file included.

#include <QCheckBox> // AzaharFrame autosave
#include <QComboBox>
#include <QDesktopServices>
#include <QFileDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QMessageBox>
#include <QSpinBox>
#include <QUrl>
#include "citra_qt/configuration/configuration_shared.h"
#include "citra_qt/configuration/configure_general.h"
#include "citra_qt/uisettings.h"
#include "common/file_util.h"
#include "common/settings.h"
#include "core/autosave.h" // AzaharFrame autosave
#include "ui_configure_general.h"

// The QSlider doesn't have an easy way to set a custom step amount,
// so we can just convert from the sliders range (0 - 198) to the expected
// settings range (5 - 995) with simple math.
static constexpr int SliderToSettings(int value) {
    return 5 * value + 5;
}

static constexpr int SettingsToSlider(int value) {
    return (value - 5) / 5;
}

ConfigureGeneral::ConfigureGeneral(QWidget* parent)
    : QWidget(parent), ui(std::make_unique<Ui::ConfigureGeneral>()) {
    ui->setupUi(this);

    connect(ui->turbo_limit, &QSlider::valueChanged, this, [&](double value) {
        Settings::values.turbo_limit.SetValue(SliderToSettings(value));
        ui->turbo_limit_display_label->setText(
            QStringLiteral("%1%").arg(Settings::values.turbo_limit.GetValue()));
    });

    // Set a minimum width for the label to prevent the slider from changing size.
    // This scales across DPIs, and is acceptable for uncapitalized strings.
    const auto width = static_cast<int>(tr("unthrottled").size() * 6);
    ui->emulation_speed_display_label->setMinimumWidth(width);
    ui->emulation_speed_combo->setVisible(!Settings::IsConfiguringGlobal());
    ui->screenshot_combo->setVisible(!Settings::IsConfiguringGlobal());
#ifndef __unix__
    ui->toggle_gamemode->setVisible(false);
#endif
#ifndef ENABLE_QT_UPDATE_CHECKER
    ui->updates_group->setVisible(false);
#endif

    SetupAutosave(); // AzaharFrame autosave
    SetupPerGameUI();
    SetConfiguration();

    connect(ui->button_reset_defaults, &QPushButton::clicked, this,
            &ConfigureGeneral::ResetDefaults);

    connect(ui->frame_limit, &QSlider::valueChanged, this, [&](int value) {
        if (value == ui->frame_limit->maximum()) {
            ui->emulation_speed_display_label->setText(tr("unthrottled"));
        } else {
            ui->emulation_speed_display_label->setText(
                QStringLiteral("%1%")
                    .arg(SliderToSettings(value))
                    .rightJustified(tr("unthrottled").size()));
        }
    });

    connect(ui->change_screenshot_dir, &QToolButton::clicked, this, [this] {
        ui->change_screenshot_dir->setEnabled(false);
        const QString dir_path = QFileDialog::getExistingDirectory(
            this, tr("Select Screenshot Directory"), ui->screenshot_dir_path->text(),
            QFileDialog::ShowDirsOnly);
        if (!dir_path.isEmpty()) {
            ui->screenshot_dir_path->setText(dir_path);
        }
        ui->change_screenshot_dir->setEnabled(true);
    });
}

ConfigureGeneral::~ConfigureGeneral() = default;

void ConfigureGeneral::SetConfiguration() {
    if (Settings::IsConfiguringGlobal()) {
        ui->turbo_limit->setValue(SettingsToSlider(Settings::values.turbo_limit.GetValue()));
        ui->turbo_limit_display_label->setText(
            QStringLiteral("%1%").arg(Settings::values.turbo_limit.GetValue()));

        ui->toggle_check_exit->setChecked(UISettings::values.confirm_before_closing.GetValue());
        ui->toggle_background_pause->setChecked(
            UISettings::values.pause_when_in_background.GetValue());
        ui->toggle_background_mute->setChecked(
            UISettings::values.mute_when_in_background.GetValue());
        ui->toggle_hide_mouse->setChecked(UISettings::values.hide_mouse.GetValue());
#ifdef ENABLE_QT_UPDATE_CHECKER
        ui->toggle_update_checker->setChecked(
            UISettings::values.check_for_update_on_start.GetValue());
        ui->update_channel_combobox->setCurrentIndex(
            UISettings::values.update_check_channel.GetValue());
#endif
#ifdef __unix__
        ui->toggle_gamemode->setChecked(Settings::values.enable_gamemode.GetValue());
#endif
    }

    if (Settings::values.frame_limit.GetValue() == 0) {
        ui->frame_limit->setValue(ui->frame_limit->maximum());
    } else {
        ui->frame_limit->setValue(SettingsToSlider(Settings::values.frame_limit.GetValue()));
    }
    if (ui->frame_limit->value() == ui->frame_limit->maximum()) {
        ui->emulation_speed_display_label->setText(tr("unthrottled"));
    } else {
        ui->emulation_speed_display_label->setText(
            QStringLiteral("%1%")
                .arg(SliderToSettings(ui->frame_limit->value()))
                .rightJustified(tr("unthrottled").size()));
    }

    if (!Settings::IsConfiguringGlobal()) {
        if (Settings::values.frame_limit.UsingGlobal()) {
            ui->emulation_speed_combo->setCurrentIndex(0);
            ui->frame_limit->setEnabled(false);
        } else {
            ui->emulation_speed_combo->setCurrentIndex(1);
            ui->frame_limit->setEnabled(true);
        }
        if (UISettings::values.screenshot_path.UsingGlobal()) {
            ui->screenshot_combo->setCurrentIndex(0);
            ui->screenshot_dir_path->setEnabled(false);
            ui->change_screenshot_dir->setEnabled(false);
        } else {
            ui->screenshot_combo->setCurrentIndex(1);
            ui->screenshot_dir_path->setEnabled(true);
            ui->change_screenshot_dir->setEnabled(true);
        }
        ConfigurationShared::SetHighlight(ui->widget_screenshot,
                                          !UISettings::values.screenshot_path.UsingGlobal());
        ConfigurationShared::SetHighlight(ui->emulation_speed_layout,
                                          !Settings::values.frame_limit.UsingGlobal());
    }

    // AzaharFrame autosave: the running game's values (its own in Properties when it has them)
    const auto& av = Settings::values;
    autosave_every->setCurrentIndex(Core::Autosave::EveryIndex(av.autosave_every.GetValue()));
    autosave_keep->setValue(
        static_cast<int>(Core::Autosave::ClampKeep(av.autosave_keep.GetValue())));
    if (Settings::IsConfiguringGlobal()) {
        autosave_group->setChecked(av.autosave_enabled.GetValue());
    } else {
        autosave_check->setChecked(av.autosave_enabled.GetValue());
        const bool own = !av.autosave_enabled.UsingGlobal() || !av.autosave_every.UsingGlobal() ||
                         !av.autosave_keep.UsingGlobal();
        autosave_combo->setCurrentIndex(own ? 1 : 0);
        ConfigurationShared::SetHighlight(autosave_group, own);
    }
    SetAutosaveEnabledState();

    UISettings::values.screenshot_path.SetGlobal(ui->screenshot_combo->currentIndex() ==
                                                 ConfigurationShared::USE_GLOBAL_INDEX);
    std::string screenshot_path = UISettings::values.screenshot_path.GetValue();
    if (screenshot_path.empty()) {
        screenshot_path = FileUtil::GetUserPath(FileUtil::UserPath::UserDir) + "screenshots/";
        FileUtil::CreateFullPath(screenshot_path);
        UISettings::values.screenshot_path = screenshot_path;
    }
    ui->screenshot_dir_path->setText(QString::fromStdString(screenshot_path));
}

void ConfigureGeneral::ResetDefaults() {
    ui->button_reset_defaults->setEnabled(false);
    QMessageBox::StandardButton answer = QMessageBox::question(
        this, QStringLiteral("Azahar"),
        tr("Are you sure you want to <b>reset your settings</b> and close Azahar?"),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);

    if (answer == QMessageBox::No) {
        ui->button_reset_defaults->setEnabled(true);
        return;
    }

    FileUtil::Delete(FileUtil::GetUserPath(FileUtil::UserPath::ConfigDir) + "qt-config.ini");
    FileUtil::DeleteDirRecursively(FileUtil::GetUserPath(FileUtil::UserPath::ConfigDir) + "custom");
    qApp->quit();
}

void ConfigureGeneral::ApplyConfiguration() {
    ConfigurationShared::ApplyPerGameSetting(
        &Settings::values.frame_limit, ui->emulation_speed_combo, [this](s32) {
            const bool is_maximum = ui->frame_limit->value() == ui->frame_limit->maximum();
            return is_maximum ? 0 : SliderToSettings(ui->frame_limit->value());
        });

    ConfigurationShared::ApplyPerGameSetting(
        &UISettings::values.screenshot_path, ui->screenshot_combo,
        [this](s32) { return ui->screenshot_dir_path->text().toStdString(); });

    // AzaharFrame autosave
    {
        auto& av = Settings::values;
        const u32 every = Core::Autosave::EveryAt(autosave_every->currentIndex());
        const u32 keep = Core::Autosave::ClampKeep(static_cast<u32>(autosave_keep->value()));
        if (Settings::IsConfiguringGlobal()) {
            av.autosave_enabled.SetValue(autosave_group->isChecked());
            av.autosave_every.SetValue(every);
            av.autosave_keep.SetValue(keep);
        } else {
            const bool global = autosave_combo->currentIndex() == 0;
            av.autosave_enabled.SetGlobal(global);
            av.autosave_every.SetGlobal(global);
            av.autosave_keep.SetGlobal(global);
            if (!global) {
                av.autosave_enabled.SetValue(autosave_check->isChecked());
                av.autosave_every.SetValue(every);
                av.autosave_keep.SetValue(keep);
            }
        }
    }

    if (Settings::IsConfiguringGlobal()) {
        UISettings::values.confirm_before_closing = ui->toggle_check_exit->isChecked();
        UISettings::values.pause_when_in_background = ui->toggle_background_pause->isChecked();
        UISettings::values.mute_when_in_background = ui->toggle_background_mute->isChecked();
        UISettings::values.hide_mouse = ui->toggle_hide_mouse->isChecked();
#ifdef ENABLE_QT_UPDATE_CHECKER
        UISettings::values.check_for_update_on_start = ui->toggle_update_checker->isChecked();
        UISettings::values.update_check_channel = ui->update_channel_combobox->currentIndex();
#endif
#ifdef __unix__
        Settings::values.enable_gamemode = ui->toggle_gamemode->isChecked();
#endif
    }
}

// AzaharFrame autosave
void ConfigureGeneral::SetupAutosave() {
    autosave_group = new QGroupBox(tr("Autosave"), this);
    autosave_group->setObjectName(QStringLiteral("autosave_group"));
    auto* form = new QFormLayout(autosave_group);
    if (Settings::IsConfiguringGlobal()) {
        // Qt's usual on/off with settings that depend on it: a checkable group
        autosave_group->setCheckable(true);
    } else {
        autosave_combo = new QComboBox(autosave_group);
        autosave_combo->addItem(tr("Use global autosave"));
        autosave_combo->addItem(tr("Set autosave"));
        form->addRow(autosave_combo);
        autosave_check = new QCheckBox(tr("Autosave"), autosave_group);
        form->addRow(autosave_check);
        connect(autosave_combo, qOverload<int>(&QComboBox::activated), this, [this](int index) {
            ConfigurationShared::SetHighlight(autosave_group, index == 1);
            SetAutosaveEnabledState();
        });
        connect(autosave_check, &QCheckBox::toggled, this, [this] { SetAutosaveEnabledState(); });
    }
    autosave_every = new QComboBox(autosave_group);
    for (const u32 minutes : Core::Autosave::kEveryMinutes) {
        autosave_every->addItem(tr("%1 Min").arg(minutes));
    }
    autosave_keep = new QSpinBox(autosave_group);
    autosave_keep->setRange(static_cast<int>(Core::Autosave::kKeepMin),
                            static_cast<int>(Core::Autosave::kKeepMax));
    form->addRow(tr("Every"), autosave_every);
    form->addRow(tr("Keep"), autosave_keep);
    connect(autosave_group, &QGroupBox::toggled, this, [this] { SetAutosaveEnabledState(); });
    ui->verticalLayout->insertWidget(ui->verticalLayout->indexOf(ui->screenshot_group),
                                     autosave_group);
}

// Every and Keep greyed while Autosave is off (and, in Properties, everything while the game
// uses the global values).
void ConfigureGeneral::SetAutosaveEnabledState() {
    if (Settings::IsConfiguringGlobal()) {
        return; // the checkable group greys its own rows
    }
    const bool own = autosave_combo->currentIndex() == 1;
    autosave_check->setEnabled(own);
    autosave_every->setEnabled(own && autosave_check->isChecked());
    autosave_keep->setEnabled(own && autosave_check->isChecked());
}

void ConfigureGeneral::RetranslateUI() {
    ui->retranslateUi(this);
}

void ConfigureGeneral::SetupPerGameUI() {
    if (Settings::IsConfiguringGlobal()) {
        ui->frame_limit->setEnabled(Settings::values.frame_limit.UsingGlobal());
        return;
    }

    connect(ui->emulation_speed_combo, qOverload<int>(&QComboBox::activated), this,
            [this](int index) {
                ui->frame_limit->setEnabled(index == 1);
                ConfigurationShared::SetHighlight(ui->emulation_speed_layout, index == 1);
            });

    connect(ui->screenshot_combo, qOverload<int>(&QComboBox::activated), this, [this](int index) {
        ui->screenshot_dir_path->setEnabled(index == 1);
        ui->change_screenshot_dir->setEnabled(index == 1);
        ConfigurationShared::SetHighlight(ui->widget_screenshot, index == 1);
    });

    // AzaharFrame integ3: the whole Turbo Speed Limit row (upstream hid only the slider, which
    // left its label bare); it stays a global setting, as upstream has it
    ui->turbo_limit_layout->setVisible(false);
    ui->general_group->setVisible(false);
    ui->button_reset_defaults->setVisible(false);
    ui->toggle_gamemode->setVisible(false);
    ui->updates_group->setVisible(false);
}
