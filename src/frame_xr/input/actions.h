// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include "frame_xr/xr_includes.h" // AzaharFrame: OpenXR without prototypes
#include "frame_xr/input/input_frame.h"

// AzaharFrame
// The OpenXR side of the controller input: one action set with the Steam Frame controller
// bindings, read once per display frame into a plain InputFrame. Everything here must run on the
// XR thread (the one that calls xrWaitFrame / xrSyncActions).
namespace FrameXr::Input {

class Actions {
public:
    /// Instance extension that provides /interaction_profiles/valve/frame_controller. The XR core
    /// enables it at xrCreateInstance when the runtime lists it (without it the Frame controllers
    /// are translated into another profile and the D-pad, View and bumpers are missing).
    static constexpr const char* kFrameControllerExtension =
        "XR_VALVE_frame_controller_interaction";

    Actions() = default;
    ~Actions();
    Actions(const Actions&) = delete;
    Actions& operator=(const Actions&) = delete;

    /// Creates the action set and actions and suggests the bindings. Call after xrCreateInstance
    /// and before xrAttachSessionActionSets (Attach does that). `has_frame_controller` is whether
    /// kFrameControllerExtension was enabled. Returns false when an action could not be made.
    bool Create(XrInstance instance, bool has_frame_controller);

    /// Creates the aim spaces and attaches the action set to the session. The session may attach
    /// action sets only once: if the XR core has its own action sets, attach them all in one call using
    /// action_set() instead and call CreateSpaces().
    bool Attach(XrSession session);

    /// Only the aim spaces (when the XR core attaches the action sets itself).
    bool CreateSpaces(XrSession session);

    /// The action set, for a caller that attaches several action sets in one call.
    XrActionSet action_set() const {
        return action_set_;
    }

    /// xrSyncActions for the action set (skip with sync = false if the caller synced already, with
    /// action_set() among the active sets), then reads every action and locates the aim spaces
    /// relative to `base` at `time`. Returns false when the sync failed; `out` is then left
    /// unchanged.
    bool Sync(XrSession session, XrSpace base, XrTime time, InputFrame& out, bool sync = true);

    void Destroy();

private:
    struct Impl;
    Impl* impl_ = nullptr;
    XrActionSet action_set_ = XR_NULL_HANDLE;
};

} // namespace FrameXr::Input
