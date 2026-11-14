// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame emerald: the Homebrew menu (in the menu bar, before Help) and the Homebrew segment of
// the in-game menu's Mods tab, for every package of the registry (core/homebrew/packages.h). Each
// package is a HomebrewPackager (frame_emerald.h for Pokémon Emerald 3DS) made in FrameHomebrew's
// constructor; to add one, add it to the registry and to that list. An installed game is listed by
// the game list as any other file (its copy in a game folder), so the menus only install, update,
// play and (AzaharFrame emhb) uninstall.

#pragma once

#include <functional>
#include <vector>
#include <QObject>
#include <QString>
#include "core/homebrew/packages.h"
#include "core/homebrew/update_check.h" // AzaharFrame ui3

class QAction;
class QMenu;
class QMenuBar;
class QWidget;

namespace FrameXr {
struct MenuRequest;
}

class HomebrewPackager : public QObject {
    Q_OBJECT
public:
    struct Status {
        bool installed = false;
        bool update = false; ///< a newer release was seen
        bool busy = false;   ///< installing or updating
        int progress = -1;   ///< 0 to 1000 while busy, -1 when unknown
        QString status;      ///< while busy: what it does; else the last error ("" none)
        QString game_path;   ///< what Play starts
        QString detail;      ///< installed: what was chosen ("Português (Brasil)"), or ""
        // AzaharFrame ui3: Check for Updates
        bool checking = false; ///< a check asks GitHub
        QString version;       ///< installed: the release ("v0.5.0")
        QString latest;        ///< with `update`: the newer release
        s64 checked = 0;       ///< unix seconds of the last check that answered (0 none)
        // AzaharFrame emhb: found from the files, whoever installed it
        bool repair = false;        ///< some of it is there but it cannot be played: Repair
        bool can_uninstall = false; ///< anything of it is there
        bool has_save = false;      ///< a save is there (Uninstall offers to delete it too)
    };

    HomebrewPackager(const Homebrew::PackageInfo& info, QObject* parent)
        : QObject(parent), info(info) {}

    const Homebrew::PackageInfo& Info() const {
        return info;
    }
    virtual Status GetStatus() const = 0;
    /// Install, or install again over what is there. `parent` is the window for its dialogs.
    virtual void Install(QWidget* parent) = 0;
    virtual void Update(QWidget* parent) = 0;
    /// The once-a-day check for a newer release (only when installed).
    virtual void CheckForUpdate() = 0;
    /// AzaharFrame ui3: Check for Updates: asks GitHub now, past the once-a-day check (at most once
    /// in Homebrew::kManualCheckGap; a press sooner gets the last result), then `done(result)` on
    /// the GUI thread. Nothing when not installed or busy.
    virtual void CheckNow(std::function<void(Homebrew::CheckResult)> done) = 0;
    /// Settings this package's game needs, applied as it boots (`path`: the booted file). True when
    /// the file is this package's.
    virtual bool ApplyBootSettings(const QString& path) = 0;
    /// An install started at launch with no dialogs (a developer setting), or false.
    virtual bool Automatic() const {
        return false;
    }
    /// AzaharFrame emhb: looks at the files again (the game folders may have changed).
    virtual void Rescan() {}
    /// AzaharFrame emhb: Uninstall's dialog in Azahar's window (Cancel / Uninstall, and Also Delete
    /// Save Data when there is a save), then UninstallNow. The game is not running.
    virtual void Uninstall(QWidget* /*parent*/) {}
    /// AzaharFrame emhb: removes what the install put in place (never the ROM; the save only with
    /// `delete_save`), off the GUI thread; `done(error)` ("" on success) on the GUI thread.
    virtual void UninstallNow(bool /*delete_save*/, std::function<void(QString)> /*done*/) {}
    /// AzaharFrame emhb: Uninstall's supporting text.
    virtual QString UninstallBody(bool /*has_save*/, bool /*delete_save*/) const {
        return {};
    }

signals:
    void Changed();

private:
    const Homebrew::PackageInfo& info;
};

class FrameHomebrew : public QObject {
    Q_OBJECT
public:
    /// `refresh_list`: the game list scans its folders again (a game or a folder was added).
    FrameHomebrew(QWidget* window, QMenuBar* bar, QMenu* before,
                  std::function<void()> refresh_list);

    /// A package's Automatic() run (a developer setting) starts once the window is up.
    void StartAutomatic();
    /// The game list loaded: each installed package checks for updates (at most once a day).
    void CheckForUpdates();
    /// The Homebrew segment's Install and Update (the window shows already) and Play's file.
    void Run(const QString& package, bool update);
    /// AzaharFrame ui3: Check for Updates for one package ("" every installed one): the result as a
    /// VR toast (`vr`), else in a message box over the window.
    void CheckNow(const QString& package, bool vr);
    QString GamePath(const QString& package) const;
    /// AzaharFrame emhb: the Homebrew tab's Uninstall and its dialog. `code` 0 opens it, 1 ticks or
    /// unticks Also Delete Save Data, 2 cancels, 3 confirms (the result as a VR toast).
    void VrUninstall(const QString& package, int code);

    /// BootGame: the booted file's package's settings (per run; RestoreGlobalState undoes them).
    static void ApplyBootSettings(const QString& path);

    static std::function<void()> RefreshList();

private:
    void Refresh();
    HomebrewPackager* Find(const QString& package) const; // AzaharFrame emhb
    /// AzaharFrame emhb: the package's game is the one running (it cannot be uninstalled then).
    bool Running(const HomebrewPackager* p) const;
    void Uninstall(HomebrewPackager* p); ///< AzaharFrame emhb: from the Homebrew menu

    QWidget* window;
    QMenu* menu;
    std::vector<HomebrewPackager*> packagers;
    std::vector<QAction*> actions;
    QAction* check_action = nullptr; ///< AzaharFrame ui3: Check for Updates
    std::vector<QAction*> uninstall_actions; ///< AzaharFrame emhb: one per package
    /// AzaharFrame emhb: the Homebrew tab's open dialog (`package` "" none)
    struct VrDialog {
        QString package;
        bool running = false; ///< "Stop the Game First" (OK only)
        bool delete_save = false;
    } vr_dialog;
};
