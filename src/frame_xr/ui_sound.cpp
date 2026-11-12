// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame menuux: the click's player (ui_sound.h). One thread: it reads and shapes the file
// once, opens an SDL audio device (queued audio, no callback) while clicks come, and closes it
// after kIdle without one.

#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <mutex>
#include <thread>
#include <utility>

#include "common/logging/log.h"
#include "frame_xr/ui_sound.h"

#ifdef HAVE_SDL2
#include <SDL.h>
#endif

namespace FrameXr::UiSound {

namespace {

constexpr float kGain = 0.5f;  ///< -6 dB: the file peaks at -16 dBFS, so about -22 here
constexpr int kMaxMs = 150;    ///< its sound is over in about 40 ms; the rest is a quiet tail
constexpr int kFadeMs = 40;
constexpr auto kIdle = std::chrono::seconds(10);

struct Player {
    std::mutex mutex;
    std::condition_variable cv;
    std::thread thread;
    bool started = false;
    bool quit = false;
    int plays = 0; ///< clicks asked for, not yet queued
    bool prepare = false;
};

// Never destroyed: a thread still running at exit must not meet a destroyed std::thread.
Player& P() {
    static Player* p = new Player;
    return *p;
}

std::optional<Clip> LoadClick() {
    const char* home = std::getenv("HOME");
    const char* xdg = std::getenv("XDG_DATA_HOME");
    for (const std::string& path : ClickPaths(home ? home : "", xdg ? xdg : "")) {
        std::ifstream f(path, std::ios::binary);
        if (!f) {
            continue;
        }
        const std::vector<uint8_t> bytes{std::istreambuf_iterator<char>(f),
                                         std::istreambuf_iterator<char>()};
        auto clip = DecodeWav(bytes, kGain, kMaxMs, kFadeMs);
        if (clip) {
            LOG_INFO(Frontend, "xr: menu click: Steam's UI sound, {} ms at {} Hz",
                     clip->samples.size() / 2 * 1000 / static_cast<std::size_t>(clip->rate),
                     clip->rate);
            return clip;
        }
        LOG_WARNING(Frontend, "xr: menu click: {} is no 16-bit PCM WAVE file", path);
    }
    LOG_INFO(Frontend, "xr: menu click: Steam's UI sound not found; no click");
    return std::nullopt;
}

void Run() {
    Player& p = P();
    bool loaded = false;
    std::optional<Clip> clip;
#ifdef HAVE_SDL2
    bool audio_init = false;
    SDL_AudioDeviceID device = 0;
#endif
    auto last = std::chrono::steady_clock::now();
    std::unique_lock lock{p.mutex};
    while (!p.quit) {
#ifdef HAVE_SDL2
        const bool open = device != 0;
#else
        const bool open = false;
#endif
        if (open) {
            p.cv.wait_until(lock, last + kIdle,
                            [&p] { return p.quit || p.plays > 0 || p.prepare; });
        } else {
            p.cv.wait(lock, [&p] { return p.quit || p.plays > 0 || p.prepare; });
        }
        if (p.quit) {
            break;
        }
        const int plays = std::exchange(p.plays, 0);
        const bool wanted = plays > 0 || std::exchange(p.prepare, false);
        lock.unlock();
        if (wanted) {
            last = std::chrono::steady_clock::now();
        }
        if (wanted && !loaded) {
            loaded = true;
            clip = LoadClick();
        }
#ifdef HAVE_SDL2
        if (wanted && clip && device == 0) {
            if (!audio_init) {
                audio_init = SDL_InitSubSystem(SDL_INIT_AUDIO) == 0;
                if (!audio_init) {
                    LOG_WARNING(Frontend, "xr: menu click: no SDL audio: {}", SDL_GetError());
                    clip.reset(); // never again this run
                }
            }
            if (audio_init) {
                SDL_AudioSpec want{};
                want.freq = clip->rate;
                want.format = AUDIO_S16SYS;
                want.channels = 2;
                want.samples = 512;
                device = SDL_OpenAudioDevice(nullptr, 0, &want, nullptr, 0); // SDL converts
                if (device == 0) {
                    LOG_WARNING(Frontend, "xr: menu click: no audio device: {}", SDL_GetError());
                } else {
                    SDL_PauseAudioDevice(device, 0);
                }
            }
        }
        if (plays > 0 && clip && device != 0) {
            SDL_ClearQueuedAudio(device); // a click still playing is cut, never queued behind
            SDL_QueueAudio(device, clip->samples.data(),
                           static_cast<Uint32>(clip->samples.size() * sizeof(int16_t)));
        }
        if (!wanted && device != 0 &&
            std::chrono::steady_clock::now() - last >= kIdle) {
            SDL_CloseAudioDevice(device);
            device = 0;
        }
#else
        (void)plays;
#endif
        lock.lock();
    }
#ifdef HAVE_SDL2
    if (device != 0) {
        SDL_CloseAudioDevice(device);
    }
    if (audio_init) {
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
    }
#endif
}

void Signal(bool play) {
    Player& p = P();
    {
        std::scoped_lock lock{p.mutex};
        if (!p.started) {
            p.started = true;
            p.quit = false;
            p.thread = std::thread(Run);
        }
        if (play) {
            p.plays++;
        } else {
            p.prepare = true;
        }
    }
    p.cv.notify_one();
}

} // namespace

void Prepare() {
    Signal(false);
}

void PlayClick() {
    Signal(true);
}

void Shutdown() {
    Player& p = P();
    {
        std::scoped_lock lock{p.mutex};
        if (!p.started) {
            return;
        }
        p.quit = true;
    }
    p.cv.notify_one();
    if (p.thread.joinable()) {
        p.thread.join();
    }
    std::scoped_lock lock{p.mutex};
    p.started = false;
}

} // namespace FrameXr::UiSound
