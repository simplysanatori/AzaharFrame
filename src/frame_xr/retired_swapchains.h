// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame projdef: the swapchains destroyed since the XR loop last looked.
//
// An absent quad keeps its slot in the layer list (layer_slots.h), collapsed, with its own
// swapchain. When that swapchain is destroyed while the list lives on (the controller models' pass
// made again at a switch between the screens' quads and the projection layer, with the same number
// of screen layers before and after), the next xrEndFrame carried a destroyed handle and SteamVR
// refused the frame: XR_ERROR_HANDLE_INVALID three times at a live switch to the projection layer
// (1.1.1-590, reports/76/session590). So every place that destroys a swapchain whose image can go
// out as a slotted quad says so here, and the XR loop drops those slots (LayerSlots::Map::Forget)
// before it builds the frame's list. A handle the runtime gives out again later is a new key.

#pragma once

#include <cstdint>
#include <mutex>
#include <vector>

namespace FrameXr::RetiredSwapchains {

namespace Detail {
struct State {
    std::mutex mutex;
    std::vector<uint64_t> handles;
};
inline State& Get() {
    static State s;
    return s;
}
} // namespace Detail

/// After xrDestroySwapchain (any thread). `handle`: the XrSwapchain as an integer (0: nothing).
inline void Note(uint64_t handle) {
    if (handle == 0) {
        return;
    }
    Detail::State& s = Detail::Get();
    std::scoped_lock lock{s.mutex};
    s.handles.push_back(handle);
}

/// The handles noted since the last call (the XR loop, once a frame).
inline std::vector<uint64_t> Take() {
    Detail::State& s = Detail::Get();
    std::scoped_lock lock{s.mutex};
    std::vector<uint64_t> out;
    out.swap(s.handles);
    return out;
}

} // namespace FrameXr::RetiredSwapchains
