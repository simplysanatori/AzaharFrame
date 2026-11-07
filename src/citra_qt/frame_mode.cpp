// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <chrono>
#include <mutex>

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

bool ReadGamepad(FrameXr::Input::InputFrame& frame) {
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

void AskSteamForFlatScreen(const QString& when) {
    const QString app_id = qEnvironmentVariable("SteamAppId").trimmed();
    const QString cef = QCoreApplication::applicationDirPath() + QStringLiteral("/steam-cef.py");
    if (!InGameMode() || app_id.isEmpty() || !QFileInfo::exists(cef)) {
        return;
    }
    const QString js =
        QStringLiteral("SteamClient.OpenVR.VROverlay.ShowDashboard('valve.steam.desktopgame.%1')")
            .arg(app_id);
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.remove(QStringLiteral("LD_LIBRARY_PATH"));
    auto* process = new QProcess(qApp);
    process->setProcessEnvironment(env);
    process->setProcessChannelMode(QProcess::MergedChannels);
    QObject::connect(process, &QProcess::finished, process,
                     [process, when](int code, QProcess::ExitStatus) {
                         LOG_INFO(Frontend, "xr: asked Steam for the flat screen {}: {} {}",
                                  when.toStdString(), code == 0 ? "done" : "failed",
                                  process->readAll().trimmed().toStdString());
                         process->deleteLater();
                     });
    process->start(QStringLiteral("python3"), {QStringLiteral("-I"), cef, js});
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
        returning = true;
        since.start();
        Log("after VR");
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
                Log("still not exposed 6 s after VR; nothing more to try");
            }
        });
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
                // Once more now that it can be drawn (asked right away, the dashboard may open
                // before the window is back and show nothing).
                AskSteamForFlatScreen(QStringLiteral("once the window was exposed"));
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
    for (int ms : {0, 1000, 2500}) {
        QTimer::singleShot(ms, qApp, [ms] {
            AskSteamForFlatScreen(QStringLiteral("%1 ms after VR").arg(ms));
        });
    }
}

} // namespace FrameMode
