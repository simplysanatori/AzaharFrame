// Copyright 2024-2026 Citra Emulator Project / Azahar Emulator Project
// Licensed under GPLv3 or any later version
// Refer to the LICENSE.txt file included.

#include <algorithm> // AzaharFrame
#include <array>     // AzaharFrame layout
#include <chrono>    // AzaharFrame
#include <clocale>
#include <cstdlib> // AzaharFrame fdm
#include <cstring> // AzaharFrame fdm
#include <format>
#include <iostream>
#include <memory>
#include <optional>
#include <thread>
#include <unordered_map>
#include <QFileDialog>
#include <QFutureWatcher>
#include <QIcon>
#include <QLabel>
#include <QPointer>
#include <QTimer>
#include <QMessageBox>
#include <QPalette>
#include <QSysInfo>
#include <QtConcurrent/QtConcurrentMap>
#include <QtConcurrent/QtConcurrentRun>
#include <QtGui>
#include <QtWidgets>
#include <boost/algorithm/string/replace.hpp>
#include <fmt/format.h>
#include <fmt/ostream.h>
#ifdef __APPLE__
#include <unistd.h> // for chdir
#endif
#ifdef _WIN32
#include <shlobj.h>
#include <windows.h>
#else
#include <getopt.h>
#endif
#ifdef __unix__
#include <QVariant>
#include <QtDBus/QDBusInterface>
#include <QtDBus/QtDBus>
#include "common/linux/gamemode.h"
#endif
#include "citra_meta/common_strings.h"
#include "citra_qt/aboutdialog.h"
#include "citra_qt/applets/mii_selector.h"
#include "citra_qt/applets/swkbd.h"
#include "citra_qt/bootmanager.h"
#include "citra_qt/camera/qt_multimedia_camera.h"
#include "citra_qt/camera/still_image_camera.h"
#include "citra_qt/citra_qt.h"
#include "citra_qt/compatibility_list.h"
#include "citra_qt/configuration/config.h"
#include "citra_qt/configuration/configure_dialog.h"
#include "citra_qt/configuration/configure_per_game.h"
#include "citra_qt/debugger/console.h"
#include "citra_qt/debugger/graphics/graphics.h"
#include "citra_qt/debugger/graphics/graphics_breakpoints.h"
#include "citra_qt/debugger/graphics/graphics_cmdlists.h"
#include "citra_qt/debugger/graphics/graphics_surface.h"
#include "citra_qt/debugger/graphics/graphics_tracing.h"
#include "citra_qt/debugger/graphics/graphics_vertex_shader.h"
#include "citra_qt/debugger/ipc/recorder.h"
#include "citra_qt/debugger/lle_service_modules.h"
#if MICROPROFILE_ENABLED
#include "citra_qt/debugger/profiler.h"
#endif
#include "citra_qt/debugger/registers.h"
#include "citra_qt/debugger/wait_tree.h"
#ifdef ENABLE_DISCORD_RPC
#include "citra_qt/discord.h"
#endif
#ifdef ENABLE_FFMPEG
#include "citra_qt/dumping/dumping_dialog.h"
#endif
#include "citra_qt/game_list.h"
#include "citra_qt/hotkeys.h"
#include "citra_qt/loading_screen.h"
#include "citra_qt/movie/movie_play_dialog.h"
#include "citra_qt/movie/movie_record_dialog.h"
#include "citra_qt/multiplayer/state.h"
#include "citra_qt/qt_image_interface.h"
#include "citra_qt/qt_swizzle.h"
#include "citra_qt/uisettings.h"
#include "common/play_time_manager.h"
#ifdef ENABLE_QT_UPDATE_CHECKER
#include "citra_qt/update_checker.h"
#endif
#include "citra_qt/util/clickable_label.h"
#include "citra_qt/util/graphics_device_info.h"
#include "citra_qt/util/util.h"
#include "common/arch.h"
#include "common/common_paths.h"
#include "common/dynamic_library.h"
#include "common/file_util.h"
#include "common/literals.h"
#include "common/logging/backend.h"
#include "common/logging/log.h"
#include "common/memory_detect.h"
#include "common/scm_rev.h"
#include "common/scope_exit.h"
#if CITRA_ARCH(x86_64)
#include "common/x64/cpu_detect.h"
#endif
#include "common/settings.h"
#include "common/string_util.h"
#include "common/zstd_compression.h"
#include "core/arm/exception_handler.h"
#include "core/core.h"
#include "core/dumping/backend.h"
#include "core/file_sys/archive_extsavedata.h"
#include "core/file_sys/archive_source_sd_savedata.h"
#include "core/frontend/applets/default_applets.h"
#include "core/hle/service/am/am.h"
#include "core/hle/service/fs/archive.h"
#include "core/hle/service/nfc/nfc.h"
#include "core/loader/loader.h"
#include "core/movie.h"
#include "core/savestate.h"
#include "core/system_titles.h"
#include "input_common/main.h"
#include "ui_main.h"
#include "video_core/gpu.h"
#include "video_core/renderer_base.h"

#ifdef __APPLE__
#include "common/apple_authorization.h"
#include "common/apple_utils.h"
Q_IMPORT_PLUGIN(QDarwinCameraPermissionPlugin);
#endif

#ifdef ENABLE_DISCORD_RPC
#include "citra_qt/discord_impl.h"
#endif

#ifdef QT_STATICPLUGIN
Q_IMPORT_PLUGIN(QWindowsIntegrationPlugin);
#endif

#ifdef HAVE_SDL2
#include <SDL.h>
#endif

#ifdef ENABLE_FRAME_XR // AzaharFrame
#include <csignal>
#include "citra_qt/frame_branding.h"
#include "citra_qt/frame_cheats.h" // AzaharFrame cheatstab
#include "citra_qt/frame_mirror.h"
#include "citra_qt/frame_mods.h" // AzaharFrame texui
#include "citra_qt/frame_bottom_window.h" // AzaharFrame window
#include "citra_qt/frame_mode.h"
#include "common/frame_bottom_window.h" // AzaharFrame window
#include "common/frame_flat.h" // AzaharFrame flat3: flat mode's own screen layout
#include "core/frontend/emu_window.h"
#include "frame_xr/frame_xr.h"
#include "frame_xr/presets.h" // AzaharFrame vrqol
#include "frame_xr/swkbd.h" // AzaharFrame kbd
#include "input_common/frame_xr.h"

namespace {
// --vr: the OpenXR instance (frame_xr) for the whole run, starting in VR; --flat: the same,
// starting flat (no session until the VR / flat chord).
bool g_frame_xr_requested = false;
bool g_frame_xr_start_flat = false;
// The launcher's profile for flat without XR (SDL). With XR both modes use the Frame profile.
constexpr char kFlatGamepadProfile[] = "Flat Gamepad";
// Switches between VR and flat (writes the mode file); the XR thread's answer, the mode now.
std::function<void(bool vr)> g_frame_xr_switch;
std::function<void(bool vr)> g_frame_xr_mode_changed;
// SIGTERM / SIGINT: stop the game and quit, so the XR session ends cleanly.
volatile std::sig_atomic_t g_frame_xr_terminate = 0;
void FrameXrOnSignal(int) {
    g_frame_xr_terminate = 1;
}
// Touches on the bottom screen panel (3DS pixels) into the render window's layout.
class FrameXrTouchSink final : public InputCommon::FrameXr::TouchSink {
public:
    explicit FrameXrTouchSink(Frontend::EmuWindow& window_) : window{window_} {}
    void TouchPressed(float x, float y) override {
        const auto [fx, fy] = Map(x, y);
        LOG_INFO(Frontend, "xr: touch down bottom {:.0f},{:.0f} -> window {},{}", x, y, fx, fy);
        last_log = std::chrono::steady_clock::now();
        window.TouchPressed(fx, fy);
    }
    void TouchMoved(float x, float y) override {
        const auto [fx, fy] = Map(x, y);
        // At most two lines a second while dragging.
        const auto now = std::chrono::steady_clock::now();
        if (now - last_log >= std::chrono::milliseconds(500)) {
            last_log = now;
            LOG_INFO(Frontend, "xr: touch move bottom {:.0f},{:.0f} -> window {},{}", x, y, fx,
                     fy);
        }
        window.TouchMoved(fx, fy);
    }
    void TouchReleased() override {
        LOG_INFO(Frontend, "xr: touch up");
        window.TouchReleased();
    }

private:
    std::pair<unsigned, unsigned> Map(float x, float y) const {
        const auto& r = window.GetFramebufferLayout().bottom_screen;
        const float u = std::clamp(x / 320.0f, 0.0f, 1.0f);
        const float v = std::clamp(y / 240.0f, 0.0f, 1.0f);
        return {r.left + static_cast<unsigned>(u * (r.GetWidth() - 1)),
                r.top + static_cast<unsigned>(v * (r.GetHeight() - 1))};
    }
    Frontend::EmuWindow& window;
    std::chrono::steady_clock::time_point last_log{};
};
std::unique_ptr<FrameXrTouchSink> g_frame_xr_touch_sink;
// The VR home panel: this window mirrored (frame_mirror.cpp); null with the picker.
QPointer<FrameMirror> g_frame_mirror;
// AzaharFrame texui: the in-game menu's Mods and Textures segments (frame_mods.cpp), made with the
// menu; the games started so far (a restart clears its snackbar).
QPointer<FrameMods> g_frame_mods;
int g_frame_runs = 0;
// AzaharFrame cheatstab: the in-game menu's Cheats segment (frame_cheats.cpp), made with the menu.
QPointer<FrameCheats> g_frame_cheats;
// Opens (pausing the game) or closes (resuming it) the in-game menu: the home panel in a game.
std::function<void(bool open)> g_frame_xr_set_menu;
// Quick Save / Quick Load as the Emulation menu does them, with a message (the in-game menu's
// buttons, and View + a bumper); GUI thread. Set in GMainWindow::InitializeHotkeys-time code below.
std::function<void()> g_frame_xr_quick_save;
// AzaharFrame widerview: the per-application config of the game on view (BootGame), which
// FrontendHooks::game_setting_changed saves.
std::string g_frame_game_config_name;
std::function<void()> g_frame_xr_quick_load;
// AzaharFrame tabmenu: the in-game menu's requests (FrontendHooks::menu_request). GUI thread.
std::function<void(const FrameXr::MenuRequest&)> g_frame_xr_menu_request;

// AzaharFrame tabmenu: a per-game VR setting as it is in memory (constructing the game's
// QtConfig to save it reads its file back into the settings; this puts the value back).
template <typename T, bool R>
struct FrameKeepSetting {
    explicit FrameKeepSetting(Settings::SwitchableSetting<T, R>& s)
        : setting{s}, global{s.UsingGlobal()}, value{s.GetValue()} {}
    void Restore() {
        setting.SetGlobal(global);
        if (!global) {
            setting.SetValue(value);
        }
    }
    Settings::SwitchableSetting<T, R>& setting;
    bool global;
    T value;
};
// Fast forward (View, then the Right Trigger): Azahar's turbo speed on or off, with the
// "Fast Forward" label in the headset; off when the game stops, the in-game menu opens or the
// mode switches. GUI thread.
std::function<void(bool on, const char* why)> g_frame_xr_fast_forward;
bool g_frame_xr_fast_forward_on = false;
// AzaharFrame flat3: a stick click in flat, in a game: the window's own screen layout
// (Settings::frame_flat_layout). GUI thread.
std::function<void(FrameXr::FlatScreens what)> g_frame_xr_flat_screens;
// ShowFullscreen made the game's window Fullscreen and HideFullscreen did not undo it yet. Qt can
// lose that state while the window manager keeps it (Fullscreen before the window was mapped).
bool g_frame_fullscreen = false;
// AzaharFrame flat4: the Flat menu left Fullscreen when it opened; it goes back when it closes.
bool g_frame_flat_menu_fullscreen = false;
// The layout a left stick click in flat goes back to from Single Screen.
Settings::LayoutOption g_frame_flat_before_single = Settings::LayoutOption::LargeScreen;

const char* FrameLayoutName(Settings::LayoutOption layout) {
    switch (layout) {
    case Settings::LayoutOption::Default:
        return "Default";
    case Settings::LayoutOption::SingleScreen:
        return "Single Screen";
    case Settings::LayoutOption::LargeScreen:
        return "Large Screen";
    case Settings::LayoutOption::HybridScreen:
        return "Hybrid Screen";
    case Settings::LayoutOption::SideScreen:
        return "Side by Side";
    case Settings::LayoutOption::SeparateWindows:
        return "Separate Windows";
    case Settings::LayoutOption::CustomLayout:
        return "Custom Layout";
    }
    return "?";
}

void LogFlatLayout(const char* why) {
    const auto& flat = Settings::frame_flat_layout;
    LOG_INFO(Frontend, "xr: flat layout: {}{} ({})", FrameLayoutName(flat.layout),
             flat.swap ? ", screens swapped" : "", why);
}

// AzaharFrame layout: the Flat layouts the left stick click held cycles through, in order.
constexpr std::array<Settings::LayoutOption, 3> kFrameFlatCycle{
    Settings::LayoutOption::LargeScreen, Settings::LayoutOption::SideScreen,
    Settings::LayoutOption::Default};

Settings::LayoutOption NextFlatLayout(Settings::LayoutOption layout, bool back) {
    const std::size_t n = kFrameFlatCycle.size();
    for (std::size_t i = 0; i < n; i++) {
        if (kFrameFlatCycle[i] == layout) {
            return kFrameFlatCycle[(i + (back ? n - 1 : 1)) % n];
        }
    }
    return kFrameFlatCycle[0]; // another layout (from the View menu): back to the start
}

// AzaharFrame layout: a short label over the game in Flat, where there is no VR toast. A native
// child of the render window, so it stacks over the Vulkan surface (itself a native window).
void ShowFlatLabel(QWidget* over, const QString& text) {
    static QPointer<QLabel> label;
    static QPointer<QTimer> timer;
    if (!over) {
        return;
    }
    if (!label || label->parentWidget() != over) {
        delete label;
        label = new QLabel(over);
        label->setAttribute(Qt::WA_NativeWindow);
        label->setAttribute(Qt::WA_TransparentForMouseEvents);
        label->setStyleSheet(QStringLiteral(
            "QLabel { background-color: #202124; color: #e8eaed; padding: 10px 20px; "
            "border-radius: 0px; font-size: 22px; }"));
        timer = new QTimer(label);
        timer->setSingleShot(true);
        QObject::connect(timer, &QTimer::timeout, label, &QLabel::hide);
    }
    label->setText(text);
    label->adjustSize();
    label->move((over->width() - label->width()) / 2, over->height() / 12);
    label->show();
    label->raise();
    timer->start(1500);
}

void FrameXrPushGameDirs() {
    std::vector<FrameXr::LibraryDir> dirs;
    const std::string title_root = FileUtil::GetUserPath(FileUtil::UserPath::SDMCDir) +
                                   "Nintendo 3DS/00000000000000000000000000000000/"
                                   "00000000000000000000000000000000/title/";
    for (const auto& d : UISettings::values.game_dirs) {
        if (d.path == QStringLiteral("SYSTEM")) {
            continue;
        }
        if (d.path == QStringLiteral("INSTALLED")) {
            // As the game list: installed games and demos, two levels deep.
            dirs.push_back({title_root + "00040000", false, 2});
            dirs.push_back({title_root + "00040002", false, 2});
            continue;
        }
        dirs.push_back({d.path.toStdString(), d.deep_scan});
    }
    FrameXr::SetLibraryDirs(dirs);
}

// AzaharFrame vrvis: the controls profile in use, for the controls help panel in VR.
void FrameXrPushControls() {
    if (!FrameXr::IsStarted()) {
        return;
    }
    const auto& profile = Settings::values.current_input_profile;
    FrameXr::SetControlBindings(
        std::vector<std::string>(profile.buttons.begin(), profile.buttons.end()),
        std::vector<std::string>(profile.analogs.begin(), profile.analogs.end()), profile.name);
}

Frontend::WindowSystemType FrameXrWindowSystemType() {
    const QString platform = QGuiApplication::platformName();
    if (platform == QStringLiteral("xcb")) {
        return Frontend::WindowSystemType::X11;
    }
    if (platform == QStringLiteral("wayland") || platform == QStringLiteral("wayland-egl")) {
        return Frontend::WindowSystemType::Wayland;
    }
    return Frontend::WindowSystemType::Headless;
}
} // namespace
#endif

constexpr int default_mouse_timeout = 2500;

/**
 * "Callouts" are one-time instructional messages shown to the user. In the config settings, there
 * is a bitfield "callout_flags" options, used to track if a message has already been shown to the
 * user. This is 32-bits - if we have more than 32 callouts, we should retire and recycle old ones.
 */

const int GMainWindow::max_recent_files_item;

// There is a bug in the QT implementation on MSYS2 builds
// that cause corners to appear when the app is switched to
// fullscreen. The following code aims to fix that issue
// until it is addressed upstream. It works by manually
// disabling corners through the DWM API.
// TODO(PabloMK7): Remove once the upstream bug is solved.
#if defined(_WIN32) && !defined(_MSC_VER)
#define NEEDS_ROUND_CORNERS_FIX
#endif

#ifdef NEEDS_ROUND_CORNERS_FIX
#include <dwmapi.h>
class WindowCornerManager {
public:
    static WindowCornerManager& instance() {
        static WindowCornerManager inst;
        return inst;
    }

    void blockRoundedCorners(QWidget* widget, bool block) {
        HWND hwnd = reinterpret_cast<HWND>(widget->winId());
        DWORD pref;

        if (block) {
            pref = DWMWCP_DEFAULT;
            if (SUCCEEDED(DwmGetWindowAttribute(hwnd, DWMWA_WINDOW_CORNER_PREFERENCE, &pref,
                                                sizeof(pref)))) {
                original_prefs[hwnd] = pref;
            } else {
                original_prefs[hwnd] = DWMWCP_DEFAULT;
            }

            pref = DWMWCP_DONOTROUND;
            DwmSetWindowAttribute(hwnd, DWMWA_WINDOW_CORNER_PREFERENCE, &pref, sizeof(pref));
        } else {
            auto it = original_prefs.find(hwnd);
            if (it == original_prefs.end())
                return;

            pref = it->second;

            DwmSetWindowAttribute(hwnd, DWMWA_WINDOW_CORNER_PREFERENCE, &pref, sizeof(pref));

            original_prefs.erase(it);
        }
    }

private:
    WindowCornerManager() = default;
    ~WindowCornerManager() = default;

    std::unordered_map<HWND, DWORD> original_prefs;
};
#endif

static QString PrettyProductName() {
#ifdef _WIN32
    // After Windows 10 Version 2004, Microsoft decided to switch to a different notation: 20H2
    // With that notation change they changed the registry key used to denote the current version
    QSettings windows_registry(
        QStringLiteral("HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion"),
        QSettings::NativeFormat);
    const QString release_id = windows_registry.value(QStringLiteral("ReleaseId")).toString();
    if (release_id == QStringLiteral("2009")) {
        const u32 current_build = windows_registry.value(QStringLiteral("CurrentBuild")).toUInt();
        const QString display_version =
            windows_registry.value(QStringLiteral("DisplayVersion")).toString();
        const u32 ubr = windows_registry.value(QStringLiteral("UBR")).toUInt();
        const u32 version = current_build >= 22000 ? 11 : 10;
        return QStringLiteral("Windows %1 Version %2 (Build %3.%4)")
            .arg(QString::number(version), display_version, QString::number(current_build),
                 QString::number(ubr));
    }
#endif
    return QSysInfo::prettyProductName();
}

void GMainWindow::ShowCommandOutput(std::string title, std::string message) {
#ifdef _WIN32
    HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);

    BOOL isSTDOutputHandled = h != nullptr && h != INVALID_HANDLE_VALUE;

    if (isSTDOutputHandled) {
        std::cout << message << std::endl;
    } else {
        boost::replace_all(message, " ", "\u00a0"); // Non-breaking space
        boost::replace_all(message, "-", "\u2011"); // Non-breaking hyphen
        QMessageBox::information(this, QString::fromStdString(title),
                                 QString::fromStdString(message));
    }

#else
    std::cout << message << std::endl;
#endif
}

bool IsPrereleaseBuild() {
    return ((strstr(Common::g_build_fullname, "alpha") != NULL) ||
            (strstr(Common::g_build_fullname, "beta") != NULL) ||
            (strstr(Common::g_build_fullname, "rc") != NULL) ||
            (strstr(Common::g_build_fullname, "test") != NULL));
}

#ifdef ENABLE_QT_UPDATE_CHECKER
static bool ShouldCheckForPrereleaseUpdates() {
    const bool update_channel = UISettings::values.update_check_channel.GetValue();
    const bool using_prerelease_channel =
        (update_channel == UISettings::UpdateCheckChannels::PRERELEASE);
    return (IsPrereleaseBuild() || using_prerelease_channel);
}

static int GetMajorVersion(const std::string& version) {
    size_t dot = version.find('.');
    try {
        return std::stoi(version.substr(0, dot));
    } catch (...) {
        return 0;
    }
}
#endif

GMainWindow::GMainWindow(Core::System& system_)
    : ui{std::make_unique<Ui::MainWindow>()}, system{system_}, movie{system.Movie()},
      user_data_migrator{this}, config{std::make_unique<QtConfig>()}, emu_thread{nullptr} {
    Common::Log::Initialize();
    Common::Log::Start();

    Debugger::ToggleConsole();

    QStringList args = QApplication::arguments();
    QString game_path;
    std::optional<bool> fullscreen_override;
    for (int i = 1; i < args.size(); ++i) {
        // Preserves drag/drop functionality
        if (args.size() == 2 && !args[1].startsWith(QChar::fromLatin1('-'))) {
            game_path = args[1];
            break;
        }

        // Dump video
        if (args[i] == QStringLiteral("--dump-video") || args[i] == QStringLiteral("-d")) {
            if (i >= args.size() - 1 || args[i + 1].startsWith(QChar::fromLatin1('-'))) {
                continue;
            }
            video_dumping_path = args[++i];
            video_dumping_on_start = true;
            continue;
        }

        // Launch game in fullscreen mode
        if (args[i] == QStringLiteral("--fullscreen") || args[i] == QStringLiteral("-f")) {
            fullscreen_override = true;
            continue;
        }

        // Enable GDB stub
        if (args[i] == QStringLiteral("--gdbport") || args[i] == QStringLiteral("-g")) {
            if (i >= args.size() - 1 || args[i + 1].startsWith(QChar::fromLatin1('-'))) {
                continue;
            }
            gdbport_from_arg = strtoul(args[++i].toLatin1(), NULL, 0);
            continue;
        }

        if (args[i] == QStringLiteral("--help") || args[i] == QStringLiteral("-h")) {
            ShowCommandOutput("Help", fmt::format(Common::help_string, args[0].toStdString()));
            exit(0);
        }

        if (args[i] == QStringLiteral("--install") || args[i] == QStringLiteral("-i")) {
            if (i >= args.size() - 1 || args[i + 1].startsWith(QChar::fromLatin1('-'))) {
                continue;
            }
            Service::AM::InstallStatus result = Service::AM::InstallCIA(args[++i].toStdString());
            if (result != Service::AM::InstallStatus::Success) {
                std::string failure_reason;

                if (result == Service::AM::InstallStatus::ErrorFailedToOpenFile)
                    failure_reason = "Unable to open file.";

                if (result == Service::AM::InstallStatus::ErrorFileNotFound)
                    failure_reason = "File not found.";

                if (result == Service::AM::InstallStatus::ErrorAborted)
                    failure_reason = "Install was aborted.";

                if (result == Service::AM::InstallStatus::ErrorInvalid)
                    failure_reason = "CIA is invalid.";

                if (result == Service::AM::InstallStatus::ErrorEncrypted)
                    failure_reason = "CIA is encrypted.";

                std::string failure_string = "Failed to install CIA: " + failure_reason;
                ShowCommandOutput("Failure", failure_string);
                exit((int)result +
                     2); // 2 is added here to avoid stepping on the toes of
                         // exit codes 1 and 2 which have pre-established conventional meanings
            }
            ShowCommandOutput("Success", "Installed CIA successfully.");
            exit(0);
        }

        if (args[i] == QStringLiteral("--movie-play") || args[i] == QStringLiteral("-p")) {
            if (i >= args.size() - 1 || args[i + 1].startsWith(QChar::fromLatin1('-'))) {
                continue;
            }
            movie_playback_path = args[++i];
            movie_playback_on_start = true;
            continue;
        }

        if (args[i] == QStringLiteral("--movie-record") || args[i] == QStringLiteral("-r")) {
            if (i >= args.size() - 1 || args[i + 1].startsWith(QChar::fromLatin1('-'))) {
                continue;
            }
            movie_record_path = args[++i];
            movie_record_on_start = true;
            continue;
        }

        if (args[i] == QStringLiteral("--movie-record-author") || args[i] == QStringLiteral("-a")) {
            if (i >= args.size() - 1 || args[i + 1].startsWith(QChar::fromLatin1('-'))) {
                continue;
            }
            movie_record_author = args[++i];
            continue;
        }

        if (args[i] == QStringLiteral("--multiplayer") || args[i] == QStringLiteral("-m")) {
            std::cout << "Warning: The --multiplayer option is not yet implemented for the Qt "
                         "frontend; Ignoring."
                      << std::endl;
            if (i < args.size() - 1 && !args[i + 1].startsWith(QChar::fromLatin1('-'))) {
                i++;
            }
            continue;
        }

        if (args[i] == QStringLiteral("--version") || args[i] == QStringLiteral("-v")) {
            const std::string version_string = std::string("Azahar for Frame ") + Common::g_build_fullname;
            ShowCommandOutput("Version", version_string);
            exit(0);
        }

        // Launch game in windowed mode
        if (args[i] == QStringLiteral("--windowed") || args[i] == QStringLiteral("-w")) {
            fullscreen_override = false;
            continue;
        }

#ifdef ENABLE_FRAME_XR // AzaharFrame
        if (args[i] == QStringLiteral("--vr")) {
            g_frame_xr_requested = true;
            continue;
        }
        if (args[i] == QStringLiteral("--flat")) {
            g_frame_xr_requested = true;
            g_frame_xr_start_flat = true;
            continue;
        }
#endif

        // Launch game at path
        if (i == args.size() - 1 && !args[i].startsWith(QChar::fromLatin1('-'))) {
            game_path = args[i];
            continue;
        }
    }

#ifdef __unix__
    SetGamemodeEnabled(Settings::values.enable_gamemode.GetValue());
#endif

    // register types to use in slots and signals
    qRegisterMetaType<std::size_t>("std::size_t");
    qRegisterMetaType<Service::AM::InstallStatus>("Service::AM::InstallStatus");

    // Register CameraFactory
    qt_cameras = std::make_shared<Camera::QtMultimediaCameraHandlerFactory>();
    Camera::RegisterFactory("image", std::make_unique<Camera::StillImageCameraFactory>());
    Camera::RegisterFactory("qt", std::make_unique<Camera::QtMultimediaCameraFactory>(qt_cameras));

    system.RegisterInfoLEDColorChanged([this]() { emit InfoLEDColorChanged(); });

    LoadTranslation();

    if (Settings::values.pica_debugging) {
        Pica::g_debug_context = Pica::DebugContext::Construct();
    } else {
        Pica::g_debug_context.reset();
    }
    setAcceptDrops(true);
    ui->setupUi(this);
    statusBar()->hide();

    default_theme_paths = QIcon::themeSearchPaths();
    UpdateUITheme();

#ifdef ENABLE_DISCORD_RPC
    SetDiscordEnabled(UISettings::values.enable_discord_presence.GetValue());
    discord_rpc->Update(false);
#endif

    play_time_manager = std::make_unique<PlayTime::PlayTimeManager>();

    Network::Init();

    movie.SetPlaybackCompletionCallback([this] {
        QMetaObject::invokeMethod(this, "OnMoviePlaybackCompleted", Qt::BlockingQueuedConnection);
    });

    InitializeWidgets();
    InitializeDebugWidgets();
    InitializeRecentFileMenuActions();
    InitializeSaveStateMenuActions();
    InitializeHotkeys();

    SetDefaultUIGeometry();
    RestoreUIState();

    ui->action_Dump_Video->setChecked(video_dumping_on_start);
    if (fullscreen_override) {
        ui->action_Fullscreen->setChecked(*fullscreen_override);
    }
    ui->action_Close_Movie->setEnabled(movie_playback_on_start || movie_record_on_start);

    ConnectAppEvents();
    ConnectMenuEvents();
    ConnectWidgetEvents();

    LOG_INFO(Frontend, "Azahar Version: {} | {}-{}", Common::g_build_fullname, Common::g_scm_branch,
             Common::g_scm_desc);
    std::string build_variant = Common::g_build_variant;
    if (build_variant == "") {
        build_variant = "N/A";
    }
    LOG_INFO(Frontend, "Build Variant: {}", build_variant);
#if CITRA_ARCH(x86_64)
    const auto& caps = Common::GetCPUCaps();
    std::string cpu_string = caps.cpu_string;
    if (caps.avx || caps.avx2 || caps.avx512) {
        cpu_string += " | AVX";
        if (caps.avx512) {
            cpu_string += "512";
        } else if (caps.avx2) {
            cpu_string += '2';
        }
        if (caps.fma) {
            cpu_string += " | FMA";
        }
    }
    LOG_INFO(Frontend, "Host CPU: {}", cpu_string);
#endif
    LOG_INFO(Frontend, "Host OS: {}", PrettyProductName().toStdString());
    const auto& mem_info = Common::GetMemInfo();
    using namespace Common::Literals;
    LOG_INFO(Frontend, "Host RAM: {:.2f} GiB", mem_info.total_physical_memory / f64{1_GiB});
    LOG_INFO(Frontend, "Host Swap: {:.2f} GiB", mem_info.total_swap_memory / f64{1_GiB});
    UpdateWindowTitle();

    QIcon azahar_icon = QIcon(QString::fromStdString(":/icons/default/256x256/azahar.png"));
#ifdef ENABLE_FRAME_XR // AzaharFrame: the Azahar for Frame icon on every window
    azahar_icon = FrameBranding::AppIcon(azahar_icon);
    QApplication::setWindowIcon(azahar_icon);
    setWindowIcon(azahar_icon);
#endif
    render_window->setWindowIcon(azahar_icon);
    secondary_window->setWindowIcon(azahar_icon);

    show();

    // If the theme is updated before show() it doesn't apply properly, so do it again here.
    // We still need the earlier call to make sure that the icons are set up correctly in the
    // game list.
    UpdateUITheme();

#ifdef __APPLE__
    if (AppleUtils::IsRunningFromTerminal()) {
        QMessageBox::warning(
            this, tr("Warning"),
            tr("The `azahar` executable is being run directly rather than via the Azahar.app "
               "bundle.\n\n"
               "When run this way, the app may be missing certain functionality such as camera "
               "emulation.\n\n"
               "It is recommended to instead run Azahar using the `open` command, e.g.:\n"
               "`open ./Azahar.app`"));
    }
#endif

#ifdef ENABLE_QT_UPDATE_CHECKER
    if (UISettings::values.check_for_update_on_start) {
        update_future = QtConcurrent::run([]() -> QString {
            const std::optional<std::string> latest_release_tag =
                UpdateChecker::GetLatestRelease(ShouldCheckForPrereleaseUpdates());

            if (latest_release_tag && latest_release_tag.value() != Common::g_build_fullname) {
                const int latest_major_version = GetMajorVersion(latest_release_tag.value());
                const int current_major_version = GetMajorVersion(Common::g_build_fullname);
                if (current_major_version <= latest_major_version) {
                    return QString::fromStdString(latest_release_tag.value());
                }
            }
            return QString{};
        });
        QObject::connect(&update_watcher, &QFutureWatcher<QString>::finished, this,
                         &GMainWindow::OnEmulatorUpdateAvailable);
        update_watcher.setFuture(update_future);
    }
#endif

    mouse_hide_timer.setInterval(default_mouse_timeout);
    connect(&mouse_hide_timer, &QTimer::timeout, this, &GMainWindow::HideMouseCursor);
    connect(ui->menubar, &QMenuBar::hovered, this, &GMainWindow::OnMouseActivity);

#ifdef ENABLE_OPENGL
    gl_renderer = GetOpenGLRenderer();
#endif

#ifdef ENABLE_VULKAN
    physical_devices = GetVulkanPhysicalDevices();
#endif

#ifdef ENABLE_FRAME_XR // AzaharFrame: the XR session starts before any game boots.
    if (g_frame_xr_requested) {
        FrameXr::StartOptions xr_options;
        xr_options.window_type = FrameXrWindowSystemType();
        xr_options.vr = !g_frame_xr_start_flat;
        if (Settings::values.graphics_api.GetValue() != Settings::GraphicsAPI::Vulkan) {
            LOG_ERROR(Frontend, "xr: --vr needs the Vulkan renderer; running flat");
        } else if (FrameXr::Start(xr_options)) {
            // AzaharFrame flat3: flat lays the window out its own way (Large Screen to start
            // with); VR keeps the configured layout, which places the touch screen.
            Settings::frame_flat_layout.active = !FrameXr::VrShowing();
            // A flat start from the launcher picked its SDL profile; with XR the controllers come
            // through frame_xr in VR and in flat, so the other profile, chosen by name, never
            // edited (a copy when there is none).
            if (Settings::values.current_input_profile.name == kFlatGamepadProfile) {
                const auto& profiles = Settings::values.input_profiles;
                const auto other = std::find_if(profiles.begin(), profiles.end(), [](const auto& p) {
                    return p.name != kFlatGamepadProfile;
                });
                if (other != profiles.end()) {
                    Settings::LoadProfile(static_cast<int>(other - profiles.begin()));
                } else {
                    Settings::CreateProfile("Frame");
                }
                system.ApplySettings();
                LOG_INFO(Frontend, "xr: input profile {} (not {}): the Frame controllers in both modes",
                         Settings::values.current_input_profile.name, kFlatGamepadProfile);
            }
            // The Frame controllers as the input profile (unless already bound).
            if (!InputCommon::FrameXr::ProfileUsesFrameXr(Settings::values.current_input_profile)) {
                InputCommon::FrameXr::ApplyDefaultProfile(Settings::values.current_input_profile);
                system.ApplySettings();
                LOG_INFO(Frontend, "xr: Frame controller bindings applied to the input profile");
            } else if (InputCommon::FrameXr::MigrateGripBindings(
                           Settings::values.current_input_profile)) {
                // Older builds had L and R on the grips; the grips now only grab the screens.
                system.ApplySettings();
                LOG_INFO(Frontend, "xr: L and R moved from the grips to the bumpers");
            }
            // AzaharFrame: a Frame controller as the 3DS gyroscope and accelerometer (Motion
            // Controls), unless the profile has another motion device.
            if (Settings::values.vr_motion_controls.GetValue() != 0 &&
                InputCommon::FrameXr::UseFrameMotionDevice(
                    Settings::values.current_input_profile)) {
                system.ApplySettings();
                LOG_INFO(Frontend, "xr: the Frame controller is the motion device");
            }
            // Touch needs the bottom screen in the window's layout.
            // No separate windows either: the second window stays hidden in VR.
            if (Settings::values.layout_option.GetValue() == Settings::LayoutOption::SingleScreen ||
                Settings::values.layout_option.GetValue() ==
                    Settings::LayoutOption::SeparateWindows) {
                Settings::values.layout_option.SetValue(Settings::LayoutOption::Default);
            }
            SyncMenuUISettings(); // AzaharFrame flat3: and flat's own layout, when flat
            g_frame_xr_touch_sink = std::make_unique<FrameXrTouchSink>(*render_window);
            InputCommon::FrameXr::SetTouchSink(g_frame_xr_touch_sink.get());
            FrameXr::FrontendHooks hooks;
            hooks.launch_game = [this](const std::string& path) {
                QMetaObject::invokeMethod(
                    this, [this, rom = QString::fromStdString(path)] { BootGame(rom); },
                    Qt::QueuedConnection);
            };
            hooks.stop_game = [this] {
                QMetaObject::invokeMethod(this, [this] { ShutdownGame(); }, Qt::QueuedConnection);
            };
            hooks.toggle_menu = [this] {
                QMetaObject::invokeMethod(
                    this,
                    [this] {
                        if (g_frame_mirror && g_frame_xr_set_menu) {
                            // VR: the in-game menu. Flat: the Flat menu (AzaharFrame flat4).
                            g_frame_xr_set_menu(!g_frame_mirror->MenuOpen());
                        } else if (!FrameXr::VrShowing()) {
                            if (g_frame_xr_fast_forward) {
                                g_frame_xr_fast_forward(false, "paused");
                            }
                            OnPauseContinueGame(); // the picker has no window: pause or continue
                        }
                    },
                    Qt::QueuedConnection);
            };
            hooks.menu_back = [this] { // AzaharFrame flat4: B in the Flat menu
                QMetaObject::invokeMethod(
                    this,
                    [] {
                        if (g_frame_mirror && g_frame_mirror->MenuOpen() && !FrameXr::VrShowing()) {
                            g_frame_mirror->Back();
                        }
                    },
                    Qt::QueuedConnection);
            };
            hooks.toggle_vr = [this] {
                QMetaObject::invokeMethod(
                    this,
                    [] {
                        if (g_frame_xr_switch) {
                            g_frame_xr_switch(!FrameXr::WantsVr());
                        }
                    },
                    Qt::QueuedConnection);
            };
            hooks.menu_request = [this](const FrameXr::MenuRequest& r) { // AzaharFrame tabmenu
                QMetaObject::invokeMethod(
                    this,
                    [r] {
                        if (g_frame_xr_menu_request) {
                            g_frame_xr_menu_request(r);
                        }
                    },
                    Qt::QueuedConnection);
            };
            hooks.quick_save = [this] { // AzaharFrame vrqol: View, then the Right Bumper
                QMetaObject::invokeMethod(
                    this,
                    [] {
                        if (g_frame_xr_quick_save) {
                            g_frame_xr_quick_save();
                        }
                    },
                    Qt::QueuedConnection);
            };
            hooks.quick_load = [this] { // AzaharFrame vrqol: View, then the Left Bumper
                QMetaObject::invokeMethod(
                    this,
                    [] {
                        if (g_frame_xr_quick_load) {
                            g_frame_xr_quick_load();
                        }
                    },
                    Qt::QueuedConnection);
            };
            hooks.toggle_fast_forward = [this] { // View, then the Right Trigger
                QMetaObject::invokeMethod(
                    this,
                    [] {
                        if (g_frame_xr_fast_forward) {
                            g_frame_xr_fast_forward(!g_frame_xr_fast_forward_on, "asked for");
                        }
                    },
                    Qt::QueuedConnection);
            };
            hooks.vr_changed = [this](bool vr) {
                QMetaObject::invokeMethod(
                    this,
                    [vr] {
                        if (g_frame_xr_mode_changed) {
                            g_frame_xr_mode_changed(vr);
                        }
                    },
                    Qt::QueuedConnection);
            };
            // AzaharFrame vrvis: the Stats button next to the window in VR.
            hooks.show_stats_changed = [this](bool on) {
                QMetaObject::invokeMethod(
                    this,
                    [this, on] {
                        Settings::values.vr_show_stats.SetValue(on);
                        config->Save();
                    },
                    Qt::QueuedConnection);
            };
            // AzaharFrame vrsize: grip + D-pad set the Immersive Offset or the Screen Curve;
            // saved once the presses stop for half a second.
            hooks.vr_setting_changed = [this] {
                QMetaObject::invokeMethod(
                    this,
                    [this] {
                        static bool queued = false;
                        if (queued) {
                            return;
                        }
                        queued = true;
                        QTimer::singleShot(500, this, [this] {
                            queued = false;
                            config->Save();
                        });
                    },
                    Qt::QueuedConnection);
            };
            // AzaharFrame widerview: grip + D-pad set a per-game VR setting of the game on view
            // (the XR thread has set it): write that game's own config, half a second after the
            // last press. Constructing the config reads its file into the settings, so the
            // value is kept across that.
            hooks.game_setting_changed = [this] {
                QMetaObject::invokeMethod(
                    this,
                    [this] {
                        static bool queued = false;
                        if (queued) {
                            return;
                        }
                        queued = true;
                        QTimer::singleShot(500, this, [this] {
                            queued = false;
                            // Stopped meanwhile: the settings are the global ones again.
                            if (!emulation_running || g_frame_game_config_name.empty()) {
                                return;
                            }
                            auto& level = Settings::values.vr_immersive_wider_view_level;
                            const bool custom = !level.UsingGlobal();
                            const u32 value = level.GetValue();
                            QtConfig game_config(g_frame_game_config_name,
                                                 QtConfig::ConfigType::PerGameConfig);
                            level.SetGlobal(!custom);
                            if (custom) {
                                level.SetValue(value);
                            }
                            game_config.Save();
                        });
                    },
                    Qt::QueuedConnection);
            };
            hooks.flat_screens = [this](FrameXr::FlatScreens what) { // AzaharFrame flat3
                QMetaObject::invokeMethod(
                    this,
                    [what] {
                        if (g_frame_xr_flat_screens) {
                            g_frame_xr_flat_screens(what);
                        }
                    },
                    Qt::QueuedConnection);
            };
            FrameXr::SetFrontendHooks(std::move(hooks));
            FrameXr::SetFlatInput(FrameMode::ReadGamepad);
            // Both bumpers + View + Menu, in VR or flat: the other mode, kept for the next start.
            g_frame_xr_switch = [this](bool vr) {
                const auto apply = [vr] {
                    if (!FrameXr::SetVr(vr)) {
                        LOG_WARNING(Frontend, "xr: cannot switch to {} (no XR any more)",
                                    vr ? "VR" : "flat");
                        return;
                    }
                    FrameMode::WriteModeFile(vr);
                };
                if (!vr) {
                    apply();
                    return;
                }
                // Flat to VR: the window's frame back from the theater first (Game Mode).
                static bool leaving = false;
                if (leaving) {
                    return;
                }
                leaving = true;
                FrameMode::LeaveFlat(this, [apply] {
                    leaving = false;
                    apply();
                });
            };
            g_frame_xr_fast_forward = [this](bool on, const char* why) {
                if (on && (!emulation_running || !system.IsPoweredOn())) {
                    return;
                }
                if (on == g_frame_xr_fast_forward_on) {
                    return;
                }
                g_frame_xr_fast_forward_on = on;
                SetTurboEnabled(on); // Azahar's turbo speed limit (Settings, turbo_limit)
                FrameXr::SetFastForwardShown(on);
                LOG_INFO(Frontend, "xr: fast forward {} ({})", on ? "on" : "off", why);
            };
            // AzaharFrame flat3: the stick clicks in flat change the window's own layout the way
            // Azahar's Swap Screens and Toggle Screen Layout hotkeys change the configured one.
            g_frame_xr_flat_screens = [this](FrameXr::FlatScreens what) {
                auto& flat = Settings::frame_flat_layout;
                if (!flat.active || !emulation_running) {
                    return;
                }
                switch (what) {
                case FrameXr::FlatScreens::Swap:
                    TriggerSwapScreens(); // OnSwapScreens, which swaps flat's screens in flat
                    return;
                case FrameXr::FlatScreens::ToggleOther:
                    if (flat.layout == Settings::LayoutOption::SingleScreen) {
                        flat.layout = g_frame_flat_before_single;
                    } else {
                        g_frame_flat_before_single = flat.layout;
                        flat.layout = Settings::LayoutOption::SingleScreen;
                    }
                    LogFlatLayout("left stick click");
                    break;
                case FrameXr::FlatScreens::Reset:
                    flat.layout = Settings::LayoutOption::LargeScreen;
                    flat.swap = false;
                    g_frame_flat_before_single = Settings::LayoutOption::LargeScreen;
                    LogFlatLayout("right stick click held");
                    ShowFlatLabel(render_window, tr("Large Screen"));
                    break;
                case FrameXr::FlatScreens::CycleLayout:
                case FrameXr::FlatScreens::PreviousLayout: {
                    // AzaharFrame layout: from Single Screen, the next after the one it hides.
                    const Settings::LayoutOption now = flat.layout;
                    const Settings::LayoutOption from =
                        now == Settings::LayoutOption::SingleScreen ? g_frame_flat_before_single
                                                                    : now;
                    flat.layout =
                        NextFlatLayout(from, what == FrameXr::FlatScreens::PreviousLayout);
                    g_frame_flat_before_single = flat.layout;
                    LogFlatLayout(what == FrameXr::FlatScreens::PreviousLayout ? "previous layout"
                                                                                : "next layout");
                    ShowFlatLabel(render_window, tr(FrameLayoutName(flat.layout)));
                    break;
                }
                }
                SyncMenuUISettings();
                system.ApplySettings();
                UpdateSecondaryWindowVisibility();
            };
            g_frame_xr_mode_changed = [this](bool vr) {
                LOG_INFO(Frontend, "xr: now {}", vr ? "in VR" : "flat");
                if (g_frame_xr_fast_forward) {
                    g_frame_xr_fast_forward(false, "the mode switched");
                }
                // A switch leaves the in-game menu or the Flat menu as every other way out of it
                // does (Menu held, B, Emulation > Continue), which continues the game. Kept
                // paused, the other mode would show nothing new of the game (VR's last clear in
                // the window; in VR no picture at all, the screens get none while paused).
                // AzaharFrame flat4: before the layout and size changes below (the Flat menu may
                // have left Fullscreen, and must not go back to it in VR).
                bool paused_without_menu = false;
                if (g_frame_mirror && g_frame_mirror->MenuOpen() && g_frame_xr_set_menu) {
                    if (vr) {
                        g_frame_flat_menu_fullscreen = false;
                    }
                    g_frame_xr_set_menu(false);
                } else if (emulation_running && system.frame_limiter.IsFrameAdvancing()) {
                    if (g_frame_mirror && g_frame_xr_set_menu) {
                        // Paused with no menu open: it stays paused, and the menu of the new
                        // mode opens below, so that something shows and Continue is at hand.
                        paused_without_menu = true;
                    } else if (!vr) {
                        // The picker: one frame, so the window shows the game again (in VR it
                        // only got a clear once a second). It stays paused.
                        system.frame_limiter.AdvanceFrame();
                        LOG_INFO(Frontend, "xr: flat, paused: one frame for the window");
                    }
                }
                // AzaharFrame flat3: Fullscreen (a game started in flat) is for flat only. In VR
                // the window is the in-game menu, with its menu bar, at the VR size.
                const bool was_fullscreen =
                    vr && emulation_running && (WindowFullscreen() || g_frame_fullscreen);
                if (was_fullscreen) {
                    HideFullscreen();
                    LOG_INFO(Frontend, "xr: VR: the window leaves Fullscreen");
                }
                // Flat's own screen layout in flat, the configured one in VR.
                Settings::frame_flat_layout.active = !vr;
                SyncMenuUISettings();
                UpdateSecondaryWindowVisibility(); // AzaharFrame window
                if (emulation_running && system.IsPoweredOn()) {
                    system.GPU().Renderer().UpdateCurrentFramebufferLayout();
                }
                if (!vr) {
                    LogFlatLayout("flat");
                }
                if (g_frame_mirror) {
                    g_frame_mirror->SetVrShowing(vr);
                }
                if (vr) {
                    if (g_frame_mirror) {
                        if (was_fullscreen) {
                            // Its geometry from before Fullscreen is flat's: the VR size again,
                            // now and once the window manager has answered.
                            g_frame_mirror->RefitWindow();
                            QTimer::singleShot(1000, this, [] {
                                if (g_frame_mirror && FrameXr::VrShowing()) {
                                    g_frame_mirror->RefitWindow();
                                }
                            });
                        }
                        FrameMode::FadeWindow(this, true); // the panel is the window in VR
                    }
                } else {
                    FrameMode::ReturnToFlat(this);
                    if (emulation_running && ui->action_Fullscreen->isChecked()) {
                        if (paused_without_menu) {
                            g_frame_flat_menu_fullscreen = true; // once the Flat menu closes
                        } else {
                            ShowFullscreen(); // as when a game starts in flat
                        }
                    }
                }
                if (paused_without_menu && g_frame_mirror && g_frame_xr_set_menu) {
                    LOG_INFO(Frontend, "xr: {}, the game paused: its menu opens",
                             vr ? "VR" : "flat");
                    g_frame_xr_set_menu(true);
                    if (vr) {
                        // The screens get no picture while paused: one frame once the session
                        // runs, so the game shows behind the menu.
                        QTimer::singleShot(500, this, [this] {
                            if (FrameXr::VrShowing() && emulation_running &&
                                system.frame_limiter.IsFrameAdvancing()) {
                                system.frame_limiter.AdvanceFrame();
                                LOG_INFO(Frontend, "xr: VR, paused: one frame for the screens");
                            }
                        });
                    }
                }
            };
            // AzaharFrame vrqol: Quick Save and Quick Load from the controllers or the in-game
            // menu. The Emulation menu's own actions do the work; this adds the confirmation: a
            // toast in the headset, else (flat, or the in-game menu is open and shows the status
            // bar) a status bar message.
            const auto notify = [this](FrameXr::Toast kind, const QString& text) {
                if (FrameXr::VrShowing() && !(g_frame_mirror && g_frame_mirror->MenuOpen())) {
                    FrameXr::ShowToast(kind);
                } else if (!message_label_used_for_movie) {
                    // Every widget in Azahar's status bar is permanent, so showMessage() has no
                    // room there: the message label (left, stretched) shows it for 3 s instead.
                    message_label->setText(text);
                    QTimer::singleShot(3000, this, [this, text] {
                        if (!message_label_used_for_movie && message_label->text() == text) {
                            message_label->clear();
                        }
                    });
                }
            };
            g_frame_xr_quick_save = [this, notify] {
                static QElapsedTimer last;
                if (!emulation_running || !system.IsPoweredOn() ||
                    (last.isValid() && last.elapsed() < 700)) {
                    return;
                }
                last.start();
                UpdateSaveStates();
                const auto started = static_cast<u64>(QDateTime::currentSecsSinceEpoch());
                ui->action_Quick_Save->trigger();
                // Confirm once the file is there (the emulation thread saves at its next frame).
                auto* timer = new QTimer(this);
                timer->setInterval(150);
                int tries = 0;
                connect(timer, &QTimer::timeout, this, [this, timer, notify, started, tries]() mutable {
                    u64 title_id{};
                    bool done = false;
                    if (system.GetAppLoader().ReadProgramId(title_id) ==
                        Loader::ResultStatus::Success) {
                        for (const auto& state :
                             Core::ListSaveStates(title_id, movie.GetCurrentMovieID())) {
                            done |= state.slot == 0 && state.time + 1 >= started;
                        }
                    }
                    if (done) {
                        notify(FrameXr::Toast::QuickSaved, tr("Quick Saved"));
                    }
                    if (done || ++tries >= 40) { // a failed save shows Azahar's own error
                        timer->deleteLater();
                    }
                });
                timer->start();
            };
            g_frame_xr_quick_load = [this, notify] {
                static QElapsedTimer last;
                if (!emulation_running || !system.IsPoweredOn() ||
                    (last.isValid() && last.elapsed() < 700)) {
                    return;
                }
                last.start();
                UpdateSaveStates();
                if (!actions_load_state[0]->isEnabled()) {
                    notify(FrameXr::Toast::NoQuickSave, tr("No Quick Save"));
                    return;
                }
                // The first-time savestate warning is a dialog: open the in-game menu so that
                // it can be seen and answered in the headset.
                if (UISettings::values.save_state_warning && FrameXr::VrShowing() &&
                    g_frame_mirror && !g_frame_mirror->MenuOpen() && g_frame_xr_set_menu) {
                    g_frame_xr_set_menu(true);
                }
                ui->action_Quick_Load->trigger();
                notify(FrameXr::Toast::QuickLoaded, tr("Quick Loaded"));
            };

            if (FrameMirror::Wanted()) {
                g_frame_mirror = new FrameMirror(this);
                // Menu held 1 s in a game: pause (Emulation > Pause) and show this window over
                // the game; held again, B with nothing open, or Emulation > Continue: resume.
                // AzaharFrame flat4: in flat the same is the Flat menu: the window leaves
                // Fullscreen (its menu bar shows) at flat's 16:9 size, and goes back to Fullscreen
                // when the menu closes.
                g_frame_xr_set_menu = [this](bool open) {
                    if (!g_frame_mirror || g_frame_mirror->MenuOpen() == open) {
                        return;
                    }
                    if (open && (!emulation_running || !emu_thread)) {
                        return;
                    }
                    // Which one is open: the Flat menu (flat) or the in-game menu (VR).
                    static bool flat = false;
                    if (open) {
                        flat = !FrameXr::VrShowing();
                    }
                    if (open && g_frame_xr_fast_forward) {
                        g_frame_xr_fast_forward(false, flat ? "the Flat menu opened"
                                                            : "the in-game menu opened");
                    }
                    if (open && emu_thread->IsRunning() &&
                        !system.frame_limiter.IsFrameAdvancing()) {
                        OnPauseGame();
                    }
                    if (open && !flat) { // AzaharFrame tabmenu: the title and save slots
                        std::vector<FrameXr::MenuSaveSlot> menu_slots; // "slots" is Qt's
                        u64 title_id{};
                        const bool have_id = system.GetAppLoader().ReadProgramId(title_id) ==
                                             Loader::ResultStatus::Success;
                        if (have_id) {
                            for (const auto& state :
                                 Core::ListSaveStates(title_id, movie.GetCurrentMovieID())) {
                                menu_slots.push_back(
                                    {static_cast<int>(state.slot),
                                     QDateTime::fromSecsSinceEpoch(static_cast<qint64>(state.time))
                                         .toString(QStringLiteral("yyyy-MM-dd hh:mm"))
                                         .toStdString()});
                            }
                        }
                        FrameXr::SetInGameMenuInfo(game_title.toStdString(), menu_slots,
                                                   have_id && title_id != 0 &&
                                                       !g_frame_game_config_name.empty());
                        if (!g_frame_mods) { // AzaharFrame texui
                            g_frame_mods = new FrameMods(this);
                        }
                        g_frame_mods->Open(have_id ? title_id : 0, g_frame_runs);
                        if (!g_frame_cheats) { // AzaharFrame cheatstab
                            g_frame_cheats = new FrameCheats(
                                system,
                                [] { return g_frame_mirror && g_frame_mirror->MenuOpen(); }, this);
                        }
                        g_frame_cheats->Open(have_id ? title_id : 0, this->game_path.toStdString());
                    }
                    if (open && flat) {
                        g_frame_flat_menu_fullscreen |= WindowFullscreen() || g_frame_fullscreen;
                        if (WindowFullscreen() || g_frame_fullscreen) {
                            HideFullscreen();
                        }
                        // Flat's size again (the geometry from before Fullscreen may be another
                        // mode's), now and once the window manager has answered.
                        g_frame_mirror->RefitWindow();
                        QTimer::singleShot(1000, this, [] {
                            if (g_frame_mirror && g_frame_mirror->MenuOpen() &&
                                !FrameXr::VrShowing()) {
                                g_frame_mirror->RefitWindow();
                            }
                        });
                        // One frame at the new size, so the window shows the paused game under
                        // the menu bar and not the picture of the Fullscreen window stretched.
                        QTimer::singleShot(300, this, [this] {
                            if (g_frame_mirror && g_frame_mirror->MenuOpen() &&
                                !FrameXr::VrShowing() && emulation_running &&
                                system.frame_limiter.IsFrameAdvancing()) {
                                system.frame_limiter.AdvanceFrame();
                            }
                        });
                        activateWindow();
                    }
                    g_frame_mirror->SetMenuOpen(open);
                    FrameXr::SetInGameMenu(open);
                    if (flat) {
                        LOG_INFO(Frontend, "xr: Flat menu {}",
                                 open ? (g_frame_flat_menu_fullscreen
                                             ? "open, game paused, the window left Fullscreen"
                                             : "open, game paused")
                                      : "closed");
                    } else {
                        LOG_INFO(Frontend, "xr: in-game menu {}",
                                 open ? "open, game paused" : "closed");
                    }
                    if (!open && emulation_running && system.frame_limiter.IsFrameAdvancing()) {
                        OnResumeGame(false);
                    }
                    if (!open) {
                        const bool fullscreen_again = g_frame_flat_menu_fullscreen;
                        g_frame_flat_menu_fullscreen = false;
                        if (flat && fullscreen_again && emulation_running &&
                            !FrameXr::VrShowing()) {
                            ShowFullscreen();
                            LOG_INFO(Frontend, "xr: Flat menu closed: Fullscreen again");
                        }
                    }
                };
                g_frame_mirror->SetCloseMenuHandler([] {
                    if (g_frame_xr_set_menu) {
                        g_frame_xr_set_menu(false);
                    }
                });
                g_frame_xr_menu_request = [this](const FrameXr::MenuRequest& r) {
                    FrameMenuRequest(r);
                };
                if (FrameXr::VrShowing()) {
                    FrameMode::FadeWindow(this, true);
                } else {
                    g_frame_mirror->SetVrShowing(false);
                }
            }
            if (!FrameXr::VrShowing()) {
                FrameMode::StartFlat(this); // the window to SteamVR's theater (Game Mode)
            }
            FrameXrPushGameDirs();
            FrameXrPushControls(); // AzaharFrame vrvis

            std::signal(SIGTERM, FrameXrOnSignal);
            std::signal(SIGINT, FrameXrOnSignal);
            auto* signal_timer = new QTimer(this);
            connect(signal_timer, &QTimer::timeout, this, [this] {
                if (g_frame_xr_terminate) {
                    g_frame_xr_terminate = 0;
                    LOG_INFO(Frontend, "xr: termination signal, stopping");
                    ShutdownGame();
                    close();
                }
            });
            signal_timer->start(100);

            // Test driver: AZAHARFRAME_TEST_VR_TOGGLES="20,35" switches VR / flat at those seconds
            // after start, as the chord does.
            for (const QString& at :
                 qEnvironmentVariable("AZAHARFRAME_TEST_VR_TOGGLES").split(QLatin1Char(','),
                                                                          Qt::SkipEmptyParts)) {
                const int ms = static_cast<int>(at.toDouble() * 1000.0);
                QTimer::singleShot(ms, this, [ms] {
                    LOG_INFO(Frontend, "xr test: VR / flat switch at {} ms", ms);
                    if (g_frame_xr_switch) {
                        g_frame_xr_switch(!FrameXr::WantsVr());
                    }
                });
            }

            // Test driver: AZAHARFRAME_TEST_RESOLUTIONS="20:1,35:4" sets Internal Resolution
            // (the setting's value) at those seconds after start, as the settings would.
            for (const QString& step :
                 qEnvironmentVariable("AZAHARFRAME_TEST_RESOLUTIONS").split(QLatin1Char(','),
                                                                           Qt::SkipEmptyParts)) {
                const auto parts = step.split(QLatin1Char(':'));
                if (parts.size() != 2) {
                    continue;
                }
                const int ms = static_cast<int>(parts[0].toDouble() * 1000.0);
                const u32 factor = parts[1].toUInt();
                QTimer::singleShot(ms, this, [ms, factor] {
                    LOG_INFO(Frontend, "xr test: Internal Resolution {} at {} ms", factor, ms);
                    Settings::values.resolution_factor.SetValue(factor);
                });
            }

            // Test driver: AZAHARFRAME_TEST_SHAPES="20:3,35:0" sets Screen Shape (vr_top_aspect)
            // at those seconds after start, as the settings would.
            for (const QString& step :
                 qEnvironmentVariable("AZAHARFRAME_TEST_SHAPES").split(QLatin1Char(','),
                                                                      Qt::SkipEmptyParts)) {
                const auto parts = step.split(QLatin1Char(':'));
                if (parts.size() != 2) {
                    continue;
                }
                const int ms = static_cast<int>(parts[0].toDouble() * 1000.0);
                const u32 shape = parts[1].toUInt();
                QTimer::singleShot(ms, this, [ms, shape] {
                    LOG_INFO(Frontend, "xr test: Screen Shape {} at {} ms", shape, ms);
                    Settings::values.vr_top_aspect.SetValue(shape);
                });
            }

            // Test driver (headless runs): AZAHARFRAME_TEST_BOOTS="a.3ds|b.cci" boots
            // each ROM in turn after the first, each for AZAHARFRAME_TEST_SECONDS (15).
            const QString boots = qEnvironmentVariable("AZAHARFRAME_TEST_BOOTS");
            if (!boots.isEmpty()) {
                const int seconds = qEnvironmentVariableIntValue("AZAHARFRAME_TEST_SECONDS");
                const int period_ms = (seconds > 0 ? seconds : 15) * 1000;
                int at_ms = game_path.isEmpty() ? 1000 : period_ms;
                for (const QString& rom : boots.split(QLatin1Char('|'), Qt::SkipEmptyParts)) {
                    QTimer::singleShot(at_ms, this, [this] {
                        LOG_INFO(Frontend, "xr test: stopping the game");
                        ShutdownGame();
                    });
                    QTimer::singleShot(at_ms + 2000, this, [this, rom] {
                        LOG_INFO(Frontend, "xr test: booting {}", rom.toStdString());
                        BootGame(rom);
                    });
                    at_ms += 2000 + period_ms;
                }
            }
        }
    }
#endif

    if (!game_path.isEmpty()) {
        BootGame(game_path);
    }
}

GMainWindow::~GMainWindow() {
    // Will get automatically deleted otherwise
    if (!render_window->parent()) {
        delete render_window;
    }
    filter->hotkey_registry = nullptr;

    Pica::g_debug_context.reset();
    Network::Shutdown();
}

void GMainWindow::InitializeWidgets() {
    render_window = new GRenderWindow(this, emu_thread.get(), system, false);
    secondary_window = new GRenderWindow(this, emu_thread.get(), system, true);
    render_window->hide();
    secondary_window->hide();
    secondary_window->setParent(nullptr);

#ifndef ENABLE_FFMPEG
    ui->action_Dump_Video->setVisible(false);
#endif
    game_list = new GameList(*play_time_manager, this);
    ui->horizontalLayout->addWidget(game_list);

    game_list_placeholder = new GameListPlaceholder(this);
    ui->horizontalLayout->addWidget(game_list_placeholder);
    game_list_placeholder->setVisible(false);

    loading_screen = new LoadingScreen(this);
    loading_screen->hide();
    ui->horizontalLayout->addWidget(loading_screen);
    connect(loading_screen, &LoadingScreen::Hidden, this, [&] {
        loading_screen->Clear();
        if (emulation_running) {
            render_window->show();
            render_window->setFocus();
            render_window->activateWindow();
        }
    });

    InputCommon::Init();
    multiplayer_state = new MultiplayerState(system, this, game_list->GetModel(),
                                             ui->action_Leave_Room, ui->action_Show_Room);
    multiplayer_state->setVisible(false);

    UpdateBootHomeMenuState();

    // Create status bar
    message_label = new QLabel();
    // Configured separately for left alignment
    message_label->setFrameStyle(QFrame::NoFrame);
    message_label->setContentsMargins(4, 0, 4, 0);
    message_label->setAlignment(Qt::AlignLeft);
    statusBar()->addPermanentWidget(message_label, 1);

    progress_bar = new QProgressBar();
    progress_bar->hide();
    statusBar()->addPermanentWidget(progress_bar);

    loading_shaders_label = new QLabel();

    artic_traffic_label = new QLabel();
    artic_traffic_label->setToolTip(
        tr("Current Artic traffic speed. Higher values indicate bigger transfer loads."));

    emu_speed_label = new QLabel();
    emu_speed_label->setToolTip(tr("Current emulation speed. Values higher or lower than 100% "
                                   "indicate emulation is running faster or slower than a 3DS."));
    game_fps_label = new QLabel();
    game_fps_label->setToolTip(tr("How many frames per second the app is currently displaying. "
                                  "This will vary from app to app and scene to scene."));
    emu_frametime_label = new QLabel();
    emu_frametime_label->setToolTip(
        tr("Time taken to emulate a 3DS frame, not counting framelimiting or v-sync. For "
           "full-speed emulation this should be at most 16.67 ms."));

    for (auto& label : {loading_shaders_label, artic_traffic_label, emu_speed_label, game_fps_label,
                        emu_frametime_label}) {
        label->setVisible(false);
        label->setFrameStyle(QFrame::NoFrame);
        label->setContentsMargins(4, 0, 4, 0);
        statusBar()->addPermanentWidget(label);
    }

    // Setup Graphics API button
    graphics_api_button = new QPushButton();
    graphics_api_button->setObjectName(QStringLiteral("GraphicsAPIStatusBarButton"));
    graphics_api_button->setFocusPolicy(Qt::NoFocus);
    UpdateAPIIndicator();

    connect(graphics_api_button, &QPushButton::clicked, this, [this] { UpdateAPIIndicator(true); });

    statusBar()->insertPermanentWidget(0, graphics_api_button);

    volume_popup = new QWidget(this);
    volume_popup->setWindowFlags(Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint | Qt::Popup);
    volume_popup->setLayout(new QVBoxLayout());
    volume_popup->setMinimumWidth(200);

    volume_slider = new QSlider(Qt::Horizontal);
    volume_slider->setObjectName(QStringLiteral("volume_slider"));
    volume_slider->setMaximum(100);
    volume_slider->setPageStep(5);
    connect(volume_slider, &QSlider::valueChanged, this, [this](int percentage) {
        Settings::values.audio_muted = false;
        const auto value = static_cast<float>(percentage) / volume_slider->maximum();
        Settings::values.volume.SetValue(value);
        UpdateVolumeUI();
    });
    volume_popup->layout()->addWidget(volume_slider);

    volume_button = new QPushButton();
    volume_button->setObjectName(QStringLiteral("TogglableStatusBarButton"));
    volume_button->setFocusPolicy(Qt::NoFocus);
    volume_button->setCheckable(true);
    UpdateVolumeUI();
    connect(volume_button, &QPushButton::clicked, this, [&] {
        UpdateVolumeUI();
        volume_popup->setVisible(!volume_popup->isVisible());
        QRect rect = volume_button->geometry();
        QPoint bottomLeft = statusBar()->mapToGlobal(rect.topLeft());
        bottomLeft.setY(bottomLeft.y() - volume_popup->geometry().height());
        volume_popup->setGeometry(QRect(bottomLeft, QSize(rect.width(), rect.height())));
    });
    statusBar()->insertPermanentWidget(1, volume_button);

    statusBar()->addPermanentWidget(multiplayer_state->GetStatusText());
    statusBar()->addPermanentWidget(multiplayer_state->GetStatusIcon());

    QFrame* sep = new QFrame(this);
    sep->setFrameShape(QFrame::VLine);
    sep->setFrameShadow(QFrame::Sunken);
    sep->setFixedHeight(16);
    statusBar()->addPermanentWidget(sep);

    notification_led = new LedWidget();
    notification_led->setToolTip(tr("Emulated notification LED"));
    statusBar()->addPermanentWidget(notification_led);
    connect(this, &GMainWindow::InfoLEDColorChanged, this, [this] {
        auto led_color = system.GetInfoLEDColor();
        notification_led->setColor(QColor(led_color.r(), led_color.g(), led_color.b()));
    });

    statusBar()->setVisible(true);

    // Removes an ugly inner border from the status bar widgets under Linux
    setStyleSheet(QStringLiteral("QStatusBar::item{border: none;}"));

    QActionGroup* actionGroup_ScreenLayouts = new QActionGroup(this);
    actionGroup_ScreenLayouts->addAction(ui->action_Screen_Layout_Default);
    actionGroup_ScreenLayouts->addAction(ui->action_Screen_Layout_Single_Screen);
    actionGroup_ScreenLayouts->addAction(ui->action_Screen_Layout_Large_Screen);
    actionGroup_ScreenLayouts->addAction(ui->action_Screen_Layout_Side_by_Side);
    actionGroup_ScreenLayouts->addAction(ui->action_Screen_Layout_Separate_Windows);
    actionGroup_ScreenLayouts->addAction(ui->action_Screen_Layout_Hybrid_Screen);
    actionGroup_ScreenLayouts->addAction(ui->action_Screen_Layout_Custom_Layout);

    QActionGroup* actionGroup_SmallPositions = new QActionGroup(this);
    actionGroup_SmallPositions->addAction(ui->action_Small_Screen_TopRight);
    actionGroup_SmallPositions->addAction(ui->action_Small_Screen_MiddleRight);
    actionGroup_SmallPositions->addAction(ui->action_Small_Screen_BottomRight);
    actionGroup_SmallPositions->addAction(ui->action_Small_Screen_TopLeft);
    actionGroup_SmallPositions->addAction(ui->action_Small_Screen_MiddleLeft);
    actionGroup_SmallPositions->addAction(ui->action_Small_Screen_BottomLeft);
    actionGroup_SmallPositions->addAction(ui->action_Small_Screen_Above);
    actionGroup_SmallPositions->addAction(ui->action_Small_Screen_Below);
}

void GMainWindow::InitializeDebugWidgets() {
    if (Pica::g_debug_context) {
        connect(ui->action_Create_Pica_Surface_Viewer, &QAction::triggered, this,
                &GMainWindow::OnCreateGraphicsSurfaceViewer);
    } else {
        ui->action_Create_Pica_Surface_Viewer->setEnabled(false);
    }

    QMenu* debug_menu = ui->menu_View_Debugging;

#if MICROPROFILE_ENABLED
    microProfileDialog = new MicroProfileDialog(this);
    microProfileDialog->hide();
    debug_menu->addAction(microProfileDialog->toggleViewAction());
#endif

    registersWidget = new RegistersWidget(system, this);
    addDockWidget(Qt::RightDockWidgetArea, registersWidget);
    registersWidget->hide();
    debug_menu->addAction(registersWidget->toggleViewAction());
    connect(this, &GMainWindow::EmulationStarting, registersWidget,
            &RegistersWidget::OnEmulationStarting);
    connect(this, &GMainWindow::EmulationStopping, registersWidget,
            &RegistersWidget::OnEmulationStopping);

    if (Pica::g_debug_context) {
        graphicsWidget = new GPUCommandStreamWidget(system, this);
        addDockWidget(Qt::RightDockWidgetArea, graphicsWidget);
        graphicsWidget->hide();
        debug_menu->addAction(graphicsWidget->toggleViewAction());

        graphicsCommandsWidget = new GPUCommandListWidget(system, this);
        addDockWidget(Qt::RightDockWidgetArea, graphicsCommandsWidget);
        graphicsCommandsWidget->hide();
        debug_menu->addAction(graphicsCommandsWidget->toggleViewAction());

        graphicsBreakpointsWidget = new GraphicsBreakPointsWidget(Pica::g_debug_context, this);
        addDockWidget(Qt::RightDockWidgetArea, graphicsBreakpointsWidget);
        graphicsBreakpointsWidget->hide();
        debug_menu->addAction(graphicsBreakpointsWidget->toggleViewAction());

        graphicsVertexShaderWidget =
            new GraphicsVertexShaderWidget(system, Pica::g_debug_context, this);
        addDockWidget(Qt::RightDockWidgetArea, graphicsVertexShaderWidget);
        graphicsVertexShaderWidget->hide();
        debug_menu->addAction(graphicsVertexShaderWidget->toggleViewAction());

        graphicsTracingWidget = new GraphicsTracingWidget(system, Pica::g_debug_context, this);
        addDockWidget(Qt::RightDockWidgetArea, graphicsTracingWidget);
        graphicsTracingWidget->hide();
        debug_menu->addAction(graphicsTracingWidget->toggleViewAction());
        connect(this, &GMainWindow::EmulationStarting, graphicsTracingWidget,
                &GraphicsTracingWidget::OnEmulationStarting);
        connect(this, &GMainWindow::EmulationStopping, graphicsTracingWidget,
                &GraphicsTracingWidget::OnEmulationStopping);
    }

    waitTreeWidget = new WaitTreeWidget(system, this);
    addDockWidget(Qt::LeftDockWidgetArea, waitTreeWidget);
    waitTreeWidget->hide();
    debug_menu->addAction(waitTreeWidget->toggleViewAction());
    connect(this, &GMainWindow::EmulationStarting, waitTreeWidget,
            &WaitTreeWidget::OnEmulationStarting);
    connect(this, &GMainWindow::EmulationStopping, waitTreeWidget,
            &WaitTreeWidget::OnEmulationStopping);

    lleServiceModulesWidget = new LLEServiceModulesWidget(this);
    addDockWidget(Qt::RightDockWidgetArea, lleServiceModulesWidget);
    lleServiceModulesWidget->hide();
    debug_menu->addAction(lleServiceModulesWidget->toggleViewAction());
    connect(this, &GMainWindow::EmulationStarting,
            [this] { lleServiceModulesWidget->setDisabled(true); });
    connect(this, &GMainWindow::EmulationStopping, waitTreeWidget,
            [this] { lleServiceModulesWidget->setDisabled(false); });

    ipcRecorderWidget = new IPCRecorderWidget(system, this);
    addDockWidget(Qt::RightDockWidgetArea, ipcRecorderWidget);
    ipcRecorderWidget->hide();
    debug_menu->addAction(ipcRecorderWidget->toggleViewAction());
    connect(this, &GMainWindow::EmulationStarting, ipcRecorderWidget,
            &IPCRecorderWidget::OnEmulationStarting);
}

void GMainWindow::InitializeRecentFileMenuActions() {
    for (int i = 0; i < max_recent_files_item; ++i) {
        actions_recent_files[i] = new QAction(this);
        actions_recent_files[i]->setVisible(false);
        connect(actions_recent_files[i], &QAction::triggered, this, &GMainWindow::OnMenuRecentFile);

        ui->menu_recent_files->addAction(actions_recent_files[i]);
    }
    ui->menu_recent_files->addSeparator();
    QAction* action_clear_recent_files = new QAction(this);
    action_clear_recent_files->setText(tr("Clear Recent Files"));
    connect(action_clear_recent_files, &QAction::triggered, this, [this] {
        UISettings::values.recent_files.clear();
        UpdateRecentFiles();
    });
    ui->menu_recent_files->addAction(action_clear_recent_files);

    UpdateRecentFiles();
}

void GMainWindow::InitializeSaveStateMenuActions() {
    for (u32 i = 0; i < Core::SaveStateSlotCount; ++i) {
        actions_load_state[i] = new QAction(this);
        actions_load_state[i]->setData(i);
        connect(actions_load_state[i], &QAction::triggered, this, &GMainWindow::OnLoadState);
        if (i > 0)
            ui->menu_Load_State->addAction(actions_load_state[i]);
        actions_save_state[i] = new QAction(this);
        actions_save_state[i]->setData(i);
        connect(actions_save_state[i], &QAction::triggered, this, &GMainWindow::OnSaveState);
        if (i > 0)
            ui->menu_Save_State->addAction(actions_save_state[i]);
    }

    connect(ui->action_Load_from_Newest_Slot, &QAction::triggered, this, [this] {
        UpdateSaveStates();
        if (newest_slot != 0) {
            actions_load_state[newest_slot]->trigger();
        }
    });
    connect(ui->action_Save_to_Oldest_Slot, &QAction::triggered, this, [this] {
        UpdateSaveStates();
        actions_save_state[oldest_slot]->trigger();
    });

    // Quick save / load uses slot
    connect(ui->action_Quick_Save, &QAction::triggered, this, [this] {
        UpdateSaveStates();
        actions_save_state[0]->trigger();
    });
    connect(ui->action_Quick_Load, &QAction::triggered, this, [this] {
        UpdateSaveStates();
        actions_load_state[0]->trigger();
    });

    connect(ui->menu_Load_State->menuAction(), &QAction::hovered, this,
            &GMainWindow::UpdateSaveStates);
    connect(ui->menu_Save_State->menuAction(), &QAction::hovered, this,
            &GMainWindow::UpdateSaveStates);

    UpdateSaveStates();
}

void GMainWindow::InitializeHotkeys() {
    hotkey_registry.LoadHotkeys();
    hotkey_registry.buttonMonitor.start(16);
    LOG_DEBUG(Frontend, "Initializing hotkeys");
    const QString main_window = QStringLiteral("Main Window");
    const QString fullscreen = QStringLiteral("Fullscreen");

    // QAction Hotkeys
    const auto link_action_shortcut = [&](QAction* action, const QString& action_name,
                                          const bool primary_only = false,
                                          const bool auto_repeat = false) {
        static const QString main_window = QStringLiteral("Main Window");
        auto context = hotkey_registry.GetShortcutContext(main_window, action_name);
        auto shortcut = hotkey_registry.GetKeySequence(main_window, action_name);
        action->setShortcut(shortcut);
        action->setShortcutContext(context);
        action->setAutoRepeat(auto_repeat);
        this->addAction(action);
        // handle the shortcuts that are different per-screen
        if (context == Qt::WidgetShortcut) {
            render_window->addAction(action);
            if (!primary_only) {
                secondary_window->addAction(action);
            }
        }
        hotkey_registry.SetAction(main_window, action_name, action);
    };

    link_action_shortcut(ui->action_Load_File, QStringLiteral("Load File"));
    link_action_shortcut(ui->action_Load_Amiibo, QStringLiteral("Load Amiibo"));
    link_action_shortcut(ui->action_Remove_Amiibo, QStringLiteral("Remove Amiibo"));
    link_action_shortcut(ui->action_Exit, QStringLiteral("Exit Azahar"));
    link_action_shortcut(ui->action_Restart, QStringLiteral("Restart Emulation"));
    link_action_shortcut(ui->action_Pause, QStringLiteral("Continue/Pause Emulation"));
    link_action_shortcut(ui->action_Stop, QStringLiteral("Stop Emulation"));
    link_action_shortcut(ui->action_Configure, QStringLiteral("Configure"));
    link_action_shortcut(ui->action_Configure_Current_Game,
                         QStringLiteral("Configure Current Application"));
    link_action_shortcut(ui->action_Show_Filter_Bar, QStringLiteral("Toggle Filter Bar"));
    link_action_shortcut(ui->action_Show_Status_Bar, QStringLiteral("Toggle Status Bar"));
    link_action_shortcut(ui->action_Fullscreen, fullscreen);
    link_action_shortcut(ui->action_Capture_Screenshot, QStringLiteral("Capture Screenshot"));
    link_action_shortcut(ui->action_Debug_Pause, QStringLiteral("Debug Pause"));
    link_action_shortcut(ui->action_Debug_Resume, QStringLiteral("Debug Resume"));
    link_action_shortcut(ui->action_Debug_Step, QStringLiteral("Debug Step"), false, true);
    link_action_shortcut(ui->action_Debug_Unschedule_All, QStringLiteral("Debug Unschedule All"));
    link_action_shortcut(ui->action_Debug_Schedule_All, QStringLiteral("Debug Schedule All"));
    link_action_shortcut(ui->action_Screen_Layout_Swap_Screens, QStringLiteral("Swap Screens"));
    link_action_shortcut(ui->action_Screen_Layout_Upright_Screens,
                         QStringLiteral("Rotate Screens Upright"));
    link_action_shortcut(ui->action_Advance_Frame, QStringLiteral("Advance Frame"));
    link_action_shortcut(ui->action_Load_from_Newest_Slot,
                         QStringLiteral("Load from Newest Non-Quicksave Slot"));
    link_action_shortcut(ui->action_Save_to_Oldest_Slot,
                         QStringLiteral("Save to Oldest Non-Quicksave Slot"));
    link_action_shortcut(ui->action_Quick_Save, QStringLiteral("Quick Save"));
    link_action_shortcut(ui->action_Quick_Load, QStringLiteral("Quick Load"));
    link_action_shortcut(ui->action_View_Lobby, QStringLiteral("Multiplayer Browse Public Rooms"));
    link_action_shortcut(ui->action_Start_Room, QStringLiteral("Multiplayer Create Room"));
    link_action_shortcut(ui->action_Connect_To_Room,
                         QStringLiteral("Multiplayer Direct Connect to Room"));
    link_action_shortcut(ui->action_Show_Room, QStringLiteral("Multiplayer Show Current Room"));
    link_action_shortcut(ui->action_Leave_Room, QStringLiteral("Multiplayer Leave Room"));

    // QShortcut Hotkeys
    const auto connect_shortcut = [&](const QString& action_name, const auto& function) {
        const auto* hotkey = hotkey_registry.GetHotkey(main_window, action_name, this);
        connect(hotkey, &QShortcut::activated, this, function);
    };

    connect_shortcut(QStringLiteral("Toggle Screen Layout"), &GMainWindow::ToggleScreenLayout);
    connect_shortcut(QStringLiteral("Exit Fullscreen"), [&] {
        if (emulation_running) {
            if (secondary_window->isActiveWindow()) {
                secondary_window->showNormal();
            } else {
                ui->action_Fullscreen->setChecked(false);
                ToggleFullscreen();
            }
        }
    });

    connect_shortcut(QStringLiteral("Toggle Per-Application Speed"), [&] {
        if (!hotkey_registry
                 .GetKeySequence(QStringLiteral("Main Window"), QStringLiteral("Toggle Turbo Mode"))
                 .isEmpty()) {
            return;
        }
        Settings::values.frame_limit.SetGlobal(!Settings::values.frame_limit.UsingGlobal());
        UpdateStatusBar();
    });
    connect_shortcut(QStringLiteral("Toggle Texture Dumping"),
                     [&] { Settings::values.dump_textures = !Settings::values.dump_textures; });
    connect_shortcut(QStringLiteral("Toggle Custom Textures"),
                     [&] { Settings::values.custom_textures = !Settings::values.custom_textures; });

    connect_shortcut(QStringLiteral("Toggle Turbo Mode"),
                     [&] { GMainWindow::SetTurboEnabled(!GMainWindow::IsTurboEnabled()); });

    connect_shortcut(QStringLiteral("Enable Turbo"), [&] { GMainWindow::SetTurboEnabled(true); });

    connect_shortcut(QStringLiteral("Disable Turbo"), [&] { GMainWindow::SetTurboEnabled(false); });

    connect_shortcut(QStringLiteral("Increase Speed Limit"), [&] { AdjustSpeedLimit(true); });
    connect_shortcut(QStringLiteral("Decrease Speed Limit"), [&] { AdjustSpeedLimit(false); });

    connect_shortcut(QStringLiteral("Audio Mute/Unmute"), &GMainWindow::OnMute);
    connect_shortcut(QStringLiteral("Audio Volume Down"), &GMainWindow::OnDecreaseVolume);
    connect_shortcut(QStringLiteral("Audio Volume Up"), &GMainWindow::OnIncreaseVolume);

    // We use "static" here in order to avoid capturing by lambda due to a MSVC bug, which makes the
    // variable hold a garbage value after this function exits
    static constexpr u16 FACTOR_3D_STEP = 5;
    connect_shortcut(QStringLiteral("Decrease 3D Factor"), [this] {
        const auto factor_3d = Settings::values.factor_3d.GetValue();
        if (factor_3d > 0) {
            if (factor_3d % FACTOR_3D_STEP != 0) {
                Settings::values.factor_3d = factor_3d - (factor_3d % FACTOR_3D_STEP);
            } else {
                Settings::values.factor_3d = factor_3d - FACTOR_3D_STEP;
            }
            UpdateStatusBar();
        }
    });
    connect_shortcut(QStringLiteral("Increase 3D Factor"), [this] {
        const auto factor_3d = Settings::values.factor_3d.GetValue();
        if (factor_3d < 255) {
            if (factor_3d % FACTOR_3D_STEP != 0) {
                Settings::values.factor_3d =
                    factor_3d + FACTOR_3D_STEP - (factor_3d % FACTOR_3D_STEP);
            } else {
                Settings::values.factor_3d = factor_3d + FACTOR_3D_STEP;
            }
            UpdateStatusBar();
        }
    });

    // handle the Turbo Hold hotkey (use the same pattern for others!)
    const QString turbo_hold = QStringLiteral("Turbo Mode (Hold)");
    const QKeySequence turbo_seq = hotkey_registry.GetKeySequence(main_window, turbo_hold);
    auto turbo_callback = [this](bool held) {
        if (held && !emulation_running) {
            return;
        }
        SetTurboEnabled(held);
    };
    hotkey_registry.SetHoldCallback(main_window, turbo_hold, turbo_callback);
}

void GMainWindow::SetDefaultUIGeometry() {
    // geometry: 55% of the window contents are in the upper screen half, 45% in the lower half
    const QRect screenRect = screen()->geometry();

    const int w = screenRect.width() * 2 / 3;
    const int h = screenRect.height() / 2;
    const int x = (screenRect.x() + screenRect.width()) / 2 - w / 2;
    const int y = (screenRect.y() + screenRect.height()) / 2 - h * 55 / 100;

    setGeometry(x, y, w, h);
}

void GMainWindow::RestoreUIState() {
    restoreGeometry(UISettings::values.geometry);
    restoreState(UISettings::values.state);
    render_window->restoreGeometry(UISettings::values.renderwindow_geometry);
    secondary_window->restoreGeometry(UISettings::values.secondarywindow_geometry);
#if MICROPROFILE_ENABLED
    microProfileDialog->restoreGeometry(UISettings::values.microprofile_geometry);
    microProfileDialog->setVisible(UISettings::values.microprofile_visible.GetValue());
#endif

    game_list->LoadInterfaceLayout();

    ui->action_Single_Window_Mode->setChecked(UISettings::values.single_window_mode.GetValue());
    ToggleWindowMode();

    ui->action_Fullscreen->setChecked(UISettings::values.fullscreen.GetValue());
    SyncMenuUISettings();

    ui->action_Display_Dock_Widget_Headers->setChecked(
        UISettings::values.display_titlebar.GetValue());
    OnDisplayTitleBars(ui->action_Display_Dock_Widget_Headers->isChecked());

    ui->action_Show_Filter_Bar->setChecked(UISettings::values.show_filter_bar.GetValue());
    game_list->SetFilterVisible(ui->action_Show_Filter_Bar->isChecked());

    ui->action_Show_Status_Bar->setChecked(UISettings::values.show_status_bar.GetValue());
    statusBar()->setVisible(ui->action_Show_Status_Bar->isChecked());
}

void GMainWindow::OnAppFocusStateChanged(Qt::ApplicationState state) {
    if (state != Qt::ApplicationHidden && state != Qt::ApplicationInactive &&
        state != Qt::ApplicationActive) {
        LOG_DEBUG(Frontend, "ApplicationState unusual flag: {} ", state);
    }
    if (!emulation_running) {
        return;
    }
    if (UISettings::values.pause_when_in_background) {
        if (emu_thread->IsRunning() &&
            (state & (Qt::ApplicationHidden | Qt::ApplicationInactive))) {
            auto_paused = true;
            OnPauseGame();
        } else if (!emu_thread->IsRunning() && auto_paused && state == Qt::ApplicationActive) {
            auto_paused = false;
            OnResumeGame(false);
        }
    }
    if (UISettings::values.mute_when_in_background) {
        if (!Settings::values.audio_muted &&
            (state & (Qt::ApplicationHidden | Qt::ApplicationInactive))) {
            Settings::values.audio_muted = true;
            auto_muted = true;
        } else if (auto_muted && state == Qt::ApplicationActive) {
            Settings::values.audio_muted = false;
            auto_muted = false;
        }
        UpdateVolumeUI();
    }
}

// helper method to check if a keyevent matches a key sequence.
bool matchesKeyEvent(const QKeyEvent* event, const QKeySequence& seq) {
    if (seq.isEmpty())
        return false;

    int seqKey = seq[0];
    int eventKey = (event->key() | event->modifiers());
    return eventKey == seqKey;
}

// Returns true if releasing the key in `event` should count as releasing `seq`,
// i.e. the event is the sequence's main key or one of the sequence's modifiers.
static bool releaseBreaksSequence(const QKeyEvent* event, const QKeySequence& seq) {
    if (seq.isEmpty())
        return false;

    const int main_key = seq[0].key();
    const Qt::KeyboardModifiers mods = seq[0].keyboardModifiers();

    switch (event->key()) {
    case Qt::Key_Control:
        return mods.testFlag(Qt::ControlModifier);
    case Qt::Key_Shift:
        return mods.testFlag(Qt::ShiftModifier);
    case Qt::Key_Alt:
        return mods.testFlag(Qt::AltModifier);
    case Qt::Key_Meta:
        return mods.testFlag(Qt::MetaModifier);
    default:
        return event->key() == main_key;
    }
}

bool GApplicationEventFilter::eventFilter(QObject* object, QEvent* event) {
    if (event->type() == QEvent::FileOpen) {
        emit FileOpen(static_cast<QFileOpenEvent*>(event));
        return true;
    }

    if (hotkey_registry != nullptr &&
        (event->type() == QEvent::ShortcutOverride || event->type() == QEvent::KeyPress)) {

        QKeyEvent* key = static_cast<QKeyEvent*>(event);
        for (auto const hk : hotkey_registry->hold_hotkeys) {
            auto& seq = hk->keyseq;
            if (seq.isEmpty() || !hk->hold_callback)
                continue;
            if (matchesKeyEvent(key, seq)) {
                if (hotkeys_currently_held.insert(hk).second) {
                    hk->hold_callback(true);
                }
                return true;
            }
        }
    } else if (event->type() == QEvent::KeyRelease) {
        QKeyEvent* key = static_cast<QKeyEvent*>(event);
        if (key->isAutoRepeat()) {
            return false;
        }
        const auto pressed = hotkeys_currently_held;
        bool handled = false;
        for (auto hk : pressed) {
            auto& seq = hk->keyseq;
            if (releaseBreaksSequence(key, seq)) {
                hotkeys_currently_held.erase(hk);
                if (hk->hold_callback)
                    hk->hold_callback(false);
                handled = true;
            }
        }
        return handled;
    }
    return false;
}

void GMainWindow::ConnectAppEvents() {
    filter->hotkey_registry = &hotkey_registry;
    QGuiApplication::instance()->installEventFilter(filter);

    connect(filter, &GApplicationEventFilter::FileOpen, this, &GMainWindow::OnFileOpen);
}

void GMainWindow::ConnectWidgetEvents() {
    connect(game_list, &GameList::GameChosen, this, &GMainWindow::OnGameListLoadFile);
    connect(game_list, &GameList::OpenDirectory, this, &GMainWindow::OnGameListOpenDirectory);
    connect(game_list, &GameList::OpenFolderRequested, this, &GMainWindow::OnGameListOpenFolder);
    connect(game_list, &GameList::RemovePlayTimeRequested, this,
            &GMainWindow::OnGameListRemovePlayTimeData);
    connect(game_list, &GameList::CreateShortcut, this, &GMainWindow::OnGameListCreateShortcut);
    connect(game_list, &GameList::DumpRomFSRequested, this, &GMainWindow::OnGameListDumpRomFS);
    connect(game_list, &GameList::AddDirectory, this, &GMainWindow::OnGameListAddDirectory);
    connect(game_list_placeholder, &GameListPlaceholder::AddDirectory, this,
            &GMainWindow::OnGameListAddDirectory);
    connect(game_list, &GameList::ShowList, this, &GMainWindow::OnGameListShowList);
    connect(game_list, &GameList::PopulatingCompleted, this,
            [this] { multiplayer_state->UpdateGameList(game_list->GetModel()); });
#ifdef ENABLE_FRAME_XR // AzaharFrame: the VR picker follows folder removals and scan changes
    connect(game_list, &GameList::GameDirsChanged, this, [] {
        if (FrameXr::IsStarted()) {
            FrameXrPushGameDirs();
        }
    });
#endif
#ifdef ENABLE_DEVELOPER_OPTIONS
    connect(game_list, &GameList::StartingLaunchStressTest, this,
            &GMainWindow::StartLaunchStressTest);
#endif

    connect(game_list, &GameList::OpenPerGameGeneralRequested, this,
            &GMainWindow::OnGameListOpenPerGameProperties);

    connect(this, &GMainWindow::EmulationStarting, render_window,
            &GRenderWindow::OnEmulationStarting);
    connect(this, &GMainWindow::EmulationStopping, render_window,
            &GRenderWindow::OnEmulationStopping);
    connect(this, &GMainWindow::EmulationStarting, secondary_window,
            &GRenderWindow::OnEmulationStarting);
    connect(this, &GMainWindow::EmulationStopping, secondary_window,
            &GRenderWindow::OnEmulationStopping);

    connect(&status_bar_update_timer, &QTimer::timeout, this, &GMainWindow::UpdateStatusBar);

    connect(this, &GMainWindow::UpdateProgress, this, &GMainWindow::OnUpdateProgress);
    connect(this, &GMainWindow::CIAInstallReport, this, &GMainWindow::OnCIAInstallReport);
    connect(this, &GMainWindow::CIAInstallFinished, this, &GMainWindow::OnCIAInstallFinished);
    connect(this, &GMainWindow::CompressFinished, this, &GMainWindow::OnCompressFinished);
    connect(this, &GMainWindow::BundleFinished, this, &GMainWindow::OnBundleFinished);
    connect(this, &GMainWindow::UpdateThemedIcons, multiplayer_state,
            &MultiplayerState::UpdateThemedIcons);
}

void GMainWindow::ConnectMenuEvents() {
    const auto connect_menu = [&](QAction* action, const auto& event_fn,
                                  QAction::MenuRole role = QAction::NoRole) {
        action->setMenuRole(role);
        connect(action, &QAction::triggered, this, event_fn);
        // Add actions to this window so that hiding menus in fullscreen won't disable them
        addAction(action);
        // Add actions to the render window so that they work outside of single window mode
        render_window->addAction(action);
    };

    // File
    connect_menu(ui->action_Load_File, &GMainWindow::OnMenuLoadFile);
    connect_menu(ui->action_Install_CIA, &GMainWindow::OnMenuInstallCIA);
    connect_menu(ui->action_Connect_Artic, &GMainWindow::OnMenuConnectArticBase);
    connect_menu(ui->action_Setup_System_Files, &GMainWindow::OnMenuSetUpSystemFiles);
    for (u32 region = 0; region < Core::NUM_SYSTEM_TITLE_REGIONS; region++) {
        connect_menu(ui->menu_Boot_Home_Menu->actions().at(region),
                     [this, region] { OnMenuBootHomeMenu(region); });
    }
    connect_menu(ui->action_Exit, &QMainWindow::close, QAction::QuitRole);
    connect_menu(ui->action_Load_Amiibo, &GMainWindow::OnLoadAmiibo);
    connect_menu(ui->action_Remove_Amiibo, &GMainWindow::OnRemoveAmiibo);
    connect_menu(ui->action_Open_Citra_Folder, &GMainWindow::OnOpenCitraFolder);
    connect_menu(ui->action_Open_NAND_Folder, &GMainWindow::OnOpenNANDFolder);
    connect_menu(ui->action_Open_SDMC_Folder, &GMainWindow::OnOpenSDMCFolder);

    // Emulation
    connect_menu(ui->action_Pause, &GMainWindow::OnPauseContinueGame);
    connect_menu(ui->action_Stop, &GMainWindow::OnStopGame);
    connect_menu(ui->action_Restart, [this] { BootGame(QString(game_path)); });
    connect_menu(ui->action_Report_Compatibility, []() {
        QDesktopServices::openUrl(QUrl(QStringLiteral(
            "https://github.com/azahar-emu/compatibility-list/blob/master/CONTRIBUTING.md")));
    });
    connect_menu(ui->action_Configure, &GMainWindow::OnConfigure, QAction::PreferencesRole);
    connect_menu(ui->action_Configure_Current_Game, &GMainWindow::OnConfigurePerGame);

    // View
    connect_menu(ui->action_Single_Window_Mode, &GMainWindow::ToggleWindowMode);
    connect_menu(ui->action_Display_Dock_Widget_Headers, &GMainWindow::OnDisplayTitleBars);
    connect_menu(ui->action_Show_Filter_Bar, &GMainWindow::OnToggleFilterBar);
    connect(ui->action_Show_Status_Bar, &QAction::triggered, statusBar(), &QStatusBar::setVisible);

    // Multiplayer
    connect(ui->action_View_Lobby, &QAction::triggered, multiplayer_state,
            &MultiplayerState::OnViewLobby);
    connect(ui->action_Start_Room, &QAction::triggered, multiplayer_state,
            &MultiplayerState::OnCreateRoom);
    connect(ui->action_Leave_Room, &QAction::triggered, multiplayer_state,
            &MultiplayerState::OnCloseRoom);
    connect(ui->action_Connect_To_Room, &QAction::triggered, multiplayer_state,
            &MultiplayerState::OnDirectConnectToRoom);
    connect(ui->action_Show_Room, &QAction::triggered, multiplayer_state,
            &MultiplayerState::OnOpenNetworkRoom);

    connect_menu(ui->action_Fullscreen, &GMainWindow::ToggleFullscreen);
    connect_menu(ui->action_Screen_Layout_Default, &GMainWindow::ChangeScreenLayout);
    connect_menu(ui->action_Screen_Layout_Single_Screen, &GMainWindow::ChangeScreenLayout);
    connect_menu(ui->action_Screen_Layout_Large_Screen, &GMainWindow::ChangeScreenLayout);
    connect_menu(ui->action_Screen_Layout_Hybrid_Screen, &GMainWindow::ChangeScreenLayout);
    connect_menu(ui->action_Screen_Layout_Side_by_Side, &GMainWindow::ChangeScreenLayout);
    connect_menu(ui->action_Screen_Layout_Separate_Windows, &GMainWindow::ChangeScreenLayout);
    connect_menu(ui->action_Screen_Layout_Custom_Layout, &GMainWindow::ChangeScreenLayout);
    connect_menu(ui->action_Screen_Layout_Swap_Screens, &GMainWindow::OnSwapScreens);
    connect_menu(ui->action_Screen_Layout_Upright_Screens, &GMainWindow::OnRotateScreens);
    connect_menu(ui->action_Small_Screen_TopRight, &GMainWindow::ChangeSmallScreenPosition);
    connect_menu(ui->action_Small_Screen_MiddleRight, &GMainWindow::ChangeSmallScreenPosition);
    connect_menu(ui->action_Small_Screen_BottomRight, &GMainWindow::ChangeSmallScreenPosition);
    connect_menu(ui->action_Small_Screen_TopLeft, &GMainWindow::ChangeSmallScreenPosition);
    connect_menu(ui->action_Small_Screen_MiddleLeft, &GMainWindow::ChangeSmallScreenPosition);
    connect_menu(ui->action_Small_Screen_BottomLeft, &GMainWindow::ChangeSmallScreenPosition);
    connect_menu(ui->action_Small_Screen_Above, &GMainWindow::ChangeSmallScreenPosition);
    connect_menu(ui->action_Small_Screen_Below, &GMainWindow::ChangeSmallScreenPosition);

    // Movie
    connect_menu(ui->action_Record_Movie, &GMainWindow::OnRecordMovie);
    connect_menu(ui->action_Play_Movie, &GMainWindow::OnPlayMovie);
    connect_menu(ui->action_Close_Movie, &GMainWindow::OnCloseMovie);
    connect_menu(ui->action_Save_Movie, &GMainWindow::OnSaveMovie);
    connect_menu(ui->action_Movie_Read_Only_Mode,
                 [this](bool checked) { movie.SetReadOnly(checked); });
    connect_menu(ui->action_Advance_Frame, [this] {
        if (emulation_running && system.frame_limiter.IsFrameAdvancing()) {
            system.frame_limiter.AdvanceFrame();
        }
    });
    connect_menu(ui->action_Capture_Screenshot, &GMainWindow::OnCaptureScreenshot);
#ifdef ENABLE_FFMPEG
    connect_menu(ui->action_Dump_Video, &GMainWindow::OnDumpVideo);
#endif

    // Tools debug
    connect_menu(ui->action_Debug_Pause, [this] {
        if (emu_thread) {
            emu_thread->SetRunning(false);
        }
    });
    connect_menu(ui->action_Debug_Resume, [this] {
        if (emu_thread) {
            emu_thread->SetRunning(true);
        }
    });
    connect_menu(ui->action_Debug_Step, [this] {
        if (emu_thread) {
            emu_thread->ExecStep();
        }
    });
    connect_menu(ui->action_Debug_Unschedule_All,
                 [this] { system.DebugUnscheduleAllThreadsFromFrontend(true); });
    connect_menu(ui->action_Debug_Schedule_All,
                 [this] { system.DebugUnscheduleAllThreadsFromFrontend(false); });

    // Tools
    connect_menu(ui->action_Compress_ROM_File, &GMainWindow::OnCompressFile);
    connect_menu(ui->action_Decompress_ROM_File, &GMainWindow::OnDecompressFile);
    connect_menu(ui->action_Create_ROM_Bundle, &GMainWindow::OnCreateBundle);
    connect_menu(ui->action_Extract_ROM_Bundle, &GMainWindow::OnExtractBundle);

    // Help
    connect_menu(ui->action_Open_Log_Folder, []() {
        QString path = QString::fromStdString(FileUtil::GetUserPath(FileUtil::UserPath::LogDir));
        QDesktopServices::openUrl(QUrl::fromLocalFile(path));
    });
    connect_menu(ui->action_FAQ, []() {
        QDesktopServices::openUrl(QUrl(QStringLiteral("https://azahar-emu.org/pages/faq/")));
    });
    connect_menu(ui->action_About, &GMainWindow::OnMenuAboutCitra, QAction::AboutRole);
}

void GMainWindow::UpdateMenuState() {
    const bool is_paused =
        !emu_thread || !emu_thread->IsRunning() || system.frame_limiter.IsFrameAdvancing();

    const std::array running_actions{
        ui->action_Stop,
        ui->action_Restart,
        ui->action_Configure_Current_Game,
        ui->action_Report_Compatibility,
        ui->action_Load_Amiibo,
        ui->action_Remove_Amiibo,
        ui->action_Pause,
        ui->action_Advance_Frame,
    };

    for (QAction* action : running_actions) {
        action->setEnabled(emulation_running);
    }

    ui->action_Capture_Screenshot->setEnabled(emulation_running);
    ui->action_Advance_Frame->setEnabled(emulation_running && is_paused);

    if (emulation_running && is_paused) {
        ui->action_Pause->setText(tr("Continue"));
    } else {
        ui->action_Pause->setText(tr("Pause"));
    }
}

void GMainWindow::OnDisplayTitleBars(bool show) {
    QList<QDockWidget*> widgets = findChildren<QDockWidget*>();

    if (show) {
        for (QDockWidget* widget : widgets) {
            QWidget* old = widget->titleBarWidget();
            widget->setTitleBarWidget(nullptr);
            if (old) {
                delete old;
            }
        }
    } else {
        for (QDockWidget* widget : widgets) {
            QWidget* old = widget->titleBarWidget();
            widget->setTitleBarWidget(new QWidget());
            if (old) {
                delete old;
            }
        }
    }
}

#if defined(HAVE_SDL2) && defined(__unix__) && !defined(__APPLE__)
static std::optional<QDBusObjectPath> HoldWakeLockLinux(u32 window_id = 0) {
    if (!QDBusConnection::sessionBus().isConnected()) {
        return {};
    }
    // reference: https://flatpak.github.io/xdg-desktop-portal/#gdbus-org.freedesktop.portal.Inhibit
    QDBusInterface xdp(QStringLiteral("org.freedesktop.portal.Desktop"),
                       QStringLiteral("/org/freedesktop/portal/desktop"),
                       QStringLiteral("org.freedesktop.portal.Inhibit"));
    if (!xdp.isValid()) {
        LOG_WARNING(Frontend, "Couldn't connect to XDP D-Bus endpoint");
        return {};
    }
    QVariantMap options = {};
    //: TRANSLATORS: This string is shown to the user to explain why Citra needs to prevent the
    //: computer from sleeping
    options.insert(QString::fromLatin1("reason"),
                   QCoreApplication::translate("GMainWindow", "Azahar is running an application"));
    // 0x4: Suspend lock; 0x8: Idle lock
    QDBusReply<QDBusObjectPath> reply =
        xdp.call(QString::fromLatin1("Inhibit"),
                 QString::fromLatin1("x11:") + QString::number(window_id, 16), 12U, options);

    if (reply.isValid()) {
        return reply.value();
    }
    LOG_WARNING(Frontend, "Couldn't read Inhibit reply from XDP: {}",
                reply.error().message().toStdString());
    return {};
}

static void ReleaseWakeLockLinux(const QDBusObjectPath& lock) {
    if (!QDBusConnection::sessionBus().isConnected()) {
        return;
    }
    QDBusInterface unlocker(QString::fromLatin1("org.freedesktop.portal.Desktop"), lock.path(),
                            QString::fromLatin1("org.freedesktop.portal.Request"));
    unlocker.call(QString::fromLatin1("Close"));
}
#endif // __unix__

void GMainWindow::PreventOSSleep() {
#ifdef _WIN32
    SetThreadExecutionState(ES_CONTINUOUS | ES_SYSTEM_REQUIRED | ES_DISPLAY_REQUIRED);
#elif defined(HAVE_SDL2)
    SDL_DisableScreenSaver();
#if defined(__unix__) && !defined(__APPLE__)
    auto reply = HoldWakeLockLinux(winId());
    if (reply) {
        wake_lock = std::move(reply.value());
    }
#endif // defined(__unix__) && !defined(__APPLE__)
#endif // _WIN32
}

void GMainWindow::AllowOSSleep() {
#ifdef _WIN32
    SetThreadExecutionState(ES_CONTINUOUS);
#elif defined(HAVE_SDL2)
    SDL_EnableScreenSaver();
#if defined(__unix__) && !defined(__APPLE__)
    if (!wake_lock.path().isEmpty()) {
        ReleaseWakeLockLinux(wake_lock);
    }
#endif // defined(__unix__) && !defined(__APPLE__)
#endif // _WIN32
}

bool GMainWindow::LoadROM(const QString& filename) {
    // Shutdown previous session if the emu thread is still active...
    if (emu_thread) {
        ShutdownGame();
    }

    if (!render_window->InitRenderTarget() || !secondary_window->InitRenderTarget()) {
        LOG_CRITICAL(Frontend, "Failed to initialize render targets!");
        return false;
    }

    const auto scope = render_window->Acquire();

    if (!UISettings::values.inserted_cartridge.GetValue().empty()) {
        system.InsertCartridge(UISettings::values.inserted_cartridge.GetValue());
    }

    const Core::System::ResultStatus result{
        system.Load(*render_window, filename.toStdString(), secondary_window)};

    if (result != Core::System::ResultStatus::Success) {
        QString invalid_format = tr("Invalid application format");
        QString invalid_format_description =
            tr("The application file format not supported.<br>Please make sure you are using one "
               "of the compatible file formats:<ul><li>Cartridge images: "
               "<b>.cci/.zcci/.bcci/.3ds</b></li><li>Installable archives: "
               "<b>.cia/.zcia/.bcia</b></li><li>Homebrew titles: <b>.3dsx/.z3dsx</b></li><li>NCCH "
               "containers: <b>.cxi/.zcxi/.bcxi/.app</b></li><li>ELF files: "
               "<b>.elf/.axf</b></li></ul>");

        switch (result) {
        case Core::System::ResultStatus::ErrorGetLoader:
            LOG_CRITICAL(Frontend, "Failed to obtain loader for {}", filename.toStdString());
            QMessageBox::critical(this, invalid_format, invalid_format_description);
            break;

        case Core::System::ResultStatus::ErrorSystemMode:
            LOG_CRITICAL(Frontend, "Failed to load application!");
            QMessageBox::critical(this, invalid_format, invalid_format_description);
            break;

        case Core::System::ResultStatus::ErrorLoader_ErrorEncrypted: {
            QMessageBox::critical(this, tr("Encrypted application"),
                                  tr("Encrypted applications are not supported.<br/>"
                                     "<a "
                                     "href='https://azahar-emu.org/blog/game-loading-changes/'>"
                                     "Please check our blog for more info.</a>"));
            break;
        }
        case Core::System::ResultStatus::ErrorLoader_ErrorInvalidFormat:
            QMessageBox::critical(this, invalid_format, invalid_format_description);
            break;

        case Core::System::ResultStatus::ErrorLoader_ErrorGbaTitle:
            QMessageBox::critical(this, tr("Unsupported application"),
                                  tr("GBA Virtual Console is not supported by Azahar."));
            break;

        case Core::System::ResultStatus::ErrorArticDisconnected:
            QMessageBox::critical(
                this, tr("Artic Server"),
                tr(fmt::format(
                       "An error has occurred whilst communicating with the Artic Server.\n{}",
                       system.GetStatusDetails())
                       .c_str()));
            break;
        case Core::System::ResultStatus::ErrorN3DSApplication:
            QMessageBox::critical(this, tr("Invalid system mode"),
                                  tr("New 3DS exclusive applications cannot be loaded without "
                                     "enabling the New 3DS mode."));
            break;
        case Core::System::ResultStatus::ErrorLoader:
            QMessageBox::critical(this, tr("Generic load error"),
                                  tr("A generic load error occurred while loading the "
                                     "application.<br/>Please check the log for more details."));
            break;
        case Core::System::ResultStatus::ErrorLoader_ErrorPatches:
            QMessageBox::critical(this, tr("Error applying patches"),
                                  tr("A generic error occurred while applying a patch to the "
                                     "application.<br/>Please check the log for more details."));
            break;
        case Core::System::ResultStatus::ErrorLoader_ErrorPatchesInvalidTitle:
            QMessageBox::critical(
                this, tr("Error applying patches"),
                tr("Failed to apply a patch because it is designed for a different "
                   "application.<br/>Please make sure you are using the patches for "
                   "the right application, region and version."));
            break;
        default:
            QMessageBox::critical(
                this, tr("Error while loading application"),
                tr("An unknown error occurred.<br/>Please see the log for more details."));
            break;
        }
        return false;
    }

    std::string title;
    system.GetAppLoader().ReadTitle(title, true);
    game_title = QString::fromStdString(title);
    UpdateWindowTitle();

    u64 title_id;
    system.GetAppLoader().ReadProgramId(title_id);

    game_path = filename;
    game_title_id = title_id;

    return true;
}

void GMainWindow::BootGame(const QString& filename) {
    if (emu_thread) {
        ShutdownGame();
    }

    // NOTE: We could make this not hardcoded, but it's a temp thing anyway so probs not worth. -OS
    if (Settings::values.graphics_api.GetValue() == Settings::GraphicsAPI::Vulkan &&
        Settings::values.texture_filter.GetValue() == Settings::TextureFilter::Anime4K) {
        const auto answer = QMessageBox::warning(
            this, tr("Incompatible texture filter"),
            tr("The selected texture filter (Anime4K) is not compatible with the Vulkan "
               "renderer.\nThe texture filter will be set to None.\nProceed?"),
            QMessageBox::Yes | QMessageBox::No);

        if (answer == QMessageBox::No) {
            return;
        }
        if (answer == QMessageBox::Yes) {
            Settings::values.texture_filter = Settings::TextureFilter::NoFilter;
        }
    }

    const bool is_artic = filename.startsWith(QString::fromStdString("articbase:/")) ||
                          filename.startsWith(QString::fromStdString("articinio:/")) ||
                          filename.startsWith(QString::fromStdString("articinin:/"));

    if (!is_artic && filename.endsWith(QStringLiteral(".cia"))) {
        const auto answer = QMessageBox::question(
            this, tr("CIA must be installed before usage"),
            tr("Before using this CIA, you must install it. Do you want to install it now?"),
            QMessageBox::Yes | QMessageBox::No);

        if (answer == QMessageBox::Yes)
            InstallCIA(QStringList(filename));

        return;
    }

    show_artic_label = is_artic;

    LOG_INFO(Frontend, "Azahar starting...");
    if (!is_artic) {
        StoreRecentFile(filename); // Put the filename on top of the list
    }

    if (movie_record_on_start) {
        movie.PrepareForRecording();
    }
    if (movie_playback_on_start) {
        movie.PrepareForPlayback(movie_playback_path.toStdString());
    }

    const std::string path = filename.toStdString();
    auto loader = Loader::GetLoader(path);

    u64 title_id{0};
    Loader::ResultStatus res =
        loader ? loader->ReadProgramId(title_id) : Loader::ResultStatus::Error;

    if (Loader::ResultStatus::Success == res) {
        // Load per game settings
        const std::string name{is_artic ? "" : FileUtil::GetFilename(filename.toStdString())};
        const std::string config_file_name =
            title_id == 0 ? name : fmt::format("{:016X}", title_id);
        LOG_INFO(Frontend, "Loading per application config file for title {}", config_file_name);
        QtConfig per_game_config(config_file_name, QtConfig::ConfigType::PerGameConfig);
#ifdef ENABLE_FRAME_XR
        g_frame_game_config_name = config_file_name; // AzaharFrame widerview
        g_frame_runs++;                              // AzaharFrame texui
#endif
#ifdef ENABLE_FRAME_XR // AzaharFrame vrqol: known immersive setups, unless the player chose
        if (const FrameXr::ImmersivePreset* preset = FrameXr::FindImmersivePreset(title_id)) {
            auto& mode = Settings::values.vr_immersive_mode;
            auto& offset = Settings::values.vr_si_mode_register_offset;
            // Only where this game's config has no value of its own and the global one is still
            // the default (Off, Auto); a value the player set, even Off, is left alone.
            if (mode.UsingGlobal() && mode.GetValue() == 0) {
                mode.SetGlobal(false);
                mode.SetValue(preset->mode);
                LOG_INFO(Frontend, "xr: {}: Immersive Mode preset {}", preset->name,
                         preset->mode);
            }
            if (preset->register_offset != -1 && mode.GetValue() == preset->mode &&
                offset.UsingGlobal() && offset.GetValue() == -1) {
                offset.SetGlobal(false);
                offset.SetValue(preset->register_offset);
                LOG_INFO(Frontend, "xr: {}: View Matrix Register preset {}", preset->name,
                         preset->register_offset);
            }
            auto& wider = Settings::values.vr_immersive_wider_view_level; // AzaharFrame widerview
            if (preset->wider_view && mode.GetValue() == preset->mode && wider.UsingGlobal() &&
                wider.GetValue() == 0) {
                wider.SetGlobal(false);
                wider.SetValue(4);
                LOG_INFO(Frontend, "xr: {}: Wider View preset", preset->name);
            }
            // AzaharFrame depth: the game's world scale (the eye distance in mode 3).
            auto& depth = Settings::values.vr_immersive_positional_game_scaler;
            if (preset->depth_multiplier != 0 && mode.GetValue() == preset->mode &&
                depth.UsingGlobal() && depth.GetValue() == 0) {
                depth.SetGlobal(false);
                depth.SetValue(static_cast<u32>(preset->depth_multiplier));
                LOG_INFO(Frontend, "xr: {}: Depth Factor Multiplier preset {}", preset->name,
                         preset->depth_multiplier);
            }
            // AzaharFrame depth: the game camera's measured angle, the eye indicator and
            // Narrow Screen, under the same rule (this game's own value or a changed global
            // one wins).
            auto& fov = Settings::values.vr_immersive_game_fov;
            if (preset->game_fov != 0 && mode.GetValue() == preset->mode && fov.UsingGlobal() &&
                fov.GetValue() == fov.GetDefault()) {
                fov.SetGlobal(false);
                fov.SetValue(preset->game_fov);
                LOG_INFO(Frontend, "xr: {}: Immersive Game FOV preset {}", preset->name,
                         preset->game_fov);
            }
            auto& eye = Settings::values.vr_immersive_eye_indicator;
            if (preset->eye_indicator != nullptr && mode.GetValue() == preset->mode &&
                eye.UsingGlobal() && eye.GetValue().empty()) {
                eye.SetGlobal(false);
                eye.SetValue(preset->eye_indicator);
                LOG_INFO(Frontend, "xr: {}: Eye Indicator Register preset {}", preset->name,
                         preset->eye_indicator);
            }
            auto& squeeze = Settings::values.vr_immersive_squeeze;
            if (preset->squeeze != 0 && mode.GetValue() == preset->mode &&
                squeeze.UsingGlobal() && squeeze.GetValue() == 0) {
                squeeze.SetGlobal(false);
                squeeze.SetValue(preset->squeeze);
                LOG_INFO(Frontend, "xr: {}: Narrow Screen preset {}", preset->name,
                         preset->squeeze);
            }
        }
#endif
    }

#ifdef ENABLE_FRAME_XR // AzaharFrame: the immersive screen's placement is saved per title id.
    FrameXr::SetGameTitleId(Loader::ResultStatus::Success == res ? title_id : 0);
#endif

#ifdef ENABLE_FRAME_XR // AzaharFrame: in VR the 3D slider is always on (stereo top screen).
    // AzaharFrame depth3d: a game's own 3D Depth (the in-game menu, Per-Game) is kept, 0 too.
    if (FrameXr::IsStarted() && Settings::values.factor_3d.UsingGlobal() &&
        Settings::values.factor_3d.GetValue() == 0) {
        LOG_INFO(Frontend, "xr: 3D slider was 0, set to 50 for VR");
        Settings::values.factor_3d.SetValue(50);
    }
#endif

    // Artic Server cannot accept a client multiple times, so multiple loaders are not
    // possible. Instead register the app loader early and do not create it again on system load.
    if (loader && !loader->SupportsMultipleInstancesForSameFile()) {
        system.RegisterAppLoaderEarly(loader);
    }

    // Override GDB settings if emulator was launched with
    // GDB port option.
    if (gdbport_from_arg != -1) {
        system.SetGDBPortOverride(gdbport_from_arg);
        system.SetDebugNextProcessFlag();
    }

    system.ApplySettings();

    Settings::LogSettings();

    // Save configurations
    UpdateUISettings();
    game_list->SaveInterfaceLayout();
    config->Save();

    if (!LoadROM(filename)) {
        render_window->ReleaseRenderTarget();
        secondary_window->ReleaseRenderTarget();
        return;
    }

    // Set everything up
    if (movie_record_on_start) {
        movie.StartRecording(movie_record_path.toStdString(), movie_record_author.toStdString());
        movie_record_on_start = false;
        movie_record_path.clear();
        movie_record_author.clear();
    }
    if (movie_playback_on_start) {
        movie.StartPlayback(movie_playback_path.toStdString());
        movie_playback_on_start = false;
        movie_playback_path.clear();
    }

    ui->action_Advance_Frame->setEnabled(false);

#ifdef ENABLE_FFMPEG
    if (video_dumping_on_start) {
        StartVideoDumping(video_dumping_path);
        video_dumping_on_start = false;
        video_dumping_path.clear();
    }
#endif()

    // Register debug widgets
    if (graphicsWidget && graphicsWidget->isVisible()) {
        graphicsWidget->Register();
    }

    // Create and start the emulation thread
    emu_thread = std::make_unique<EmuThread>(system, *render_window);
    emit EmulationStarting(emu_thread.get());
    emu_thread->start();

    connect(render_window, &GRenderWindow::Closed, this, &GMainWindow::OnStopGame);
    connect(render_window, &GRenderWindow::MouseActivity, this, &GMainWindow::OnMouseActivity);
    connect(secondary_window, &GRenderWindow::Closed, this, &GMainWindow::OnStopGame);
    connect(secondary_window, &GRenderWindow::MouseActivity, this, &GMainWindow::OnMouseActivity);

    // BlockingQueuedConnection is important here, it makes sure we've finished refreshing our views
    // before the CPU continues
    connect(emu_thread.get(), &EmuThread::DebugModeEntered, registersWidget,
            &RegistersWidget::OnDebugModeEntered, Qt::BlockingQueuedConnection);
    connect(emu_thread.get(), &EmuThread::DebugModeEntered, waitTreeWidget,
            &WaitTreeWidget::OnDebugModeEntered, Qt::BlockingQueuedConnection);
    connect(emu_thread.get(), &EmuThread::DebugModeLeft, registersWidget,
            &RegistersWidget::OnDebugModeLeft, Qt::BlockingQueuedConnection);
    connect(emu_thread.get(), &EmuThread::DebugModeLeft, waitTreeWidget,
            &WaitTreeWidget::OnDebugModeLeft, Qt::BlockingQueuedConnection);

    connect(emu_thread.get(), &EmuThread::LoadProgress, loading_screen,
            &LoadingScreen::OnLoadProgress, Qt::QueuedConnection);
    connect(emu_thread.get(), &EmuThread::SwitchDiskResources, this,
            &GMainWindow::OnSwitchDiskResources, Qt::QueuedConnection);
    connect(emu_thread.get(), &EmuThread::HideLoadingScreen, loading_screen,
            &LoadingScreen::OnLoadComplete);

    // Update the GUI
    registersWidget->OnDebugModeEntered();
    if (ui->action_Single_Window_Mode->isChecked()) {
        game_list->hide();
        game_list_placeholder->hide();
    }
    status_bar_update_timer.start(1000);

    if (UISettings::values.hide_mouse) {
        mouse_hide_timer.start();
        setMouseTracking(true);
    }

    loading_screen->Prepare(system.GetAppLoader());
    loading_screen->show();

    emulation_running = true;
#ifdef ENABLE_FRAME_XR // AzaharFrame: hides the picker / home panel (and the in-game menu)
    FrameXr::SetInGameMenu(false);
    FrameXr::SetGameRunning(true);
    FrameXrPushControls(); // AzaharFrame vrvis
    if (g_frame_mirror) {
        g_frame_mirror->SetGameRunning(true);
    }
#endif
    if (ui->action_Fullscreen->isChecked()) {
        ShowFullscreen();
    }

    OnResumeGame(true);
}

void GMainWindow::ShutdownGame() {
    if (!emulation_running) {
        return;
    }

    if (ui->action_Fullscreen->isChecked()) {
        HideFullscreen();
    }

#ifdef ENABLE_FFMPEG
    auto video_dumper = system.GetVideoDumper();
    if (video_dumper && video_dumper->IsDumping()) {
        game_shutdown_delayed = true;
        OnStopVideoDumping();
        return;
    }
#endif

    AllowOSSleep();

#ifdef ENABLE_DISCORD_RPC
    discord_rpc->Pause();
#endif

    emu_thread->RequestStop();
#ifdef ENABLE_FRAME_XR // AzaharFrame kbd: the emulation thread may wait for the VR keyboard
    FrameXr::CancelGameKeyboard();
#endif

    // Release emu threads from any breakpoints
    // This belongs after RequestStop() and before wait() because if emulation stops on a GPU
    // breakpoint after (or before) RequestStop() is called, the emulation would never be able
    // to continue out to the main loop and terminate. Thus wait() would hang forever.
    // TODO(bunnei): This function is not thread safe, but it's being used as if it were
    if (Pica::g_debug_context) {
        Pica::g_debug_context->ClearBreakpoints();
    }

    // Unregister debug widgets
    if (graphicsWidget && graphicsWidget->isVisible()) {
        graphicsWidget->Unregister();
    }

    // Frame advancing must be cancelled in order to release the emu thread from waiting
    system.frame_limiter.SetFrameAdvancing(false);

    emit EmulationStopping();

    // Wait for emulation thread to complete and delete it
    emu_thread->wait();
    emu_thread = nullptr;

    system.EjectCartridge();

    OnCloseMovie();

#ifdef ENABLE_DISCORD_RPC
    discord_rpc->Update(false);
#endif
#ifdef __unix__
    Common::Linux::StopGamemode();
#endif

    // The emulation is stopped, so closing the window or not does not matter anymore
    disconnect(render_window, &GRenderWindow::Closed, this, &GMainWindow::OnStopGame);
    disconnect(secondary_window, &GRenderWindow::Closed, this, &GMainWindow::OnStopGame);

    render_window->hide();
#ifdef ENABLE_FRAME_XR
    // AzaharFrame window: the bottom screen's window goes see-through, never unmapped.
    FrameBottomWindow::Update(secondary_window, false, false);
    if (!FrameBottomWindow::KeepMapped())
#endif
        secondary_window->hide();
    loading_screen->hide();
    loading_screen->Clear();

    if (game_list->IsEmpty()) {
        game_list_placeholder->show();
    } else {
        game_list->show();
    }
    game_list->SetFilterFocus();

    setMouseTracking(false);

    // Disable status bar updates
    status_bar_update_timer.stop();
    message_label_used_for_movie = false;
    show_artic_label = false;
    loading_shaders_label->setVisible(false);
    artic_traffic_label->setVisible(false);
    emu_speed_label->setVisible(false);
    game_fps_label->setVisible(false);
    emu_frametime_label->setVisible(false);
    notification_led->setColor(QColor(0, 0, 0));

    UpdateSaveStates();

    emulation_running = false;
#ifdef ENABLE_FRAME_XR // AzaharFrame: back to the picker or home panel (folders may have changed)
    if (FrameXr::IsStarted()) {
        if (g_frame_xr_fast_forward) {
            g_frame_xr_fast_forward(false, "the game stopped");
        }
        FrameXr::SetInGameMenu(false);
        FrameXr::SetGameRunning(false);
        g_frame_flat_menu_fullscreen = false; // AzaharFrame flat4
        FrameXrPushGameDirs();
    }
    if (g_frame_mirror) {
        g_frame_mirror->SetGameRunning(false);
    }
#endif

    game_title.clear();
    UpdateWindowTitle();

    game_path.clear();
    game_title_id = 0;

    // Update the GUI
    UpdateMenuState();

    // When closing the game, destroy the GLWindow to clear the context after the game is closed
    render_window->ReleaseRenderTarget();
    secondary_window->ReleaseRenderTarget();

    // AzaharFrame: drop the per-application overrides here too, not only in OnStopGame, so the
    // VR picker, the global settings window and the final config->Save() in closeEvent never see
    // them (the VR and window-close paths call ShutdownGame() directly).
    Settings::RestoreGlobalState(false);
}

#ifdef ENABLE_DEVELOPER_OPTIONS
void GMainWindow::StartLaunchStressTest(const QString& game_path) {
    QThreadPool::globalInstance()->start([this, game_path] {
        do {
            ui->action_Stop->trigger();
            emit game_list->GameChosen(game_path);
            QThread::sleep(2);
        } while (emulation_running);
    });
}
#endif

void GMainWindow::StoreRecentFile(const QString& filename) {
    UISettings::values.recent_files.prepend(filename);
    UISettings::values.recent_files.removeDuplicates();
    while (UISettings::values.recent_files.size() > max_recent_files_item) {
        UISettings::values.recent_files.removeLast();
    }

    UpdateRecentFiles();
}

void GMainWindow::UpdateRecentFiles() {
    const int num_recent_files =
        std::min(static_cast<int>(UISettings::values.recent_files.size()), max_recent_files_item);

    for (int i = 0; i < num_recent_files; i++) {
        const QString text = QStringLiteral("&%1. %2").arg(i + 1).arg(
            QFileInfo(UISettings::values.recent_files[i]).fileName());
        actions_recent_files[i]->setText(text);
        actions_recent_files[i]->setData(UISettings::values.recent_files[i]);
        actions_recent_files[i]->setToolTip(UISettings::values.recent_files[i]);
        actions_recent_files[i]->setVisible(true);
    }

    for (int j = num_recent_files; j < max_recent_files_item; ++j) {
        actions_recent_files[j]->setVisible(false);
    }

    // Enable the recent files menu if the list isn't empty
    ui->menu_recent_files->setEnabled(num_recent_files != 0);
}

void GMainWindow::UpdateSaveStates() {
    if (!system.IsPoweredOn()) {
        ui->menu_Load_State->setEnabled(false);
        ui->menu_Save_State->setEnabled(false);
        return;
    }

    ui->menu_Load_State->setEnabled(true);
    ui->menu_Save_State->setEnabled(true);
    ui->action_Load_from_Newest_Slot->setEnabled(false);

    oldest_slot = newest_slot = 1;
    oldest_slot_time = std::numeric_limits<u64>::max();
    newest_slot_time = 0;

    u64 title_id;
    if (system.GetAppLoader().ReadProgramId(title_id) != Loader::ResultStatus::Success) {
        return;
    }
    auto savestates = Core::ListSaveStates(title_id, movie.GetCurrentMovieID());
    for (u32 i = 0; i < Core::SaveStateSlotCount; ++i) {
        actions_load_state[i]->setEnabled(false);
        if (i == 0) {
            actions_load_state[i]->setText(tr("Quick Load"));
            actions_save_state[i]->setText(tr("Quick Save"));
        } else {
            actions_load_state[i]->setText(tr("Slot %1").arg(i));
            actions_save_state[i]->setText(tr("Slot %1").arg(i));
        }
    }
    for (const auto& savestate : savestates) {
        if (savestate.slot >= Core::SaveStateSlotCount) {
            continue;
        }
        actions_load_state[savestate.slot]->setEnabled(true);
        if (savestate.slot == 0) {
            const auto text = QStringLiteral("%2")
                                  .arg(QDateTime::fromSecsSinceEpoch(savestate.time)
                                           .toString(QStringLiteral("yyyy-MM-dd hh:mm:ss")))
                                  .trimmed();
            ui->action_Quick_Save->setText(tr("Quick Save - %1").arg(text).trimmed());
            ui->action_Quick_Load->setText(tr("Quick Load - %1").arg(text).trimmed());
            continue;
        }
        const auto text = tr("Slot %1 - %2")
                              .arg(savestate.slot)
                              .arg(QDateTime::fromSecsSinceEpoch(savestate.time)
                                       .toString(QStringLiteral("yyyy-MM-dd hh:mm:ss")))
                              .trimmed();

        actions_load_state[savestate.slot]->setText(text);
        actions_save_state[savestate.slot]->setText(text);

        ui->action_Load_from_Newest_Slot->setEnabled(true);
        if (savestate.time > newest_slot_time) {
            newest_slot = savestate.slot;
            newest_slot_time = savestate.time;
        }
        if (savestate.time < oldest_slot_time) {
            oldest_slot = savestate.slot;
            oldest_slot_time = savestate.time;
        }
    }
    // Value as 1 because quicksave slot is not used for this calculation
    for (u32 i = 1; i < Core::SaveStateSlotCount; ++i) {
        if (!actions_load_state[i]->isEnabled()) {
            // Prefer empty slot
            oldest_slot = i;
            oldest_slot_time = 0;
            break;
        }
    }
}

void GMainWindow::OnGameListLoadFile(QString game_path) {
    if (ConfirmChangeGame()) {
        BootGame(game_path);
    }
}

void GMainWindow::OnGameListOpenFolder(u64 data_id, GameListOpenTarget target) {
    std::string path;
    std::string open_target;

    switch (target) {
    case GameListOpenTarget::SAVE_DATA: {
        open_target = "Save Data";
        std::string sdmc_dir = FileUtil::GetUserPath(FileUtil::UserPath::SDMCDir);
        path = FileSys::ArchiveSource_SDSaveData::GetSaveDataPathFor(sdmc_dir, data_id);
        break;
    }
    case GameListOpenTarget::EXT_DATA: {
        open_target = "Extra Data";
        std::string sdmc_dir = FileUtil::GetUserPath(FileUtil::UserPath::SDMCDir);
        path = FileSys::GetExtDataPathFromId(sdmc_dir, data_id);
        break;
    }
    case GameListOpenTarget::APPLICATION: {
        open_target = "Application";
        auto media_type = Service::AM::GetTitleMediaType(data_id);
        path = Service::AM::GetTitlePath(media_type, data_id) + "content/";
        break;
    }
    case GameListOpenTarget::UPDATE_DATA: {
        open_target = "Update Data";
        path = Service::AM::GetTitlePath(Service::FS::MediaType::SDMC, data_id + 0xe00000000) +
               "content/";
        break;
    }
    case GameListOpenTarget::TEXTURE_DUMP: {
        open_target = "Dumped Textures";
        path = fmt::format("{}textures/{:016X}/",
                           FileUtil::GetUserPath(FileUtil::UserPath::DumpDir), data_id);
        break;
    }
    case GameListOpenTarget::TEXTURE_LOAD: {
        open_target = "Custom Textures";
        path = fmt::format("{}textures/{:016X}/",
                           FileUtil::GetUserPath(FileUtil::UserPath::LoadDir), data_id);
        break;
    }
    case GameListOpenTarget::MODS: {
        open_target = "Mods";
        path = fmt::format("{}mods/{:016X}/", FileUtil::GetUserPath(FileUtil::UserPath::LoadDir),
                           data_id);
        break;
    }
    case GameListOpenTarget::DLC_DATA: {
        open_target = "DLC Data";
        path = fmt::format("{}Nintendo 3DS/00000000000000000000000000000000/"
                           "00000000000000000000000000000000/title/0004008c/{:08x}/content/",
                           FileUtil::GetUserPath(FileUtil::UserPath::SDMCDir), data_id);
        break;
    }
    case GameListOpenTarget::SHADER_CACHE: {
        open_target = "Shader Cache";
        path = FileUtil::GetUserPath(FileUtil::UserPath::ShaderDir);
        break;
    }
    default:
        LOG_ERROR(Frontend, "Unexpected target {}", static_cast<int>(target));
        return;
    }

    QString qpath = QString::fromStdString(path);

    QDir dir(qpath);
    if (!dir.exists()) {
        QMessageBox::critical(
            this, tr("Error Opening %1 Folder").arg(QString::fromStdString(open_target)),
            tr("Folder does not exist!"));
        return;
    }

    LOG_INFO(Frontend, "Opening {} path for data_id={:016x}", open_target, data_id);

    QDesktopServices::openUrl(QUrl::fromLocalFile(qpath));
}

void GMainWindow::OnGameListRemovePlayTimeData(u64 program_id) {
    if (QMessageBox::question(this, tr("Remove Play Time Data"), tr("Reset play time?"),
                              QMessageBox::Yes | QMessageBox::No,
                              QMessageBox::No) != QMessageBox::Yes) {
        return;
    }

    play_time_manager->ResetProgramPlayTime(program_id);
    game_list->PopulateAsync(UISettings::values.game_dirs);
}

bool GMainWindow::CreateShortcutLink(const std::filesystem::path& shortcut_path,
                                     const std::string& comment,
                                     const std::filesystem::path& icon_path,
                                     const std::string& command, const std::string& arguments,
                                     const std::string& categories, const std::string& keywords,
                                     const std::string& name, const bool& skip_tryexec) try {
#if defined(__linux__) || defined(__FreeBSD__) // Linux and FreeBSD
    std::filesystem::path shortcut_path_full = shortcut_path / PathFromUTF8(name + ".desktop");
    std::ofstream shortcut_stream(shortcut_path_full, std::ios::binary | std::ios::trunc);
    if (!shortcut_stream.is_open()) {
        LOG_ERROR(Frontend, "Failed to create shortcut");
        return false;
    }
    // TODO: Migrate fmt::print to std::print in futures STD C++ 23.
    fmt::print(shortcut_stream, "[Desktop Entry]\n");
    fmt::print(shortcut_stream, "Type=Application\n");
    fmt::print(shortcut_stream, "Version=1.0\n");
    fmt::print(shortcut_stream, "Name={}\n", name);
    if (!comment.empty()) {
        fmt::print(shortcut_stream, "Comment={}\n", comment);
    }
    if (std::filesystem::is_regular_file(icon_path)) {
        fmt::print(shortcut_stream, "Icon={}\n", PathToUTF8(icon_path));
    }
    if (!skip_tryexec) {
        fmt::print(shortcut_stream, "TryExec={}\n", command);
    }
    fmt::print(shortcut_stream, "Exec={} {}\n", command, arguments);
    if (!categories.empty()) {
        fmt::print(shortcut_stream, "Categories={}\n", categories);
    }
    if (!keywords.empty()) {
        fmt::print(shortcut_stream, "Keywords={}\n", keywords);
    }
    return true;
#elif defined(_WIN32) // Windows
    HRESULT hr = CoInitialize(nullptr);
    if (FAILED(hr)) {
        LOG_ERROR(Frontend, "CoInitialize failed");
        return false;
    }
    SCOPE_EXIT({ CoUninitialize(); });
    IShellLinkW* ps1 = nullptr;
    IPersistFile* persist_file = nullptr;
    SCOPE_EXIT({
        if (persist_file != nullptr) {
            persist_file->Release();
        }
        if (ps1 != nullptr) {
            ps1->Release();
        }
    });
    HRESULT hres = CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER, IID_IShellLinkW,
                                    reinterpret_cast<void**>(&ps1));
    if (FAILED(hres)) {
        LOG_ERROR(Frontend, "Failed to create IShellLinkW instance");
        return false;
    }
    hres = ps1->SetPath(Common::UTF8ToUTF16W(command).data());
    if (FAILED(hres)) {
        LOG_ERROR(Frontend, "Failed to set path");
        return false;
    }
    if (!arguments.empty()) {
        hres = ps1->SetArguments(Common::UTF8ToUTF16W(arguments).data());
        if (FAILED(hres)) {
            LOG_ERROR(Frontend, "Failed to set arguments");
            return false;
        }
    }
    if (!comment.empty()) {
        hres = ps1->SetDescription(Common::UTF8ToUTF16W(comment).data());
        if (FAILED(hres)) {
            LOG_ERROR(Frontend, "Failed to set description");
            return false;
        }
    }
    if (std::filesystem::is_regular_file(icon_path)) {
        hres = ps1->SetIconLocation(icon_path.c_str(), 0);
        if (FAILED(hres)) {
            LOG_ERROR(Frontend, "Failed to set icon location");
            return false;
        }
    }
    hres = ps1->QueryInterface(IID_IPersistFile, reinterpret_cast<void**>(&persist_file));
    if (FAILED(hres)) {
        LOG_ERROR(Frontend, "Failed to get IPersistFile interface");
        return false;
    }
    hres = persist_file->Save(
        std::filesystem::path{shortcut_path / (Common::UTF8ToUTF16W(name) + L".lnk")}.c_str(),
        TRUE);
    if (FAILED(hres)) {
        LOG_ERROR(Frontend, "Failed to save shortcut");
        return false;
    }
    return true;
#else                 // Unsupported platform
    return false;
#endif
} catch (const std::exception& e) {
    LOG_ERROR(Frontend, "Failed to create shortcut: {}", e.what());
    return false;
}

// Messages in pre-defined message boxes for less code spaghetti
bool GMainWindow::CreateShortcutMessagesGUI(QWidget* parent, int message,
                                            const QString& game_title) {
    int result = 0;
    QMessageBox::StandardButtons buttons;
    switch (message) {
    case GMainWindow::CREATE_SHORTCUT_MSGBOX_FULLSCREEN_PROMPT:
        buttons = QMessageBox::Yes | QMessageBox::No;
        result = QMessageBox::information(
            parent, tr("Create Shortcut"),
            tr("Do you want to launch the application in fullscreen?"), buttons);
        return result == QMessageBox::Yes;
    case GMainWindow::CREATE_SHORTCUT_MSGBOX_SUCCESS:
        QMessageBox::information(parent, tr("Create Shortcut"),
                                 tr("Successfully created a shortcut to %1").arg(game_title));
        return false;
    case GMainWindow::CREATE_SHORTCUT_MSGBOX_APPIMAGE_VOLATILE_WARNING:
        buttons = QMessageBox::StandardButton::Ok | QMessageBox::StandardButton::Cancel;
        result =
            QMessageBox::warning(this, tr("Create Shortcut"),
                                 tr("This will create a shortcut to the current AppImage. This may "
                                    "not work well if you update. Continue?"),
                                 buttons);
        return result == QMessageBox::Ok;
    default:
        buttons = QMessageBox::Ok;
        QMessageBox::critical(parent, tr("Create Shortcut"),
                              tr("Failed to create a shortcut to %1").arg(game_title), buttons);
        return false;
    }
}

bool GMainWindow::MakeShortcutIcoPath(const u64 program_id, const std::string_view game_file_name,
                                      std::filesystem::path& out_icon_path) {
    // Get path to Citra icons directory & icon extension
    std::string ico_extension = "png";
#if defined(_WIN32)
    out_icon_path = PathFromUTF8(FileUtil::GetUserPath(FileUtil::UserPath::IconsDir));
    ico_extension = "ico";
#elif defined(__linux__) || defined(__FreeBSD__)
    out_icon_path =
        PathFromUTF8(FileUtil::GetUserDirectory("XDG_DATA_HOME") + "/icons/hicolor/256x256/");
#endif
    // Create icons directory if it doesn't exist
    if (!FileUtil::CreateFullPath(PathToUTF8(out_icon_path))) {
        QMessageBox::critical(
            this, tr("Create Icon"),
            tr("Cannot create icon file. Path \"%1\" does not exist and cannot be created.")
                .arg(QString::fromStdString(PathToUTF8(out_icon_path))),
            QMessageBox::StandardButton::Ok);
        out_icon_path.clear();
        return false;
    }

    // Create icon file path
    out_icon_path /=
        PathFromUTF8(program_id == 0 ? fmt::format("citra-{}.{}", game_file_name, ico_extension)
                                     : fmt::format("citra-{:016X}.{}", program_id, ico_extension));
    return true;
}

void GMainWindow::OnGameListCreateShortcut(u64 program_id, const std::string& game_path,
                                           GameListShortcutTarget target) {
    std::string citra_command{};
    bool skip_tryexec = false;
    const char* env_flatpak_id = getenv("FLATPAK_ID");
    if (env_flatpak_id) {
        citra_command = fmt::format("flatpak run {}", env_flatpak_id);
        skip_tryexec = true;
    } else {
        // Get path to Citra executable
        const QStringList args = QApplication::arguments();
        citra_command = args[0].toStdString();
        // If relative path, make it an absolute path
        if (citra_command.c_str()[0] == '.') {
            citra_command = FileUtil::GetCurrentDir().value_or("") + DIR_SEP + citra_command;
        }
    }

    // Shortcut path
    std::filesystem::path shortcut_path{};
    if (target == GameListShortcutTarget::Desktop) {
        shortcut_path = PathFromUTF8(
            QStandardPaths::writableLocation(QStandardPaths::DesktopLocation).toStdString());
    } else if (target == GameListShortcutTarget::Applications) {
        shortcut_path = PathFromUTF8(GetApplicationsDirectory());
    }

    // Icon path and title
    if (!std::filesystem::exists(shortcut_path)) {
        CreateShortcutMessagesGUI(this, CREATE_SHORTCUT_MSGBOX_ERROR, {});
        LOG_ERROR(Frontend, "Invalid shortcut target");
        return;
    }

    // Get title from game file
    const auto loader = Loader::GetLoader(game_path);
    std::string game_title = fmt::format("{:016X}", program_id);
    if (loader->ReadTitle(game_title, false) != Loader::ResultStatus::Success) {
        game_title = fmt::format("{:016x}", program_id);
    }

    // Delete illegal characters from title
    const std::string illegal_chars = "<>:\"/\\|?*.";
    for (auto it = game_title.rbegin(); it != game_title.rend(); ++it) {
        if (illegal_chars.find(*it) != std::string::npos) {
            game_title.erase(it.base() - 1);
        }
    }

    // Get icon from game file
    std::vector<u8> icon_image_file;
    if (loader->ReadIcon(icon_image_file, false) != Loader::ResultStatus::Success) {
        LOG_WARNING(Frontend, "Could not read icon from {:s}", game_path);
    }

    const QPixmap pixmap = GetQPixmapFromSMDH(icon_image_file);
    const QImage icon_data = pixmap.toImage();
    std::filesystem::path out_icon_path;
    if (MakeShortcutIcoPath(program_id, game_title, out_icon_path)) {
        if (!SaveIconToFile(out_icon_path, icon_data)) {
            LOG_ERROR(Frontend, "Could not write icon to file");
        }
    }

    const auto qt_game_title = QString::fromStdString(game_title);
#if defined(__linux__)
    // Special case for AppImages
    // Warn once if we are making a shortcut to a volatile AppImage
    const std::string appimage_ending =
        std::string(Common::g_scm_rev).substr(0, 9).append(".AppImage");
    if (citra_command.ends_with(appimage_ending) && !UISettings::values.shortcut_already_warned) {
        if (CreateShortcutMessagesGUI(this, CREATE_SHORTCUT_MSGBOX_APPIMAGE_VOLATILE_WARNING,
                                      qt_game_title)) {
            return;
        }
        UISettings::values.shortcut_already_warned = true;
    }
#endif // __linux__
    // Create shortcut
    std::string arguments = fmt::format("\"{:s}\"", game_path);
    if (CreateShortcutMessagesGUI(this, CREATE_SHORTCUT_MSGBOX_FULLSCREEN_PROMPT, qt_game_title)) {
        arguments = "-f " + arguments;
    }
    const std::string comment = fmt::format("Start {:s} with the Azahar Emulator", game_title);
    const std::string categories = "Game;Emulator;Qt;";
    const std::string keywords = "3ds;Nintendo;";

    if (CreateShortcutLink(shortcut_path, comment, out_icon_path, citra_command, arguments,
                           categories, keywords, game_title, skip_tryexec)) {
        CreateShortcutMessagesGUI(this, CREATE_SHORTCUT_MSGBOX_SUCCESS, qt_game_title);
        return;
    }
    CreateShortcutMessagesGUI(this, CREATE_SHORTCUT_MSGBOX_ERROR, qt_game_title);
}

void GMainWindow::OnGameListDumpRomFS(QString game_path, u64 program_id) {
    auto* dialog = new QProgressDialog(tr("Dumping..."), tr("Cancel"), 0, 0, this);
    dialog->setWindowModality(Qt::WindowModal);
    dialog->setWindowFlags(dialog->windowFlags() &
                           ~(Qt::WindowCloseButtonHint | Qt::WindowContextHelpButtonHint));
    dialog->setCancelButton(nullptr);
    dialog->setMinimumDuration(0);
    dialog->setValue(0);

    const auto base_path = fmt::format(
        "{}romfs/{:016X}", FileUtil::GetUserPath(FileUtil::UserPath::DumpDir), program_id);
    const auto update_path =
        fmt::format("{}romfs/{:016X}", FileUtil::GetUserPath(FileUtil::UserPath::DumpDir),
                    program_id | 0x0004000e00000000);
    using FutureWatcher = QFutureWatcher<std::pair<Loader::ResultStatus, Loader::ResultStatus>>;
    auto* future_watcher = new FutureWatcher(this);
    connect(future_watcher, &FutureWatcher::finished, this,
            [this, dialog, base_path, update_path, future_watcher] {
                dialog->hide();
                const auto& [base, update] = future_watcher->result();
                if (base != Loader::ResultStatus::Success) {
                    QMessageBox::critical(
                        this, QStringLiteral("Azahar"),
                        tr("Could not dump base RomFS.\nRefer to the log for details."));
                    return;
                }
                QDesktopServices::openUrl(QUrl::fromLocalFile(QString::fromStdString(base_path)));
                if (update == Loader::ResultStatus::Success) {
                    QDesktopServices::openUrl(
                        QUrl::fromLocalFile(QString::fromStdString(update_path)));
                }
            });

    auto future = QtConcurrent::run([game_path, base_path, update_path] {
        std::unique_ptr<Loader::AppLoader> loader = Loader::GetLoader(game_path.toStdString());
        return std::make_pair(loader->DumpRomFS(base_path), loader->DumpUpdateRomFS(update_path));
    });
    future_watcher->setFuture(future);
}

void GMainWindow::OnGameListOpenDirectory(const QString& directory) {
    QString path;
    if (directory == QStringLiteral("INSTALLED")) {
        path = QString::fromStdString(FileUtil::GetUserPath(FileUtil::UserPath::SDMCDir) +
                                      "Nintendo "
                                      "3DS/00000000000000000000000000000000/"
                                      "00000000000000000000000000000000/title/00040000");
    } else if (directory == QStringLiteral("SYSTEM")) {
        path = QString::fromStdString(FileUtil::GetUserPath(FileUtil::UserPath::NANDDir) +
                                      "00000000000000000000000000000000/title/00040010");
    } else {
        path = directory;
    }
    if (!QFileInfo::exists(path)) {
        QMessageBox::critical(this, tr("Error Opening %1").arg(path), tr("Folder does not exist!"));
        return;
    }
    QDesktopServices::openUrl(QUrl::fromLocalFile(path));
}

void GMainWindow::OnGameListAddDirectory() {
    const QString dir_path = QFileDialog::getExistingDirectory(this, tr("Select Directory"));
    if (dir_path.isEmpty())
        return;
    UISettings::GameDir game_dir{dir_path, false, true};
    if (!UISettings::values.game_dirs.contains(game_dir)) {
        UISettings::values.game_dirs.append(game_dir);
        game_list->PopulateAsync(UISettings::values.game_dirs);
#ifdef ENABLE_FRAME_XR // AzaharFrame: the picker in VR lists the new folder at once
        if (FrameXr::IsStarted()) {
            FrameXrPushGameDirs();
        }
#endif
    } else {
        LOG_WARNING(Frontend, "Selected directory is already in the application list");
    }
}

void GMainWindow::OnGameListShowList(bool show) {
    if (emulation_running && ui->action_Single_Window_Mode->isChecked())
        return;
    game_list->setVisible(show);
    game_list_placeholder->setVisible(!show);
};

void GMainWindow::OnGameListOpenPerGameProperties(const QString& file) {
    const auto loader = Loader::GetLoader(file.toStdString());

    u64 title_id{};
    if (!loader || loader->ReadProgramId(title_id) != Loader::ResultStatus::Success) {
        QMessageBox::information(this, tr("Properties"),
                                 tr("The application properties could not be loaded."));
        return;
    }

    OpenPerGameConfiguration(title_id, file);
}

void GMainWindow::OnMenuLoadFile() {
    const QString extensions = QStringLiteral("*.").append(
        GameList::supported_file_extensions.join(QStringLiteral(" *.")));
    const QString file_filter = tr("3DS Executable") + QStringLiteral(" (%1)").arg(extensions) +
                                QStringLiteral(";;") + tr("All Files") + QStringLiteral(" (*.*)");
    const QString filename = QFileDialog::getOpenFileName(
        this, tr("Load File"), UISettings::values.roms_path, file_filter);

    if (filename.isEmpty()) {
        return;
    }

    UISettings::values.roms_path = QFileInfo(filename).path();
    BootGame(filename);
}

void GMainWindow::OnMenuSetUpSystemFiles() {
    QDialog dialog(this);
    dialog.setWindowTitle(tr("Set Up System Files"));

    QVBoxLayout layout(&dialog);

    QLabel label_description(
        tr("<p>Azahar needs console unique data and firmware files from a real console to be "
           "able to use some of its features.<br>Such files and data can be set up with the <a "
           "href=https://github.com/azahar-emu/ArticSetupTool>Azahar "
           "Artic Setup Tool</a><br>Notes:<ul><li><b>This operation will install console unique "
           "data to Azahar, do not share your user or nand folders<br>after performing the setup "
           "process!</b></li><li>While doing the setup process, Azahar will link to the console "
           "running the setup tool. You can unlink the<br>console later from the System tab in the "
           "emulator configuration menu.</li><li>Do not go online with both Azahar and your 3DS "
           "console at the same time after setting up system files,<br>as it could cause "
           "issues.</li><li>Old 3DS setup is needed for the New 3DS setup to work (doing both "
           "setup modes is recommended).</li><li>Both setup modes will work regardless of the "
           "model of the console running the setup tool.</li></ul><hr></p>"),
        &dialog);
    label_description.setOpenExternalLinks(true);
    layout.addWidget(&label_description);

    QHBoxLayout layout_h(&dialog);
    layout.addLayout(&layout_h);

    QLabel label_enter(tr("Enter Azahar Artic Setup Tool address:"), &dialog);

    layout_h.addWidget(&label_enter);

    QLineEdit textInput(UISettings::values.last_artic_base_addr, &dialog);
    layout_h.addWidget(&textInput);

    QLabel label_select(QStringLiteral("<br>") + tr("Choose setup mode:"), &dialog);
    layout.addWidget(&label_select);

    std::pair<bool, bool> install_state = Core::AreSystemTitlesInstalled();

    QRadioButton radio1(&dialog);
    QRadioButton radio2(&dialog);
    QString new3dsSetupString = tr("New 3DS setup");
    QString old3dsSetupString = tr("Old 3DS setup");
    QString availableIcon = QStringLiteral("(\u2139\uFE0F) ");
    QString unavailableIcon = QStringLiteral("(\u26A0) ");
    QString installedIcon = QStringLiteral("(\u2705) ");
    if (!install_state.first) {
        radio1.setChecked(true);

        radio1.setText(availableIcon + old3dsSetupString);
        radio1.setToolTip(tr("Setup is possible."));

        radio2.setText(unavailableIcon + new3dsSetupString);
        radio2.setToolTip(tr("Old 3DS setup is required first."));
        radio2.setEnabled(false);
    } else {
        radio1.setText(installedIcon + old3dsSetupString);
        radio1.setToolTip(tr("Setup completed."));

        if (!install_state.second) {
            radio2.setChecked(true);

            radio2.setText(availableIcon + new3dsSetupString);
            radio2.setToolTip(tr("Setup is possible."));
        } else {
            radio1.setChecked(true);

            radio2.setText(installedIcon + new3dsSetupString);
            radio2.setToolTip(tr("Setup completed."));
        }
    }
    layout.addWidget(&radio1);
    layout.addWidget(&radio2);

    QDialogButtonBox buttonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    connect(&buttonBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(&buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout.addWidget(&buttonBox);

    int res = dialog.exec();
    if (res == QDialog::Accepted) {
        bool is_o3ds = radio1.isChecked();
        if ((is_o3ds && install_state.first) || (!is_o3ds && install_state.second)) {
            QMessageBox::StandardButton answer =
                QMessageBox::question(this, tr("Set Up System Files"),
                                      tr("The system files for the selected mode are already set "
                                         "up.\nReinstall the files anyway?"),
                                      QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
            if (answer != QMessageBox::Yes) {
                return;
            }
        }
        Core::UninstallSystemFiles(is_o3ds ? Core::SystemTitleSet::Old3ds
                                           : Core::SystemTitleSet::New3ds);
        QString addr = textInput.text();
        UISettings::values.last_artic_base_addr = addr;
        BootGame(QString::fromStdString(is_o3ds ? "articinio://" : "articinin://").append(addr));
    }
}

void GMainWindow::OnMenuInstallCIA() {
    QStringList filepaths = QFileDialog::getOpenFileNames(
        this, tr("Load Files"), UISettings::values.roms_path,
        tr("3DS Installation File") + QStringLiteral(" (*.cia *.zcia *.bcia);;") + tr("All Files") +
            QStringLiteral(" (*.*)"));

    if (filepaths.isEmpty()) {
        return;
    }

    UISettings::values.roms_path = QFileInfo(filepaths[0]).path();
    InstallCIA(filepaths);
}

void GMainWindow::OnMenuConnectArticBase() {
    bool ok = false;
    auto res = QInputDialog::getText(this, tr("Connect to Artic Base"),
                                     tr("Enter Artic Base server address:"), QLineEdit::Normal,
                                     UISettings::values.last_artic_base_addr, &ok);
    if (ok) {
        UISettings::values.last_artic_base_addr = res;
        BootGame(QString::fromStdString("articbase://").append(res));
    }
}

void GMainWindow::OnMenuBootHomeMenu(u32 region) {
    BootGame(QString::fromStdString(Core::GetHomeMenuNcchPath(region)));
}

void GMainWindow::InstallCIA(QStringList filepaths) {
    ui->action_Install_CIA->setEnabled(false);
    game_list->SetDirectoryWatcherEnabled(false);

    emit UpdateProgress(0, 0);

    (void)QtConcurrent::run([this, filepaths] {
        Service::AM::InstallStatus status;
        const auto cia_progress = [&](std::size_t written, std::size_t total) {
            emit UpdateProgress(written, total);
        };
        for (const auto& current_path : filepaths) {
            status = Service::AM::InstallCIA(current_path.toStdString(), cia_progress);
            emit CIAInstallReport(status, current_path);
        }
        emit CIAInstallFinished();
    });
}

void GMainWindow::OnUpdateProgress(std::size_t written, std::size_t total) {
    if (written == 0 and total == 0) {
        progress_bar->show();
        progress_bar->setValue(0);
        progress_bar->setMaximum(INT_MAX);
    }
    progress_bar->setValue(
        static_cast<int>(INT_MAX * (static_cast<double>(written) / static_cast<double>(total))));
}

void GMainWindow::OnCIAInstallReport(Service::AM::InstallStatus status, QString filepath) {
    QString filename = QFileInfo(filepath).fileName();
    switch (status) {
    case Service::AM::InstallStatus::Success:
        this->statusBar()->showMessage(tr("%1 has been installed successfully.").arg(filename));
        break;
    case Service::AM::InstallStatus::ErrorFailedToOpenFile:
        QMessageBox::critical(this, tr("Unable to open File"),
                              tr("Could not open %1").arg(filename));
        break;
    case Service::AM::InstallStatus::ErrorAborted:
        QMessageBox::critical(
            this, tr("Installation aborted"),
            tr("The installation of %1 was aborted. Please see the log for more details")
                .arg(filename));
        break;
    case Service::AM::InstallStatus::ErrorInvalid:
        QMessageBox::critical(this, tr("Invalid File"), tr("%1 is not a valid CIA").arg(filename));
        break;
    case Service::AM::InstallStatus::ErrorEncrypted:
        QMessageBox::critical(this, tr("CIA Encrypted"),
                              tr("Your CIA file is encrypted.<br/>"
                                 "<a "
                                 "href='https://azahar-emu.org/blog/game-loading-changes/'>"
                                 "Please check our blog for more info.</a>"));
        break;
    case Service::AM::InstallStatus::ErrorFileNotFound:
        QMessageBox::critical(this, tr("Unable to find File"),
                              tr("Could not find %1").arg(filename));
        break;
    case Service::AM::InstallStatus::ErrorCannotCheckBundle:
        break;
    }
}

void GMainWindow::OnCompressFinished(bool is_compress, bool success) {
    progress_bar->hide();
    progress_bar->setValue(0);

    if (!success) {
        if (is_compress) {
            QMessageBox::critical(this, tr("Z3DS Compression"),
                                  tr("Failed to compress some files, check log for details."));
        } else {
            QMessageBox::critical(this, tr("Z3DS Compression"),
                                  tr("Failed to decompress some files, check log for details."));
        }
    } else {
        if (is_compress) {
            QMessageBox::information(this, tr("Z3DS Compression"),
                                     tr("All files have been compressed successfully."));
        } else {
            QMessageBox::information(this, tr("Z3DS Compression"),
                                     tr("All files have been decompressed successfully."));
        }
    }
}

void GMainWindow::OnBundleFinished(bool is_bundle, bool success) {
    progress_bar->hide();
    progress_bar->setValue(0);

    if (!success) {
        if (is_bundle) {
            QMessageBox::critical(this, tr("Bundle File"),
                                  tr("Failed to create the bundle, check log for details."));
        } else {
            QMessageBox::critical(this, tr("Bundle File"),
                                  tr("Failed to extract the bundle, check log for details."));
        }
    } else {
        if (is_bundle) {
            QMessageBox::information(this, tr("Bundle File"),
                                     tr("Bundle has been created successfully."));
        } else {
            QMessageBox::information(this, tr("Bundle File"),
                                     tr("Bundle has been extracted successfully."));
        }
    }
}

void GMainWindow::OnCIAInstallFinished() {
    progress_bar->hide();
    progress_bar->setValue(0);
    game_list->SetDirectoryWatcherEnabled(true);
    ui->action_Install_CIA->setEnabled(true);
    game_list->PopulateAsync(UISettings::values.game_dirs);
}

void GMainWindow::UninstallTitles(
    const std::vector<std::tuple<Service::FS::MediaType, u64, QString>>& titles) {
    if (titles.empty()) {
        return;
    }

    // Select the first title in the list as representative.
    const auto first_name = std::get<QString>(titles[0]);

    QProgressDialog progress(tr("Uninstalling '%1'...").arg(first_name), tr("Cancel"), 0,
                             static_cast<int>(titles.size()), this);
    progress.setWindowModality(Qt::WindowModal);

    QFutureWatcher<void> future_watcher;
    QObject::connect(&future_watcher, &QFutureWatcher<void>::finished, &progress,
                     &QProgressDialog::reset);
    QObject::connect(&progress, &QProgressDialog::canceled, &future_watcher,
                     &QFutureWatcher<void>::cancel);
    QObject::connect(&future_watcher, &QFutureWatcher<void>::progressValueChanged, &progress,
                     &QProgressDialog::setValue);

    auto failed = false;
    QString failed_name;

    const auto uninstall_title = [&future_watcher, &failed, &failed_name](const auto& title) {
        const auto name = std::get<QString>(title);
        const auto media_type = std::get<Service::FS::MediaType>(title);
        const auto program_id = std::get<u64>(title);

        const auto result = Service::AM::UninstallProgram(media_type, program_id);
        if (result.IsError()) {
            LOG_ERROR(Frontend, "Failed to uninstall '{}': 0x{:08X}", name.toStdString(),
                      result.raw);
            failed = true;
            failed_name = name;
            future_watcher.cancel();
        }
    };

    future_watcher.setFuture(QtConcurrent::map(titles, uninstall_title));
    progress.exec();
    future_watcher.waitForFinished();

    if (failed) {
        QMessageBox::critical(this, QStringLiteral("Azahar"),
                              tr("Failed to uninstall '%1'.").arg(failed_name));
    } else if (!future_watcher.isCanceled()) {
        QMessageBox::information(this, QStringLiteral("Azahar"),
                                 tr("Successfully uninstalled '%1'.").arg(first_name));
        emit InstalledTitlesChanged();
    }
}

void GMainWindow::OnMenuRecentFile() {
    QAction* action = qobject_cast<QAction*>(sender());
    ASSERT(action);

    const QString filename = action->data().toString();
    if (QFileInfo::exists(filename)) {
        BootGame(filename);
    } else {
        // Display an error message and remove the file from the list.
        QMessageBox::information(this, tr("File not found"),
                                 tr("File \"%1\" not found").arg(filename));

        UISettings::values.recent_files.removeOne(filename);
        UpdateRecentFiles();
    }
}

void GMainWindow::OnResumeGame(bool first_start) {
    qt_cameras->ResumeCameras();

    PreventOSSleep();

    emu_thread->SetRunning(true);
    system.frame_limiter.SetFrameAdvancing(false);
    graphics_api_button->setEnabled(false);
    qRegisterMetaType<Core::System::ResultStatus>("Core::System::ResultStatus");
    qRegisterMetaType<std::string>("std::string");
    connect(emu_thread.get(), &EmuThread::ErrorThrown, this, &GMainWindow::OnCoreError);

    UpdateMenuState();

    play_time_manager->SetProgramId(game_title_id);
    play_time_manager->Start();

    if (first_start) {
#ifdef ENABLE_DISCORD_RPC
        discord_rpc->Update(true);
#endif
    }

#ifdef __unix__
    Common::Linux::StartGamemode();
#endif

    UpdateSaveStates();
    UpdateStatusButtons();
#ifdef ENABLE_FRAME_XR // AzaharFrame: Emulation > Continue closes the in-game menu
    if (g_frame_mirror && g_frame_mirror->MenuOpen() && g_frame_xr_set_menu) {
        g_frame_xr_set_menu(false);
    }
#endif
}

void GMainWindow::OnRestartGame() {
    if (!system.IsPoweredOn()) {
        return;
    }
    // Make a copy since BootGame edits game_path
    BootGame(QString(game_path));
}

void GMainWindow::OnPauseGame() {
    system.frame_limiter.SetFrameAdvancing(true);
    qt_cameras->PauseCameras();

    play_time_manager->Stop();

    UpdateMenuState();
    AllowOSSleep();

#ifdef __unix__
    Common::Linux::StopGamemode();
#endif
}

void GMainWindow::OnPauseContinueGame() {
    if (emulation_running) {
        if (emu_thread->IsRunning() && !system.frame_limiter.IsFrameAdvancing()) {
            OnPauseGame();
        } else {
            OnResumeGame(false);
        }
    }
}

void GMainWindow::OnStopGame() {
    SetTurboEnabled(false);

    play_time_manager->Stop();
    // Update game list to show new play time
    game_list->PopulateAsync(UISettings::values.game_dirs);

    ShutdownGame();
    graphics_api_button->setEnabled(true);
    Settings::RestoreGlobalState(false);
    UpdateStatusButtons();
}

void GMainWindow::OnLoadComplete() {
    loading_screen->OnLoadComplete();
    UpdateSecondaryWindowVisibility();
}

void GMainWindow::ToggleFullscreen() {
    if (!emulation_running) {
        return;
    }
    if (secondary_window->isVisible() && secondary_window->isActiveWindow()) {
        // undo the action and fullscreen secondary manually
        ui->action_Fullscreen->toggle();
        ToggleSecondaryFullscreen();
    } else {
        if (ui->action_Fullscreen->isChecked()) {
            ShowFullscreen();
        } else {
            HideFullscreen();
        }
    }
}

void GMainWindow::ToggleSecondaryFullscreen() {
    if (!emulation_running) {
        return;
    }
    if (secondary_window->isFullScreen()) {
#ifdef NEEDS_ROUND_CORNERS_FIX
        WindowCornerManager::instance().blockRoundedCorners(secondary_window, false);
#endif
        secondary_window->restoreGeometry(UISettings::values.secondarywindow_geometry);
        secondary_window->showNormal();
    } else {
#ifdef NEEDS_ROUND_CORNERS_FIX
        WindowCornerManager::instance().blockRoundedCorners(secondary_window, true);
#endif
        UISettings::values.secondarywindow_geometry = secondary_window->saveGeometry();
        LOG_INFO(Frontend, "Attempting to fullscreen secondary window");
        secondary_window->showFullScreen();
    }
}

bool GMainWindow::WindowFullscreen() const {
    return ui->action_Single_Window_Mode->isChecked() ? isFullScreen()
                                                      : render_window->isFullScreen();
}

#ifdef ENABLE_FRAME_XR
// AzaharFrame tabmenu: what the in-game menu's buttons and settings do. The XR thread has set a
// setting already (the menu shows it at once); here it is set again and saved.
void GMainWindow::FrameMenuRequest(const FrameXr::MenuRequest& r) {
    using C = FrameXr::MenuCommand;
    using S = FrameXr::MenuSetting;
    if (!emulation_running || !g_frame_mirror || !g_frame_mirror->MenuOpen()) {
        return;
    }
    const auto close_menu = [] {
        if (g_frame_xr_set_menu) {
            g_frame_xr_set_menu(false);
        }
    };
    switch (r.command) {
    case C::Continue:
        close_menu();
        return;
    case C::QuickSave:
        close_menu();
        if (g_frame_xr_quick_save) {
            g_frame_xr_quick_save();
        }
        return;
    case C::QuickLoad:
        close_menu();
        if (g_frame_xr_quick_load) {
            g_frame_xr_quick_load();
        }
        return;
    case C::SaveSlot:
    case C::LoadSlot: {
        if (r.value < 1 || r.value >= static_cast<int>(Core::SaveStateSlotCount)) {
            return;
        }
        UpdateSaveStates();
        const bool save = r.command == C::SaveSlot;
        QAction* action = (save ? actions_save_state : actions_load_state)[r.value];
        if (!action->isEnabled()) {
            return;
        }
        LOG_INFO(Frontend, "xr: in-game menu: {} slot {}", save ? "save to" : "load from",
                 r.value);
        if (!save && UISettings::values.save_state_warning) {
            action->trigger(); // its warning shows in Azahar's window, over the paused game
            close_menu();
        } else {
            close_menu(); // saved or loaded at the emulation thread's next frame
            action->trigger();
        }
        return;
    }
    case C::Screenshot:
        close_menu(); // the next frame of the game, not of the paused one
        OnCaptureScreenshot();
        return;
    case C::Restart:
        close_menu();
        OnRestartGame();
        return;
    case C::CloseGame:
        LOG_INFO(Frontend, "xr: in-game menu: Close Game");
        ShutdownGame();
        return;
    case C::SwitchToFlat:
        if (g_frame_xr_switch) {
            g_frame_xr_switch(false);
        }
        return;
    case C::ShowWindow:
        g_frame_mirror->SetMenuWindow(true);
        return;
    case C::MoreSettings:
        g_frame_mirror->SetMenuWindow(true);
        QTimer::singleShot(0, this, [this] {
            if (emulation_running && g_frame_mirror && g_frame_mirror->MenuOpen()) {
                OnConfigure();
            }
        });
        return;
    case C::Cheats: // AzaharFrame menu: Emulation > Configure Current Game..., Cheats tab
        g_frame_mirror->SetMenuWindow(true);
        QTimer::singleShot(0, this, [this] {
            if (emulation_running && g_frame_mirror && g_frame_mirror->MenuOpen()) {
                u64 title_id{};
                system.GetAppLoader().ReadProgramId(title_id);
                OpenPerGameConfiguration(title_id, game_path, true);
            }
        });
        return;
    case C::EditControls: // AzaharFrame menu: the remap lane's
        return;
    case C::ToggleCheat: // AzaharFrame cheatstab: the Mods tab's Cheats segment (frame_cheats.cpp)
    case C::DownloadCheats:
        if (g_frame_cheats) {
            g_frame_cheats->Command(r);
        }
        return;
    case C::SelectTexturePack: // AzaharFrame texui: the Mods tab (frame_mods.cpp)
    case C::LoadModCatalog:
    case C::InstallMod:
    case C::SetModActive:
    case C::RemoveMod:
    case C::ModsDialog:
    case C::OpenUrl:
    case C::LoadPackDownloads:
    case C::DownloadPack:
    case C::CancelPackDownload:
    case C::DeletePack:
    case C::TexturesDialog:
        if (g_frame_mods) {
            g_frame_mods->Command(r);
        }
        return;
    case C::SetSetting:
        break;
    }

    // ---- A setting ----
    auto& v = Settings::values;
    bool per_game_setting = false;
    const auto set = [&r, &per_game_setting](auto& setting, auto value) {
        if (r.per_game) {
            setting.SetGlobal(false);
            per_game_setting = true;
        } else if (!setting.UsingGlobal()) {
            return; // the game has its own value (the menu does not offer this)
        }
        setting.SetValue(value);
    };
    const u32 u = static_cast<u32>(std::max(r.value, 0));
    switch (r.setting) {
    case S::HideBottomScreen:
        set(v.vr_hide_bottom_screen, r.value != 0);
        break;
    case S::ScreenShape:
        set(v.vr_top_aspect, u);
        break;
    case S::ScreenCurve:
        set(v.vr_screen_curve, u);
        break;
    case S::ScreenLayout: // AzaharFrame layout
        set(v.vr_screen_layout, u);
        break;
    case S::ImmersiveOffsetReset:
        set(v.vr_immersive_offset_x, 0.0f);
        set(v.vr_immersive_offset_y, 0.0f);
        break;
    case S::ShowStats:
        set(v.vr_show_stats, r.value != 0);
        break;
    case S::StatsPosition: // AzaharFrame statspos
        set(v.vr_stats_position, u);
        break;
    case S::Background:
        set(v.vr_background, u);
        break;
    case S::FaceButtons:
        set(v.vr_face_buttons, u);
        break;
    case S::MotionControls:
        set(v.vr_motion_controls, u);
        break;
    case S::ShowControllers:
        set(v.vr_show_controllers, r.value != 0);
        break;
    case S::WindowScale:
        set(v.vr_window_scale, u);
        g_frame_mirror->RefitWindow();
        break;
    case S::ImmersiveMode:
        set(v.vr_immersive_mode, u);
        break;
    case S::WiderView:
        set(v.vr_immersive_wider_view_level, u);
        break;
    case S::GameFov:
        set(v.vr_immersive_game_fov, static_cast<s32>(r.value));
        break;
    case S::MatchCamera: // AzaharFrame depth
        set(v.vr_immersive_match_camera, r.value != 0);
        break;
    case S::NarrowScreen:
        set(v.vr_immersive_squeeze, u);
        break;
    case S::DepthFactor:
        set(v.vr_immersive_positional_game_scaler, u);
        break;
    case S::Depth3d: // AzaharFrame depth3d
        set(v.factor_3d, std::min<u32>(u, 100));
        break;
    case S::MoveScreens: // AzaharFrame menu
        set(v.vr_move_screens, std::min<u32>(u, 2));
        break;
    case S::TouchpadTouch: // AzaharFrame menu
        set(v.vr_touchpad_touch, std::min<u32>(u, 2));
        break;
    case S::CustomTextures: // AzaharFrame texui: from the next start
        set(v.custom_textures, r.value != 0);
        if (g_frame_mods) {
            g_frame_mods->TextureSettingChanged();
        }
        break;
    case S::PreloadTextures:
        set(v.preload_textures, r.value != 0);
        if (g_frame_mods) {
            g_frame_mods->TextureSettingChanged();
        }
        break;
    }
    if (!per_game_setting) {
        config->Save(); // the global file gets the global values only (WriteGlobalSetting)
        return;
    }
    if (g_frame_game_config_name.empty()) {
        return;
    }
    // The game's own file: reading it (QtConfig's constructor) puts its saved values back into
    // the settings, so the VR ones are kept across that.
    FrameKeepSetting keep_mode{v.vr_immersive_mode};
    FrameKeepSetting keep_scaler{v.vr_immersive_positional_game_scaler};
    FrameKeepSetting keep_hide{v.vr_hide_bottom_screen};
    FrameKeepSetting keep_shape{v.vr_top_aspect};
    FrameKeepSetting keep_fov{v.vr_immersive_game_fov};
    FrameKeepSetting keep_match{v.vr_immersive_match_camera}; // AzaharFrame depth
    FrameKeepSetting keep_wider{v.vr_immersive_wider_view_level};
    FrameKeepSetting keep_squeeze{v.vr_immersive_squeeze};
    FrameKeepSetting keep_depth{v.factor_3d}; // AzaharFrame depth3d
    FrameKeepSetting keep_custom_textures{v.custom_textures}; // AzaharFrame texui
    FrameKeepSetting keep_preload{v.preload_textures};
    QtConfig game_config(g_frame_game_config_name, QtConfig::ConfigType::PerGameConfig);
    keep_mode.Restore();
    keep_scaler.Restore();
    keep_hide.Restore();
    keep_shape.Restore();
    keep_fov.Restore();
    keep_match.Restore();
    keep_wider.Restore();
    keep_squeeze.Restore();
    keep_depth.Restore();
    keep_custom_textures.Restore();
    keep_preload.Restore();
    game_config.Save();
    LOG_INFO(Frontend, "xr: in-game menu: saved {}", g_frame_game_config_name);
}
#endif

void GMainWindow::ShowFullscreen() {
#ifdef ENABLE_FRAME_XR
    // AzaharFrame flat3: not in VR, where the window is the in-game menu (it needs its menu bar
    // and the VR size). The setting stays; flat goes Fullscreen again.
    if (FrameXr::VrShowing()) {
        return;
    }
    g_frame_fullscreen = FrameXr::IsStarted();
#endif
    if (ui->action_Single_Window_Mode->isChecked()) {
        UISettings::values.geometry = saveGeometry();
        ui->menubar->hide();
        statusBar()->hide();
#ifdef NEEDS_ROUND_CORNERS_FIX
        WindowCornerManager::instance().blockRoundedCorners(this, true);
#endif
        showFullScreen();
    } else {
        UISettings::values.renderwindow_geometry = render_window->saveGeometry();
#ifdef NEEDS_ROUND_CORNERS_FIX
        WindowCornerManager::instance().blockRoundedCorners(render_window, true);
#endif
        render_window->showFullScreen();
    }
}

void GMainWindow::HideFullscreen() {
#ifdef ENABLE_FRAME_XR
    // AzaharFrame flat3: only a window that is Fullscreen (VR never is): the geometry saved
    // before Fullscreen would undo the VR size.
    if (FrameXr::IsStarted()) {
        if (!WindowFullscreen() && !g_frame_fullscreen) {
            return;
        }
        if (!WindowFullscreen()) {
            // Qt lost the state the window manager kept: Fullscreen again first, so that the
            // window manager is told to leave it.
            QWidget* const window = ui->action_Single_Window_Mode->isChecked()
                                        ? static_cast<QWidget*>(this)
                                        : render_window;
            window->setWindowState(window->windowState() | Qt::WindowFullScreen);
            LOG_INFO(Frontend, "xr: Fullscreen again first, to leave it");
        }
        g_frame_fullscreen = false;
    }
#endif
    if (ui->action_Single_Window_Mode->isChecked()) {
        statusBar()->setVisible(ui->action_Show_Status_Bar->isChecked());
        ui->menubar->show();
#ifdef NEEDS_ROUND_CORNERS_FIX
        WindowCornerManager::instance().blockRoundedCorners(this, false);
#endif
        showNormal();
        restoreGeometry(UISettings::values.geometry);
    } else {
#ifdef NEEDS_ROUND_CORNERS_FIX
        WindowCornerManager::instance().blockRoundedCorners(render_window, false);
#endif
        render_window->showNormal();
        render_window->restoreGeometry(UISettings::values.renderwindow_geometry);
    }
}

void GMainWindow::ToggleWindowMode() {
    if (ui->action_Single_Window_Mode->isChecked()) {
        // Render in the main window...
        UISettings::values.renderwindow_geometry = render_window->saveGeometry();
        ui->horizontalLayout->addWidget(render_window);
        render_window->setFocusPolicy(Qt::StrongFocus);
        if (emulation_running) {
            render_window->setVisible(true);
            render_window->setFocus();
            game_list->hide();
        }

    } else {
        // Render in a separate window...
        ui->horizontalLayout->removeWidget(render_window);
        render_window->setParent(nullptr);
        if (emulation_running) {
            render_window->setVisible(true);
            render_window->restoreGeometry(UISettings::values.renderwindow_geometry);
            game_list->show();
        }
    }
}

void GMainWindow::UpdateSecondaryWindowVisibility() {
#ifdef ENABLE_FRAME_XR
    // AzaharFrame window: in VR the bottom screen's own window (vr_bottom_screen_window).
    const bool bottom_window =
        FrameBottomWindow::Update(secondary_window, FrameXr::VrShowing(), emulation_running);
#else
    const bool bottom_window = false;
#endif
    if (!emulation_running) {
        return;
    }
    if (bottom_window) {
        // AzaharFrame window: shown above; the focus stays with the main window below.
    } else if (Settings::values.layout_option.GetValue() ==
               Settings::LayoutOption::SeparateWindows) {
        secondary_window->restoreGeometry(UISettings::values.secondarywindow_geometry);
        secondary_window->setWindowOpacity(1.0); // AzaharFrame window: see-through after VR
        secondary_window->show();
    } else {
        UISettings::values.secondarywindow_geometry = secondary_window->saveGeometry();
#ifdef ENABLE_FRAME_XR
        if (!FrameBottomWindow::KeepMapped()) // AzaharFrame window: see-through, never unmapped
#endif
            secondary_window->hide();
    }
    // make sure focus is on primary window whenever this changes
    if (UISettings::values.single_window_mode.GetValue()) {
        QApplication::setActiveWindow(this);
    } else {
        QApplication::setActiveWindow(render_window);
    }
}

void GMainWindow::ChangeScreenLayout() {
    Settings::LayoutOption new_layout = Settings::LayoutOption::Default;
    if (ui->action_Screen_Layout_Default->isChecked()) {
        new_layout = Settings::LayoutOption::Default;
    } else if (ui->action_Screen_Layout_Single_Screen->isChecked()) {
        new_layout = Settings::LayoutOption::SingleScreen;
    } else if (ui->action_Screen_Layout_Large_Screen->isChecked()) {
        new_layout = Settings::LayoutOption::LargeScreen;
        ui->menu_Small_Screen_Position->setEnabled(true);
    } else if (ui->action_Screen_Layout_Hybrid_Screen->isChecked()) {
        new_layout = Settings::LayoutOption::HybridScreen;
    } else if (ui->action_Screen_Layout_Side_by_Side->isChecked()) {
        new_layout = Settings::LayoutOption::SideScreen;
    } else if (ui->action_Screen_Layout_Separate_Windows->isChecked()) {
        new_layout = Settings::LayoutOption::SeparateWindows;
    } else if (ui->action_Screen_Layout_Custom_Layout->isChecked()) {
        new_layout = Settings::LayoutOption::CustomLayout;
    }

#ifdef ENABLE_FRAME_XR
    // AzaharFrame flat3: in flat the menu sets flat's own layout; the configured one stays for
    // VR. Separate Windows has no second window with XR: the layout stays.
    auto& flat = Settings::frame_flat_layout;
    if (flat.active) {
        if (new_layout != Settings::LayoutOption::SeparateWindows) {
            flat.layout = new_layout;
            if (new_layout != Settings::LayoutOption::SingleScreen) {
                g_frame_flat_before_single = new_layout;
            }
        }
        LogFlatLayout("View menu");
        SyncMenuUISettings();
        system.ApplySettings();
        return;
    }
#endif
    Settings::values.layout_option = new_layout;
    SyncMenuUISettings();
    system.ApplySettings();
    UpdateSecondaryWindowVisibility();
}

void GMainWindow::ChangeSmallScreenPosition() {
    Settings::SmallScreenPosition new_position = Settings::SmallScreenPosition::BottomRight;

    if (ui->action_Small_Screen_TopRight->isChecked()) {
        new_position = Settings::SmallScreenPosition::TopRight;
    } else if (ui->action_Small_Screen_MiddleRight->isChecked()) {
        new_position = Settings::SmallScreenPosition::MiddleRight;
    } else if (ui->action_Small_Screen_BottomRight->isChecked()) {
        new_position = Settings::SmallScreenPosition::BottomRight;
    } else if (ui->action_Small_Screen_TopLeft->isChecked()) {
        new_position = Settings::SmallScreenPosition::TopLeft;
    } else if (ui->action_Small_Screen_MiddleLeft->isChecked()) {
        new_position = Settings::SmallScreenPosition::MiddleLeft;
    } else if (ui->action_Small_Screen_BottomLeft->isChecked()) {
        new_position = Settings::SmallScreenPosition::BottomLeft;
    } else if (ui->action_Small_Screen_Above->isChecked()) {
        new_position = Settings::SmallScreenPosition::AboveLarge;
    } else if (ui->action_Small_Screen_Below->isChecked()) {
        new_position = Settings::SmallScreenPosition::BelowLarge;
    }

    Settings::values.small_screen_position = new_position;
    SyncMenuUISettings();
    system.ApplySettings();
    UpdateSecondaryWindowVisibility();
}

bool GMainWindow::IsTurboEnabled() {
    return turbo_mode_active;
}

void GMainWindow::SetTurboEnabled(bool state) {
    turbo_mode_active = state;
    GMainWindow::ReloadTurbo();
}

void GMainWindow::ReloadTurbo() {
    if (IsTurboEnabled()) {
        Settings::temporary_frame_limit = Settings::values.turbo_limit.GetValue();
        Settings::is_temporary_frame_limit = true;
    } else {
        Settings::is_temporary_frame_limit = false;
    }

    UpdateStatusBar();
}

// TODO: This should probably take in something more descriptive than a bool. -OS
void GMainWindow::AdjustSpeedLimit(bool increase) {
    const int SPEED_LIMIT_STEP = 5;
    auto active_limit =
        IsTurboEnabled() ? &Settings::values.turbo_limit : &Settings::values.frame_limit;
    const auto active_limit_value = active_limit->GetValue();

    if (increase) {
        if (active_limit_value < 995) {
            active_limit->SetValue(active_limit_value + SPEED_LIMIT_STEP);
        }
    } else {
        if (active_limit_value > SPEED_LIMIT_STEP) {
            active_limit->SetValue(active_limit_value - SPEED_LIMIT_STEP);
        }
    }

    if (IsTurboEnabled()) {
        ReloadTurbo();
    }

    UpdateStatusBar();
}

void GMainWindow::ToggleScreenLayout() {
    const Settings::LayoutOption new_layout = []() {
        // AzaharFrame flat3: in flat, from flat's own layout.
        const Settings::LayoutOption current_layout = Settings::WindowLayoutOption();
        std::vector<Settings::LayoutOption> layouts_to_cycle =
            Settings::values.layouts_to_cycle.GetValue();
        const auto current_pos =
            distance(layouts_to_cycle.begin(),
                     std::find(layouts_to_cycle.begin(), layouts_to_cycle.end(), current_layout));
        // if the layouts_to_cycle setting has somehow been
        // cleared out, add just default back in
        if (layouts_to_cycle.size() == 0) {
            layouts_to_cycle.push_back(Settings::LayoutOption::Default);
        }
        if (current_pos >= layouts_to_cycle.size() - 1) {
            // either this layout wasn't found or it was last so move to the beginning
            return layouts_to_cycle[0];
        } else {
            return layouts_to_cycle[current_pos + 1];
        }
    }();

#ifdef ENABLE_FRAME_XR
    auto& flat = Settings::frame_flat_layout; // AzaharFrame flat3: as ChangeScreenLayout
    if (flat.active) {
        if (new_layout != Settings::LayoutOption::SeparateWindows) {
            flat.layout = new_layout;
            if (new_layout != Settings::LayoutOption::SingleScreen) {
                g_frame_flat_before_single = new_layout;
            }
        }
        LogFlatLayout("Toggle Screen Layout");
        SyncMenuUISettings();
        system.ApplySettings();
        return;
    }
#endif
    Settings::values.layout_option = new_layout;
    SyncMenuUISettings();
    system.ApplySettings();
    UpdateSecondaryWindowVisibility();
}

void GMainWindow::OnSwapScreens() {
#ifdef ENABLE_FRAME_XR
    if (Settings::frame_flat_layout.active) { // AzaharFrame flat3: flat's own, as above
        Settings::frame_flat_layout.swap = ui->action_Screen_Layout_Swap_Screens->isChecked();
        LogFlatLayout("Swap Screens");
        system.ApplySettings();
        return;
    }
#endif
    Settings::values.swap_screen = ui->action_Screen_Layout_Swap_Screens->isChecked();
    system.ApplySettings();
}

void GMainWindow::OnRotateScreens() {
    Settings::values.upright_screen = ui->action_Screen_Layout_Upright_Screens->isChecked();
    system.ApplySettings();
}

void GMainWindow::TriggerSwapScreens() {
    ui->action_Screen_Layout_Swap_Screens->trigger();
}

void GMainWindow::TriggerRotateScreens() {
    ui->action_Screen_Layout_Upright_Screens->trigger();
}

void GMainWindow::OnSaveState() {
    if (!system.IsPoweredOn()) {
        return;
    }

    QAction* action = qobject_cast<QAction*>(sender());
    ASSERT(action);

    system.SendSignal(Core::System::Signal::Save, action->data().toUInt());
    system.frame_limiter.AdvanceFrame();
    newest_slot = action->data().toUInt();
}

void GMainWindow::OnLoadState() {
    if (!system.IsPoweredOn()) {
        return;
    }

    QAction* action = qobject_cast<QAction*>(sender());
    ASSERT(action);

    if (UISettings::values.save_state_warning) {
        QMessageBox::warning(
            this, tr("Savestates"),
            tr("Warning: Savestates are NOT a replacement for in-application saves, "
               "and are not meant to be reliable.\n\nUse at your own risk!"));
        UISettings::values.save_state_warning = false;
        config->Save();
    }

    system.SendSignal(Core::System::Signal::Load, action->data().toUInt());
    system.frame_limiter.AdvanceFrame();
}

void GMainWindow::OnConfigure() {
    game_list->SetDirectoryWatcherEnabled(false);
    Settings::SetConfiguringGlobal(true);
    ConfigureDialog configureDialog(this, hotkey_registry, system, gl_renderer, physical_devices,
                                    !multiplayer_state->IsHostingPublicRoom());
    connect(&configureDialog, &ConfigureDialog::LanguageChanged, this,
            &GMainWindow::OnLanguageChanged);
    auto old_theme = UISettings::values.theme;
    const int old_input_profile_index = Settings::values.current_input_profile_index;
    const auto old_input_profiles = Settings::values.input_profiles;
    const auto old_touch_from_button_maps = Settings::values.touch_from_button_maps;
#ifdef ENABLE_DISCORD_RPC
    const bool old_discord_presence = UISettings::values.enable_discord_presence.GetValue();
#endif
#ifdef __unix__
    const bool old_gamemode = Settings::values.enable_gamemode.GetValue();
#endif
    auto result = configureDialog.exec();
    game_list->SetDirectoryWatcherEnabled(true);
    if (result == QDialog::Accepted) {
        configureDialog.ApplyConfiguration();
        InitializeHotkeys();
        if (UISettings::values.theme != old_theme) {
            UpdateUITheme();
        }
#ifdef ENABLE_DISCORD_RPC
        if (UISettings::values.enable_discord_presence.GetValue() != old_discord_presence) {
            SetDiscordEnabled(UISettings::values.enable_discord_presence.GetValue());
            discord_rpc->Update(system.IsPoweredOn());
        }
#endif
#ifdef __unix__
        if (Settings::values.enable_gamemode.GetValue() != old_gamemode) {
            SetGamemodeEnabled(Settings::values.enable_gamemode.GetValue());
        }
#endif
        if (!multiplayer_state->IsHostingPublicRoom())
            multiplayer_state->UpdateCredentials();
        emit UpdateThemedIcons();
        SyncMenuUISettings();
        game_list->RefreshGameDirectory();
        config->Save();
#ifdef ENABLE_FRAME_XR
        FrameXrPushControls(); // AzaharFrame vrvis
#endif
        if (UISettings::values.hide_mouse && emulation_running) {
            setMouseTracking(true);
            mouse_hide_timer.start();
        } else {
            setMouseTracking(false);
        }
        ReloadTurbo();
        UpdateSecondaryWindowVisibility();
        UpdateBootHomeMenuState();
        UpdateStatusButtons();
    } else {
        Settings::values.input_profiles = old_input_profiles;
        Settings::values.touch_from_button_maps = old_touch_from_button_maps;
        Settings::LoadProfile(old_input_profile_index);
    }
}

void GMainWindow::OnLoadAmiibo() {
    if (!emu_thread || !emu_thread->IsRunning()) [[unlikely]] {
        return;
    }

    Service::SM::ServiceManager& sm = system.ServiceManager();
    auto nfc = sm.GetService<Service::NFC::Module::Interface>("nfc:u");
    if (nfc == nullptr) {
        return;
    }

    std::scoped_lock lock{system.Kernel().GetHLELock()};
    if (nfc->IsTagActive()) {
        QMessageBox::warning(this, tr("Error opening amiibo data file"),
                             tr("A tag is already in use."));
        return;
    }

    if (!nfc->IsSearchingForAmiibos()) {
        QMessageBox::warning(this, tr("Error opening amiibo data file"),
                             tr("Application is not looking for amiibos."));
        return;
    }

    const QString extensions{QStringLiteral("*.bin")};
    const QString file_filter = tr("Amiibo File") + QStringLiteral(" (%1);;").arg(extensions) +
                                tr("All Files") + QStringLiteral(" (*.*)");
    const QString filename = QFileDialog::getOpenFileName(this, tr("Load Amiibo"), {}, file_filter);

    if (filename.isEmpty()) {
        return;
    }

    LoadAmiibo(filename);
}

void GMainWindow::LoadAmiibo(const QString& filename) {
    Service::SM::ServiceManager& sm = system.ServiceManager();
    auto nfc = sm.GetService<Service::NFC::Module::Interface>("nfc:u");
    if (!nfc) [[unlikely]] {
        return;
    }

    std::scoped_lock lock{system.Kernel().GetHLELock()};
    if (!nfc->LoadAmiibo(filename.toStdString())) {
        QMessageBox::warning(this, tr("Error opening amiibo data file"),
                             tr("Unable to open amiibo file \"%1\" for reading.").arg(filename));
        return;
    }

    ui->action_Remove_Amiibo->setEnabled(true);
}

void GMainWindow::OnRemoveAmiibo() {
    Service::SM::ServiceManager& sm = system.ServiceManager();
    auto nfc = sm.GetService<Service::NFC::Module::Interface>("nfc:u");
    if (!nfc) [[unlikely]] {
        return;
    }

    std::scoped_lock lock{system.Kernel().GetHLELock()};
    nfc->RemoveAmiibo();
    ui->action_Remove_Amiibo->setEnabled(false);
}

void GMainWindow::OnOpenCitraFolder() {
    QDesktopServices::openUrl(QUrl::fromLocalFile(
        QString::fromStdString(FileUtil::GetUserPath(FileUtil::UserPath::UserDir))));
}

void GMainWindow::OnOpenNANDFolder() {
    QDesktopServices::openUrl(QUrl::fromLocalFile(
        QString::fromStdString(FileUtil::GetUserPath(FileUtil::UserPath::NANDDir))));
}

void GMainWindow::OnOpenSDMCFolder() {
    QDesktopServices::openUrl(QUrl::fromLocalFile(
        QString::fromStdString(FileUtil::GetUserPath(FileUtil::UserPath::SDMCDir))));
}

void GMainWindow::OnToggleFilterBar() {
    game_list->SetFilterVisible(ui->action_Show_Filter_Bar->isChecked());
    if (ui->action_Show_Filter_Bar->isChecked()) {
        game_list->SetFilterFocus();
    } else {
        game_list->ClearFilter();
    }
}

void GMainWindow::OnCreateGraphicsSurfaceViewer() {
    auto graphicsSurfaceViewerWidget =
        new GraphicsSurfaceWidget(system, Pica::g_debug_context, this);
    addDockWidget(Qt::RightDockWidgetArea, graphicsSurfaceViewerWidget);
    // TODO: Maybe graphicsSurfaceViewerWidget->setFloating(true);
    graphicsSurfaceViewerWidget->show();
}

void GMainWindow::OnRecordMovie() {
    MovieRecordDialog dialog(this, system);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    movie_record_on_start = true;
    movie_record_path = dialog.GetPath();
    movie_record_author = dialog.GetAuthor();

    if (emulation_running) { // Restart game
        BootGame(QString(game_path));
    }
    ui->action_Close_Movie->setEnabled(true);
    ui->action_Save_Movie->setEnabled(true);
}

void GMainWindow::OnPlayMovie() {
    MoviePlayDialog dialog(this, game_list, system);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    movie_playback_on_start = true;
    movie_playback_path = dialog.GetMoviePath();
    BootGame(dialog.GetGamePath());

    ui->action_Close_Movie->setEnabled(true);
    ui->action_Save_Movie->setEnabled(false);
}

void GMainWindow::OnCloseMovie() {
    if (movie_record_on_start) {
        QMessageBox::information(this, tr("Record Movie"), tr("Movie recording cancelled."));
        movie_record_on_start = false;
        movie_record_path.clear();
        movie_record_author.clear();
    } else {
        const bool was_running = emu_thread && emu_thread->IsRunning();
        if (was_running) {
            OnPauseGame();
        }

        const bool was_recording = movie.GetPlayMode() == Core::Movie::PlayMode::Recording;
        movie.Shutdown();
        if (was_recording) {
            QMessageBox::information(this, tr("Movie Saved"),
                                     tr("The movie is successfully saved."));
        }

        if (was_running) {
            OnResumeGame(false);
        }
    }

    ui->action_Close_Movie->setEnabled(false);
    ui->action_Save_Movie->setEnabled(false);
}

void GMainWindow::OnSaveMovie() {
    const bool was_running = emu_thread && emu_thread->IsRunning();
    if (was_running) {
        OnPauseGame();
    }

    if (movie.GetPlayMode() == Core::Movie::PlayMode::Recording) {
        movie.SaveMovie();
        QMessageBox::information(this, tr("Movie Saved"), tr("The movie is successfully saved."));
    } else {
        LOG_ERROR(Frontend, "Tried to save movie while movie is not being recorded");
    }

    if (was_running) {
        OnResumeGame(false);
    }
}

void GMainWindow::OnCaptureScreenshot() {
    if (!emu_thread) [[unlikely]] {
        return;
    }

    const bool was_running = emu_thread->IsRunning();

    if (was_running || (QMessageBox::question(this, tr("Application will unpause"),
                                              tr("The application will be unpaused, and the next "
                                                 "frame will be captured. Is this okay?"),
                                              QMessageBox::Yes | QMessageBox::No,
                                              QMessageBox::No) == QMessageBox::Yes)) {
        if (was_running) {
            OnPauseGame();
        }
        std::string path = UISettings::values.screenshot_path.GetValue();
        if (!FileUtil::IsDirectory(path)) {
            if (!FileUtil::CreateFullPath(path)) {
                QMessageBox::information(
                    this, tr("Invalid Screenshot Directory"),
                    tr("Cannot create specified screenshot directory. Screenshot "
                       "path is set back to its default value."));
                path = FileUtil::GetUserPath(FileUtil::UserPath::UserDir);
                path.append("screenshots/");
                UISettings::values.screenshot_path = path;
            };
        }

        static QRegularExpression expr(QStringLiteral("[\\/:?\"<>|]"));
        const std::string filename = game_title.remove(expr).toStdString();
        const std::string timestamp = QDateTime::currentDateTime()
                                          .toString(QStringLiteral("dd.MM.yy_hh.mm.ss.z"))
                                          .toStdString();
        path.append(fmt::format("/{}_{}.png", filename, timestamp));
        auto* const screenshot_window =
            secondary_window->HasFocus() ? secondary_window : render_window;
        screenshot_window->CaptureScreenshot(
            UISettings::values.screenshot_resolution_factor.GetValue(),
            QString::fromStdString(path));
        OnResumeGame(false);
    }
}

#ifdef ENABLE_FFMPEG
void GMainWindow::OnDumpVideo() {
    if (ui->action_Dump_Video->isChecked()) {
        OnStartVideoDumping();
    } else {
        OnStopVideoDumping();
    }
}
#endif

void GMainWindow::OnCompressFile() {
    // NOTE: Encrypted files SHOULD NEVER be compressed, otherwise the resulting
    // compressed file will have very poor compression ratios, due to the high
    // entropy caused by encryption. This may cause confusion to the user as they
    // will see the files do not compress well and blame the emulator.
    //
    // This is enforced using the loaders as they already return an error on encryption.

    QStringList filepaths = QFileDialog::getOpenFileNames(
        this, tr("Load 3DS ROM Files"), UISettings::values.roms_path,
        tr("3DS ROM Files") + QStringLiteral(" (*.cia *.cci *.3dsx *.cxi *.3ds);;") +
            tr("All Files") + QStringLiteral(" (*.*)"));

    QString out_path;

    if (filepaths.isEmpty()) {
        return;
    }

    bool single_file = filepaths.size() == 1;
    if (single_file) {
        // If it's a single file, ask the user for the output file.
        auto compress_info = Loader::GetLoaderFileInfo(filepaths[0].toStdString());
        if (!compress_info.is_valid || !compress_info.is_compress_supported) {
            LOG_ERROR(Frontend,
                      "Error compressing file {}, the selected file is not a compatible 3DS ROM "
                      "format or is encrypted or bundled.",
                      filepaths[0].toStdString());
            emit CompressFinished(true, false);
            return;
        }
        if (compress_info.is_compressed) {
            LOG_ERROR(Frontend,
                      "Error compressing file {}, the selected file is already compressed.",
                      filepaths[0].toStdString());
            emit CompressFinished(true, false);
            return;
        }

        QFileInfo fileinfo(filepaths[0]);
        QString final_path = fileinfo.path() + QStringLiteral(DIR_SEP) +
                             fileinfo.completeBaseName() + QStringLiteral(".") +
                             QString::fromStdString(compress_info.recommended_compressed_extension);

        QString out_filter =
            tr("3DS Compressed ROM File") + QStringLiteral(" (*.%1)").arg(QString::fromStdString(
                                                compress_info.recommended_compressed_extension));
        out_path = QFileDialog::getSaveFileName(this, tr("Save 3DS Compressed ROM File"),
                                                final_path, out_filter);
        if (out_path.isEmpty()) {
            return;
        }
    } else {
        // Otherwise, ask the user the directory to output the files.
        out_path = QFileDialog::getExistingDirectory(
            this, tr("Select Output 3DS Compressed ROM Folder"), UISettings::values.roms_path,
            QFileDialog::ShowDirsOnly);
        if (out_path.isEmpty()) {
            return;
        }
    }

    (void)QtConcurrent::run([this, filepaths, out_path] {
        bool single_file = filepaths.size() == 1;
        QString out_filepath;
        bool total_success = true;

        for (const QString& filepath : filepaths) {

            std::string in_path = filepath.toStdString();

            // Identify file type
            auto compress_info = Loader::GetLoaderFileInfo(filepath.toStdString());
            if (!compress_info.is_valid || !compress_info.is_compress_supported) {
                LOG_ERROR(
                    Frontend,
                    "Error decompressing file {}, the selected file is not a compatible 3DS ROM "
                    "format or is encrypted or bundled.",
                    in_path);
                total_success = false;
                continue;
            }
            if (compress_info.is_compressed) {
                LOG_ERROR(Frontend,
                          "Error compressing file {}, the selected file is already compressed.",
                          in_path);
                total_success = false;
                continue;
            }

            if (single_file) {
                out_filepath = out_path;
            } else {
                QFileInfo fileinfo(filepath);
                out_filepath =
                    out_path + QStringLiteral(DIR_SEP) + fileinfo.completeBaseName() +
                    QStringLiteral(".") +
                    QString::fromStdString(compress_info.recommended_compressed_extension);
            }

            std::string out_path = out_filepath.toStdString();

            emit UpdateProgress(0, 0);

            const auto progress = [&](std::size_t written, std::size_t total) {
                emit UpdateProgress(written, total);
            };
            bool success =
                FileUtil::CompressZ3DSFile(in_path, out_path, compress_info.underlying_magic,
                                           compress_info.default_compress_frame_size, progress,
                                           compress_info.default_metadata);
            if (!success) {
                total_success = false;
                FileUtil::Delete(out_path);
            }
        }

        emit CompressFinished(true, total_success);
    });
}

void GMainWindow::OnDecompressFile() {

    QStringList filepaths = QFileDialog::getOpenFileNames(
        this, tr("Load 3DS Compressed ROM Files"), UISettings::values.roms_path,
        tr("3DS Compressed ROM Files") + QStringLiteral(" (*.zcia *zcci *z3dsx *zcxi)") +
            QStringLiteral(";;") + tr("All Files") + QStringLiteral(" (*.*)"));

    QString out_path;

    if (filepaths.isEmpty()) {
        return;
    }

    bool single_file = filepaths.size() == 1;
    if (single_file) {
        // If it's a single file, ask the user for the output file.
        auto compress_info = Loader::GetLoaderFileInfo(filepaths[0].toStdString());
        if (!compress_info.is_valid || !compress_info.is_compress_supported) {
            LOG_ERROR(Frontend,
                      "Error decompressing file {}, the selected file is not a compatible 3DS ROM "
                      "format.",
                      filepaths[0].toStdString());
            emit CompressFinished(false, false);
            return;
        }
        if (!compress_info.is_compressed) {
            LOG_ERROR(Frontend,
                      "Error decompressing file {}, the selected file is already decompressed",
                      filepaths[0].toStdString());
            emit CompressFinished(false, false);
            return;
        }

        QFileInfo fileinfo(filepaths[0]);
        QString final_path = fileinfo.path() + QStringLiteral(DIR_SEP) +
                             fileinfo.completeBaseName() + QStringLiteral(".") +
                             QString::fromStdString(compress_info.recommended_plain_extension);

        QString out_filter = tr("3DS ROM File") +
                             QStringLiteral(" (*.%1)").arg(
                                 QString::fromStdString(compress_info.recommended_plain_extension));
        out_path =
            QFileDialog::getSaveFileName(this, tr("Save 3DS ROM File"), final_path, out_filter);
        if (out_path.isEmpty()) {
            return;
        }
    } else {
        // Otherwise, ask the user the directory to output the files.
        out_path = QFileDialog::getExistingDirectory(this, tr("Select Output 3DS ROM Folder"),
                                                     UISettings::values.roms_path,
                                                     QFileDialog::ShowDirsOnly);
        if (out_path.isEmpty()) {
            return;
        }
    }

    (void)QtConcurrent::run([this, filepaths, out_path] {
        bool single_file = filepaths.size() == 1;
        QString out_filepath;
        bool total_success = true;

        for (const QString& filepath : filepaths) {

            std::string in_path = filepath.toStdString();

            // Identify file type
            auto compress_info = Loader::GetLoaderFileInfo(filepath.toStdString());
            if (!compress_info.is_valid || !compress_info.is_compress_supported) {
                LOG_ERROR(
                    Frontend,
                    "Error decompressing file {}, the selected file is not a compatible 3DS ROM "
                    "format.",
                    in_path);
                total_success = false;
                continue;
            }
            if (!compress_info.is_compressed) {
                LOG_ERROR(Frontend,
                          "Error decompressing file {}, the selected file is already decompressed.",
                          in_path);
                total_success = false;
                continue;
            }

            if (single_file) {
                out_filepath = out_path;
            } else {
                QFileInfo fileinfo(filepath);
                out_filepath = out_path + QStringLiteral(DIR_SEP) + fileinfo.completeBaseName() +
                               QStringLiteral(".") +
                               QString::fromStdString(compress_info.recommended_plain_extension);
            }

            std::string out_path = out_filepath.toStdString();

            emit UpdateProgress(0, 0);

            const auto progress = [&](std::size_t written, std::size_t total) {
                emit UpdateProgress(written, total);
            };

            // TODO(PabloMK7): What should we do with the metadata?
            bool success = FileUtil::DeCompressZ3DSFile(in_path, out_path, progress);
            if (!success) {
                total_success = false;
                FileUtil::Delete(out_path);
            }
        }

        emit CompressFinished(false, total_success);
    });
}

void GMainWindow::OnCreateBundle() {
    QString filepath = QFileDialog::getOpenFileName(
        this, tr("Load Main 3DS ROM File"), UISettings::values.roms_path,
        tr("3DS ROM Files") + QStringLiteral(" (*.cci *.zcci *.cia *.zcia *.cxi *.3ds);;") +
            tr("All Files") + QStringLiteral(" (*.*)"));

    if (filepath.isEmpty()) {
        return;
    }

    std::string in_path = filepath.toStdString();

    auto file_info = Loader::GetLoaderFileInfo(filepath.toStdString());
    if (!file_info.is_valid || !file_info.is_bundle_supported) {
        LOG_ERROR(Frontend,
                  "Error bundling file {}, the selected file is not a compatible 3DS ROM "
                  "format.",
                  in_path);
        emit BundleFinished(true, false);
        return;
    }
    if (file_info.is_bundled) {
        LOG_ERROR(Frontend, "Error bundling file {}, the selected file is already bundled.",
                  in_path);
        emit BundleFinished(true, false);
        return;
    }

    QFileInfo fileinfo(filepath);
    QString final_path = fileinfo.path() + QStringLiteral(DIR_SEP) + fileinfo.completeBaseName() +
                         QStringLiteral(".") +
                         QString::fromStdString(file_info.recommended_bundle_extension);

    QString out_filter = tr("3DS Bundle ROM File") +
                         QStringLiteral(" (*.%1)").arg(
                             QString::fromStdString(file_info.recommended_bundle_extension));

    QString out_path = QFileDialog::getSaveFileName(this, tr("Select Output 3DS Bundled ROM File"),
                                                    final_path, out_filter);
    if (out_path.isEmpty()) {
        return;
    }

    QStringList depends = QFileDialog::getOpenFileNames(
        this, tr("Load CIA File Dependencies"), UISettings::values.roms_path,
        tr("3DS ROM Files") + QStringLiteral(" (*.cia *.zcia);;") + tr("All Files") +
            QStringLiteral(" (*.*)"));

    if (depends.isEmpty()) {
        return;
    }

    std::vector<std::string> depends_vector;

    for (const QString& dep : depends) {

        std::string dep_path = dep.toStdString();

        // Identify file type
        auto dep_file_info = Loader::GetLoaderFileInfo(dep_path);
        if (!dep_file_info.is_valid || !dep_file_info.is_bundle_supported ||
            dep_file_info.is_bundled ||
            dep_file_info.underlying_magic != std::array<u8, 4>({'C', 'I', 'A', '\0'})) {
            LOG_ERROR(Frontend,
                      "Error processing dependency file {}, the selected file is not a compatible "
                      "CIA file.",
                      dep_path);
            emit BundleFinished(true, false);
            return;
        }

        depends_vector.push_back(dep_path);
    }

    emit UpdateProgress(0, 0);

    (void)QtConcurrent::run([this, out_path, in_path, depends_vector] {
        const auto progress = [&](std::size_t written, std::size_t total) {
            emit UpdateProgress(written, total);
        };

        [[maybe_unused]] auto result =
            Loader::CreateBundleFile(out_path.toStdString(), in_path, depends_vector, progress);
        emit BundleFinished(true, result == Loader::ResultStatus::Success);
    });
}

void GMainWindow::OnExtractBundle() {
    // TODO(PabloMK7): Implement before 2127 release, do not translate temporary strings.
    QMessageBox::warning(
        this, tr("Bundle File"),
        QStringLiteral("Bundle extraction is not implemented yet, please use a TAR file browser."));
}

#ifdef ENABLE_FFMPEG
void GMainWindow::OnStartVideoDumping() {
    DumpingDialog dialog(this, system);
    if (dialog.exec() != QDialog::DialogCode::Accepted) {
        ui->action_Dump_Video->setChecked(false);
        return;
    }
    const auto path = dialog.GetFilePath();
    if (emulation_running) {
        StartVideoDumping(path);
    } else {
        video_dumping_on_start = true;
        video_dumping_path = path;
    }
}

void GMainWindow::StartVideoDumping(const QString& path) {
    auto& renderer = system.GPU().Renderer();
    const auto layout{Layout::FrameLayoutFromResolutionScale(renderer.GetResolutionScaleFactor())};

    auto dumper = std::make_shared<VideoDumper::FFmpegBackend>(renderer);
    if (dumper->StartDumping(path.toStdString(), layout)) {
        system.RegisterVideoDumper(dumper);
    } else {
        QMessageBox::critical(
            this, QStringLiteral("Azahar"),
            tr("Could not start video dumping.<br>Please ensure that the video encoder is "
               "configured correctly.<br>Refer to the log for details."));
        ui->action_Dump_Video->setChecked(false);
    }
}

void GMainWindow::OnStopVideoDumping() {
    ui->action_Dump_Video->setChecked(false);

    if (video_dumping_on_start) {
        video_dumping_on_start = false;
        video_dumping_path.clear();
    } else {
        auto dumper = system.GetVideoDumper();
        if (!dumper || !dumper->IsDumping()) {
            return;
        }

        game_paused_for_dumping = emu_thread->IsRunning();
        OnPauseGame();

        auto future = QtConcurrent::run([dumper] { dumper->StopDumping(); });
        auto* future_watcher = new QFutureWatcher<void>(this);
        connect(future_watcher, &QFutureWatcher<void>::finished, this, [this] {
            if (game_shutdown_delayed) {
                game_shutdown_delayed = false;
                ShutdownGame();
            } else if (game_paused_for_dumping) {
                game_paused_for_dumping = false;
                OnResumeGame(false);
            }
        });
        future_watcher->setFuture(future);
    }
}
#endif

void GMainWindow::UpdateStatusBar() {
    if (!emu_thread) [[unlikely]] {
        status_bar_update_timer.stop();
        return;
    }

    // Update movie status
    const u64 current = movie.GetCurrentInputIndex();
    const u64 total = movie.GetTotalInputCount();
    const auto play_mode = movie.GetPlayMode();
    if (play_mode == Core::Movie::PlayMode::Recording) {
        message_label->setText(tr("Recording %1").arg(current));
        message_label_used_for_movie = true;
        ui->action_Save_Movie->setEnabled(true);
    } else if (play_mode == Core::Movie::PlayMode::Playing) {
        message_label->setText(tr("Playing %1 / %2").arg(current).arg(total));
        message_label_used_for_movie = true;
        ui->action_Save_Movie->setEnabled(false);
    } else if (play_mode == Core::Movie::PlayMode::MovieFinished) {
        message_label->setText(tr("Movie Finished"));
        message_label_used_for_movie = true;
        ui->action_Save_Movie->setEnabled(false);
    } else if (message_label_used_for_movie) { // Clear the label if movie was just closed
        message_label->setText(QString{});
        message_label_used_for_movie = false;
        ui->action_Save_Movie->setEnabled(false);
    }

    auto results = system.GetAndResetPerfStats();
#ifdef ENABLE_FRAME_XR // AzaharFrame vrvis: the same numbers for the stats panel in VR
    if (FrameXr::IsStarted()) {
        FrameXr::EmuStats xr_stats;
        xr_stats.game_fps = results.game_fps;
        xr_stats.emulation_speed = results.emulation_speed;
        xr_stats.frame_ms = results.time_vblank_interval * 1000.0;
        xr_stats.frame_peak_ms = results.frametime_peak * 1000.0;
        xr_stats.slow_frames = results.slow_frames;
        FrameXr::SetEmuStats(xr_stats);
    }
#endif

    if (show_artic_label) {
        const bool do_mb = results.artic_transmitted >= (1000.0 * 1000.0);
        const double value = do_mb ? (results.artic_transmitted / (1000.0 * 1000.0))
                                   : (results.artic_transmitted / 1000.0);
        static const std::array<std::pair<Core::PerfStats::PerfArticEventBits, QString>, 5>
            perf_events = {
                std::make_pair(Core::PerfStats::PerfArticEventBits::ARTIC_SHARED_EXT_DATA,
                               tr("(Accessing SharedExtData)")),
                std::make_pair(Core::PerfStats::PerfArticEventBits::ARTIC_SYSTEM_SAVE_DATA,
                               tr("(Accessing SystemSaveData)")),
                std::make_pair(Core::PerfStats::PerfArticEventBits::ARTIC_BOSS_EXT_DATA,
                               tr("(Accessing BossExtData)")),
                std::make_pair(Core::PerfStats::PerfArticEventBits::ARTIC_EXT_DATA,
                               tr("(Accessing ExtData)")),
                std::make_pair(Core::PerfStats::PerfArticEventBits::ARTIC_SAVE_DATA,
                               tr("(Accessing SaveData)")),
            };

        const QString unit = do_mb ? QStringLiteral("MB/s") : QStringLiteral("KB/s");
        QString event{};
        for (auto p : perf_events) {
            if (results.artic_events.Get(p.first)) {
                event = QString::fromStdString(" ") + p.second;
                break;
            }
        }

        static const std::array label_color = {QStringLiteral(""), QStringLiteral("#eed202"),
                                               QStringLiteral("#ff3333")};

        int style_index;

        if (value > 200.0) {
            style_index = 2;
        } else if (value > 125.0) {
            style_index = 1;
        } else {
            style_index = 0;
        }

        QString style_sheet;
        if (!label_color[style_index].isEmpty()) {
            style_sheet = QStringLiteral("QLabel { color: %0; }").arg(label_color[style_index]);
        }

        artic_traffic_label->setText(
            tr("Artic Traffic: %1 %2%3").arg(value, 0, 'f', 0).arg(unit).arg(event));
        artic_traffic_label->setStyleSheet(style_sheet);
    }

    if (Settings::GetFrameLimit() == 0) {
        emu_speed_label->setText(tr("Speed: %1%").arg(results.emulation_speed * 100.0, 0, 'f', 0));
    } else {
        emu_speed_label->setText(tr("Speed: %1% / %2%")
                                     .arg(results.emulation_speed * 100.0, 0, 'f', 0)
                                     .arg(Settings::GetFrameLimit()));
    }
    game_fps_label->setText(tr("App: %1 FPS").arg(results.game_fps, 0, 'f', 0));
    if (UISettings::values.show_advanced_frametime_info) {
        emu_frametime_label->setText(
            tr("Frame: %1 ms (GPU: [CMD: %2 ms, SWP: %3 ms], IPC: %4 ms, SVC: %5 ms, Rem: %6 ms)")
                .arg(results.time_vblank_interval * 1000.0, 2, 'f', 2)
                .arg(results.time_gpu * 1000.0, 2, 'f', 2)
                .arg(results.time_swap * 1000.0, 2, 'f', 2)
                .arg(results.time_hle_ipc * 1000.0, 2, 'f', 2)
                .arg(results.time_hle_svc * 1000.0, 2, 'f', 2)
                .arg(results.time_remaining * 1000.0, 2, 'f', 2));
    } else {
        emu_frametime_label->setText(
            tr("Frame: %1 ms").arg(results.time_vblank_interval * 1000.0, 2, 'f', 2));
    }

    if (show_artic_label) {
        artic_traffic_label->setVisible(true);
    }
    emu_speed_label->setVisible(true);
    game_fps_label->setVisible(true);
    emu_frametime_label->setVisible(true);
}

void GMainWindow::UpdateBootHomeMenuState() {
    const auto current_region = Settings::values.region_value.GetValue();
    for (u32 region = 0; region < Core::NUM_SYSTEM_TITLE_REGIONS; region++) {
        const auto path = Core::GetHomeMenuNcchPath(region);
        ui->menu_Boot_Home_Menu->actions().at(region)->setEnabled(
            (current_region == Settings::REGION_VALUE_AUTO_SELECT ||
             current_region == static_cast<int>(region)) &&
            !path.empty() && FileUtil::Exists(path));
    }
}

void GMainWindow::HideMouseCursor() {
    if (!emu_thread || !UISettings::values.hide_mouse.GetValue()) {
        mouse_hide_timer.stop();
        ShowMouseCursor();
        return;
    }
    render_window->setCursor(QCursor(Qt::BlankCursor));
    secondary_window->setCursor(QCursor(Qt::BlankCursor));
    if (UISettings::values.single_window_mode.GetValue()) {
        setCursor(QCursor(Qt::BlankCursor));
    }
}

void GMainWindow::ShowMouseCursor() {
    unsetCursor();
    render_window->unsetCursor();
    secondary_window->unsetCursor();
    if (emu_thread && UISettings::values.hide_mouse) {
        mouse_hide_timer.start();
    }
}

void GMainWindow::OnMute() {
    Settings::values.audio_muted = !Settings::values.audio_muted;
    UpdateVolumeUI();
}

void GMainWindow::OnDecreaseVolume() {
    Settings::values.audio_muted = false;
    const auto current_volume =
        static_cast<s32>(Settings::values.volume.GetValue() * volume_slider->maximum());
    int step = 5;
    if (current_volume <= 30) {
        step = 2;
    }
    if (current_volume <= 6) {
        step = 1;
    }
    const auto value =
        static_cast<float>(std::max(current_volume - step, 0)) / volume_slider->maximum();
    Settings::values.volume.SetValue(value);
    UpdateVolumeUI();
}

void GMainWindow::OnIncreaseVolume() {
    Settings::values.audio_muted = false;
    const auto current_volume =
        static_cast<s32>(Settings::values.volume.GetValue() * volume_slider->maximum());
    int step = 5;
    if (current_volume < 30) {
        step = 2;
    }
    if (current_volume < 6) {
        step = 1;
    }
    const auto value = static_cast<float>(current_volume + step) / volume_slider->maximum();
    Settings::values.volume.SetValue(value);
    UpdateVolumeUI();
}

void GMainWindow::UpdateVolumeUI() {
    const auto volume_value =
        static_cast<int>(Settings::values.volume.GetValue() * volume_slider->maximum());
    volume_slider->setValue(volume_value);
    if (Settings::values.audio_muted) {
        volume_button->setChecked(false);
        volume_button->setText(tr("VOLUME: MUTE"));
    } else {
        volume_button->setChecked(true);
        volume_button->setText(tr("VOLUME: %1%", "Volume percentage (e.g. 50%)").arg(volume_value));
    }
}

void GMainWindow::UpdateAPIIndicator(bool update) {
    static std::array graphics_apis = {QStringLiteral("SOFTWARE"), QStringLiteral("OPENGL"),
                                       QStringLiteral("VULKAN")};

    static std::array graphics_api_colors = {QStringLiteral("#3ae400"), QStringLiteral("#00ccdd"),
                                             QStringLiteral("#91242a")};

    u32 api_index = static_cast<u32>(Settings::values.graphics_api.GetValue());
    if (update) {
        api_index = (api_index + 1) % graphics_apis.size();
        // Skip past any disabled renderers.
#ifndef ENABLE_SOFTWARE_RENDERER
        if (api_index == static_cast<u32>(Settings::GraphicsAPI::Software)) {
            api_index = (api_index + 1) % graphics_apis.size();
        }
#endif
#ifndef ENABLE_OPENGL
        if (api_index == static_cast<u32>(Settings::GraphicsAPI::OpenGL)) {
            api_index = (api_index + 1) % graphics_apis.size();
        }
#endif
#ifndef ENABLE_VULKAN
        if (api_index == static_cast<u32>(Settings::GraphicsAPI::Vulkan)) {
            api_index = (api_index + 1) % graphics_apis.size();
        }
#else
        if (physical_devices.empty()) {
            if (api_index == static_cast<u32>(Settings::GraphicsAPI::Vulkan)) {
                api_index = (api_index + 1) % graphics_apis.size();
            }
        }
#endif
        Settings::values.graphics_api = static_cast<Settings::GraphicsAPI>(api_index);
    }

    const QString style_sheet = QStringLiteral("QPushButton { font-weight: bold; color: %0; }")
                                    .arg(graphics_api_colors[api_index]);

    graphics_api_button->setText(graphics_apis[api_index]);
    graphics_api_button->setStyleSheet(style_sheet);
}

void GMainWindow::UpdateStatusButtons() {
    UpdateAPIIndicator();
    UpdateVolumeUI();
}

void GMainWindow::OnMouseActivity() {
    ShowMouseCursor();
}

void GMainWindow::mouseMoveEvent([[maybe_unused]] QMouseEvent* event) {
    OnMouseActivity();
}

void GMainWindow::mousePressEvent([[maybe_unused]] QMouseEvent* event) {
    OnMouseActivity();
}

void GMainWindow::mouseReleaseEvent([[maybe_unused]] QMouseEvent* event) {
    OnMouseActivity();
}

void GMainWindow::showEvent([[maybe_unused]] QShowEvent* event) {
    game_list->LoadCompatibilityList();
    game_list->PopulateAsync(UISettings::values.game_dirs);
}

bool GMainWindow::ShowExceptionDialog(Core::System::ResultStatus result,
                                      const std::string& details) {
    QDialog dialog(this);
    dialog.setWindowTitle(result == Core::System::ResultStatus::ErrorCoreExceptionRaised
                              ? tr("An exception occurred")
                              : tr("An invalid memory access occurred"));

    dialog.setMinimumSize(600, 500);

    auto* layout = new QVBoxLayout(&dialog);

    auto* label = new QLabel(
        result == Core::System::ResultStatus::ErrorCoreExceptionRaised
            ? tr("An exception occurred while executing the emulated application.")
            : tr("An invalid memory access occurred while executing the emulated application."));

    layout->addWidget(label);

    auto* textEdit = new QPlainTextEdit();
    textEdit->setPlainText(QString::fromStdString(details));
    textEdit->setReadOnly(true);
    QFont monoFont(QStringLiteral("Monospace"));
    monoFont.setStyleHint(QFont::TypeWriter);
    monoFont.setPointSize(10);
    textEdit->setFont(monoFont);
    textEdit->setLineWrapMode(QPlainTextEdit::NoWrap);
    layout->addWidget(textEdit);

    auto* buttonLayout = new QHBoxLayout();

    auto* ignoreButton = new QPushButton(tr("Ignore for this Session"));

    QObject::connect(ignoreButton, &QPushButton::clicked,
                     []() { Core::SetIgnoreExceptionsForSession(true); });

    buttonLayout->addWidget(ignoreButton);
    buttonLayout->addStretch();

    auto* continueButton = new QPushButton(tr("Continue"));
    QObject::connect(continueButton, &QPushButton::clicked, &dialog, &QDialog::reject);
    buttonLayout->addWidget(continueButton);

    auto* stopButton = new QPushButton(tr("Stop Emulation"));
    QObject::connect(stopButton, &QPushButton::clicked, &dialog, &QDialog::accept);
    buttonLayout->addWidget(stopButton);

    layout->addLayout(buttonLayout);

    return dialog.exec() == QDialog::Accepted;
}

void GMainWindow::OnCoreError(Core::System::ResultStatus result, std::string details) {
    QString status_message;

    // Handle exception dialogs separately
    if (result == Core::System::ResultStatus::ErrorCoreExceptionRaised) {
        if (ShowExceptionDialog(result, details)) {
            if (emu_thread) {
                ShutdownGame();
                return;
            }
        }

        if (emu_thread) {
            emu_thread->SetRunning(true);
            message_label->setText(status_message);
            message_label_used_for_movie = false;
        }
        return;
    }

    QString title, message;
    QMessageBox::Icon error_severity_icon;
    bool can_continue = true;
    if (result == Core::System::ResultStatus::ErrorSystemFiles) {
        const QString common_message =
            tr("%1 is missing. Please <a "
               "href='https://github.com/azahar-emu/azahar/wiki/Dumping-System-Files'>dump your "
               "system archives</a>.<br/>Continuing emulation may result in crashes and bugs.");

        if (!details.empty()) {
            message = common_message.arg(QString::fromStdString(details));
        } else {
            message = common_message.arg(tr("A system archive"));
        }

        title = tr("System Archive Not Found");
        status_message = tr("System Archive Missing");
        error_severity_icon = QMessageBox::Icon::Critical;
    } else if (result == Core::System::ResultStatus::ErrorSavestate) {
        title = tr("Save/load Error");
        message = tr("The savestate could not be created or restored.<br/><br/>%1")
                      .arg(QString::fromStdString(details).toHtmlEscaped());
        error_severity_icon = QMessageBox::Icon::Warning;
    } else if (result == Core::System::ResultStatus::ErrorArticDisconnected) {
        title = tr("Artic Server");
        message =
            tr(fmt::format("A communication error has occurred. The game will quit.\n{}", details)
                   .c_str());
        error_severity_icon = QMessageBox::Icon::Critical;
        can_continue = false;
    } else if (result == Core::System::ResultStatus::ErrorShaderCacheCrash) {
        title = tr("Shader Cache Crash");
        message =
            tr("Azahar closed unexpectedly the last time it built the Vulkan shader cache of the "
               "current title. The graphics driver may have crashed on one of the cached shaders, "
               "and continuing may crash it again.<br/><br/>To reset the shader cache, "
               "right-click the application in the game list and select "
               "<b>Disk Shader Cache &gt; Delete Vulkan Shader Cache</b>.");
        status_message = tr("Shader cache crash");
        error_severity_icon = QMessageBox::Icon::Critical;
    } else if (result == Core::System::ResultStatus::ErrorSavestateBuildMismatch) {
        title = tr("Savestate version mismatch");
        message = tr("Could not load savestate because it was created on a different Azahar "
                     "version:<br/>"
                     "<b>Azahar %1</b>.<br/><br/>Please read our blog entry <a "
                     "href='https://azahar-emu.org/blog/understanding-save-states/'>understanding "
                     "savestates</a> for more information.<br/><br/>To recover your progress, "
                     "downgrade to <b>Azahar %1</b>, load this savestate and use the application's "
                     "built-in save functionality.")
                      .arg(QString::fromStdString(details));
        error_severity_icon = QMessageBox::Icon::Critical;
    } else {
        title = tr("Fatal Error");
        message = tr("A fatal error occurred. "
                     "Check the log for details."
                     "<br/>Continuing emulation may result in crashes and bugs.");
        status_message = tr("Fatal Error encountered");
        error_severity_icon = QMessageBox::Icon::Critical;
    }

    QMessageBox message_box;
    message_box.setWindowTitle(title);
    message_box.setText(message);
    message_box.setIcon(error_severity_icon);
    if (error_severity_icon == QMessageBox::Icon::Critical) {
        if (can_continue) {
            message_box.addButton(tr("Continue"), QMessageBox::RejectRole);
        }
        QPushButton* abort_button =
            message_box.addButton(tr("Quit Application"), QMessageBox::AcceptRole);
        if (result != Core::System::ResultStatus::ShutdownRequested)
            message_box.exec();

        if (!can_continue || result == Core::System::ResultStatus::ShutdownRequested ||
            message_box.clickedButton() == abort_button) {
            if (emu_thread) {
                ShutdownGame();
                return;
            }
        }
    } else {
        // This block should run when the error isn't too big of a deal
        // e.g. when a save state can't be saved or loaded
        message_box.addButton(tr("OK"), QMessageBox::RejectRole);
        message_box.exec();
    }

    // Only show the message if the game is still running.
    if (emu_thread) {
        emu_thread->SetRunning(true);
        message_label->setText(status_message);
        message_label_used_for_movie = false;
    }
}

void GMainWindow::OnMenuAboutCitra() {
    AboutDialog about{this};
    about.exec();
}

bool GMainWindow::ConfirmClose() {
    if (!emu_thread || !UISettings::values.confirm_before_closing) {
        return true;
    }

    QMessageBox::StandardButton answer =
        QMessageBox::question(this, QStringLiteral("Azahar"), tr("Would you like to exit now?"),
                              QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    return answer != QMessageBox::No;
}

void GMainWindow::closeEvent(QCloseEvent* event) {
    if (!ConfirmClose()) {
        event->ignore();
        return;
    }

    UpdateUISettings();
    game_list->SaveInterfaceLayout();
    hotkey_registry.SaveHotkeys();

    // Shutdown session if the emu thread is active...
    if (emu_thread) {
        ShutdownGame();
    }

    // Save settings in case they were changed from outside the configuration menu.
    config->Save();

    render_window->close();
    secondary_window->close();
    multiplayer_state->Close();
    InputCommon::Shutdown();
    QWidget::closeEvent(event);
}

static bool IsSingleFileDropEvent(const QMimeData* mime) {
    return mime->hasUrls() && mime->urls().length() == 1;
}

static const std::array<std::string, 13> AcceptedExtensions = {
    "cci",  "cxi",  "bin",   "3dsx", "app",  "elf", "axf",
    "zcci", "zcxi", "z3dsx", "3ds",  "bcci", "bcxi"};

static bool IsCorrectFileExtension(const QMimeData* mime) {
    const QString& filename = mime->urls().at(0).toLocalFile();
    return std::find(AcceptedExtensions.begin(), AcceptedExtensions.end(),
                     QFileInfo(filename).suffix().toStdString()) != AcceptedExtensions.end();
}

static bool IsAcceptableDropEvent(QDropEvent* event) {
    return IsSingleFileDropEvent(event->mimeData()) && IsCorrectFileExtension(event->mimeData());
}

void GMainWindow::AcceptDropEvent(QDropEvent* event) {
    if (IsAcceptableDropEvent(event)) {
        event->setDropAction(Qt::DropAction::LinkAction);
        event->accept();
    }
}

bool GMainWindow::DropAction(QDropEvent* event) {
    if (!IsAcceptableDropEvent(event)) {
        return false;
    }

    const QMimeData* mime_data = event->mimeData();
    const QString& filename = mime_data->urls().at(0).toLocalFile();

    if (emulation_running && QFileInfo(filename).suffix() == QStringLiteral("bin")) {
        // Amiibo
        LoadAmiibo(filename);
    } else {
        // Game
        if (ConfirmChangeGame()) {
            BootGame(filename);
        }
    }
    return true;
}

void GMainWindow::OnFileOpen(const QFileOpenEvent* event) {
    BootGame(event->file());
}

void GMainWindow::dropEvent(QDropEvent* event) {
    DropAction(event);
}

void GMainWindow::dragEnterEvent(QDragEnterEvent* event) {
    AcceptDropEvent(event);
}

void GMainWindow::dragMoveEvent(QDragMoveEvent* event) {
    AcceptDropEvent(event);
}

bool GMainWindow::ConfirmChangeGame() {
    if (!emu_thread) [[unlikely]] {
        return true;
    }

    auto answer = QMessageBox::question(
        this, QStringLiteral("Azahar"),
        tr("The application is still running. Would you like to stop emulation?"),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    return answer != QMessageBox::No;
}

void GMainWindow::filterBarSetChecked(bool state) {
    ui->action_Show_Filter_Bar->setChecked(state);
    emit(OnToggleFilterBar());
}

inline bool isDarkMode() {
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    // Use colorScheme for Qt 6.5 and later
    const auto scheme = QGuiApplication::styleHints()->colorScheme();
    return scheme == Qt::ColorScheme::Dark;
#else
    // Fallback for Qt 6.4: Check the window palette
    QPalette palette = QGuiApplication::palette();
    return palette.color(QPalette::Window).lightness() < 128; // Rough check for dark mode
#endif
}

void GMainWindow::UpdateUITheme() {
    const QString icons_base_path = QStringLiteral(":/icons/");
    const QString default_system_theme = QStringLiteral("default");
    const QString light_icon_system_theme = QStringLiteral("default_with_light_icons");
    const QString colorful_icon_system_theme = QStringLiteral("colorful");

    QString default_theme;
    if (!isDarkMode()) {
        default_theme = default_system_theme;
    } else {
        default_theme = light_icon_system_theme;
    }

    const QString default_theme_path = icons_base_path + default_theme;

    const QString& current_theme = UISettings::values.theme;
    const bool is_default_theme = (current_theme == default_system_theme);
    QStringList theme_paths(default_theme_paths);

    if (is_default_theme || current_theme.isEmpty()) {
        const QString theme_uri(QStringLiteral(":default/style.qss"));
        QFile f(theme_uri);
        if (f.open(QFile::ReadOnly | QFile::Text)) {
            QTextStream ts(&f);
            qApp->setStyleSheet(ts.readAll());
            setStyleSheet(ts.readAll());
        } else {
            LOG_ERROR(Frontend,
                      "Unable to open default stylesheet, falling back to empty stylesheet");
            qApp->setStyleSheet({});
            setStyleSheet({});
        }
        theme_paths.append(default_theme_path);
        QIcon::setThemeName(default_theme);
    } else {
        const QString theme_uri(QLatin1Char{':'} + current_theme + QStringLiteral("/style.qss"));
        QFile f(theme_uri);
        if (f.open(QFile::ReadOnly | QFile::Text)) {
            QTextStream ts(&f);
            qApp->setStyleSheet(ts.readAll());
            setStyleSheet(ts.readAll());
        } else {
            LOG_ERROR(Frontend, "Unable to set style, stylesheet file not found");
        }

        const QString current_theme_path = icons_base_path + current_theme;
        theme_paths.append({default_theme_path, current_theme_path});
        QIcon::setThemeName(current_theme);
    }

#ifdef _WIN32
    if (current_theme == default_system_theme || current_theme == light_icon_system_theme ||
        current_theme == colorful_icon_system_theme) {
        SetCustomWindowsScrollbar();
    }
#endif

    QIcon::setThemeSearchPaths(theme_paths);
}

void GMainWindow::LoadTranslation() {
    bool loaded = false;

    const QString lang_en = QStringLiteral("en");
    const QString languages_dir = QStringLiteral(":/languages/");

    // Workaround for incorrect Qt system language detection
    // TODO: Allow the "<System>" option to actually be selected rather than overriding the
    //       selected language option? Current behaviour is better than the issue it fixes,
    //       but not ideal.
    if (UISettings::values.language.isEmpty()) {
        QStringList languages;
#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
        languages = QLocale::system().uiLanguages(QLocale::TagSeparator::Underscore);
#else
        languages = QLocale::system().uiLanguages();
        for (auto& lang : languages)
            lang.replace(u'-', u'_');
#endif
        for (const auto& lang : languages) {
            // If the first language found is English, no need to install any translation
            if (lang == lang_en) {
                UISettings::values.language = lang_en;
                return;
            }
            loaded = citraTranslator.load(lang, languages_dir);
            if (loaded) {
                UISettings::values.language = lang;
                break;
            }
        }
    }

    // If the selected language is English, no need to install any translation
    if (UISettings::values.language == lang_en) {
        return;
    }

    const QString qtbase_prefix = QStringLiteral("qtbase_");
    if (UISettings::values.language.isEmpty() && !loaded) {
        // Use the system's default locale
        qtTranslator.load(qtbase_prefix + QLocale::system().name(), {}, {},
                          QStringLiteral(":/languages/"));
        loaded = citraTranslator.load(QLocale::system(), {}, {}, QStringLiteral(":/languages/"));
    } else {
        // Otherwise load from the specified file
        qtTranslator.load(qtbase_prefix + UISettings::values.language,
                          QStringLiteral(":/languages/"));
        loaded = citraTranslator.load(UISettings::values.language, QStringLiteral(":/languages/"));
    }

    if (loaded) {
        qApp->installTranslator(&qtTranslator);
        qApp->installTranslator(&citraTranslator);
    } else {
        UISettings::values.language = lang_en;
    }
}

void GMainWindow::OnLanguageChanged(const QString& locale) {
    if (UISettings::values.language != QStringLiteral("en")) {
        qApp->removeTranslator(&qtTranslator);
        qApp->removeTranslator(&citraTranslator);
    }

    UISettings::values.language = locale;
    LoadTranslation();
    ui->retranslateUi(this);
    RetranslateStatusBar();
    UpdateWindowTitle();
}

void GMainWindow::OnConfigurePerGame() {
    u64 title_id{};
    system.GetAppLoader().ReadProgramId(title_id);
    OpenPerGameConfiguration(title_id, game_path);
}

void GMainWindow::OpenPerGameConfiguration(u64 title_id, const QString& file_name,
                                           bool show_cheats) {
    Settings::SetConfiguringGlobal(false);
    ConfigurePerGame dialog(this, title_id, file_name, gl_renderer, physical_devices, system);
    if (show_cheats) {
        dialog.ShowCheatsTab(); // AzaharFrame menu
    }
    const auto result = dialog.exec();

    if (result != QDialog::Accepted) {
        Settings::RestoreGlobalState(system.IsPoweredOn());
        return;
    } else if (result == QDialog::Accepted) {
        dialog.ApplyConfiguration();
    }

    // Do not cause the global config to write local settings into the config file
    const bool is_powered_on = system.IsPoweredOn();
    Settings::RestoreGlobalState(system.IsPoweredOn());

    if (!is_powered_on) {
        config->Save();
    }

    UpdateStatusButtons();
}

void GMainWindow::OnMoviePlaybackCompleted() {
    OnPauseGame();
    QMessageBox::information(this, tr("Playback Completed"), tr("Movie playback completed."));
}

#ifdef ENABLE_QT_UPDATE_CHECKER
void GMainWindow::OnEmulatorUpdateAvailable() {
    QString version_string = update_future.result();
    if (version_string.isEmpty())
        return;

    QMessageBox update_prompt(this);
    update_prompt.setWindowTitle(tr("Update Available"));
    update_prompt.setIcon(QMessageBox::Information);
    update_prompt.addButton(QMessageBox::Yes);
    update_prompt.addButton(QMessageBox::Ignore);
    update_prompt.setText(tr("Update %1 for Azahar is available.\nWould you like to download it?")
                              .arg(version_string));
    update_prompt.exec();
    if (update_prompt.button(QMessageBox::Yes) == update_prompt.clickedButton()) {
        std::string update_page_url;
        if (ShouldCheckForPrereleaseUpdates()) {
            update_page_url = "https://github.com/azahar-emu/azahar/releases";
        } else {
            update_page_url = "https://azahar-emu.org/pages/download/";
        }
        QDesktopServices::openUrl(QUrl(QString::fromStdString(update_page_url)));
    }
}
#endif

void GMainWindow::OnSwitchDiskResources(VideoCore::LoadCallbackStage stage, std::size_t value,
                                        std::size_t total, const std::string& object) {
    if (stage == VideoCore::LoadCallbackStage::Prepare) {
        loading_shaders_label->setText(QString());
        loading_shaders_label->setVisible(true);
    } else if (stage == VideoCore::LoadCallbackStage::Complete) {
        loading_shaders_label->setVisible(false);
    } else {
        loading_shaders_label->setText(
            loading_screen->GetStageTranslation(stage, value, total, object));
    }
}

void GMainWindow::UpdateWindowTitle() {
    const QString full_name = QString::fromUtf8(Common::g_build_fullname);

    if (game_title.isEmpty()) {
        setWindowTitle(QStringLiteral("Azahar for Frame %1").arg(full_name));
    } else {
        setWindowTitle(QStringLiteral("Azahar for Frame %1 | %2").arg(full_name, game_title));
        render_window->setWindowTitle(
            QStringLiteral("Azahar for Frame %1 | %2 | %3").arg(full_name, game_title, tr("Primary Window")));
        secondary_window->setWindowTitle(QStringLiteral("Azahar for Frame %1 | %2 | %3")
                                             .arg(full_name, game_title, tr("Secondary Window")));
    }
#ifdef ENABLE_FRAME_XR
    if (Settings::frame_vr_bottom_window) { // AzaharFrame window
        secondary_window->setWindowTitle(FrameBottomWindow::Title());
    }
#endif
}

void GMainWindow::UpdateUISettings() {
    if (!ui->action_Fullscreen->isChecked()) {
        UISettings::values.geometry = saveGeometry();
        UISettings::values.renderwindow_geometry = render_window->saveGeometry();
    }
    if (!secondary_window->isFullScreen()) {
        UISettings::values.secondarywindow_geometry = secondary_window->saveGeometry();
    }
    UISettings::values.state = saveState();
#if MICROPROFILE_ENABLED
    UISettings::values.microprofile_geometry = microProfileDialog->saveGeometry();
    UISettings::values.microprofile_visible = microProfileDialog->isVisible();
#endif
    UISettings::values.single_window_mode = ui->action_Single_Window_Mode->isChecked();
    UISettings::values.fullscreen = ui->action_Fullscreen->isChecked();
    UISettings::values.display_titlebar = ui->action_Display_Dock_Widget_Headers->isChecked();
    UISettings::values.show_filter_bar = ui->action_Show_Filter_Bar->isChecked();
    UISettings::values.show_status_bar = ui->action_Show_Status_Bar->isChecked();
    UISettings::values.first_start = false;
}

void GMainWindow::SyncMenuUISettings() {
    // AzaharFrame flat3: the layout the window uses (flat's own in flat, common/frame_flat.h).
    ui->action_Screen_Layout_Default->setChecked(Settings::WindowLayoutOption() ==
                                                 Settings::LayoutOption::Default);
    ui->action_Screen_Layout_Single_Screen->setChecked(Settings::WindowLayoutOption() ==
                                                       Settings::LayoutOption::SingleScreen);
    ui->action_Screen_Layout_Large_Screen->setChecked(Settings::WindowLayoutOption() ==
                                                      Settings::LayoutOption::LargeScreen);
    ui->action_Screen_Layout_Hybrid_Screen->setChecked(Settings::WindowLayoutOption() ==
                                                       Settings::LayoutOption::HybridScreen);
    ui->action_Screen_Layout_Side_by_Side->setChecked(Settings::WindowLayoutOption() ==
                                                      Settings::LayoutOption::SideScreen);
    ui->action_Screen_Layout_Separate_Windows->setChecked(Settings::WindowLayoutOption() ==
                                                          Settings::LayoutOption::SeparateWindows);
    ui->action_Screen_Layout_Custom_Layout->setChecked(Settings::WindowLayoutOption() ==
                                                       Settings::LayoutOption::CustomLayout);
    ui->action_Screen_Layout_Swap_Screens->setChecked(Settings::WindowSwapScreen());
    ui->action_Screen_Layout_Upright_Screens->setChecked(
        Settings::values.upright_screen.GetValue());

    ui->menu_Small_Screen_Position->setEnabled(Settings::WindowLayoutOption() ==
                                               Settings::LayoutOption::LargeScreen);

    ui->action_Small_Screen_TopRight->setChecked(
        Settings::values.small_screen_position.GetValue() ==
        Settings::SmallScreenPosition::TopRight);
    ui->action_Small_Screen_MiddleRight->setChecked(
        Settings::values.small_screen_position.GetValue() ==
        Settings::SmallScreenPosition::MiddleRight);
    ui->action_Small_Screen_BottomRight->setChecked(
        Settings::values.small_screen_position.GetValue() ==
        Settings::SmallScreenPosition::BottomRight);
    ui->action_Small_Screen_TopLeft->setChecked(Settings::values.small_screen_position.GetValue() ==
                                                Settings::SmallScreenPosition::TopLeft);
    ui->action_Small_Screen_MiddleLeft->setChecked(
        Settings::values.small_screen_position.GetValue() ==
        Settings::SmallScreenPosition::MiddleLeft);
    ui->action_Small_Screen_BottomLeft->setChecked(
        Settings::values.small_screen_position.GetValue() ==
        Settings::SmallScreenPosition::BottomLeft);
    ui->action_Small_Screen_Above->setChecked(Settings::values.small_screen_position.GetValue() ==
                                              Settings::SmallScreenPosition::AboveLarge);
    ui->action_Small_Screen_Below->setChecked(Settings::values.small_screen_position.GetValue() ==
                                              Settings::SmallScreenPosition::BelowLarge);
}

void GMainWindow::RetranslateStatusBar() {
    if (emu_thread)
        UpdateStatusBar();

    emu_speed_label->setToolTip(tr("Current emulation speed. Values higher or lower than 100% "
                                   "indicate emulation is running faster or slower than a 3DS."));
    game_fps_label->setToolTip(tr("How many frames per second the app is currently displaying. "
                                  "This will vary from app to app and scene to scene."));
    emu_frametime_label->setToolTip(
        tr("Time taken to emulate a 3DS frame, not counting framelimiting or v-sync. For "
           "full-speed emulation this should be at most 16.67 ms."));

    multiplayer_state->retranslateUi();
}

#ifdef ENABLE_DISCORD_RPC
void GMainWindow::SetDiscordEnabled([[maybe_unused]] bool state) {
    if (state) {
        discord_rpc = std::make_unique<DiscordRPC::DiscordImpl>(system);
    } else {
        discord_rpc = std::make_unique<DiscordRPC::NullImpl>();
    }
}
#endif

#ifdef __unix__
void GMainWindow::SetGamemodeEnabled(bool state) {
    if (emulation_running) {
        Common::Linux::SetGamemodeState(state);
    }
}
#endif

#ifdef main
#undef main
#endif

static Qt::HighDpiScaleFactorRoundingPolicy GetHighDpiRoundingPolicy() {
#ifdef _WIN32
    // For Windows, we want to avoid scaling artifacts on fractional scaling ratios.
    // This is done by setting the optimal scaling policy for the primary screen.

    // Create a temporary QApplication.
    int temp_argc = 0;
    char** temp_argv = nullptr;
    QApplication temp{temp_argc, temp_argv};

    // Get the current screen geometry.
    const QScreen* primary_screen = QGuiApplication::primaryScreen();
    if (!primary_screen) {
        return Qt::HighDpiScaleFactorRoundingPolicy::PassThrough;
    }

    const QRect screen_rect = primary_screen->geometry();
    const qreal real_ratio = primary_screen->devicePixelRatio();
    const qreal real_width = std::trunc(screen_rect.width() * real_ratio);
    const qreal real_height = std::trunc(screen_rect.height() * real_ratio);

    // Recommended minimum width and height for proper window fit.
    // Any screen with a lower resolution than this will still have a scale of 1.
    constexpr qreal minimum_width = 1350.0;
    constexpr qreal minimum_height = 900.0;

    const qreal width_ratio = std::max(1.0, real_width / minimum_width);
    const qreal height_ratio = std::max(1.0, real_height / minimum_height);

    // Get the lower of the 2 ratios and truncate, this is the maximum integer scale.
    const qreal max_ratio = std::trunc(std::min(width_ratio, height_ratio));

    if (max_ratio > real_ratio) {
        return Qt::HighDpiScaleFactorRoundingPolicy::Round;
    } else {
        return Qt::HighDpiScaleFactorRoundingPolicy::Floor;
    }
#else
    // Other OSes should be better than Windows at fractional scaling.
    return Qt::HighDpiScaleFactorRoundingPolicy::PassThrough;
#endif
}

#ifdef _WIN32
void GMainWindow::SetCustomWindowsScrollbar() {
    constexpr auto customScrollbarStyleTemplate = (R"(
        QScrollBar:vertical {{
            background: palette(window);
            width: 16px;
            margin: 0px;
            border-left: 1px solid palette({0});
        }}
        QScrollBar::handle:vertical {{
            background: palette({1});
            border: none;
            border-radius: 4px;
            margin: 3px;
        }}
        QScrollBar::handle:vertical:hover {{
            background: palette({0});
        }}
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {{
            border: none;
            height: 0px;
        }}
        QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical {{
            background: none;
        }}
    )");

    const auto darkCustomScrollbarStyle =
        QString::fromUtf8(std::format(customScrollbarStyleTemplate, "midlight", "light"));
    const auto lightCustomScrollbarStyle =
        QString::fromUtf8(std::format(customScrollbarStyleTemplate, "dark", "mid"));

    const bool isDarkMode = (QGuiApplication::styleHints()->colorScheme() == Qt::ColorScheme::Dark);

    if (isDarkMode) {
        setStyleSheet(darkCustomScrollbarStyle);
    } else {
        setStyleSheet(lightCustomScrollbarStyle);
    }
}
#endif

int LaunchQtFrontend(int argc, char* argv[]) {
#ifdef ENABLE_FRAME_XR
    // AzaharFrame fdm: Valve's FDM injection layer off before anything loads Vulkan or OpenXR
    // (frame_xr/session.cpp says why and logs it); AZAHARFRAME_FDM=1 leaves it on.
    if (const char* fdm = std::getenv("AZAHARFRAME_FDM"); fdm && std::strcmp(fdm, "1") == 0) {
        unsetenv("DISABLE_VULKAN_FDM_INJECTION_LAYER");
    } else if (!std::getenv("DISABLE_VULKAN_FDM_INJECTION_LAYER")) {
        setenv("DISABLE_VULKAN_FDM_INJECTION_LAYER", "1", 1);
    }
#endif
#ifdef __APPLE__
    // Ensure that the linker doesn't optimize qt_swizzle.mm out of existence.
    QtSwizzle::Dummy();
#endif

#if MICROPROFILE_ENABLED
    MicroProfileOnThreadCreate("Frontend");
    SCOPE_EXIT({ MicroProfileShutdown(); });
#endif

    // Init settings params
    QCoreApplication::setOrganizationName(QStringLiteral("Azahar Developers"));
    QCoreApplication::setOrganizationDomain(QStringLiteral("azahar_emu.org"));
    QCoreApplication::setApplicationName(QStringLiteral("Azahar"));
#ifdef ENABLE_FRAME_XR // AzaharFrame: the name the desktop and SteamVR know this app by
    QGuiApplication::setDesktopFileName(QStringLiteral("azaharframe"));
#else
    QGuiApplication::setDesktopFileName(QStringLiteral("org.azahar_emu.Azahar"));
#endif

    auto rounding_policy = GetHighDpiRoundingPolicy();
    QApplication::setHighDpiScaleFactorRoundingPolicy(rounding_policy);

#ifdef __APPLE__
    auto bundle_dir = FileUtil::GetBundleDirectory();
    if (bundle_dir) {
        FileUtil::SetCurrentDir(bundle_dir.value() + "..");
    }
#endif

#ifdef ENABLE_OPENGL
    QCoreApplication::setAttribute(Qt::AA_DontCheckOpenGLContextThreadAffinity);
    QCoreApplication::setAttribute(Qt::AA_ShareOpenGLContexts);
#endif

    QApplication app(argc, argv);

    // Required when using .qrc resources from within a static library.
    // See https://doc.qt.io/qt-5/resources.html#using-resources-in-a-library
    Q_INIT_RESOURCE(compatibility_list);
    Q_INIT_RESOURCE(theme_colorful);
    Q_INIT_RESOURCE(theme_colorful_dark);
    Q_INIT_RESOURCE(theme_colorful_midnight_blue);
    Q_INIT_RESOURCE(theme_default);
    Q_INIT_RESOURCE(theme_qdarkstyle);
    Q_INIT_RESOURCE(theme_qdarkstyle_midnight_blue);
#ifdef ENABLE_QT_TRANSLATION
    Q_INIT_RESOURCE(languages);
#endif

    // Qt changes the locale and causes issues in float conversion using std::to_string() when
    // generating shaders
    setlocale(LC_ALL, "C");

    auto& system{Core::System::GetInstance()};

    // Register Qt image interface
    system.RegisterImageInterface(std::make_shared<QtImageInterface>());

    GMainWindow main_window(system);

    // Register frontend applets
    Frontend::RegisterDefaultApplets(system);

    system.RegisterMiiSelector(std::make_shared<QtMiiSelector>(main_window));
    system.RegisterSoftwareKeyboard(std::make_shared<QtKeyboard>(main_window));

#ifdef __APPLE__
    // Register microphone permission check.
    system.RegisterMicPermissionCheck(&AppleAuthorization::CheckAuthorizationForMicrophone);
#endif

    main_window.show();

    QObject::connect(&app, &QGuiApplication::applicationStateChanged, &main_window,
                     &GMainWindow::OnAppFocusStateChanged);

    // Process any pending events before executing the app (prevents freeze-on–boot on macOS)
    app.processEvents();

    int result = app.exec();
#ifdef ENABLE_FRAME_XR // AzaharFrame: no renderer exists any more.
    InputCommon::FrameXr::SetTouchSink(nullptr);
    g_frame_xr_touch_sink.reset();
    FrameXr::Shutdown();
#endif
    return result;
}
