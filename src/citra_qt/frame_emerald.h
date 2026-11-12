// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame emerald: the Pokémon Emerald 3DS package (core/homebrew/emerald3ds.h): its dialogs
// and steps. Install: the player's own ROM (its SHA-1 checked), a game folder, then the latest
// release from GitHub, emerald3ds.pak made from the ROM and the release's build recipe by the program itself
// (core/homebrew/emerald3ds_build.h, no Python, nothing downloaded is run), and the files put in place. Update: the .3dsx
// and .smdh alone when the release keeps the data ABI, else the pack made again from the kept ROM.
// The save is backed up first and never touched.

#pragma once

#include <atomic>
#include <memory>
#include <string>
#include <QString>
#include "citra_qt/frame_homebrew.h"
#include "core/homebrew/emerald3ds.h"

class FrameEmerald : public HomebrewPackager {
    Q_OBJECT
public:
    FrameEmerald(const Homebrew::PackageInfo& info, QObject* parent);

    Status GetStatus() const override;
    void Install(QWidget* parent) override;
    void Update(QWidget* parent) override;
    void CheckForUpdate() override;
    void CheckNow(std::function<void(Homebrew::CheckResult)> done) override; // AzaharFrame ui3
    bool ApplyBootSettings(const QString& path) override;

    /// The one place the ROM file is picked (to be swapped for the shared picker later).
    QString PickRom(QWidget* parent) const;

    /// Developer and test runs: AZAHARFRAME_HOMEBREW_AUTO=emerald3ds installs at start with no
    /// dialog, from AZAHARFRAME_HOMEBREW_ROM into AZAHARFRAME_HOMEBREW_ROMDIR (required: never a
    /// game folder of the settings), in AZAHARFRAME_HOMEBREW_LANG (or the ROM's own); the result
    /// is logged, and AZAHARFRAME_HOMEBREW_AUTO_QUIT=1 then quits.
    bool Automatic() const override {
        return automatic;
    }

private:
    struct Job {
        std::string rom; ///< the ROM to build from
        bool rom_is_kept = false;
        std::string rom_dir;  ///< the game folder for the .3dsx copy
        std::string language; ///< "" for the ROM's own
        bool update = false;
    };
    struct Prepared;

    void LoadState();
    void SaveState();
    /// The game folder for the copy: the installed one, else chosen (may add a game folder).
    QString ChooseRomDir(QWidget* parent);
    /// The game language when the ROM builds more than one ("en" or "pt" from the English ROM),
    /// `current` preselected; "" when cancelled.
    std::string ChooseLanguage(QWidget* parent, const Homebrew::Emerald3ds::RomProfile& rom,
                               const std::string& current);
    void Warn(QWidget* parent, const QString& title, const QString& text);
    bool ConfirmSource(QWidget* parent, const QString& title); // AzaharFrame cpphelpers
    void Start(QWidget* parent, Job job);
    void SetBusy(bool busy, const QString& status, int progress = -1);
    void Finish(QWidget* parent, const QString& error, const QString& done_text);

    Homebrew::Emerald3ds::Paths paths;
    Homebrew::Emerald3ds::State state;
    bool have_state = false;
    bool busy = false;
    int progress = -1;
    QString status;
    bool checking = false;
    bool automatic = false;
    // AzaharFrame ui3: the presses of Check for Updates waiting for the check that runs, and the
    // last one's request and result (at most one request in Homebrew::kManualCheckGap)
    std::vector<std::function<void(Homebrew::CheckResult)>> waiting;
    Homebrew::ManualCheck manual;
    /// Asks GitHub for the latest release (off the GUI thread); the answer is kept and `waiting`
    /// get the result.
    void Fetch();
};
