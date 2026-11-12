// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame flat2: one XrInstance per VR session.
//
// SteamVR's OpenXR client sets an app up as the scene application once per XrInstance: the first
// session of an instance "captures scene focus" (vrclient: "Capturing Scene Focus"), which is what
// routes the controllers to the app, and makes its compositor client (shared semaphore, GPU
// timing). A later session of the same instance does neither: SteamVR still reports it FOCUSED and
// the action sets attach, but xrSyncActions gives no controller input, its GPU timing calls fail
// ("No GPU timing Vulkan device"), and the next session after that never came (owner's test of
// 2026-11-11, xrclient_azahar.txt 23:14:49 / 23:15:08, no session after 23:23:13). So the frame
// loop ends the instance with its session and makes a new one for the next: every session is the
// first of its instance. The Vulkan instance and device (made once, through the first instance)
// stay; the new instance only checks that they are on the GPU it wants.
// AZAHARFRAME_XR_INSTANCE=keep keeps one instance for the whole run (the 1.1 behaviour).

#pragma once

#include <cstdint>
#include <cstring>

namespace FrameXr {

enum class InstanceMode {
    PerSession, ///< default: a new XrInstance for each session
    KeepForRun, ///< AZAHARFRAME_XR_INSTANCE=keep
};

inline InstanceMode InstanceModeFrom(const char* env) {
    return env && std::strcmp(env, "keep") == 0 ? InstanceMode::KeepForRun
                                                 : InstanceMode::PerSession;
}

/// How many sessions the current instance has had, and what to do around the next one.
class InstanceSessions {
public:
    explicit InstanceSessions(InstanceMode mode_ = InstanceMode::PerSession) : mode{mode_} {}

    InstanceMode Mode() const {
        return mode;
    }

    /// A new instance was made.
    void InstanceMade() {
        has_instance = true;
        sessions = 0;
    }
    /// The instance was destroyed.
    void InstanceGone() {
        has_instance = false;
        sessions = 0;
    }
    /// A session was made on the current instance. Returns its number on that instance (1 first).
    uint32_t SessionMade() {
        return ++sessions;
    }

    /// Before a session: a new instance is needed (none, or this one had its session).
    bool NeedsNewInstance() const {
        return !has_instance || (mode == InstanceMode::PerSession && sessions > 0);
    }
    /// After a session ended (not at shutdown): the instance goes too.
    bool EndInstanceAfterSession() const {
        return has_instance && mode == InstanceMode::PerSession;
    }
    /// The next session would not be the first of its instance: SteamVR gives it no controllers.
    bool NextSessionReused() const {
        return has_instance && sessions > 0;
    }

private:
    InstanceMode mode;
    bool has_instance = false;
    uint32_t sessions = 0;
};

} // namespace FrameXr
