// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
//
// AzaharFrame: "Optimize Texture Pack for Frame..." in the game list menu. Runs
// tools/convert-texture-pack.sh (steam-frame/tools in the source) on load/textures/<TitleID>
// with --activate: the pack is converted to ASTC in KTX files with mip chains, next to the
// original, and the title's folder becomes a symlink to the new pack. "Use Original Texture
// Pack" runs it with --restore. The encoder it needs is built once by build-astcenc.sh.
#pragma once

#ifdef ENABLE_FRAME_XR

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QLocale>
#include <QMessageBox>
#include <QProcess>
#include <QProgressDialog>
#include <QPushButton>
#include <QString>
#include <QStringList>
#include <QWidget>
#include <fmt/format.h>
#include "common/common_types.h"
#include "common/file_util.h"

namespace FrameTexturePack {

// Exit codes of convert_texture_pack.py
constexpr int EXIT_NO_ENCODER = 3;
constexpr int EXIT_STOPPED = 4;

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

/// Runs a tool with a progress dialog. `determinate` reads its "progress DONE TOTAL PERMILLE"
/// lines and offers Stop, which terminates it. `done` gets the exit code, whether it was
/// stopped, and its output.
template <typename Done>
void RunWithProgress(QWidget* parent, const QString& label, const QString& program,
                     const QStringList& args, bool determinate, Done done) {
    // Only a conversion can be stopped (and carried on later); a build runs to the end
    auto* dialog = new QProgressDialog(label, determinate ? Tr("Stop") : QString(), 0,
                                       determinate ? 1000 : 0, parent);
    dialog->setWindowTitle(Tr("Optimize Texture Pack"));
    dialog->setWindowModality(Qt::WindowModal);
    dialog->setMinimumDuration(0);
    dialog->setAutoClose(false);
    dialog->setAutoReset(false);
    dialog->setValue(0);

    auto* process = new QProcess(dialog);
    auto* output = new QString;
    auto* stopped = new bool{false};
    QObject::connect(process, &QProcess::readyReadStandardOutput, dialog,
                     [dialog, process, output, label] {
                         while (process->canReadLine()) {
                             const QString line = QString::fromUtf8(process->readLine()).trimmed();
                             const QStringList parts = line.split(QLatin1Char(' '));
                             if (parts.size() == 4 && parts[0] == QStringLiteral("progress")) {
                                 dialog->setValue(parts[3].toInt());
                                 dialog->setLabelText(label + QStringLiteral("\n\n") +
                                                      Tr("%1 of %2 textures")
                                                          .arg(parts[1], parts[2]));
                             } else {
                                 output->append(line + QLatin1Char('\n'));
                             }
                         }
                     });
    QObject::connect(process, &QProcess::readyReadStandardError, dialog, [process, output] {
        output->append(QString::fromUtf8(process->readAllStandardError()));
    });
    QObject::connect(dialog, &QProgressDialog::canceled, process, [dialog, process, stopped] {
        *stopped = true;
        dialog->setLabelText(Tr("Stopping..."));
        process->terminate();
    });
    QObject::connect(process, &QProcess::finished, dialog,
                     [dialog, process, output, stopped, done](int code, QProcess::ExitStatus) {
                         output->append(QString::fromUtf8(process->readAllStandardOutput()));
                         output->append(QString::fromUtf8(process->readAllStandardError()));
                         const QString text = *output;
                         const bool was_stopped = *stopped || code == EXIT_STOPPED;
                         delete output;
                         delete stopped;
                         dialog->hide();
                         dialog->deleteLater();
                         done(code, was_stopped, text);
                     });
    QObject::connect(process, &QProcess::errorOccurred, dialog,
                     [dialog, process, output, stopped, done](QProcess::ProcessError error) {
                         if (error != QProcess::FailedToStart) {
                             return;
                         }
                         const QString text = process->errorString();
                         delete output;
                         delete stopped;
                         dialog->hide();
                         dialog->deleteLater();
                         done(-1, false, text);
                     });
    process->start(program, args);
}

inline void Convert(QWidget* parent, u64 program_id, const QString& name, const QString& script) {
    const QString pack = PackPath(program_id);

    // How much there is to do, from the PNG headers (a second or two for a big pack)
    QProcess estimate;
    estimate.start(script, {QStringLiteral("--estimate"), pack});
    estimate.waitForFinished(120000);
    const QStringList parts =
        QString::fromUtf8(estimate.readAllStandardOutput()).trimmed().split(QLatin1Char(' '));
    if (estimate.exitCode() != 0 || parts.size() != 4 || parts[0] != QStringLiteral("estimate")) {
        QMessageBox::warning(parent, Tr("Optimize Texture Pack"),
                             Tr("The texture pack could not be read.") + QStringLiteral("\n\n") +
                                 Tail(QString::fromUtf8(estimate.readAllStandardError())));
        return;
    }
    const QLocale locale;
    const QString size = locale.formattedDataSize(parts[2].toLongLong(), 1,
                                                  QLocale::DataSizeSIFormat);
    const QString text =
        Tr("Convert the texture pack of %1 to a format the GPU reads directly? HD textures "
           "then show as soon as they are needed, and use less memory.\n\n"
           "%2 textures, about %3 on disk. The original pack is kept, and you can switch "
           "back to it from this menu. You can stop at any time and carry on later.")
            .arg(name, parts[1], size);
    QMessageBox box(QMessageBox::Question, Tr("Optimize Texture Pack"), text,
                    QMessageBox::Cancel, parent);
    QPushButton* optimize = box.addButton(Tr("Optimize"), QMessageBox::AcceptRole);
    box.setDefaultButton(optimize);
    box.exec();
    if (box.clickedButton() != optimize) {
        return;
    }

    RunWithProgress(
        parent, Tr("Converting the texture pack of %1...").arg(name), script,
        {QStringLiteral("--machine"), QStringLiteral("--activate"), pack}, true,
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
    const QString script = ToolPath(QStringLiteral("convert-texture-pack.sh"));
    if (script.isEmpty()) {
        QMessageBox::warning(parent, Tr("Optimize Texture Pack"),
                             Tr("The texture pack converter is missing. Install Azahar for "
                                "Frame again to get it."));
        return;
    }

    if (IsOptimized(program_id)) {
        QProcess restore;
        restore.start(script, {QStringLiteral("--restore"), PackPath(program_id)});
        restore.waitForFinished(60000);
        if (restore.exitCode() == 0) {
            QMessageBox::information(parent, Tr("Use Original Texture Pack"),
                                     Tr("%1 uses its original texture pack again. The optimized "
                                        "pack is kept beside it; delete it to free the space.")
                                         .arg(name));
        } else {
            QMessageBox::warning(parent, Tr("Use Original Texture Pack"),
                                 Tr("The original texture pack could not be restored.") +
                                     QStringLiteral("\n\n") +
                                     Tail(QString::fromUtf8(restore.readAllStandardError())));
        }
        return;
    }

    if (QProcess::execute(script, {QStringLiteral("--check")}) != EXIT_NO_ENCODER) {
        Convert(parent, program_id, name, script);
        return;
    }
    const QString build = ToolPath(QStringLiteral("build-astcenc.sh"));
    if (build.isEmpty() ||
        QMessageBox::question(parent, Tr("Optimize Texture Pack"),
                              Tr("The texture encoder is built once before the first use. This "
                                 "takes about 2 minutes and needs git, CMake and a C++ "
                                 "compiler.\n\nBuild it now?"),
                              QMessageBox::Yes | QMessageBox::No,
                              QMessageBox::Yes) != QMessageBox::Yes) {
        return;
    }
    RunWithProgress(parent, Tr("Building the texture encoder..."), build, {}, false,
                    [parent, program_id, name, script](int code, bool stopped,
                                                       const QString& output) {
                        if (stopped) {
                            return;
                        }
                        if (code != 0) {
                            QMessageBox::warning(parent, Tr("Optimize Texture Pack"),
                                                 Tr("The texture encoder could not be built.") +
                                                     QStringLiteral("\n\n") + Tail(output));
                            return;
                        }
                        Convert(parent, program_id, name, script);
                    });
}

} // namespace FrameTexturePack

#endif // ENABLE_FRAME_XR
