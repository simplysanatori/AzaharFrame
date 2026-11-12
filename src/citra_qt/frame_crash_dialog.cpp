// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv2 or any later version
// Refer to the misc/licenses/gplv2.txt file included.

// AzaharFrame crashreport: the start after a run that ended badly (frame_crash_dialog.h).

#include "citra_qt/frame_test_env.h"
#include "citra_qt/frame_crash_dialog.h"

#include <filesystem>
#include <memory>
#include <optional>

#include <QApplication>
#include <QBuffer>
#include <QCheckBox>
#include <QDateTime>
#include <QDialog>
#include <QDir>
#include <QEvent>
#include <QFile>
#include <QFileInfo>
#include <QFontDatabase>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QMetaObject>
#include <QMouseEvent>
#include <QPainter>
#include <QPlainTextEdit>
#include <QPointer>
#include <QPushButton>
#include <QStandardPaths>
#include <QTextBlock>
#include <QTextBrowser>
#include <QTextCursor>
#include <QTextDocument>
#include <QThread>
#include <QTimer>
#include <QVBoxLayout>

#include "citra_qt/frame_mode.h"
#include "citra_qt/uisettings.h"
#include <fmt/format.h>
#include "common/common_paths.h"
#include "common/file_util.h"
#include "common/frame_crash.h"
#include "common/logging/log.h"
#include "common/scm_rev.h"
#include "core/frame_crash_report.h"
#include "core/frame_crash_send.h"
#include "core/mods/crash_guard.h"
#include "core/save_backup.h"
#include "frame_xr/frame_xr.h"
#include "frame_xr/menu/crash_note.h"
#include "frame_xr/menu/md3.h"
#include "frame_xr/menu/swkbd_view.h"
#include "frame_xr/swkbd.h"
#include "video_core/custom_textures/preload_budget.h"

namespace CR = Core::CrashReport;

namespace FrameCrashDialog {

namespace {

std::optional<CR::Report> g_pending;

std::string CrashDir() {
    return FileUtil::GetUserPath(FileUtil::UserPath::UserDir) + "crash-reports/";
}

std::vector<std::string> Lines(const std::string& text) {
    std::vector<std::string> out;
    std::size_t at = 0;
    while (at < text.size()) {
        const std::size_t nl = text.find('\n', at);
        const std::string line = text.substr(at, nl == std::string::npos ? nl : nl - at);
        if (!line.empty()) {
            out.push_back(line);
        }
        if (nl == std::string::npos) {
            break;
        }
        at = nl + 1;
    }
    return out;
}

// What else the run left that helps: the mods the crash guard watches, Load Anyway's marker.
std::vector<std::string> Clues(u64 title_id) {
    std::vector<std::string> clues;
    if (title_id == 0) {
        return clues;
    }
    Mods::CrashGuard::Guard guard(Mods::CrashGuard::DefaultDir());
    if (guard.Armed(title_id)) {
        std::string names;
        for (const auto& item : guard.New(title_id)) {
            names += (names.empty() ? "" : ", ") + item.name;
        }
        clues.push_back("Mods New Since a Clean Run: " + (names.empty() ? "Yes" : names));
    }
    const std::string root =
        Core::SaveBackup::Root(FileUtil::GetUserPath(FileUtil::UserPath::UserDir), title_id);
    if (Core::SaveBackup::Armed(root)) {
        clues.push_back("Loaded a Save State From Another Version: Yes");
    }
    return clues;
}

std::string VersionText() {
    return fmt::format("{} ({} {})", Common::g_build_fullname, Common::g_scm_branch,
                       Common::g_scm_desc);
}

QString Shown(const QString& path) {
    const QString home = QDir::homePath();
    return path.startsWith(home) ? QStringLiteral("~") + path.mid(home.size()) : path;
}

QLabel* Headline(const QString& text) {
    auto* l = new QLabel(text);
    QFont f = l->font();
    f.setPointSizeF(f.pointSizeF() * 1.5);
    l->setFont(f);
    l->setWordWrap(true);
    return l;
}

QPushButton* TextButton(const QString& label) {
    auto* b = new QPushButton(label);
    b->setFlat(true);
    b->setAutoDefault(false);
    return b;
}

// The note box: in VR a tap opens the VR keyboard (the headset keyboard of Azahar's window would
// otherwise open: the box is read-only there), and Done puts its text back.
class NoteFilter : public QObject {
public:
    NoteFilter(QPlainTextEdit* box_, const char* label_) : QObject(box_), box(box_), label(label_) {}

    bool eventFilter(QObject* watched, QEvent* e) override {
        const bool vr = FrameXr::IsStarted() && FrameXr::VrShowing();
        if (e->type() == QEvent::MouseButtonPress || e->type() == QEvent::MouseButtonDblClick) {
            box->setReadOnly(vr);
            if (vr) {
                Open();
                return true;
            }
        }
        return QObject::eventFilter(watched, e);
    }

    void Open() {
        QPointer<QPlainTextEdit> target = box;
        const std::string now = box->toPlainText().toStdString();
        const bool open = FrameXr::OpenGameKeyboard(
            FrameXr::Menu::CrashNoteSpec(now, label), [target](const FrameXr::SwkbdOutcome& o) {
                if (!o.shown || o.button != 2) {
                    return; // Cancel, or VR ended: the box keeps what it had
                }
                const QString text = QString::fromStdString(o.text);
                QMetaObject::invokeMethod(qApp, [target, text] {
                    if (target) {
                        target->setPlainText(text);
                        LOG_INFO(Frontend, "crash report: note typed on the VR keyboard ({} characters)",
                                 text.size());
                    }
                });
            });
        LOG_INFO(Frontend, "crash report: {} on the VR keyboard: {}", label,
                 open ? "open" : "no VR");
    }

private:
    QPlainTextEdit* box;
    const char* label;
};

// The crash dialog, and (manual) Report a Problem: the same dialog with "What Happened?" (needed
// for Send Report), Include a Screenshot while a game runs, and Cancel for Don't Send.
class CrashDialog : public QDialog {
public:
    CrashDialog(QWidget* parent, CR::Report r, bool can_send, ScreenshotFn screenshot_ = {})
        : QDialog(parent), report(std::move(r)), manual(report.kind == CR::Kind::Manual),
          screenshot(std::move(screenshot_)) {
        setWindowTitle(QString::fromStdString(report.headline));
        auto* layout = new QVBoxLayout(this);
        layout->setContentsMargins(24, 24, 24, 16);
        layout->setSpacing(12);
        layout->addWidget(Headline(QString::fromStdString(report.headline)));
        if (!report.body.empty()) {
            auto* body = new QLabel(QString::fromStdString(report.body));
            body->setWordWrap(true);
            layout->addWidget(body);
        }
        // The reason and the game: label and value rows (the headline says it for Closed
        // Unexpectedly and Ran Out of Memory)
        auto* grid = new QGridLayout;
        grid->setHorizontalSpacing(24);
        grid->setVerticalSpacing(8);
        int row = 0;
        const auto add_row = [&](const QString& label, const QString& value) {
            auto* l = new QLabel(label);
            auto* v = new QLabel(value);
            QFont vf = v->font();
            vf.setBold(true);
            v->setFont(vf);
            v->setWordWrap(true);
            grid->addWidget(l, row, 0, Qt::AlignTop);
            grid->addWidget(v, row, 1);
            row++;
        };
        if (report.kind != CR::Kind::Unexpected && report.kind != CR::Kind::OutOfMemory &&
            !manual) {
            add_row(tr("Reason"), QString::fromStdString(report.reason));
        }
        if (!report.title.empty()) {
            add_row(tr("Game"), QString::fromStdString(report.title));
        }
        grid->setColumnStretch(1, 1);
        if (row > 0) {
            layout->addLayout(grid);
        }
        const char* label = manual ? FrameXr::Menu::kProblemNoteLabel : FrameXr::Menu::kCrashNoteLabel;
        auto* note_label = new QLabel(QString::fromUtf8(label));
        layout->addSpacing(4);
        layout->addWidget(note_label);
        note = new QPlainTextEdit;
        note->setTabChangesFocus(true);
        note->setFixedHeight(note->fontMetrics().lineSpacing() * 3 + 16);
        note->setReadOnly(FrameXr::IsStarted() && FrameXr::VrShowing());
        note->viewport()->installEventFilter(new NoteFilter(note, label));
        connect(note, &QPlainTextEdit::textChanged, this, [this] {
            const QString t = note->toPlainText();
            if (t.size() > FrameXr::Menu::kCrashNoteMax) {
                note->setPlainText(t.left(FrameXr::Menu::kCrashNoteMax));
                note->moveCursor(QTextCursor::End);
            }
            UpdateSend();
        });
        note_label->setBuddy(note);
        layout->addWidget(note);
        if (manual && screenshot) {
            // the top and bottom screens of the game, never the VR view: off until ticked
            shot_box = new QCheckBox(tr("Include a Screenshot"));
            connect(shot_box, &QCheckBox::toggled, this, [this](bool on) { Screenshot(on); });
            layout->addWidget(shot_box);
        }
        status = new QLabel;
        status->setWordWrap(true);
        status->hide();
        layout->addWidget(status);

        // See What's Sent at the start; Don't Send, Save Report and Send Report at the end, the
        // confirming one last (Material). Don't Send is the default: nothing goes by accident.
        auto* buttons = new QHBoxLayout;
        see = TextButton(tr("See What's Sent"));
        buttons->addWidget(see);
        buttons->addStretch(1);
        dont = TextButton(manual ? tr("Cancel") : tr("Don't Send"));
        save = TextButton(tr("Save Report"));
        buttons->addWidget(dont);
        buttons->addWidget(save);
        if (can_send) {
            send = TextButton(tr("Send Report"));
            buttons->addWidget(send);
            connect(send, &QPushButton::clicked, this, [this] { Send(); });
        }
        dont->setDefault(true);
        connect(dont, &QPushButton::clicked, this, [this] {
            LOG_INFO(Frontend, "{}: {}", Topic(), manual ? "Cancel" : "Don't Send");
            reject();
        });
        connect(save, &QPushButton::clicked, this, [this] { Save(); });
        connect(see, &QPushButton::clicked, this, [this, can_send] {
            auto* view = MakeWhatsSent(this, Current(), can_send);
            view->setAttribute(Qt::WA_DeleteOnClose);
            view->open();
        });
        layout->addLayout(buttons);
        setMinimumWidth(520);
        UpdateSend();
    }

    CR::Report Current() const {
        CR::Report r = report;
        r.note = note->toPlainText().trimmed().toStdString();
        // the note is scrubbed like everything else: only the home folder and the user name
        r.note = CR::Scrubber::ForThisMachine().Apply(r.note);
        if (shot_box && shot_box->isChecked()) {
            r.screenshot_png = shot_png.toStdString();
        }
        return r;
    }

    QCheckBox* ScreenshotBox() {
        return shot_box;
    }
    /// For the pictures: a screenshot as if taken
    void SetScreenshot(const QByteArray& png) {
        shot_png = png;
        if (shot_box) {
            const QSignalBlocker block(shot_box);
            shot_box->setChecked(!png.isEmpty());
        }
    }

    QPlainTextEdit* Note() {
        return note;
    }
    QPushButton* SaveButton() {
        return save;
    }
    QPushButton* SendButton() {
        return send;
    }
    QPushButton* DontButton() {
        return dont;
    }

private:
    const char* Topic() const {
        return manual ? "problem report" : "crash report";
    }

    // Send Report needs "What Happened?" in a manual report
    void UpdateSend() {
        if (send && manual && !busy) {
            send->setEnabled(!note->toPlainText().trimmed().isEmpty());
        }
    }

    void Screenshot(bool on) {
        shot_png.clear();
        if (!on) {
            Busy(false, {});
            return;
        }
        Busy(true, tr("Taking a Screenshot..."));
        QPointer<CrashDialog> self = this;
        screenshot([self](QByteArray png) {
            if (!self) {
                return;
            }
            self->Busy(false, {});
            if (png.isEmpty()) {
                const QSignalBlocker block(self->shot_box);
                self->shot_box->setChecked(false);
                self->Busy(false, tr("Couldn't Take a Screenshot"));
                LOG_WARNING(Frontend, "problem report: no screenshot");
                return;
            }
            self->shot_png = png;
            LOG_INFO(Frontend, "problem report: screenshot taken ({} bytes)", png.size());
        });
    }

    void Busy(bool busy_, const QString& text) {
        busy = busy_;
        for (QPushButton* b : {see, dont, save, send}) {
            if (b) {
                b->setEnabled(!busy);
            }
        }
        note->setEnabled(!busy);
        if (shot_box) {
            shot_box->setEnabled(!busy);
        }
        status->setText(text);
        status->setVisible(!text.isEmpty());
        UpdateSend();
    }

    void Save() {
        std::string error;
        const long long now = QDateTime::currentSecsSinceEpoch();
        const std::string path = CR::SaveZip(Current(), ReportsDir().toStdString(), now, &error);
        if (path.empty()) {
            LOG_ERROR(Frontend, "{}: not saved: {}", Topic(), error);
            Busy(false, tr("Couldn't Save the Report: %1").arg(QString::fromStdString(error)));
            return;
        }
        LOG_INFO(Frontend, "{}: saved as {}", Topic(), Shown(QString::fromStdString(path)).toStdString());
        auto* saved = MakeSaved(parentWidget(), Shown(QString::fromStdString(path)));
        saved->setAttribute(Qt::WA_DeleteOnClose);
        accept();
        saved->open();
    }

    void Send() {
        Busy(true, tr("Sending..."));
        const CR::Report r = Current();
        const std::string work = CrashDir() + "send";
        QPointer<CrashDialog> self = this;
        auto result = std::make_shared<CR::SendResult>();
        QThread* thread = QThread::create([r, work, result] {
            *result = CR::Send(r, CR::BuiltInDsn(), work);
            std::error_code ec;
            std::filesystem::remove(work, ec); // empty again: Send took its database away
        });
        const char* topic = Topic();
        connect(thread, &QThread::finished, this, [self, result, thread, topic] {
            thread->deleteLater();
            if (!self) {
                return;
            }
            if (result->ok) {
                LOG_INFO(Frontend, "{}: sent ({})", topic, result->event_id);
                auto* sent = new QDialog(self->parentWidget());
                auto* l = new QVBoxLayout(sent);
                l->setContentsMargins(24, 24, 24, 16);
                l->setSpacing(12);
                l->addWidget(Headline(tr("Report Sent")));
                auto* body = new QLabel(tr("Thank you. Report ID: %1")
                                            .arg(QString::fromStdString(result->event_id).left(8)));
                l->addWidget(body);
                auto* row = new QHBoxLayout;
                row->addStretch(1);
                auto* ok = TextButton(tr("OK"));
                row->addWidget(ok);
                l->addLayout(row);
                connect(ok, &QPushButton::clicked, sent, &QDialog::accept);
                sent->setMinimumWidth(360);
                sent->setAttribute(Qt::WA_DeleteOnClose);
                self->accept();
                sent->open();
            } else {
                LOG_WARNING(Frontend, "{}: not sent: {}", topic, result->error);
                self->Busy(false, tr("Couldn't Send the Report: %1. You can save it instead.")
                                      .arg(QString::fromStdString(result->error)));
            }
        });
        thread->start();
    }

    CR::Report report;
    bool manual = false;
    bool busy = false;
    ScreenshotFn screenshot;
    QByteArray shot_png;
    QCheckBox* shot_box = nullptr;
    QPlainTextEdit* note = nullptr;
    QLabel* status = nullptr;
    QPushButton* see = nullptr;
    QPushButton* dont = nullptr;
    QPushButton* save = nullptr;
    QPushButton* send = nullptr;
};

// A sample report for the pictures: a real record of this process (its maps, a stack through
// this file) read back through the same code as a real one.
CR::Report SampleReport(CR::Kind kind) {
    CR::LastRun run;
    run.marker = true;
    run.version = VersionText();
    run.release = CR::ReleaseName();
    run.title_id = 0x0004000000054000;
    run.title = "Super Mario 3D Land";
    run.settings = Lines(CR::SettingsSummary());
    if (kind == CR::Kind::DeviceLost) {
        run.cause = Common::FrameCrash::Cause::DeviceLost;
        run.detail = "Device lost during submit: vk::Queue::submit: ErrorDeviceLost";
        run.crash = true;
        run.signal = 6;
        run.code = -6;
        run.thread = "GPU";
        run.frames = {reinterpret_cast<u64>(&SampleReport), reinterpret_cast<u64>(&Lines)};
        QFile maps(QStringLiteral("/proc/self/maps"));
        if (maps.open(QIODevice::ReadOnly)) {
            run.maps = maps.readAll().toStdString();
        }
    }
    CR::Context ctx;
    ctx.log_path = FileUtil::GetUserPath(FileUtil::UserPath::LogDir) + LOG_FILE;
    ctx.log_lines = 40;
    ctx.os = CR::OsText();
    ctx.scrub = CR::Scrubber::ForThisMachine();
    CR::Report r = CR::Build(run, kind == CR::Kind::OutOfMemory, ctx);
    r.headline = "Azahar Closed Unexpectedly";
    if (r.kind == CR::Kind::OutOfMemory) {
        const auto t = VideoCore::PreloadBudget::MakeOutOfMemoryText(5734);
        r.headline = t.title;
        r.body = t.body;
    }
    if (r.gpu.empty()) {
        r.gpu = "Turnip Adreno (TM) 750";
        r.driver = "turnip Mesa driver 26.2.99";
    }
    return r;
}

void Grab(QWidget* w, const QString& path) {
    w->adjustSize();
    w->show();
    QApplication::processEvents();
    w->grab().save(path);
    LOG_INFO(Frontend, "xr test: crash report picture {}", path.toStdString());
}

// The dialog over Azahar's window, as VR shows it (Azahar's window is the VR panel at start)
void GrabOver(QWidget* parent, QWidget* dialog, const QString& path) {
    dialog->adjustSize();
    dialog->show();
    QApplication::processEvents();
    QPixmap all = parent->grab();
    const QPixmap box = dialog->grab();
    QPainter p(&all);
    p.fillRect(all.rect(), QColor(0, 0, 0, 90));
    p.drawPixmap((all.width() - box.width()) / 2, (all.height() - box.height()) / 2, box);
    p.end();
    all.save(path);
    LOG_INFO(Frontend, "xr test: crash report picture {}", path.toStdString());
}

// The VR keyboard as the panel draws it (frame_xr/menu/swkbd_view.h), for the pictures
QImage KeyboardImage(const FrameXr::Swkbd::Model& m, int hover) {
    constexpr float px_per_dp = 1.75f; // the swapchain's own scale
    FrameXr::Menu::Kit kit(px_per_dp);
    frame_xr::ui::Canvas c;
    c.Init(static_cast<int>(FrameXr::Swkbd::kPanelW * px_per_dp + 0.5f),
           static_cast<int>(FrameXr::Swkbd::kPanelH * px_per_dp + 0.5f));
    kit.Begin(c);
    FrameXr::Swkbd::Draw(kit, m, hover, -1);
    return QImage(reinterpret_cast<const uchar*>(c.px.data()), c.w, c.h,
                  QImage::Format_RGBA8888_Premultiplied)
        .copy();
}

// The window with the dialog, and the keyboard under it, as VR places them (controls.cpp,
// VrUi::EnterSwkbd: 80 % of the window's width, its top a third over the window's bottom edge)
void GrabKeyboardOver(QWidget* parent, QWidget* dialog, const QImage& keyboard,
                      const QString& path) {
    dialog->adjustSize();
    dialog->show();
    QApplication::processEvents();
    QPixmap window = parent->grab();
    {
        const QPixmap box = dialog->grab();
        QPainter p(&window);
        p.fillRect(window.rect(), QColor(0, 0, 0, 90));
        p.drawPixmap((window.width() - box.width()) / 2, (window.height() - box.height()) / 2, box);
    }
    const int kb_w = window.width() * 8 / 10;
    const QImage kb = keyboard.scaledToWidth(kb_w, Qt::SmoothTransformation);
    const int overlap = kb.height() * 35 / 100;
    QPixmap all(window.width() + 80, window.height() + kb.height() - overlap + 80);
    all.fill(QColor(0x2A, 0x2D, 0x31));
    QPainter p(&all);
    p.drawPixmap(40, 40, window);
    p.drawImage((all.width() - kb_w) / 2, 40 + window.height() - overlap, kb);
    p.end();
    all.save(path);
    LOG_INFO(Frontend, "xr test: crash report picture {}", path.toStdString());
}

bool VrNow() {
    return FrameXr::IsStarted() && FrameXr::VrShowing();
}

#ifndef AZAHARFRAME_RELEASE_BUILD
void Answer(CrashDialog* self, const QString& answer) {
    LOG_INFO(Frontend, "xr test: report dialog answered {}", answer.toStdString());
    if (answer == QStringLiteral("keyboard")) { // the VR keyboard on the box, left open
        self->Note()->setPlainText(QStringLiteral(
            "I opened the Mods tab and turned on the 4K pack, then pressed Continue."));
        QMouseEvent press(QEvent::MouseButtonPress, QPointF(10, 10), QPointF(10, 10),
                          Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(self->Note()->viewport(), &press);
    } else if (answer == QStringLiteral("send") && self->SendButton()) {
        self->SendButton()->click();
    } else if (answer == QStringLiteral("save")) {
        self->SaveButton()->click();
    } else {
        self->DontButton()->click();
    }
}
#endif

// Test driver (not in release builds): answers a dialog by itself (a mock has no hands)
void AnswerForTest(CrashDialog* d, const QString& note) {
#ifndef AZAHARFRAME_RELEASE_BUILD
    const QString answer = FrameTestEnv("AZAHARFRAME_TEST_CRASH_ANSWER");
    if (answer.isEmpty()) {
        return;
    }
    QPointer<CrashDialog> self = d;
    QTimer::singleShot(2000, d, [self, answer, note] {
        if (!self) {
            return;
        }
        self->Note()->setPlainText(note);
        if (self->ScreenshotBox() && answer != QStringLiteral("dont")) {
            self->ScreenshotBox()->setChecked(true); // Include a Screenshot, then answer
            QTimer::singleShot(4000, self, [self, answer] {
                if (self) {
                    Answer(self, answer);
                }
            });
            return;
        }
        Answer(self, answer);
    });
#endif
}

// The credits: THIRD_PARTY_NOTICES.md next to the program (the release and the install have it)
QString NoticesText() {
    QFile f(QCoreApplication::applicationDirPath() + QStringLiteral("/THIRD_PARTY_NOTICES.md"));
    if (f.open(QIODevice::ReadOnly)) {
        return QString::fromUtf8(f.readAll());
    }
    return {};
}

constexpr const char* kGitHubUrl = "https://github.com/simplysanatori/AzaharFrame";
constexpr const char* kKofiUrl = "https://ko-fi.com/sana748653";

} // namespace

QString ReportsDir() {
    const QString env = qEnvironmentVariable("AZAHARFRAME_REPORTS_DIR");
    if (!env.isEmpty()) {
        return env;
    }
    QString docs = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    if (docs.isEmpty()) {
        docs = QDir::homePath() + QStringLiteral("/Documents");
    }
    return docs + QStringLiteral("/AzaharFrame Reports");
}

void Detect() {
    const std::string dir = CrashDir();
    const auto run = CR::TakeLastRun(dir, &CR::PidIsAzahar);
    const unsigned oom = FrameMode::TakeOutOfMemory(); // AzaharFrame memguard's note, folded in
    if (run || oom) {
        CR::Context ctx;
        std::string log = FileUtil::GetUserPath(FileUtil::UserPath::LogDir) + LOG_FILE;
        if (const std::size_t txt = log.rfind(".txt"); txt != std::string::npos) {
            log.replace(txt, 4, ".old.txt"); // the run's log, moved aside as this one started
        }
        ctx.log_path = log;
        ctx.os = CR::OsText();
        ctx.settings_now = Lines(CR::SettingsSummary());
        ctx.scrub = CR::Scrubber::ForThisMachine();
        const CR::LastRun last = run ? *run : CR::LastRun{};
        ctx.clues = Clues(last.title_id);
        CR::Report r = CR::Build(last, oom != 0, ctx);
        if (r.kind == CR::Kind::OutOfMemory) {
            const auto t = VideoCore::PreloadBudget::MakeOutOfMemoryText(oom > 1 ? oom : 0);
            r.headline = t.title;
            r.body = t.body;
        }
        LOG_WARNING(Frontend, "crash report: the last run did not end cleanly: {}{}{}",
                    r.reason, r.title.empty() ? "" : ", game ", r.title);
        g_pending = std::move(r);
    }
    Common::FrameCrash::Start(dir, VersionText(), CR::ReleaseName());
    {
        std::error_code ec;
        const std::string exe = std::filesystem::read_symlink("/proc/self/exe", ec).string();
        if (!ec) {
            Common::FrameCrash::NoteExe(exe, CR::ReadBuildId(exe));
        }
    }
}

void ShowPending(QWidget* parent) {
    if (!g_pending) {
        return;
    }
    const CR::Report r = std::move(*g_pending);
    g_pending.reset();
    auto* d = static_cast<CrashDialog*>(MakeDialog(parent, r, CR::CanSend()));
    d->setAttribute(Qt::WA_DeleteOnClose);
    d->open();
    LOG_INFO(Frontend, "crash report: {} shown ({}; Send Report {})", r.headline, r.reason,
             CR::CanSend() ? "offered" : "not built in");
    AnswerForTest(d, QStringLiteral("Mock run: Send Report test"));
}

QDialog* MakeDialog(QWidget* parent, const CR::Report& report, bool can_send) {
    return new CrashDialog(parent, report, can_send);
}

QDialog* MakeProblem(QWidget* parent, const ProblemContext& context) {
    CR::LastRun now;
    now.marker = true;
    now.version = VersionText();
    now.release = CR::ReleaseName();
    now.title_id = context.title_id;
    now.title = context.title;
    now.vr = VrNow();
    CR::Context ctx;
    ctx.log_path = FileUtil::GetUserPath(FileUtil::UserPath::LogDir) + LOG_FILE; // this run's
    ctx.os = CR::OsText();
    ctx.settings_now = Lines(CR::SettingsSummary());
    ctx.clues = Clues(context.title_id);
    ctx.scrub = CR::Scrubber::ForThisMachine();
    return new CrashDialog(parent, CR::BuildManual(now, ctx), CR::CanSend(),
                           context.title_id != 0 || !context.title.empty() ? context.screenshot
                                                                           : ScreenshotFn{});
}

void ShowReportProblem(QWidget* parent, const ProblemContext& context) {
    auto* d = static_cast<CrashDialog*>(MakeProblem(parent, context));
    d->setAttribute(Qt::WA_DeleteOnClose);
    d->open();
    LOG_INFO(Frontend, "problem report: shown ({}; Send Report {})",
             context.title.empty() ? "no game" : "a game runs",
             CR::CanSend() ? "offered" : "not built in");
    AnswerForTest(d, QStringLiteral("Sentry test feedback %1")
                         .arg(QDateTime::currentDateTimeUtc().toString(Qt::ISODate)));
}

QDialog* MakeAbout(QWidget* parent, bool links_open) {
    auto* d = new QDialog(parent);
    d->setWindowTitle(QObject::tr("About Azahar for Frame"));
    // links readable on a dark theme (Qt's default link blue is not; the theme is a style sheet,
    // so the palette does not tell)
    const bool dark = UISettings::values.theme.contains(QStringLiteral("dark")) ||
                      d->palette().color(QPalette::Window).lightness() < 128;
    const QString link_css = dark ? QStringLiteral("a { color: #A8C7FA; }") : QString{};
    auto* layout = new QVBoxLayout(d);
    layout->setContentsMargins(24, 24, 24, 16);
    layout->setSpacing(12);
    layout->addWidget(Headline(QObject::tr("Azahar for Frame")));
    auto* grid = new QGridLayout;
    grid->setHorizontalSpacing(24);
    grid->setVerticalSpacing(8);
    int row = 0;
    const auto add_row = [&](const QString& label, const QString& value, const char* url) {
        auto* l = new QLabel(label);
        auto* v = new QLabel;
        if (url != nullptr && links_open) {
            v->setText(QStringLiteral("<style>%2</style><a href=\"%1\">%1</a>")
                           .arg(QString::fromUtf8(url), link_css));
            v->setTextFormat(Qt::RichText);
            v->setOpenExternalLinks(true);
        } else {
            v->setText(url != nullptr ? QString::fromUtf8(url) : value); // VR: no browser
            v->setTextFormat(Qt::PlainText);
        }
        v->setWordWrap(true);
        grid->addWidget(l, row, 0, Qt::AlignTop);
        grid->addWidget(v, row, 1);
        row++;
    };
    add_row(QObject::tr("Version"), QString::fromUtf8(Common::g_build_fullname), nullptr);
    add_row(QObject::tr("Build"),
            QStringLiteral("%1 %2").arg(QString::fromUtf8(Common::g_scm_branch),
                                        QString::fromUtf8(Common::g_scm_desc)),
            nullptr);
    add_row(QObject::tr("GitHub"), {}, kGitHubUrl);
    add_row(QObject::tr("Ko-fi"), {}, kKofiUrl);
    grid->setColumnStretch(1, 1);
    layout->addLayout(grid);
    layout->addSpacing(4);
    layout->addWidget(new QLabel(QObject::tr("Credits")));
    auto* credits = new QTextBrowser;
    credits->setOpenExternalLinks(links_open);
    credits->setOpenLinks(links_open);
    credits->document()->setDefaultStyleSheet(link_css);
    const QString notices = NoticesText();
    if (notices.isEmpty()) {
        credits->setPlainText(QObject::tr("THIRD_PARTY_NOTICES.md: %1/blob/steam-frame/"
                                          "THIRD_PARTY_NOTICES.md")
                                  .arg(QString::fromUtf8(kGitHubUrl)));
    } else {
        credits->setMarkdown(notices);
        if (dark) {
            // the markdown import colours links itself (Qt's dark blue): recolour them
            QTextDocument* doc = credits->document();
            std::vector<std::pair<int, int>> links;
            for (QTextBlock b = doc->begin(); b.isValid(); b = b.next()) {
                for (auto it = b.begin(); !it.atEnd(); ++it) {
                    const QTextFragment f = it.fragment();
                    if (f.charFormat().isAnchor()) {
                        links.emplace_back(f.position(), f.position() + f.length());
                    }
                }
            }
            QTextCharFormat colour;
            colour.setForeground(QColor(0xA8, 0xC7, 0xFA));
            for (const auto& [from, to] : links) {
                QTextCursor c(doc);
                c.setPosition(from);
                c.setPosition(to, QTextCursor::KeepAnchor);
                c.mergeCharFormat(colour);
            }
        }
    }
    layout->addWidget(credits, 1);
    auto* buttons = new QHBoxLayout;
    buttons->addStretch(1);
    auto* close = TextButton(QObject::tr("Close"));
    close->setDefault(true);
    buttons->addWidget(close);
    QObject::connect(close, &QPushButton::clicked, d, &QDialog::accept);
    layout->addLayout(buttons);
    d->setMinimumSize(720, 520);
    return d;
}

void ShowAbout(QWidget* parent) {
    const bool vr = VrNow();
    auto* d = MakeAbout(parent, !vr);
    d->setAttribute(Qt::WA_DeleteOnClose);
    d->open();
    LOG_INFO(Frontend, "about: shown ({})", vr ? "VR, links as text" : "Flat");
}

QDialog* MakeWhatsSent(QWidget* parent, const CR::Report& report, bool can_send) {
    auto* d = new QDialog(parent);
    d->setWindowTitle(QObject::tr("What's Sent"));
    auto* layout = new QVBoxLayout(d);
    layout->setContentsMargins(24, 24, 24, 16);
    layout->setSpacing(12);
    layout->addWidget(Headline(QObject::tr("What's Sent")));
    const bool shot = !report.screenshot_png.empty();
    auto* body = new QLabel(
        (can_send
            ? (shot ? QObject::tr("Send Report sends this text, the log under it and the "
                                  "screenshot, and nothing else. Save Report puts the same three "
                                  "files in a zip.")
                    : QObject::tr("Send Report sends this text and the log under it, and nothing "
                                  "else. Save Report puts the same two files in a zip."))
            : (shot ? QObject::tr("Save Report puts this text, the log under it and the "
                                  "screenshot in a zip.")
                    : QObject::tr("Save Report puts this text and the log under it in a zip."))) +
        QLatin1Char(' ') +
        QObject::tr("The log can name your games and their folders. Your home folder shows as ~, "
                    "IP and MAC addresses as <ip> and <mac>."));
    body->setTextFormat(Qt::PlainText);
    body->setWordWrap(true);
    layout->addWidget(body);
    auto* text = new QPlainTextEdit;
    text->setReadOnly(true);
    text->setLineWrapMode(QPlainTextEdit::NoWrap);
    text->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    text->setPlainText(QString::fromStdString(CR::FullText(report)));
    layout->addWidget(text, 1);
    auto* buttons = new QHBoxLayout;
    buttons->addStretch(1);
    auto* close = TextButton(QObject::tr("Close"));
    close->setDefault(true);
    buttons->addWidget(close);
    QObject::connect(close, &QPushButton::clicked, d, &QDialog::accept);
    layout->addLayout(buttons);
    d->setMinimumSize(720, 480);
    return d;
}

QDialog* MakeSaved(QWidget* parent, const QString& shown_path) {
    auto* d = new QDialog(parent);
    d->setWindowTitle(QObject::tr("Report Saved"));
    auto* layout = new QVBoxLayout(d);
    layout->setContentsMargins(24, 24, 24, 16);
    layout->setSpacing(12);
    layout->addWidget(Headline(QObject::tr("Report Saved")));
    const QFileInfo info(shown_path);
    auto* body = new QLabel(QObject::tr("Saved in %1 as %2").arg(info.path(), info.fileName()));
    body->setWordWrap(true);
    body->setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout->addWidget(body);
    auto* buttons = new QHBoxLayout;
    buttons->addStretch(1);
    auto* ok = TextButton(QObject::tr("OK"));
    ok->setDefault(true);
    buttons->addWidget(ok);
    QObject::connect(ok, &QPushButton::clicked, d, &QDialog::accept);
    layout->addLayout(buttons);
    d->setMinimumWidth(420);
    return d;
}

void TestShots(QWidget* parent) {
#ifndef AZAHARFRAME_RELEASE_BUILD
    const QString dir = FrameTestEnv("AZAHARFRAME_TEST_CRASH_SHOTS");
    if (dir.isEmpty()) {
        return;
    }
    QDir().mkpath(dir);
    const auto file = [&](const char* name) { return dir + QLatin1Char('/') + QLatin1String(name); };
    const CR::Report lost = SampleReport(CR::Kind::DeviceLost);
    const CR::Report oom = SampleReport(CR::Kind::OutOfMemory);
    {
        std::unique_ptr<QDialog> d{MakeDialog(parent, lost, true)};
        Grab(d.get(), file("crash-dialog-qt.png"));
        GrabOver(parent, d.get(), file("crash-dialog-vr-window.png"));
    }
    {
        std::unique_ptr<QDialog> d{MakeDialog(parent, lost, false)};
        Grab(d.get(), file("crash-dialog-no-send-qt.png"));
    }
    {
        std::unique_ptr<QDialog> d{MakeDialog(parent, oom, true)};
        Grab(d.get(), file("crash-dialog-oom-qt.png"));
        GrabOver(parent, d.get(), file("crash-dialog-oom-vr-window.png"));
    }
    {
        auto* raw = static_cast<CrashDialog*>(MakeDialog(parent, lost, true));
        std::unique_ptr<QDialog> d{raw};
        raw->Note()->setReadOnly(false);
        raw->Note()->setPlainText(QStringLiteral(
            "I opened the Mods tab and turned on the 4K pack, then pressed Continue.\n"
            "It froze after a few seconds"));
        raw->Note()->setFocus();
        raw->Note()->moveCursor(QTextCursor::End);
        Grab(d.get(), file("crash-dialog-note-qt.png"));
        // the VR keyboard open on the box, with the same text, under the window and the dialog
        FrameXr::Swkbd::Model kb(FrameXr::Menu::CrashNoteSpec(raw->Note()->toPlainText().toStdString()));
        GrabKeyboardOver(parent, d.get(), KeyboardImage(kb, -1), file("crash-keyboard-vr.png"));
        KeyboardImage(kb, -1).save(file("crash-keyboard-panel.png"));
        std::unique_ptr<QDialog> view{MakeWhatsSent(parent, raw->Current(), true)};
        Grab(view.get(), file("crash-whats-sent-qt.png"));
        GrabOver(parent, view.get(), file("crash-whats-sent-vr-window.png"));
    }
    {
        const QString shown = Shown(ReportsDir()) + QLatin1Char('/') +
                              QString::fromStdString(CR::ZipName(QDateTime::currentSecsSinceEpoch()));
        std::unique_ptr<QDialog> d{MakeSaved(parent, shown)};
        Grab(d.get(), file("crash-saved-qt.png"));
        GrabOver(parent, d.get(), file("crash-saved-vr-window.png"));
    }
    // AzaharFrame crashreport: Report a Problem and About
    {
        // a screenshot as Include a Screenshot takes it: here, a picture of the window's middle
        QByteArray png;
        {
            QBuffer buf(&png);
            buf.open(QIODevice::WriteOnly);
            parent->grab().toImage().scaled(400, 480).save(&buf, "PNG");
        }
        ProblemContext pc;
        pc.title_id = 0x0004000000054000;
        pc.title = "Super Mario 3D Land";
        pc.screenshot = [png](std::function<void(QByteArray)> done) { done(png); };
        auto* raw = static_cast<CrashDialog*>(MakeProblem(parent, pc));
        std::unique_ptr<QDialog> d{raw};
        Grab(d.get(), file("problem-dialog-qt.png")); // empty: Send Report greyed
        GrabOver(parent, d.get(), file("problem-dialog-vr-window.png"));
        raw->Note()->setReadOnly(false);
        raw->Note()->setPlainText(QStringLiteral(
            "The top screen flickers when I open the map in World 1-3.\nIt stops after a restart."));
        raw->SetScreenshot(png);
        Grab(d.get(), file("problem-dialog-typed-qt.png"));
        FrameXr::Swkbd::Model kb(FrameXr::Menu::CrashNoteSpec(
            raw->Note()->toPlainText().toStdString(), FrameXr::Menu::kProblemNoteLabel));
        GrabKeyboardOver(parent, d.get(), KeyboardImage(kb, -1), file("problem-keyboard-vr.png"));
        KeyboardImage(kb, -1).save(file("problem-keyboard-panel.png"));
        std::unique_ptr<QDialog> view{MakeWhatsSent(parent, raw->Current(), true)};
        Grab(view.get(), file("problem-whats-sent-qt.png"));
        // with no game: no Game line, no Include a Screenshot
        std::unique_ptr<QDialog> none{MakeProblem(parent, ProblemContext{})};
        Grab(none.get(), file("problem-dialog-no-game-qt.png"));
        // the sample, as Save Report saves it
        std::string error;
        CR::SaveZip(raw->Current(), dir.toStdString(), QDateTime::currentSecsSinceEpoch(), &error);
        QFile out(file("sample-problem-report.txt"));
        if (out.open(QIODevice::WriteOnly)) {
            out.write(QByteArray::fromStdString(CR::FullText(raw->Current())));
        }
    }
    {
        std::unique_ptr<QDialog> flat{MakeAbout(parent, true)};
        Grab(flat.get(), file("about-qt.png"));
        std::unique_ptr<QDialog> vr{MakeAbout(parent, false)};
        GrabOver(parent, vr.get(), file("about-vr-window.png"));
    }
    if (auto* help = parent->findChild<QMenu*>(QStringLiteral("menu_Help"))) {
        // Help, open over the window
        help->popup(parent->mapToGlobal(QPoint(0, 0)));
        QApplication::processEvents();
        const QPixmap menu = help->grab();
        help->hide();
        QPixmap all = parent->grab();
        QPoint at(0, 0);
        if (auto* bar = parent->findChild<QMenuBar*>()) {
            const QRect r = bar->actionGeometry(help->menuAction());
            at = bar->mapTo(parent, r.bottomLeft());
        }
        QPainter painter(&all);
        painter.drawPixmap(at, menu);
        painter.end();
        all.save(file("help-menu-qt.png"));
    }
    {
        // the sample report itself, saved as Save Report saves it (into DIR, not Documents)
        std::string error;
        CR::Report r = lost;
        r.note = "I opened the Mods tab and turned on the 4K pack.";
        CR::SaveZip(r, dir.toStdString(), QDateTime::currentSecsSinceEpoch(), &error);
        QFile out(file("sample-report.txt"));
        if (out.open(QIODevice::WriteOnly)) {
            out.write(QByteArray::fromStdString(CR::FullText(r)));
        }
    }
#endif
}

} // namespace FrameCrashDialog
