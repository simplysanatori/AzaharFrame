// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame emerald: the Pokémon Emerald 3DS package (core/homebrew/emerald3ds.h): its dialogs
// and steps. Install: the player's own ROM (its SHA-1 checked), a game folder, then the latest
// release from GitHub, emerald3ds.pak made by Emerald 3DS's own builder (steam-frame/tools/
// emerald3ds-build.py, a separate Python process), and the files put in place. Update: the .3dsx
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
    bool ApplyBootSettings(const QString& path) override;

    /// The one place the ROM file is picked (to be swapped for the shared picker later).
    static QString PickRom(QWidget* parent);

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
};
