// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame ctgp7: the Qt side of the Mods tab's CTGP-7 segment (frame_xr/menu/ctgp7_tab.h):
// the one-click install of CTGP-7 for the Mario Kart 7 that is open, and Launch. The work is
// core/ctgp7 (no Qt); this object lives on the GUI thread and runs the long steps on a thread of
// their own, as the Emerald 3DS package does (frame_emerald.cpp):
//
//   1. Checks: a Mario Kart 7 CTGP-7 supports, its v1.2 update installed, its save data there
//      (a question in the segment says what to do; nothing is fetched from Nintendo).
//   2. The game as a title: already installed, else the open file installed (a .cia as it is, a
//      decrypted .3ds/.cci/.cxi converted first: core/ctgp7/cia_build.h). A file that can't be
//      used asks for another copy (Azahar's window, a file dialog).
//   3. The official offline installer from its GameBanana page (MD5 checked), unpacked.
//   4. The installer run in the emulator: A is pressed for its "Start with install?" until its
//      files appear; the segment shows "This Takes About 5 Minutes" and a bar. Done when its
//      version file, plugin and the launcher title are there and the files stop changing.
//   5. The CTGP-7 launcher started, the plugin loader on for that run only (Ctgp7::RunOverride,
//      from BootGame). Updates are CTGP-7's own.
//
// Cancel stops any step at once (the download's socket too). The downloads and the unpacked
// installer are deleted when done; a cancelled download is resumed next time.
//
// Test driver (builds with the test hooks): AZAHARFRAME_TEST_CTGP7="install@SECONDS" presses
// Install that many seconds after a game starts; AZAHARFRAME_TEST_CTGP7_STOP=unpacked quits once
// the installer is unpacked (a networked run, then an offline one); AZAHARFRAME_TEST_CTGP7_ROM is
// the file a "choose your game" question picks; AZAHARFRAME_TEST_CTGP7="uninstall@SECONDS[:both][:progress]"
// answers Uninstall; AZAHARFRAME_TEST_CTGP7_A=N presses A every 5 s from 15 s on (N times) in
// Mario Kart 7 and the launcher.

#pragma once

#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <QElapsedTimer>
#include <QObject>
#include <QString>
#include "common/common_types.h"
#include "core/ctgp7/ctgp7.h"
#include "core/mods/transfer.h"
#include "frame_xr/menu/ctgp7_tab.h"

class QTimer;
class QWidget;

namespace FrameXr {
struct MenuRequest;
}

class FrameCtgp7 : public QObject {
public:
    struct Hooks {
        std::function<void(const QString& path)> boot; ///< BootGame (stops the running game first)
        std::function<void()> stop;                    ///< ShutdownGame
        std::function<void()> close_menu;              ///< the in-game menu closes
        std::function<bool()> running;                 ///< a game runs (not just paused)
        std::function<void(bool down)> press_a;        ///< the 3DS's A, from outside the controls
        std::function<void(const std::string&)> notify; ///< a toast in VR, the status bar in Flat
        std::function<void()> refresh_list;            ///< the game list scans again
        std::function<QWidget*()> window;              ///< Azahar's window, shown (dialogs)
        std::function<void()> quit;                    ///< the app quits (test runs)
    };

    FrameCtgp7(Hooks hooks, QObject* parent);
    ~FrameCtgp7() override;

    /// The in-game menu opened on this game (`path`: the file it was started from).
    void Open(u64 title_id, const QString& path);
    /// A game started (BootGame, after its settings): the plugin loader for the launcher's run,
    /// the test driver, and the installer's session.
    void GameBooted(u64 title_id, const QString& path);
    /// The running game stopped (ShutdownGame).
    void GameStopped();
    void Command(const FrameXr::MenuRequest& r);

private:
    enum class Step { Idle, Preparing, Installer, Finishing, Uninstalling };

    void Publish();
    void Install();
    void Launch();
    void Cancel();
    void Fail(const std::string& why);
    void Ask(const std::string& title, const std::string& body);
    /// Steps 2 and 3 off the GUI thread; `game`: the file to install ("" already installed).
    void Prepare(std::string game);
    void BootInstaller(const std::string& path);
    void SessionTick();
    void EndSession();
    /// Finish Setup for a copy with its launcher's CIA on the SD card: that CIA installed.
    void FinishSetup();
    /// "Uninstall CTGP-7?", then the uninstall (`both`: the Mario Kart 7 title the app installed too).
    void AskUninstall();
    void Uninstall(bool both, bool delete_progress);

    Hooks hooks;
    Ctgp7::Paths paths;
    u64 title_id = 0;     ///< the Mario Kart 7 the menu is on
    QString game_path;
    u64 running_title = 0;
    Step step = Step::Idle;
    int generation = 0;   ///< a run's answers after Cancel are dropped
    /// One install's Cancel: the flag and the download's socket.
    struct Run {
        std::atomic_bool cancel{false};
        Mods::Transfer::Stopper stopper{cancel};
    };
    std::shared_ptr<Run> run;
    // the worker's progress (any thread)
    struct Progress {
        std::mutex mutex;
        std::string phase, detail;
        int permille = -1;
    };
    std::shared_ptr<Progress> progress;
    std::string error;
    FrameXr::Menu::ModsDialog dialog;
    QTimer* tick = nullptr;
    // the installer's run
    std::string installer;
    QElapsedTimer session;
    u64 base_bytes = 0, last_bytes = 0;
    std::filesystem::file_time_type base_time; ///< the installer's run began (files after it are its)
    bool had_dir = false;
    bool started = false;
    int quiet_ticks = 0;
    bool booting = false; ///< our own BootGame is under way (its ShutdownGame is ours)
    // the test driver
    bool test_fired = false;
};
