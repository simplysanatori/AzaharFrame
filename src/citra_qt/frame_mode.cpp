// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <cmath>
#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <QApplication>
#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QEvent>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QPointer>
#include <QProcess>
#include <QPushButton>
#include <QSaveFile>
#include <QTimer>
#include <QWidget>
#include <QWindow>

#include <fmt/format.h>

#include "citra_qt/frame_mode.h"
#include "common/logging/log.h"
#include "frame_xr/input/flat_pads.h"
#include "input_common/main.h"

#ifdef HAVE_SDL2
#include <SDL.h>
#endif

namespace FrameMode {

// ---- The gamepad ------------------------------------------------------------------------

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

// AzaharFrame flatfix: AZAHARFRAME_TEST_FLAT_PAD_SDL=1 sends the test script through SDL instead:
// a thread of its own plays it on an SDL virtual game controller ("AzaharFrame Test Pad"), and
// ReadGamepad reads SDL as it reads Steam Input's pad, so a mock run covers input_common's SDL
// thread and its locks too. Two more controls: "attach 0" takes the pad away and "attach 1"
// brings it back (as Steam Input re-creates its pad), "vrbutton 1" clicks the status bar's VR
// button. The pad comes with the first Flat pass; the thread ends after the last event.
bool TestPadThroughSdl() {
    static const bool on = qEnvironmentVariable("AZAHARFRAME_TEST_FLAT_PAD_SDL") ==
                           QStringLiteral("1");
    return on;
}

#ifdef HAVE_SDL2
void ClickVrButton() {
    QMetaObject::invokeMethod(
        qApp,
        [] {
            for (QWidget* w : QApplication::allWidgets()) {
                auto* button = qobject_cast<QPushButton*>(w);
                if (button && button->objectName() == QStringLiteral("GraphicsAPIStatusBarButton") &&
                    button->text() == QStringLiteral("VR")) {
                    LOG_INFO(Frontend, "xr test: clicking the status bar's VR button ({})",
                             button->isVisible() && button->isEnabled() ? "shown" : "hidden");
                    button->click();
                }
            }
        },
        Qt::QueuedConnection);
}

void PlaySdlTestPad(const std::vector<TestPadEvent>& script) {
    SDL_Joystick* joystick = nullptr;
    SDL_JoystickID instance = -1;
    // Device index calls under SDL's joystick lock: input_common's thread uses indices too.
    const auto attach = [&] {
        SDL_LockJoysticks();
        SDL_VirtualJoystickDesc desc{};
        desc.version = SDL_VIRTUAL_JOYSTICK_DESC_VERSION;
        desc.type = SDL_JOYSTICK_TYPE_GAMECONTROLLER;
        desc.naxes = SDL_CONTROLLER_AXIS_MAX;
        desc.nbuttons = SDL_CONTROLLER_BUTTON_MAX;
        desc.vendor_id = 0x045e; // an Xbox 360 pad: a plain gamepad to the pad picker
        desc.product_id = 0x028e;
        desc.name = "AzaharFrame Test Pad";
        const int index = SDL_JoystickAttachVirtualEx(&desc);
        joystick = index >= 0 ? SDL_JoystickOpen(index) : nullptr;
        instance = joystick ? SDL_JoystickInstanceID(joystick) : -1;
        SDL_UnlockJoysticks();
        LOG_INFO(Frontend, "xr test: SDL test pad attached (index {}, instance {}{})", index,
                 instance, joystick ? "" : fmt::format(", {}", SDL_GetError()));
    };
    const auto detach = [&] {
        SDL_LockJoysticks();
        for (int i = 0; joystick && i < SDL_NumJoysticks(); i++) {
            if (SDL_JoystickGetDeviceInstanceID(i) == instance) {
                SDL_JoystickClose(joystick);
                SDL_JoystickDetachVirtual(i);
                joystick = nullptr;
            }
        }
        SDL_UnlockJoysticks();
        LOG_INFO(Frontend, "xr test: SDL test pad detached");
    };
    const auto button = [](const std::string& c) -> int {
        static const std::vector<std::pair<std::string, int>> names{
            {"a", SDL_CONTROLLER_BUTTON_A},
            {"b", SDL_CONTROLLER_BUTTON_B},
            {"x", SDL_CONTROLLER_BUTTON_X},
            {"y", SDL_CONTROLLER_BUTTON_Y},
            {"view", SDL_CONTROLLER_BUTTON_BACK},
            {"menu", SDL_CONTROLLER_BUTTON_START},
            {"ls", SDL_CONTROLLER_BUTTON_LEFTSTICK},
            {"rs", SDL_CONTROLLER_BUTTON_RIGHTSTICK},
            {"lb", SDL_CONTROLLER_BUTTON_LEFTSHOULDER},
            {"rb", SDL_CONTROLLER_BUTTON_RIGHTSHOULDER},
            {"up", SDL_CONTROLLER_BUTTON_DPAD_UP},
            {"down", SDL_CONTROLLER_BUTTON_DPAD_DOWN},
            {"left", SDL_CONTROLLER_BUTTON_DPAD_LEFT},
            {"right", SDL_CONTROLLER_BUTTON_DPAD_RIGHT}};
        for (const auto& [name, b] : names) {
            if (name == c) {
                return b;
            }
        }
        return -1;
    };
    const auto axis = [](const std::string& c) -> int {
        static const std::vector<std::pair<std::string, int>> names{
            {"lx", SDL_CONTROLLER_AXIS_LEFTX},         {"ly", SDL_CONTROLLER_AXIS_LEFTY},
            {"rx", SDL_CONTROLLER_AXIS_RIGHTX},        {"ry", SDL_CONTROLLER_AXIS_RIGHTY},
            {"lt", SDL_CONTROLLER_AXIS_TRIGGERLEFT},   {"rt", SDL_CONTROLLER_AXIS_TRIGGERRIGHT}};
        for (const auto& [name, a] : names) {
            if (name == c) {
                return a;
            }
        }
        return -1;
    };
    attach();
    for (const TestPadEvent& e : script) {
        while (std::chrono::steady_clock::now() - g_start <
               std::chrono::duration<double>(e.at)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
        if (e.control == "attach") {
            if (e.value > 0.5f && !joystick) {
                attach();
            } else if (e.value <= 0.5f && joystick) {
                detach();
            }
        } else if (e.control == "vrbutton") {
            if (e.value > 0.5f) {
                ClickVrButton();
            }
        } else if (const int b = button(e.control); b >= 0 && joystick) {
            SDL_JoystickSetVirtualButton(joystick, b, e.value > 0.5f ? 1 : 0);
        } else if (const int a = axis(e.control); a >= 0 && joystick) {
            // The script's stick Y grows upwards, SDL's downwards; triggers 0..1.
            const bool y = e.control == "ly" || e.control == "ry";
            const float v = std::clamp(y ? -e.value : e.value, -1.0f, 1.0f);
            SDL_JoystickSetVirtualAxis(joystick, a, static_cast<Sint16>(std::lround(v * 32767.0f)));
        }
    }
    LOG_INFO(Frontend, "xr test: SDL test pad script done ({} events)", script.size());
}
#endif

void StartSdlTestPad(const std::vector<TestPadEvent>& script) {
#ifdef HAVE_SDL2
    static std::once_flag once;
    std::call_once(once, [&script] {
        LOG_INFO(Frontend, "xr test: the flat gamepad script goes through SDL "
                           "(AZAHARFRAME_TEST_FLAT_PAD_SDL=1)");
        std::thread([&script] { PlaySdlTestPad(script); }).detach();
    });
#else
    static_cast<void>(script);
#endif
}

} // namespace

// AzaharFrame flat: the controllers input_common's SDL thread already has open (Azahar's own
// SDL input), read through its locked accessors. This thread used to open a handle of its own
// with SDL's device index calls, which are not thread-safe against input_common's thread (with
// sdl2-compat every SDL_NumJoysticks call frees and rebuilds the index list, and that thread
// calls it every millisecond): a flat start aborted in malloc. And the one pad it opened was
// kept for good. Now every controller is read and FlatPadPicker (frame_xr/input/flat_pads.h)
// picks the one that drives; with none, everything is let go.
namespace {

struct FlatPads {
    std::mutex mutex;
    std::vector<InputCommon::SDLPadState> states;
    std::vector<InputCommon::SDLPadInfo> infos;
    std::uint64_t generation = 0;
    FrameXr::Input::FlatPadPicker picker;
    int logged = -2; ///< the pad last named in the log
};

FlatPads& Pads() {
    static FlatPads pads;
    return pads;
}

std::string PadName(const FlatPads& pads, int id) {
    for (const auto& info : pads.infos) {
        if (info.id == id) {
            return fmt::format("{:04x}:{:04x} \"{}\"", info.vendor, info.product, info.name);
        }
    }
    return fmt::format("id {}", id);
}

} // namespace

bool ReadGamepad(FrameXr::Input::InputFrame& frame) {
    if (const auto* script = TestPadScript()) {
        if (!TestPadThroughSdl()) {
            ReadTestPad(*script, frame);
            return true;
        }
        StartSdlTestPad(*script); // AzaharFrame flatfix: then read SDL as below
    }
    auto& pads = Pads();
    std::scoped_lock lock{pads.mutex};
    const std::uint64_t generation = InputCommon::GetSDLPadGeneration();
    if (generation != pads.generation) {
        pads.generation = generation;
        InputCommon::GetSDLPadInfos(pads.infos);
        std::string list;
        for (const auto& info : pads.infos) {
            list += (list.empty() ? "" : ", ") + PadName(pads, info.id);
        }
        LOG_INFO(Frontend, "xr: flat: gamepads: {}", list.empty() ? "none" : list);
    }
    InputCommon::GetSDLPadStates(pads.states);
    const int64_t now = std::chrono::duration_cast<std::chrono::nanoseconds>(
                            std::chrono::steady_clock::now().time_since_epoch())
                            .count();
    const int active = pads.picker.Update(pads.states, now);
    if (active != pads.logged) {
        pads.logged = active;
        if (active < 0) {
            LOG_INFO(Frontend, "xr: flat: no gamepad");
        } else {
            LOG_INFO(Frontend, "xr: flat: gamepad {}", PadName(pads, active));
        }
    }
    const InputCommon::SDLPadState* use = nullptr;
    for (const auto& st : pads.states) {
        if (st.id == active) {
            use = &st;
        }
    }
    if (!use) {
        return false;
    }
    // SDL_GameControllerButton and SDL_GameControllerAxis numbers (input_common/main.h).
    enum Button : std::size_t {
        kButton_A = 0,
        kButton_B = 1,
        kButton_X = 2,
        kButton_Y = 3,
        kButton_BACK = 4,
        kButton_START = 6,
        kButton_LEFTSTICK = 7,
        kButton_RIGHTSTICK = 8,
        kButton_LEFTSHOULDER = 9,
        kButton_RIGHTSHOULDER = 10,
        kButton_DPAD_UP = 11,
        kButton_DPAD_DOWN = 12,
        kButton_DPAD_LEFT = 13,
        kButton_DPAD_RIGHT = 14,
    };
    enum Axis : std::size_t {
        kAxis_LEFTX = 0,
        kAxis_LEFTY = 1,
        kAxis_RIGHTX = 2,
        kAxis_RIGHTY = 3,
        kAxis_TRIGGERLEFT = 4,
        kAxis_TRIGGERRIGHT = 5,
    };
    const auto button = [use](Button b) { return use->buttons[b]; };
    const auto axis = [use](Axis a) { return use->axes[a]; };
    // Steam Input's gamepad has the Frame's buttons where their names say (A is A, and so on),
    // so they mean the same as in VR. SDL's stick Y grows downwards, OpenXR's upwards.
    frame.a = button(kButton_A);
    frame.b = button(kButton_B);
    frame.x = button(kButton_X);
    frame.y = button(kButton_Y);
    frame.menu = button(kButton_START);
    frame.view = button(kButton_BACK);
    frame.dpad_up = button(kButton_DPAD_UP);
    frame.dpad_down = button(kButton_DPAD_DOWN);
    frame.dpad_left = button(kButton_DPAD_LEFT);
    frame.dpad_right = button(kButton_DPAD_RIGHT);
    auto& left = frame.hand[0];
    auto& right = frame.hand[1];
    left.bumper = button(kButton_LEFTSHOULDER);
    right.bumper = button(kButton_RIGHTSHOULDER);
    left.stick_click = button(kButton_LEFTSTICK);
    right.stick_click = button(kButton_RIGHTSTICK);
    left.stick = {axis(kAxis_LEFTX), -axis(kAxis_LEFTY)};
    right.stick = {axis(kAxis_RIGHTX), -axis(kAxis_RIGHTY)};
    left.trigger = std::max(0.0f, axis(kAxis_TRIGGERLEFT));
    right.trigger = std::max(0.0f, axis(kAxis_TRIGGERRIGHT));
    left.grip = right.grip = 0.0f;
    left.active = right.active = false;
    return true;
}

// ---- The mode file ----------------------------------------------------------------------

// The launcher's data folder (AZAHARFRAME_HOME), or empty when the launcher did not start this
// run (a test or a terminal run): its files are left alone then.
static QString LauncherDataDir() {
    if (qEnvironmentVariable("AZAHARFRAME_MODE").isEmpty()) {
        return {};
    }
    QString home = qEnvironmentVariable("AZAHARFRAME_HOME");
    if (home.isEmpty()) {
        home = QDir::homePath() + QStringLiteral("/.local/share/AzaharFrame");
    }
    return home;
}

void MarkFlatRunning(bool flat) { // AzaharFrame flat
    const QString home = LauncherDataDir();
    if (home.isEmpty()) {
        return;
    }
    const QString path = home + QStringLiteral("/flat-running");
    static int marked = -1;
    if (marked == static_cast<int>(flat)) {
        return;
    }
    marked = static_cast<int>(flat);
    if (flat) {
        QFile file(path);
        if (file.open(QIODevice::WriteOnly)) {
            file.write(QByteArray::number(QCoreApplication::applicationPid()) + '\n');
            file.close();
            LOG_INFO(Frontend, "xr: flat-running marked (gone on a clean quit)");
        } else {
            LOG_WARNING(Frontend, "xr: could not write {}", path.toStdString());
        }
    } else if (QFile::exists(path)) {
        QFile::remove(path);
        LOG_INFO(Frontend, "xr: flat-running cleared");
    }
}

unsigned TakeOutOfMemory() { // AzaharFrame memguard
    const QString home = LauncherDataDir();
    if (home.isEmpty()) {
        return 0;
    }
    QFile file(home + QStringLiteral("/out-of-memory"));
    if (!file.exists()) {
        return 0;
    }
    unsigned limit = 0;
    if (file.open(QIODevice::ReadOnly)) {
        limit = file.read(32).trimmed().toUInt();
        file.close();
    }
    file.remove();
    LOG_WARNING(Frontend, "xr: the last run was killed at its memory limit ({} MiB)", limit);
    return limit ? limit : 1;
}

void WriteModeFile(bool vr) {
    const QString home = LauncherDataDir();
    if (home.isEmpty()) {
        return; // not started by the launcher (a test or a terminal run): leave it alone
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

// AzaharFrame flatfix: test driver. AZAHARFRAME_TEST_CEF=FILE runs FILE in place of steam-cef.py,
// outside Game Mode and Steam too (a mock run's fake: Steam itself is never asked).
QString TestCefScript() {
    return qEnvironmentVariable("AZAHARFRAME_TEST_CEF");
}

QString CefScript() {
    if (const QString test = TestCefScript(); !test.isEmpty()) {
        return test;
    }
    return QCoreApplication::applicationDirPath() + QStringLiteral("/steam-cef.py");
}

/// Under gamescope, started by Steam, with steam-cef.py next to the program.
bool CanAskSteam() {
    if (!TestCefScript().isEmpty()) {
        return QFileInfo::exists(CefScript()) && FlatScreenMode() != QStringLiteral("off");
    }
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

// after_vr: steam-cef.py docks the window again (through the world) when it does not show once
// docked (--after-vr); with nothing moved (the window never became
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
    // default) steam-cef.py --after-vr moves its frame to the theater once SteamVR can show the
    // window (asked any earlier, the dock moves the active frame, Steam's library), without
    // opening the dashboard, so Steam Input leaves the controller with the app (AzaharFrame flat).
    // Kept in the dashboard (AZAHARFRAME_FLAT_SCREEN=dashboard): only shown there.
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
        // Once, when the move is done or after 5 s at the latest (vrcmd may hang, or never see
        // the overlay): VR comes either way. AzaharFrame flatfix: 5 s (was 8), logged.
        auto once = std::make_shared<std::function<void()>>(std::move(then));
        const auto run = [once] {
            if (*once) {
                auto f = std::move(*once);
                *once = nullptr;
                f();
            }
        };
        DockWindow(false, 0, run);
        QTimer::singleShot(5000, qApp, [once, run] {
            if (*once) {
                LOG_WARNING(Frontend, "xr: the window's frame did not come back into the "
                                      "dashboard within 5 s; VR anyway");
                StopScreenHelper();
                run();
            }
        });
    } else {
        StopScreenHelper(); // a move to the theater may still be under way
        if (then) {
            then();
        }
    }
}

} // namespace FrameMode
