// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame emerald: the Homebrew menu (in the menu bar, before Help) and the Homebrew segment of
// the in-game menu's Mods tab, for every package of the registry (core/homebrew/packages.h). Each
// package is a HomebrewPackager (frame_emerald.h for Pokémon Emerald 3DS) made in FrameHomebrew's
// constructor; to add one, add it to the registry and to that list. An installed game is listed by
// the game list as any other file (its copy in a game folder), so the menus only install, update
// and play.

#pragma once

#include <functional>
#include <vector>
#include <QObject>
#include <QString>
#include "core/homebrew/packages.h"

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
    /// Settings this package's game needs, applied as it boots (`path`: the booted file). True when
    /// the file is this package's.
    virtual bool ApplyBootSettings(const QString& path) = 0;

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

    /// The game list loaded: each installed package checks for updates (at most once a day).
    void CheckForUpdates();
    /// The Homebrew segment's Install and Update (the window shows already) and Play's file.
    void Run(const QString& package, bool update);
    QString GamePath(const QString& package) const;

    /// BootGame: the booted file's package's settings (per run; RestoreGlobalState undoes them).
    static void ApplyBootSettings(const QString& path);

    static std::function<void()> RefreshList();

private:
    void Refresh();

    QWidget* window;
    QMenu* menu;
    std::vector<HomebrewPackager*> packagers;
    std::vector<QAction*> actions;
};
