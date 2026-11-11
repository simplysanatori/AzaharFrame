// Copyright Citra Emulator Project / Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the misc/licenses/gplv2.txt file included.

#include <memory>
#include <optional>
#include <thread>
#include <QCheckBox>
#include <QApplication>
#include <QPointer>
#include <QMessageBox>
#include <QTableWidgetItem>
#include <QtGlobal>
#include "configure_cheats.h"
#include "core/cheats/cheat_base.h"
#include "core/cheats/cheats.h"
#include "core/cheats/gateway_cheat.h"
#include "core/cheats/sharkive.h" // AzaharFrame cheats
#include "core/hle/service/am/am.h"
#include "core/hle/service/fs/archive.h"
#include "sharkive_client.h"   // AzaharFrame cheats
#include "ui_configure_cheats.h"

ConfigureCheats::ConfigureCheats(Cheats::CheatEngine& cheat_engine_, u64 title_id_, QWidget* parent)
    : QWidget(parent), ui(std::make_unique<Ui::ConfigureCheats>()), cheat_engine{cheat_engine_},
      title_id{title_id_} {
    // Setup gui control settings
    ui->setupUi(this);
    ui->tableCheats->setColumnWidth(0, 30);
    ui->tableCheats->setColumnWidth(2, 85);
    ui->tableCheats->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Fixed);
    ui->tableCheats->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    ui->tableCheats->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Fixed);
    ui->lineName->setEnabled(false);
    ui->textCode->setEnabled(false);
    ui->textNotes->setEnabled(false);

    connect(ui->buttonAddCheat, &QPushButton::clicked, this, &ConfigureCheats::OnAddCheat);
    connect(ui->tableCheats, &QTableWidget::cellClicked, this, &ConfigureCheats::OnRowSelected);
    connect(ui->lineName, &QLineEdit::textEdited, this, &ConfigureCheats::OnTextEdited);
    connect(ui->textNotes, &QPlainTextEdit::textChanged, this, &ConfigureCheats::OnTextEdited);
    connect(ui->textCode, &QPlainTextEdit::textChanged, this, &ConfigureCheats::OnTextEdited);

    connect(ui->buttonSave, &QPushButton::clicked, this,
            [this] { SaveCheat(ui->tableCheats->currentRow()); });
    connect(ui->buttonDelete, &QPushButton::clicked, this, &ConfigureCheats::OnDeleteCheat);
    // AzaharFrame cheats: the only outbound connection, made when this button is clicked
    connect(ui->buttonDownloadSharkive, &QPushButton::clicked, this,
            &ConfigureCheats::OnDownloadSharkive);
    ui->buttonDownloadSharkive->setEnabled(title_id != 0);

    cheat_engine.LoadCheatFile(title_id);
    LoadCheats();
}

ConfigureCheats::~ConfigureCheats() = default;

void ConfigureCheats::LoadCheats() {
    cheats = cheat_engine.GetCheats();
    const int cheats_count = static_cast<int>(cheats.size());

    ui->tableCheats->setRowCount(cheats_count);

    for (int i = 0; i < cheats_count; i++) {
        QCheckBox* enabled = new QCheckBox();
        enabled->setChecked(cheats[i]->IsEnabled());
        enabled->setStyleSheet(QStringLiteral("margin-left:7px;"));
        ui->tableCheats->setItem(i, 0, new QTableWidgetItem());
        ui->tableCheats->setCellWidget(i, 0, enabled);
        ui->tableCheats->setItem(
            i, 1, new QTableWidgetItem(QString::fromStdString(cheats[i]->GetName())));
        ui->tableCheats->setItem(
            i, 2, new QTableWidgetItem(QString::fromStdString(cheats[i]->GetType())));
        enabled->setProperty("row", static_cast<int>(i));

#if QT_VERSION < QT_VERSION_CHECK(6, 7, 0)
        connect(enabled, &QCheckBox::stateChanged, this, &ConfigureCheats::OnCheckChanged);
#else
        connect(enabled, &QCheckBox::checkStateChanged, this, &ConfigureCheats::OnCheckChanged);
#endif
    }
}

bool ConfigureCheats::CheckSaveCheat() {
    auto answer = QMessageBox::warning(
        this, tr("Cheats"), tr("Would you like to save the current cheat?"),
        QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel, QMessageBox::Cancel);

    if (answer == QMessageBox::Yes) {
        return SaveCheat(last_row);
    } else {
        return answer != QMessageBox::Cancel;
    }
}

bool ConfigureCheats::SaveCheat(int row) {
    if (ui->lineName->text().isEmpty()) {
        QMessageBox::critical(this, tr("Save Cheat"), tr("Please enter a cheat name."));
        return false;
    }
    if (ui->textCode->toPlainText().isEmpty()) {
        QMessageBox::critical(this, tr("Save Cheat"), tr("Please enter the cheat code."));
        return false;
    }

    // Check if the cheat lines are valid
    auto code_lines = ui->textCode->toPlainText().split(QLatin1Char{'\n'}, Qt::SkipEmptyParts);
    for (int i = 0; i < code_lines.size(); ++i) {
        Cheats::GatewayCheat::CheatLine cheat_line(code_lines[i].toStdString());
        if (cheat_line.valid)
            continue;

        auto answer = QMessageBox::warning(
            this, tr("Save Cheat"),
            tr("Cheat code line %1 is not valid.\nWould you like to ignore the error and continue?")
                .arg(i + 1),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (answer == QMessageBox::No)
            return false;
    }

    auto cheat = std::make_shared<Cheats::GatewayCheat>(ui->lineName->text().toStdString(),
                                                        ui->textCode->toPlainText().toStdString(),
                                                        ui->textNotes->toPlainText().toStdString());

    if (newly_created) {
        cheat_engine.AddCheat(std::move(cheat));
        newly_created = false;
    } else {
        cheat_engine.UpdateCheat(row, std::move(cheat));
    }
    cheat_engine.SaveCheatFile(title_id);

    int previous_row = ui->tableCheats->currentRow();
    int previous_col = ui->tableCheats->currentColumn();
    LoadCheats();
    ui->tableCheats->setCurrentCell(previous_row, previous_col);

    edited = false;
    ui->buttonSave->setEnabled(false);
    ui->buttonAddCheat->setEnabled(true);
    return true;
}

void ConfigureCheats::OnRowSelected(int row, int column) {
    if (row == last_row) {
        return;
    }
    if (edited && !CheckSaveCheat()) {
        ui->tableCheats->setCurrentCell(last_row, last_col);
        return;
    }
    if (static_cast<std::size_t>(row) < cheats.size()) {
        if (newly_created) {
            // Remove the newly created dummy item
            newly_created = false;
            ui->tableCheats->setRowCount(ui->tableCheats->rowCount() - 1);
        }

        const auto& current_cheat = cheats[row];
        ui->lineName->setText(QString::fromStdString(current_cheat->GetName()));
        ui->textNotes->setPlainText(QString::fromStdString(current_cheat->GetComments()));
        ui->textCode->setPlainText(QString::fromStdString(current_cheat->GetCode()));
    }

    edited = false;
    ui->buttonSave->setEnabled(false);
    ui->buttonDelete->setEnabled(true);
    ui->buttonAddCheat->setEnabled(true);
    ui->lineName->setEnabled(true);
    ui->textCode->setEnabled(true);
    ui->textNotes->setEnabled(true);

    last_row = row;
    last_col = column;
}

void ConfigureCheats::OnCheckChanged(int state) {
    const QCheckBox* checkbox = qobject_cast<QCheckBox*>(sender());
    int row = static_cast<int>(checkbox->property("row").toInt());
    cheats[row]->SetEnabled(state);
    cheat_engine.SaveCheatFile(title_id);
}

void ConfigureCheats::OnTextEdited() {
    edited = true;
    ui->buttonSave->setEnabled(true);
}

void ConfigureCheats::OnDeleteCheat() {
    if (newly_created) {
        newly_created = false;
    } else {
        cheat_engine.RemoveCheat(ui->tableCheats->currentRow());
        cheat_engine.SaveCheatFile(title_id);
    }

    LoadCheats();
    if (cheats.empty()) {
        ui->lineName->clear();
        ui->textCode->clear();
        ui->textNotes->clear();
        ui->lineName->setEnabled(false);
        ui->textCode->setEnabled(false);
        ui->textNotes->setEnabled(false);
        ui->buttonDelete->setEnabled(false);
        last_row = last_col = -1;
    } else {
        if (last_row >= ui->tableCheats->rowCount()) {
            last_row = ui->tableCheats->rowCount() - 1;
        }
        ui->tableCheats->setCurrentCell(last_row, last_col);

        const auto& current_cheat = cheats[last_row];
        ui->lineName->setText(QString::fromStdString(current_cheat->GetName()));
        ui->textNotes->setPlainText(QString::fromStdString(current_cheat->GetComments()));
        ui->textCode->setPlainText(QString::fromStdString(current_cheat->GetCode()));
    }

    edited = false;
    ui->buttonSave->setEnabled(false);
    ui->buttonAddCheat->setEnabled(true);
}

bool ConfigureCheats::ApplyConfiguration() {
    if (edited) {
        return SaveCheat(ui->tableCheats->currentRow());
    }
    return true;
}

void ConfigureCheats::OnAddCheat() {
    if (edited && !CheckSaveCheat()) {
        return;
    }

    int row = ui->tableCheats->rowCount();
    ui->tableCheats->setRowCount(row + 1);
    ui->tableCheats->setCurrentCell(row, 1);

    // create a dummy item
    ui->tableCheats->setItem(row, 1, new QTableWidgetItem(tr("[new cheat]")));
    ui->tableCheats->setItem(row, 2, new QTableWidgetItem(QString{}));
    ui->lineName->clear();
    ui->lineName->setPlaceholderText(tr("[new cheat]"));
    ui->textCode->clear();
    ui->textNotes->clear();
    ui->lineName->setEnabled(true);
    ui->textCode->setEnabled(true);
    ui->textNotes->setEnabled(true);
    ui->buttonSave->setEnabled(true);
    ui->buttonDelete->setEnabled(true);
    ui->buttonAddCheat->setEnabled(false);

    edited = false;
    newly_created = true;
    last_row = row;
    last_col = 1;
}

namespace {

struct InstalledVersion {
    std::optional<Cheats::Sharkive::Version> effective; // what the cheats must match
    bool has_update = false;
};

// AzaharFrame cheats: the version the game runs at, from the TMDs the AM service keeps: the update
// title (0004000E) when one is installed, else the installed base title. A cartridge or a 3DS file
// with no update has no TMD: unknown, so no cheat is marked.
InstalledVersion QueryInstalledVersion(u64 title_id) {
    InstalledVersion out;
    const u64 low = title_id & 0xFFFFFFFFULL;
    const u64 update_id = (0x0004000EULL << 32) | low;
    const u64 base_id = (0x00040000ULL << 32) | low;
    using Service::FS::MediaType;
    for (const MediaType media : {MediaType::SDMC, MediaType::NAND}) {
        const auto tmd = Service::AM::GetTitleMetadata(media, update_id);
        if (tmd.LoadResult() == Loader::ResultStatus::Success) {
            out.has_update = true;
            out.effective = Cheats::Sharkive::FromTitleVersion(tmd.GetTitleVersion());
            return out;
        }
    }
    for (const MediaType media : {MediaType::SDMC, MediaType::NAND}) {
        const auto tmd = Service::AM::GetTitleMetadata(media, base_id);
        if (tmd.LoadResult() == Loader::ResultStatus::Success) {
            out.effective = Cheats::Sharkive::FromTitleVersion(tmd.GetTitleVersion());
            return out;
        }
    }
    return out;
}

} // namespace

// AzaharFrame cheats: one HTTPS request to raw.githubusercontent.com, made on a worker thread; the
// result is merged on the GUI thread. Cheats already in the file (same name) are left alone, new
// ones arrive disabled.
void ConfigureCheats::OnDownloadSharkive() {
    if (downloading) {
        return;
    }
    if (edited && !CheckSaveCheat()) {
        return;
    }
    downloading = true;
    ui->buttonDownloadSharkive->setEnabled(false);
    ui->labelSharkiveStatus->setText(tr("Downloading..."));

    QPointer<ConfigureCheats> self(this);
    const u64 id = title_id;
    const InstalledVersion installed = QueryInstalledVersion(id);
    std::thread([self, id, installed] {
        SharkiveClient::Result result = SharkiveClient::Fetch(id);
        QMetaObject::invokeMethod(
            qApp,
            [self, result = std::move(result)] {
                if (!self) {
                    return;
                }
                self->downloading = false;
                self->ui->buttonDownloadSharkive->setEnabled(true);
                QString status;
                switch (result.status) {
                case SharkiveClient::Status::NotFound:
                    status = tr("No cheats for this game");
                    break;
                case SharkiveClient::Status::Error:
                    status = tr("Download failed: %1").arg(QString::fromStdString(result.error));
                    break;
                case SharkiveClient::Status::Ok: {
                    std::vector<std::string> names;
                    for (const auto& cheat : self->cheat_engine.GetCheats()) {
                        names.push_back(cheat->GetName());
                    }
                    const auto parsed = Cheats::Sharkive::Parse(result.body);
                    auto added = Cheats::Sharkive::NewEntries(parsed, names);
                    const int other =
                        Cheats::Sharkive::MarkOtherVersions(added, installed.effective);
                    if (parsed.empty()) {
                        status = tr("No cheats for this game");
                        break;
                    }
                    for (const auto& entry : added) {
                        auto cheat = std::make_shared<Cheats::GatewayCheat>(
                            entry.name, entry.code, entry.comments);
                        cheat->SetEnabled(false);
                        self->cheat_engine.AddCheat(std::move(cheat));
                    }
                    if (!added.empty()) {
                        self->cheat_engine.SaveCheatFile(self->title_id);
                        const int row = self->ui->tableCheats->currentRow();
                        self->LoadCheats();
                        self->ui->tableCheats->setCurrentCell(row, 1);
                    }
                    status = added.empty() ? tr("No new cheats")
                             : added.size() == 1 ? tr("1 cheat added")
                                                 : tr("%1 cheats added").arg(added.size());
                    if (other > 0) {
                        status += tr(", %1 for another version").arg(other);
                        if (installed.has_update && installed.effective) {
                            status += tr(" (update %1 installed)")
                                          .arg(QString::fromStdString(
                                              Cheats::Sharkive::ToString(*installed.effective)));
                        }
                    }
                    break;
                }
                }
                self->ui->labelSharkiveStatus->setText(status);
            },
            Qt::QueuedConnection);
    }).detach();
}
