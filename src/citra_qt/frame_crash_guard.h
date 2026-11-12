// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame compat: the Qt side of core/mods/crash_guard.h. GMainWindow tells it when a game
// boots, starts, stops and fails; it keeps the marker, turns off what a run that ended
// unexpectedly left marked (before the game boots), and says so: in VR the in-game menu opens on
// the Mods tab with the snackbar "Turned Off <names>: ..." and Turn Back On (frame_mods.cpp
// publishes it), in Flat a message box with the same text. A normal Stop or Quit, or 60 seconds
// of the game running, is a clean run. GUI thread only.

#pragma once

#include <functional>
#include <string>
#include <vector>

#include <QObject>

#include "common/common_types.h"
#include "core/mods/crash_guard.h"

class QTimer;
class QWidget;

class FrameCrashGuard : public QObject {
public:
    /// The one instance (made on first use, a child of qApp).
    static FrameCrashGuard& Get();

    /// What the guard needs from the window: whether the game runs (not paused), whether VR
    /// shows, the in-game menu opened (VR), the window for Flat's message box.
    struct Hooks {
        std::function<bool()> running;
        std::function<bool()> vr;
        std::function<void()> open_menu;
        std::function<QWidget*()> window;
    };
    void SetHooks(Hooks hooks);

    /// Before the game boots: a marker the last run of this game left turns its mods and packs
    /// off (disabled, not deleted). Their names go to TurnedOff.
    void BeforeBoot(u64 title_id);
    /// The game loaded and starts: the new mods and packs that are on are marked, and the 60 s
    /// count begins. A notice from BeforeBoot shows once the game runs.
    void Started(u64 title_id);
    /// The game stops. A clean run unless a core error came first.
    void Stopped();
    /// The emulator reported an error: the stop that follows is not a clean run.
    void CoreError();

    /// What BeforeBoot turned off for the game running now (empty: nothing).
    const std::vector<Mods::CrashGuard::Item>& TurnedOff() const {
        return turned_off;
    }
    u64 TurnedOffTitle() const {
        return turned_off_title;
    }
    /// Raised with each new notice (the menu shows the Mods tab once per notice).
    int Serial() const {
        return serial;
    }
    /// Turn Back On: on again from the next start (they count as new again). `done(ok)` on the
    /// GUI thread.
    void TurnBackOn(std::function<void(bool)> done);
    /// The notice was answered (Flat's OK): it goes.
    void Dismiss();

    /// A mod or pack was installed or turned on (NoteOn), turned off or removed (NoteOff).
    static void NoteOn(u64 title_id, const Mods::CrashGuard::Item& item);
    static void NoteOff(u64 title_id, Mods::CrashGuard::Item::Kind kind, const std::string& id);

private:
    explicit FrameCrashGuard(QObject* parent);
    void Tick();
    void ShowNotice();

    Hooks hooks;
    QTimer* timer = nullptr;
    u64 title_id = 0;
    bool armed = false;
    bool core_error = false;
    int run_ticks = 0;     ///< seconds the game ran (not paused) since Started
    bool notice_due = false;
    std::vector<Mods::CrashGuard::Item> turned_off;
    u64 turned_off_title = 0;
    int serial = 0;
};
