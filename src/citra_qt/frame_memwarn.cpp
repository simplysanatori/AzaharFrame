// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv2 or any later version
// Refer to the misc/licenses/gplv2.txt file included.

#include <chrono>
#include <cstdlib>
#include <QDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include "citra_qt/frame_memwarn.h"
#include "common/logging/log.h"

namespace MW = FrameXr::MemWatch;

std::atomic<bool> FrameMemWarn::critical{false};

FrameMemWarn::FrameMemWarn(Callback callback_) : callback(std::move(callback_)) {
    dir = MW::ScopeDir();
    if (dir.empty()) {
        LOG_INFO(Frontend, "memwarn: no memory limit (no scope): not watching");
        return;
    }
    const MW::Sample s = MW::Read(dir);
    LOG_INFO(Frontend,
             "memwarn: watching every {} s, limit {} MiB, high {} MiB, now {} MiB (warn at {} %, "
             "pause at {} %)",
             MW::kPeriodMs / 1000, s.max >> 20, s.high >> 20, s.Used() >> 20,
             static_cast<int>(MW::kWarnAt * 100), static_cast<int>(MW::kPauseAt * 100));
    thread = std::thread([this] { Run(); });
}

FrameMemWarn::~FrameMemWarn() {
    {
        std::scoped_lock lock{mutex};
        quit = true;
    }
    cv.notify_all();
    if (thread.joinable()) {
        thread.join();
    }
}

MW::Sample FrameMemWarn::ReadNow() const {
    return dir.empty() ? MW::Sample{} : MW::Read(dir);
}

void FrameMemWarn::Run() {
    // Test driver: the ballast's steps, in seconds of the watch
    struct Step {
        double at_s;
        std::size_t mb;
        double share; ///< "SECONDS:87%": as much as brings the use to 87 % of the limit
    };
    std::vector<Step> steps;
    if (const char* env = std::getenv("AZAHARFRAME_TEST_MEMWARN_BALLAST")) {
        std::string text = env;
        std::size_t pos = 0;
        while (pos < text.size()) {
            const std::size_t comma = text.find(',', pos);
            const std::string item = text.substr(pos, comma - pos);
            if (const std::size_t colon = item.find(':'); colon != std::string::npos) {
                const std::string v = item.substr(colon + 1);
                const bool pct = !v.empty() && v.back() == '%';
                steps.push_back({std::atof(item.substr(0, colon).c_str()),
                                 pct ? 0 : static_cast<std::size_t>(std::atol(v.c_str())),
                                 pct ? std::atof(v.c_str()) / 100.0 : 0.0});
            }
            pos = comma == std::string::npos ? text.size() : comma + 1;
        }
    }
    const auto start = std::chrono::steady_clock::now();
    MW::Watcher watcher;
    std::unique_lock lock{mutex};
    while (!quit) {
        const std::int64_t now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                                        std::chrono::steady_clock::now() - start)
                                        .count();
        while (!steps.empty() && now_ms >= static_cast<std::int64_t>(steps.front().at_s * 1000)) {
            std::size_t mb = steps.front().mb;
            if (const double share = steps.front().share; share > 0.0) {
                const MW::Sample now = MW::Read(dir);
                const double want = share * static_cast<double>(now.max);
                mb = want > static_cast<double>(now.Used())
                         ? static_cast<std::size_t>((want - static_cast<double>(now.Used())) /
                                                    1048576.0)
                         : 0;
            }
            steps.erase(steps.begin());
            ballast.emplace_back(mb << 20, '\1'); // filled: every page is touched and counted
            LOG_INFO(Frontend, "memwarn test: {} MiB of ballast held", mb);
        }
        const MW::Sample s = MW::Read(dir);
        const MW::Action a = watcher.Step(s, now_ms);
        const bool was = critical.exchange(watcher.Critical(), std::memory_order_relaxed);
        if (was != watcher.Critical()) {
            LOG_INFO(Frontend, "memwarn: {} ({} of {} MiB)",
                     watcher.Critical() ? "at the pause level: autosaves wait"
                                        : "under the pause level again",
                     s.Used() >> 20, s.max >> 20);
        }
        if (a != MW::Action::None) {
            LOG_WARNING(Frontend,
                        "memwarn: {}: {} of {} MiB (RAM {} MiB less page cache {} MiB, swap {} "
                        "MiB, high events {})",
                        a == MW::Action::Warn ? "running low" : "almost out, pausing",
                        s.Used() >> 20, s.max >> 20, s.current >> 20, s.cache >> 20, s.swap >> 20,
                        s.high_events);
            lock.unlock();
            callback(a, s.Used(), s.max);
            lock.lock();
        }
        cv.wait_for(lock, std::chrono::milliseconds(MW::kPeriodMs), [this] { return quit; });
    }
}

QDialog* FrameMakeMemoryDialog(QWidget* parent, const MW::PauseText& t) {
    // As the Preload Textures? dialog (frame_preload_confirm.cpp): a Material-style dialog in Qt
    // widgets, the headline, the supporting text, text buttons at the end, the main one last.
    auto* d = new QDialog(parent);
    d->setWindowTitle(QString::fromStdString(t.title));
    auto* layout = new QVBoxLayout(d);
    layout->setContentsMargins(24, 24, 24, 16);
    layout->setSpacing(12);
    auto* headline = new QLabel(QString::fromStdString(t.title));
    QFont hf = headline->font();
    hf.setPointSizeF(hf.pointSizeF() * 1.5);
    headline->setFont(hf);
    layout->addWidget(headline);
    auto* body = new QLabel(QString::fromStdString(t.body));
    body->setWordWrap(true);
    layout->addWidget(body);
    layout->addStretch(1); // a taller window (Flat's 16:9) keeps the text together at the top
    auto* buttons = new QHBoxLayout;
    buttons->addStretch(1);
    const auto add = [&](const std::string& label, int code) {
        auto* b = new QPushButton(QString::fromStdString(label));
        b->setFlat(true);
        b->setAutoDefault(false);
        QObject::connect(b, &QPushButton::clicked, d, [d, code] { d->done(code); });
        buttons->addWidget(b);
        return b;
    };
    add(t.stop, kMemoryStop);
    QPushButton* resume = add(t.resume, kMemoryResume);
    QPushButton* main = resume;
    if (!t.textures.empty()) {
        main = add(t.textures, kMemoryTextures);
    }
    main->setDefault(true);
    layout->addLayout(buttons);
    d->setMinimumWidth(520);
    return d;
}
