// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
//
// AzaharFrame: "Optimize Texture Pack for Frame..." in the game list menu. Runs the converter
// (core/custom_tex/convert.h, in the app) on load/textures/<TitleID> on a thread of its own:
// the pack is converted to ASTC in KTX files with mip chains, next to the original, and the
// title's folder becomes a symlink to the new pack (Activate). "Use Original Texture Pack"
// switches back (Restore).
#pragma once

#ifdef ENABLE_FRAME_XR

#include <atomic>
#include <memory>
#include <mutex>
#include <thread>
#include <QApplication>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QLocale>
#include <QMessageBox>
#include <QProgressDialog>
#include <QPushButton>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <QWidget>
#include <fmt/format.h>
#include "common/common_types.h"
#include "common/file_util.h"
#include "core/custom_tex/convert.h"
#include "core/custom_tex/optimize.h"      // AzaharFrame ui3: Remove Original
#include "core/custom_tex/packs.h"
#include "frame_xr/menu/mods_segments.h" // AzaharFrame ui3: its dialog's text, as in VR

namespace FrameTexturePack {

inline QString Tr(const char* text) {
    return QCoreApplication::translate("FrameTexturePack", text);
}

/// The folder Azahar reads the title's pack from
inline QString PackPath(u64 program_id) {
    return QString::fromStdString(fmt::format(
        "{}textures/{:016X}", FileUtil::GetUserPath(FileUtil::UserPath::LoadDir), program_id));
}

inline bool HasPack(u64 program_id) {
    const QDir dir(PackPath(program_id));
    return dir.exists() && !dir.isEmpty();
}

inline bool IsOptimized(u64 program_id) {
    return QFileInfo::exists(PackPath(program_id) + QStringLiteral("/.azaharframe-ktx.json"));
}

/// tools/<name> of the installed program, or of the checkout for a build tree
inline QString ToolPath(const QString& name) {
    const QString env = qEnvironmentVariable("AZAHARFRAME_TOOLS");
    const QDir app_dir(QCoreApplication::applicationDirPath());
    const QString candidates[] = {
        env.isEmpty() ? QString() : QDir(env).filePath(name),
        app_dir.filePath(QStringLiteral("tools/") + name),
        app_dir.filePath(QStringLiteral("../../../steam-frame/tools/") + name),
    };
    for (const QString& path : candidates) {
        if (!path.isEmpty() && QFileInfo(path).isExecutable()) {
            return path;
        }
    }
    return {};
}

/// The last lines a tool printed, for an error message
inline QString Tail(const QString& output) {
    QStringList lines = output.trimmed().split(QLatin1Char('\n'));
    while (lines.size() > 6) {
        lines.removeFirst();
    }
    return lines.join(QLatin1Char('\n'));
}

/// A conversion on a thread of its own, polled by a timer of the progress dialog (so nothing
/// reaches the dialog from another thread, and the thread ends with the dialog).
struct ConvertJob {
    std::shared_ptr<CustomTex::Convert::Converter> converter;
    CustomTex::Convert::Request request;
    std::mutex mutex;
    CustomTex::Convert::Progress progress;
    std::atomic<bool> finished{false};
    CustomTex::Convert::Result result;
    std::string activate_error;
    std::thread thread;
    ~ConvertJob() {
        if (converter) {
            converter->Stop();
        }
        if (thread.joinable()) {
            thread.join();
        }
    }
};

/// Converts `pack` with a progress dialog and Stop, then makes the title's folder use the copy.
/// `done` gets the exit code, whether it was stopped, and the converter's message.
template <typename Done>
void ConvertWithProgress(QWidget* parent, const QString& label, const QString& pack, Done done) {
    auto* dialog = new QProgressDialog(label, Tr("Stop"), 0, 1000, parent);
    dialog->setWindowTitle(Tr("Optimize Texture Pack"));
    dialog->setWindowModality(Qt::WindowModal);
    dialog->setMinimumDuration(0);
    dialog->setAutoClose(false);
    dialog->setAutoReset(false);
    dialog->setValue(0);

    auto job = std::make_shared<ConvertJob>();
    CustomTex::Convert::Request& request = job->request;
    request.pack = pack.toStdString();
    ConvertJob* raw = job.get();
    job->converter = std::make_shared<CustomTex::Convert::Converter>(
        request, [raw](const CustomTex::Convert::Progress& p) {
            std::lock_guard lock(raw->mutex);
            raw->progress = p;
        });
    job->thread = std::thread([raw] {
        CustomTex::Convert::LowerThreadPriority();
        raw->result = raw->converter->Run();
        if (raw->result.code == CustomTex::Convert::kOk && !raw->result.already_optimized) {
            // Activate runs here: it is a rename and a link, and the folder is not read meanwhile
            CustomTex::Convert::Activate(raw->request.pack, raw->result.output, &raw->activate_error);
        }
        raw->finished = true;
    });
    auto* timer = new QTimer(dialog);
    auto* stopped = new bool{false};
    QObject::connect(dialog, &QProgressDialog::canceled, dialog, [dialog, job, stopped] {
        *stopped = true;
        dialog->setLabelText(Tr("Stopping..."));
        job->converter->Stop();
    });
    QObject::connect(timer, &QTimer::timeout, dialog, [=] {
        if (!job->finished) {
            CustomTex::Convert::Progress p;
            {
                std::lock_guard lock(job->mutex);
                p = job->progress;
            }
            if (!*stopped) {
                dialog->setValue(p.permille);
                dialog->setLabelText(label + QStringLiteral("\n\n") +
                                     Tr("%1 of %2 textures").arg(p.done).arg(p.total));
            }
            return;
        }
        timer->stop();
        job->thread.join();
        QString text = QString::fromStdString(job->result.message);
        for (const auto& [rel, why] : job->result.kept_as_png) {
            text += QStringLiteral("\nkept as PNG: ") + QString::fromStdString(rel) +
                    QStringLiteral(": ") + QString::fromStdString(why);
        }
        int code = job->result.code;
        if (code == CustomTex::Convert::kOk && !job->activate_error.empty()) {
            code = CustomTex::Convert::kFailed;
            text = QString::fromStdString(job->activate_error);
        }
        const bool was_stopped = *stopped || code == CustomTex::Convert::kStopped;
        delete stopped;
        dialog->hide();
        dialog->deleteLater();
        done(code, was_stopped, text);
    });
    timer->start(100);
}

inline void Convert(QWidget* parent, u64 program_id, const QString& name) {
    const QString pack = PackPath(program_id);

    // How much there is to do, from the PNG headers (a second or two for a big pack)
    CustomTex::Convert::Request estimate_request;
    estimate_request.pack = pack.toStdString();
    std::string estimate_error;
    QApplication::setOverrideCursor(Qt::WaitCursor);
    const auto estimate = CustomTex::Convert::EstimatePack(estimate_request, &estimate_error);
    QApplication::restoreOverrideCursor();
    if (!estimate) {
        QMessageBox::warning(parent, Tr("Optimize Texture Pack"),
                             Tr("The texture pack could not be read.") + QStringLiteral("\n\n") +
                                 QString::fromStdString(estimate_error));
        return;
    }
    const QLocale locale;
    const QString size = locale.formattedDataSize(static_cast<qint64>(estimate->total_bytes), 1,
                                                  QLocale::DataSizeSIFormat);
    const QString text =
        Tr("Convert the texture pack of %1 to a format the GPU reads directly? HD textures "
           "then show as soon as they are needed, and use less memory.\n\n"
           "%2 textures, about %3 on disk. The original pack is kept, and you can switch "
           "back to it from this menu. You can stop at any time and carry on later.")
            .arg(name, QString::number(estimate->textures), size);
    QMessageBox box(QMessageBox::Question, Tr("Optimize Texture Pack"), text,
                    QMessageBox::Cancel, parent);
    QPushButton* optimize = box.addButton(Tr("Optimize"), QMessageBox::AcceptRole);
    box.setDefaultButton(optimize);
    box.exec();
    if (box.clickedButton() != optimize) {
        return;
    }

    ConvertWithProgress(
        parent, Tr("Converting the texture pack of %1...").arg(name), pack,
        [parent, name](int code, bool stopped, const QString& output) {
            if (stopped) {
                QMessageBox::information(
                    parent, Tr("Optimize Texture Pack"),
                    Tr("Stopped. Choose Optimize Texture Pack for Frame again to carry on "
                       "where it left off."));
            } else if (code == 0) {
                QMessageBox::information(
                    parent, Tr("Optimize Texture Pack"),
                    Tr("%1 now uses the optimized texture pack. The original pack is kept.")
                        .arg(name));
            } else {
                QMessageBox::warning(parent, Tr("Optimize Texture Pack"),
                                     Tr("The texture pack could not be optimized.") +
                                         QStringLiteral("\n\n") + Tail(output));
            }
        });
}

/// The menu action: converts the pack, or goes back to the original one if it is optimized
inline void Run(QWidget* parent, u64 program_id, const QString& name) {
    if (IsOptimized(program_id)) {
        std::string message, error;
        if (CustomTex::Convert::Restore(PackPath(program_id).toStdString(), &message, &error)) {
            QMessageBox::information(parent, Tr("Use Original Texture Pack"),
                                     Tr("%1 uses its original texture pack again. The optimized "
                                        "pack is kept beside it; delete it to free the space.")
                                         .arg(name));
        } else {
            QMessageBox::warning(parent, Tr("Use Original Texture Pack"),
                                 Tr("The original texture pack could not be restored.") +
                                     QStringLiteral("\n\n") + Tail(QString::fromStdString(error)));
        }
        return;
    }
    Convert(parent, program_id, name);
}

/// AzaharFrame ui3: the optimized pack's original, when Remove Original may delete it (the guard
/// of CustomTex::Optimize::CheckRemoveOriginal), else "".
inline std::string RemovableOriginal(u64 program_id) {
    if (!IsOptimized(program_id)) {
        return {};
    }
    const std::string load = FileUtil::GetUserPath(FileUtil::UserPath::LoadDir);
    const auto check = CustomTex::Optimize::CheckRemoveOriginal(
        PackPath(program_id).toStdString(), CustomTex::Optimize::ManagedRoots(load),
        PackPath(program_id).toStdString());
    return check.Ok() ? check.original : std::string{};
}

/// AzaharFrame ui3: "Remove Original Texture Pack...": "Remove the Original Pack?" (as the VR
/// menu's), then the guarded delete. `running`: the game runs now (never while it may read it).
inline void RemoveOriginal(QWidget* parent, u64 program_id, bool running) {
    namespace Opt = CustomTex::Optimize;
    const QString title = QString::fromUtf8(FrameXr::Menu::kRemoveOriginalTitle);
    const std::string original = RemovableOriginal(program_id);
    if (original.empty()) {
        return;
    }
    if (running) {
        QMessageBox::warning(parent, title, Tr("Close the game first. It may still read the original."));
        return;
    }
    const auto bytes = CustomTex::ComputeSize(original);
    const std::string size = bytes ? FrameXr::Menu::SizeText(*bytes) : std::string{};
    QMessageBox box(QMessageBox::Question, title,
                    QString::fromStdString(FrameXr::Menu::RemoveOriginalBody(size, Opt::FromCatalog(original))),
                    QMessageBox::Cancel, parent);
    QPushButton* remove = box.addButton(Tr("Remove"), QMessageBox::DestructiveRole);
    box.setDefaultButton(QMessageBox::Cancel);
    box.exec();
    if (box.clickedButton() != remove) {
        return;
    }
    const std::string load = FileUtil::GetUserPath(FileUtil::UserPath::LoadDir);
    std::string error;
    if (!Opt::RemoveOriginal(PackPath(program_id).toStdString(), Opt::ManagedRoots(load),
                             PackPath(program_id).toStdString(), &error)) {
        QMessageBox::warning(parent, title, QString::fromStdString(error));
    }
}

} // namespace FrameTexturePack

#endif // ENABLE_FRAME_XR
