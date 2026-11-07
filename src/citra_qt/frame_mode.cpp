// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QEvent>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QPointer>
#include <QProcess>
#include <QSaveFile>
#include <QTimer>
#include <QWidget>
#include <QWindow>

#ifdef HAVE_SDL2
#include <SDL.h>
#endif

#include "citra_qt/frame_mode.h"
#include "common/logging/log.h"

namespace FrameMode {

// ---- The gamepad ------------------------------------------------------------------------

#ifdef HAVE_SDL2
namespace {
std::mutex g_pad_mutex;
SDL_GameController* g_pad = nullptr;
std::chrono::steady_clock::time_point g_next_scan{};
} // namespace
#endif

namespace {

// Test driver: AZAHARFRAME_TEST_FLAT_PAD=FILE is the gamepad instead of SDL's, for mock runs
// without a controller. One event per line, "<seconds since start> <control> <value>", lasting
// until changed: a b x y menu view lb rb ls rs up down left right (0 or 1), lt rt lx ly rx ry
// (-1 to 1, stick y up).
struct TestPadEvent {
    double at;
    std::string control;
    float value;
};

const std::chrono::steady_clock::time_point g_start = std::chrono::steady_clock::now();

const std::vector<TestPadEvent>* TestPadScript() {
    static const std::unique_ptr<std::vector<TestPadEvent>> script = [] {
        std::unique_ptr<std::vector<TestPadEvent>> out;
        const QString path = qEnvironmentVariable("AZAHARFRAME_TEST_FLAT_PAD");
        QFile file(path);
        if (path.isEmpty() || !file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            return out;
        }
        out = std::make_unique<std::vector<TestPadEvent>>();
        while (!file.atEnd()) {
            const QStringList f = QString::fromUtf8(file.readLine())
                                      .simplified()
                                      .split(QLatin1Char(' '), Qt::SkipEmptyParts);
            if (f.size() == 3 && !f[0].startsWith(QLatin1Char('#'))) {
                out->push_back({f[0].toDouble(), f[1].toStdString(), f[2].toFloat()});
            }
        }
        std::stable_sort(out->begin(), out->end(),
                         [](const auto& l, const auto& r) { return l.at < r.at; });
        LOG_INFO(Frontend, "xr test: flat gamepad from {} ({} events)", path.toStdString(),
                 out->size());
        return out;
    }();
    return script.get();
}

void ReadTestPad(const std::vector<TestPadEvent>& script, FrameXr::Input::InputFrame& frame) {
    const double now =
        std::chrono::duration<double>(std::chrono::steady_clock::now() - g_start).count();
    auto& l = frame.hand[0];
    auto& r = frame.hand[1];
    for (const auto& e : script) {
        if (e.at > now) {
            break;
        }
        const bool on = e.value > 0.5f;
        const std::string& c = e.control;
        if (c == "a") frame.a = on;
        else if (c == "b") frame.b = on;
        else if (c == "x") frame.x = on;
        else if (c == "y") frame.y = on;
        else if (c == "menu") frame.menu = on;
        else if (c == "view") frame.view = on;
        else if (c == "up") frame.dpad_up = on;
        else if (c == "down") frame.dpad_down = on;
        else if (c == "left") frame.dpad_left = on;
        else if (c == "right") frame.dpad_right = on;
        else if (c == "lb") l.bumper = on;
        else if (c == "rb") r.bumper = on;
        else if (c == "ls") l.stick_click = on;
        else if (c == "rs") r.stick_click = on;
        else if (c == "lt") l.trigger = e.value;
        else if (c == "rt") r.trigger = e.value;
        else if (c == "lx") l.stick.x = e.value;
        else if (c == "ly") l.stick.y = e.value;
        else if (c == "rx") r.stick.x = e.value;
        else if (c == "ry") r.stick.y = e.value;
    }
}

} // namespace

bool ReadGamepad(FrameXr::Input::InputFrame& frame) {
    if (const auto* script = TestPadScript()) {
        ReadTestPad(*script, frame);
        return true;
    }
#ifndef HAVE_SDL2
    static_cast<void>(frame);
    return false;
#else
    std::scoped_lock lock{g_pad_mutex};
    if (g_pad && !SDL_GameControllerGetAttached(g_pad)) {
        LOG_INFO(Frontend, "xr: flat: the gamepad went away");
        SDL_GameControllerClose(g_pad);
        g_pad = nullptr;
    }
    if (!g_pad) {
        // input_common initialises SDL's game controller part and pumps its events.
        const auto now = std::chrono::steady_clock::now();
        if (now < g_next_scan || SDL_WasInit(SDL_INIT_GAMECONTROLLER) == 0) {
            return false;
        }
        g_next_scan = now + std::chrono::seconds(1);
        for (int i = 0; i < SDL_NumJoysticks() && !g_pad; i++) {
            if (SDL_IsGameController(i)) {
                g_pad = SDL_GameControllerOpen(i);
            }
        }
        if (!g_pad) {
            return false;
        }
        const char* name = SDL_GameControllerName(g_pad);
        LOG_INFO(Frontend, "xr: flat: gamepad \"{}\"", name ? name : "?");
    }
    const auto button = [](SDL_GameControllerButton b) {
        return SDL_GameControllerGetButton(g_pad, b) != 0;
    };
    const auto axis = [](SDL_GameControllerAxis a) {
        return std::clamp(SDL_GameControllerGetAxis(g_pad, a) / 32767.0f, -1.0f, 1.0f);
    };
    // Steam Input's gamepad has the Frame's buttons where their names say (A is A, and so on),
    // so they mean the same as in VR. SDL's stick Y grows downwards, OpenXR's upwards.
    frame.a = button(SDL_CONTROLLER_BUTTON_A);
    frame.b = button(SDL_CONTROLLER_BUTTON_B);
    frame.x = button(SDL_CONTROLLER_BUTTON_X);
    frame.y = button(SDL_CONTROLLER_BUTTON_Y);
    frame.menu = button(SDL_CONTROLLER_BUTTON_START);
    frame.view = button(SDL_CONTROLLER_BUTTON_BACK);
    frame.dpad_up = button(SDL_CONTROLLER_BUTTON_DPAD_UP);
    frame.dpad_down = button(SDL_CONTROLLER_BUTTON_DPAD_DOWN);
    frame.dpad_left = button(SDL_CONTROLLER_BUTTON_DPAD_LEFT);
    frame.dpad_right = button(SDL_CONTROLLER_BUTTON_DPAD_RIGHT);
    auto& left = frame.hand[0];
    auto& right = frame.hand[1];
    left.bumper = button(SDL_CONTROLLER_BUTTON_LEFTSHOULDER);
    right.bumper = button(SDL_CONTROLLER_BUTTON_RIGHTSHOULDER);
    left.stick_click = button(SDL_CONTROLLER_BUTTON_LEFTSTICK);
    right.stick_click = button(SDL_CONTROLLER_BUTTON_RIGHTSTICK);
    left.stick = {axis(SDL_CONTROLLER_AXIS_LEFTX), -axis(SDL_CONTROLLER_AXIS_LEFTY)};
    right.stick = {axis(SDL_CONTROLLER_AXIS_RIGHTX), -axis(SDL_CONTROLLER_AXIS_RIGHTY)};
    left.trigger = std::max(0.0f, axis(SDL_CONTROLLER_AXIS_TRIGGERLEFT));
    right.trigger = std::max(0.0f, axis(SDL_CONTROLLER_AXIS_TRIGGERRIGHT));
    left.grip = right.grip = 0.0f;
    left.active = right.active = false;
    return true;
#endif
}

// ---- The mode file ----------------------------------------------------------------------

void WriteModeFile(bool vr) {
    if (qEnvironmentVariable("AZAHARFRAME_MODE").isEmpty()) {
        return; // not started by the launcher (a test or a terminal run): leave it alone
    }
    QString home = qEnvironmentVariable("AZAHARFRAME_HOME");
    if (home.isEmpty()) {
        home = QDir::homePath() + QStringLiteral("/.local/share/AzaharFrame");
    }
    QSaveFile file(home + QStringLiteral("/mode"));
    const QByteArray word = vr ? QByteArrayLiteral("vr\n") : QByteArrayLiteral("flat\n");
    if (file.open(QIODevice::WriteOnly) && file.write(word) == word.size() && file.commit()) {
        LOG_INFO(Frontend, "xr: mode file: {}", vr ? "vr" : "flat");
    } else {
        LOG_WARNING(Frontend, "xr: could not write the mode file in {}", home.toStdString());
    }
}

// ---- The window -------------------------------------------------------------------------

namespace {

bool InGameMode() {
    return qEnvironmentVariable("XDG_CURRENT_DESKTOP") == QStringLiteral("gamescope");
}

// Where the flat window goes in the headset: "theater" (default), "dashboard" (only shown in
// the dashboard, as before) or "off" (Steam is not asked at all). AZAHARFRAME_FLAT_SCREEN.
QString FlatScreenMode() {
    const QString mode = qEnvironmentVariable("AZAHARFRAME_FLAT_SCREEN").trimmed().toLower();
    if (mode == QStringLiteral("dashboard") || mode == QStringLiteral("off")) {
        return mode;
    }
    return QStringLiteral("theater");
}

QString SteamAppId() {
    return qEnvironmentVariable("SteamAppId").trimmed();
}

QString CefScript() {
    return QCoreApplication::applicationDirPath() + QStringLiteral("/steam-cef.py");
}

/// Under gamescope, started by Steam, with steam-cef.py next to the program.
bool CanAskSteam() {
    return InGameMode() && !SteamAppId().isEmpty() && QFileInfo::exists(CefScript()) &&
           FlatScreenMode() != QStringLiteral("off");
}

// steam-cef.py ARGS in the background; `done` gets the exit code and the output (GUI thread).
QProcess* RunCef(const QStringList& args, std::function<void(int, const QString&)> done) {
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.remove(QStringLiteral("LD_LIBRARY_PATH"));
    env.remove(QStringLiteral("LD_PRELOAD")); // Steam's overlay library: only noise in Python
    auto* process = new QProcess(qApp);
    process->setProcessEnvironment(env);
    process->setProcessChannelMode(QProcess::MergedChannels);
    QObject::connect(process, &QProcess::finished, process,
                     [process, done = std::move(done)](int code, QProcess::ExitStatus status) {
                         const QString out = QString::fromUtf8(process->readAll()).trimmed();
                         done(status == QProcess::NormalExit ? code : -1, out);
                         process->deleteLater();
                     });
    QStringList argv{QStringLiteral("-I"), CefScript()};
    argv << args;
    process->start(QStringLiteral("python3"), argv);
    return process;
}

// Shows the window in the dashboard right away (ShowDashboard of its overlay). The dashboard
// hides it again when it closes unless the window's frame is in the theater (DockWindow).
void AskSteamForFlatScreen(const QString& when) {
    if (!CanAskSteam()) {
        return;
    }
    const QString js =
        QStringLiteral("SteamClient.OpenVR.VROverlay.ShowDashboard('valve.steam.desktopgame.%1')")
            .arg(SteamAppId());
    RunCef({js}, [when](int code, const QString& out) {
        // ShowDashboard returns nothing: "null" is its normal answer.
        LOG_INFO(Frontend, "xr: asked Steam to show the window in the dashboard {}: {}{}",
                 when.toStdString(), code == 0 ? "done" : "failed",
                 code == 0 ? "" : (" " + out.toStdString()));
    });
}

// The window's frame: in the theater (the big screen, stays when the dashboard closes) or back
// in the dashboard. vrcmd --dock-overlay through steam-cef.py --screen; a few seconds (each
// vrcmd call takes about 2 s). One at a time; a new request ends the one before.
QPointer<QProcess> g_screen_helper;
bool g_in_theater = false; ///< this run docked the window's frame in the theater
int g_dock_request = 0;    ///< counts DockWindow calls: a retry only while no newer one came

void StopScreenHelper() {
    ++g_dock_request; // and a retry still waiting is not made
    if (g_screen_helper) {
        g_screen_helper->disconnect();
        g_screen_helper->kill();
        g_screen_helper->deleteLater();
        g_screen_helper = nullptr;
    }
}

// after_vr: steam-cef.py first opens the dashboard on the window once SteamVR can show it, and
// docks it again when it does not show (--after-vr); with nothing moved (the window never became
// showable) it is asked again twice, 2 s apart, while flat.
void DockWindow(bool theater, int wait_s, std::function<void()> then = {}, bool after_vr = false,
                int tries_left = 0) {
    StopScreenHelper();
    const int request = ++g_dock_request;
    if (!CanAskSteam() || (theater && FlatScreenMode() != QStringLiteral("theater"))) {
        if (then) {
            then();
        }
        return;
    }
    const QString where = theater ? QStringLiteral("theater") : QStringLiteral("dashboard");
    LOG_INFO(Frontend, "xr: moving the window's frame to the {}", where.toStdString());
    if (theater) {
        // From now on: a move cut short may still have happened (vrcmd runs on), and moving a
        // frame that is in the dashboard already back to it changes nothing.
        g_in_theater = true;
    }
    QStringList args{QStringLiteral("--screen"), where, SteamAppId(), QString::number(wait_s)};
    if (after_vr) {
        args << QStringLiteral("--after-vr");
    }
    g_screen_helper = RunCef(args, [theater, then, wait_s, after_vr, tries_left,
                                    request](int code, const QString& out) {
        // 0: moved. 1: the window's overlay was not there or could not show, nothing moved.
        // 2: moved after VR, but SteamVR does not show the window (the output says how it is).
        if (code == 0 || code == 1 || code == 2) {
            g_in_theater = theater && code != 1;
        }
        LOG_INFO(Frontend, "xr: window frame: {}{}", out.toStdString(),
                 code == 2 ? " (exit 2: not shown)" : "");
        if (code == 1 && theater && tries_left > 0) {
            QTimer::singleShot(2000, qApp, [wait_s, then, after_vr, tries_left, request] {
                if (request == g_dock_request) { // nothing else asked meanwhile (VR again, a stop)
                    LOG_INFO(Frontend, "xr: the window's frame to the theater, again");
                    DockWindow(true, wait_s, then, after_vr, tries_left - 1);
                }
            });
            return;
        }
        if (then) {
            then();
        }
    });
}

// Logs the window's states, and after VR waits for it to be exposed again.
class WindowWatch final : public QObject {
public:
    WindowWatch(QWindow* window_, QWidget* widget_)
        : QObject(window_), window{window_}, widget{widget_} {
        setObjectName(QStringLiteral("azaharframeWindowWatch"));
        window->installEventFilter(this);
        connect(window, &QWindow::visibleChanged, this,
                [this](bool visible) { Log(visible ? "shown" : "hidden"); });
        connect(window, &QWindow::activeChanged, this,
                [this] { Log(window->isActive() ? "active" : "inactive"); });
        Log("watched");
    }

    void StartReturn() {
        ++generation;
        since.start();
        Log("after VR");
        if (window->isExposed()) {
            // Under gamescope the window stays mapped (only see-through) in VR, so it usually
            // never stopped being exposed and no expose event will come.
            returning = false;
            was_exposed = true;
            return;
        }
        returning = true;
        const int gen = generation;
        for (int ms : {1000, 3000}) {
            QTimer::singleShot(ms, this, [this, gen, ms] {
                if (gen != generation || !returning || !window) {
                    return;
                }
                if (!window->isExposed()) {
                    Log(ms == 1000 ? "not exposed after 1 s, showing it again"
                                   : "not exposed after 3 s, showing it again");
                    if (widget) {
                        widget->show(); // never hidden first: the window stays mapped
                        widget->raise();
                        widget->activateWindow();
                    } else {
                        window->show();
                        window->raise();
                        window->requestActivate();
                    }
                }
            });
        }
        QTimer::singleShot(6000, this, [this, gen] {
            if (gen == generation && returning) {
                returning = false;
                Log(window && window->isExposed() ? "exposed 6 s after VR (no expose event)"
                                                  : "still not exposed 6 s after VR");
            }
        });
    }

    /// VR again: the return watch above ends.
    void StopReturn() {
        ++generation;
        returning = false;
    }

protected:
    bool eventFilter(QObject* object, QEvent* event) override {
        if (object == window && event->type() == QEvent::Expose) {
            const bool exposed = window->isExposed();
            if (exposed != was_exposed) {
                was_exposed = exposed;
                Log(exposed ? "exposed" : "no longer exposed");
            }
            if (exposed && returning) {
                returning = false;
                LOG_INFO(Frontend, "xr: window exposed {} ms after VR",
                         static_cast<long long>(since.elapsed()));
            }
        } else if (object == window && event->type() == QEvent::PlatformSurface) {
            Log("platform surface changed");
        }
        return false;
    }

private:
    void Log(const char* what) const {
        if (!window) {
            return;
        }
        LOG_INFO(Frontend, "xr: window {} (visible {}, exposed {}, active {}, opacity {:.0f})",
                 what, window->isVisible(), window->isExposed(), window->isActive(),
                 window->opacity());
    }

    QPointer<QWindow> window;
    QPointer<QWidget> widget;
    bool was_exposed = false;
    bool returning = false;
    int generation = 0;
    QElapsedTimer since;
};

WindowWatch* WatchOf(QWidget* widget) {
    QWindow* window = widget ? widget->windowHandle() : nullptr;
    if (!window) {
        return nullptr;
    }
    // By name (WindowWatch has no Q_OBJECT, so no qobject_cast); only this file makes one.
    if (auto* watch = window->findChild<QObject*>(QStringLiteral("azaharframeWindowWatch"),
                                                  Qt::FindDirectChildrenOnly)) {
        return static_cast<WindowWatch*>(watch);
    }
    return new WindowWatch(window, widget);
}

} // namespace

bool FadesWindow() {
    const QString force = qEnvironmentVariable("AZAHARFRAME_FADE_WINDOW");
    if (force == QStringLiteral("0")) {
        return false;
    }
    return force == QStringLiteral("1") || InGameMode();
}

void FadeWindow(QWidget* window, bool out) {
    if (!window) {
        return;
    }
    WatchOf(window);
    if (!FadesWindow()) {
        return;
    }
    if (out && QGuiApplication::platformName() != QStringLiteral("xcb")) {
        LOG_WARNING(Frontend, "xr: fading the window needs X11 (Qt platform {}); it stays",
                    QGuiApplication::platformName().toStdString());
    }
    // On xcb this is _NET_WM_WINDOW_OPACITY; gamescope skips windows at 0 (the window stays
    // mapped and keeps the keyboard focus gamescope expects).
    window->setWindowOpacity(out ? 0.0 : 1.0);
    LOG_INFO(Frontend, "xr: window {}", out ? "see-through for VR (still mapped)" : "opaque, flat");
}

void ReturnToFlat(QWidget* window) {
    FadeWindow(window, false);
    if (WindowWatch* watch = WatchOf(window)) {
        watch->StartReturn();
    }
    // SteamVR does not bring the window back by itself after a session. In the theater (the
    // default) steam-cef.py --after-vr shows it in the dashboard and moves its frame to the
    // theater once SteamVR can show the window: asked any earlier, the dashboard opens Steam's
    // library and the dock moves the library's frame (the theater stays empty). Kept in the
    // dashboard (AZAHARFRAME_FLAT_SCREEN=dashboard): only shown there.
    if (FlatScreenMode() == QStringLiteral("theater")) {
        DockWindow(true, 15, {}, true, 2);
    } else {
        AskSteamForFlatScreen(QStringLiteral("after VR"));
    }
}

void StartFlat(QWidget* window) {
    WatchOf(window);
    DockWindow(true, 20); // a flat start: the window may take a while to show
}

void LeaveFlat(QWidget* window, std::function<void()> then) {
    if (WindowWatch* watch = WatchOf(window)) {
        watch->StopReturn();
    }
    if (g_in_theater) {
        // Back into the dashboard while the window is still opaque (its overlay goes away once it
        // is see-through), so the theater screen does not stay over the VR game.
        // Once, when the move is done or after 8 s at the latest (vrcmd may hang).
        auto once = std::make_shared<std::function<void()>>(std::move(then));
        const auto run = [once] {
            if (*once) {
                auto f = std::move(*once);
                *once = nullptr;
                f();
            }
        };
        DockWindow(false, 0, run);
        QTimer::singleShot(8000, qApp, run);
    } else {
        StopScreenHelper(); // a move to the theater may still be under way
        if (then) {
            then();
        }
    }
}

} // namespace FrameMode
