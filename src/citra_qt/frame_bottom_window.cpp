// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame window: see frame_bottom_window.h.

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <QGuiApplication>
#include <QString>
#include "citra_qt/bootmanager.h"
#include "citra_qt/frame_bottom_window.h"
#include "common/frame_bottom_window.h"
#include "common/logging/log.h"
#include "common/settings.h"
#include "core/3ds.h"

#if defined(__linux__)
#include <dlfcn.h>
#endif

namespace FrameBottomWindow {
namespace {

bool g_shown = false;   ///< the window is the bottom screen now
bool g_mapped = false;  ///< the window was shown as the bottom screen in this run

// Not a Steam app: gamescope makes the window the SteamVR overlay valve.steam.desktopgame.<id>.
constexpr std::uint32_t kDefaultAppId = 0x41465242; // "AFRB"

#if defined(__linux__) && QT_CONFIG(xcb)
// The few xcb calls, from the libxcb Qt's xcb platform already loaded (no link to xcb).
struct XcbInternAtomCookie {
    unsigned int sequence;
};
struct XcbInternAtomReply {
    std::uint8_t response_type;
    std::uint8_t pad0;
    std::uint16_t sequence;
    std::uint32_t length;
    std::uint32_t atom;
};
using InternAtomFn = XcbInternAtomCookie (*)(void*, std::uint8_t, std::uint16_t, const char*);
using InternAtomReplyFn = XcbInternAtomReply* (*)(void*, XcbInternAtomCookie, void**);
using ChangePropertyFn = XcbInternAtomCookie (*)(void*, std::uint8_t, std::uint32_t, std::uint32_t,
                                                 std::uint32_t, std::uint8_t, std::uint32_t,
                                                 const void*);
using FlushFn = int (*)(void*);

void SetSteamGame(GRenderWindow* window, std::uint32_t app_id) {
    auto* x11 = qGuiApp->nativeInterface<QNativeInterface::QX11Application>();
    void* lib = dlopen("libxcb.so.1", RTLD_NOW | RTLD_NOLOAD);
    if (!x11 || !lib) {
        LOG_WARNING(Frontend, "xr: bottom screen window: not on X11, no app id of its own");
        return;
    }
    const auto intern = reinterpret_cast<InternAtomFn>(dlsym(lib, "xcb_intern_atom"));
    const auto reply_of = reinterpret_cast<InternAtomReplyFn>(dlsym(lib, "xcb_intern_atom_reply"));
    const auto change = reinterpret_cast<ChangePropertyFn>(dlsym(lib, "xcb_change_property"));
    const auto flush = reinterpret_cast<FlushFn>(dlsym(lib, "xcb_flush"));
    void* conn = x11->connection();
    if (!intern || !reply_of || !change || !flush || !conn) {
        dlclose(lib);
        return;
    }
    static constexpr char kName[] = "STEAM_GAME";
    XcbInternAtomReply* reply =
        reply_of(conn, intern(conn, 0, std::strlen(kName), kName), nullptr);
    if (reply) {
        constexpr std::uint8_t kReplace = 0;
        constexpr std::uint32_t kCardinal = 6;
        const auto x_window = static_cast<std::uint32_t>(window->winId());
        change(conn, kReplace, x_window, reply->atom, kCardinal, 32, 1, &app_id);
        flush(conn);
        std::free(reply);
        LOG_INFO(Frontend, "xr: bottom screen window: STEAM_GAME {}", app_id);
    }
    dlclose(lib);
}
#else
void SetSteamGame(GRenderWindow*, std::uint32_t) {}
#endif

void GiveOwnAppId(GRenderWindow* window) {
    static bool done = false;
    if (done) {
        return;
    }
    done = true;
    const char* env = std::getenv("AZAHARFRAME_BOTTOM_WINDOW_APPID");
    if (env && std::strcmp(env, "game") == 0) {
        return;
    }
    std::uint32_t app_id = kDefaultAppId;
    if (env && *env) {
        app_id = static_cast<std::uint32_t>(std::strtoul(env, nullptr, 0));
    }
    if (app_id != 0) {
        SetSteamGame(window, app_id);
    }
}

int Scale() {
    const char* env = std::getenv("AZAHARFRAME_BOTTOM_WINDOW_SCALE");
    const int scale = env ? std::atoi(env) : 2;
    return scale >= 1 && scale <= 4 ? scale : 2;
}

} // namespace

bool Update(GRenderWindow* window, bool in_vr, bool emulation_running) {
    const bool want =
        window && in_vr && emulation_running && Settings::values.vr_bottom_screen_window.GetValue();
    if (want == g_shown) {
        return want;
    }
    g_shown = want;
    // Before any size change: the window's layout is the bottom screen alone while it is set.
    Settings::frame_vr_bottom_window = want;
    if (want) {
        GiveOwnAppId(window);
        const bool upright = Settings::values.upright_screen.GetValue();
        const int w = (upright ? Core::kScreenBottomHeight : Core::kScreenBottomWidth) * Scale();
        const int h = (upright ? Core::kScreenBottomWidth : Core::kScreenBottomHeight) * Scale();
        window->setWindowTitle(Title());
        window->setWindowOpacity(1.0);
        window->resize(w, h);
        window->show();
        // Above the main window, which covers the screen see-through in VR: X picks the window
        // under the pointer by stacking order.
        window->raise();
        g_mapped = true;
        LOG_INFO(Frontend, "xr: bottom screen window shown, {}x{}", w, h);
    } else {
        // Never unmapped while the program runs (see the header): see-through only.
        window->setWindowOpacity(0.0);
        LOG_INFO(Frontend, "xr: bottom screen window see-through");
    }
    window->OnFramebufferSizeChanged();
    return want;
}

bool KeepMapped() {
    return g_mapped;
}

QString Title() {
    return QStringLiteral("Azahar for Frame: Bottom Screen");
}

} // namespace FrameBottomWindow
