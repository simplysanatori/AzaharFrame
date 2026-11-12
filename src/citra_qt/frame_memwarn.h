// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv2 or any later version
// Refer to the misc/licenses/gplv2.txt file included.

// AzaharFrame memwarn: the memory watch's thread and the Qt side of its dialog. The rules and the
// texts are in frame_xr/mem_watch.h; GMainWindow::FrameMemoryEvent (citra_qt.cpp) shows the toast
// and the dialog and pauses the game.

#pragma once

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <vector>
#include "frame_xr/mem_watch.h"

class QDialog;
class QWidget;

class FrameMemWarn {
public:
    /// Called on the watch thread with Warn or Pause, Azahar's use and its limit (bytes).
    using Callback =
        std::function<void(FrameXr::MemWatch::Action, std::uint64_t used, std::uint64_t max)>;

    /// Starts watching when this process's cgroup has a memory limit (the launcher's scope); else
    /// does nothing at all.
    explicit FrameMemWarn(Callback callback);
    ~FrameMemWarn();
    FrameMemWarn(const FrameMemWarn&) = delete;
    FrameMemWarn& operator=(const FrameMemWarn&) = delete;

    [[nodiscard]] bool Active() const {
        return thread.joinable();
    }
    /// The counters now (the GUI thread's log after Turn Off Custom Textures).
    [[nodiscard]] FrameXr::MemWatch::Sample ReadNow() const;

    /// At Level 2 (MemWatch::Watcher::Critical): the autosaves wait. Any thread.
    static bool Critical() {
        return critical.load(std::memory_order_relaxed);
    }

private:
    void Run();

    std::string dir;
    Callback callback;
    std::mutex mutex;
    std::condition_variable cv;
    bool quit = false;
    std::thread thread;
    /// Test driver: AZAHARFRAME_TEST_MEMWARN_BALLAST="SECONDS:MB,..." holds MB more memory from
    /// that second of the watch on (touched, so it counts), to reach the levels in a mock run;
    /// "SECONDS:87%" as much as brings the use to 87 % of the limit.
    std::vector<std::vector<char>> ballast;
    static std::atomic<bool> critical;
};

/// The dialog in Qt (Flat, and the window behind VR): the headline, the supporting text, and the
/// buttons at the end: Stop Game, Resume, then Turn Off Custom Textures when they are on. Not
/// shown; the caller owns it. done() codes: kMemoryTextures, kMemoryResume, kMemoryStop.
QDialog* FrameMakeMemoryDialog(QWidget* parent, const FrameXr::MemWatch::PauseText& text);
constexpr int kMemoryTextures = 11;
constexpr int kMemoryResume = 12;
constexpr int kMemoryStop = 13;
