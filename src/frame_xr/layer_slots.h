// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame flash: a fixed position in the layer list for every quad after the screens.
//
// SteamVR's OpenXR runtime (the OpenVR back end the Frame uses) makes each quad layer the
// overlay of its position among the frame's quads (openxr_quadlayer_<i>) and sets that overlay's
// texture, bounds, transform, size and visibility in separate calls the compositor applies on its
// own schedule (handoff report 55/flash/overlay-path.md). A quad that appears or goes (a toast,
// the hint chip, the Autosaved label, the shader badge, the stats, a cursor, a beam, a dot) moved
// every quad after it to another overlay for that frame, so for a refresh one quad's texture
// could show with another's bounds or place: a toast or a dot in another quad's place.
//
// Here each quad has a key (its group, its swapchain, its rect's size, and its number among the
// quads of the same group, swapchain and size this frame). A key keeps its position for as long as
// the context (home, menu, game) lasts; while its quad is absent the slot carries the quad as it
// last was, shrunk to nothing (kCollapse: its own swapchain and rect, so no overlay changes its
// texture), and the compositor draws nothing of it. A new key goes after the slot of the quad
// before it this frame, in its group, so the drawing order stays the producers' order. The list
// changes length only when a key is first seen in a context, or when the context changes (a new
// list then anyway). Over the runtime's layer limit, the slots absent longest go first. Pure
// (unit-tested: src/tests/frame_xr/layer_slots.cpp).
//
// integ13: the HUD's quads (toast, hint chip, Autosaved label, shader badge) are reserved
// (Reserve) from the start of each context, before they first show, so one that shows mid-context
// fills its own slot instead of going in before the Top group (the controllers' eye quads, the
// beams, the dots) and moving every quad of it to another overlay. Over the limit a reserved slot
// goes only when no other absent slot is left, and stays gone for the rest of the context.

#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace FrameXr::LayerSlots {

/// The groups, in drawing order (lower first). Screens are not slotted here (app.cpp puts them
/// first, in a fixed order).
enum class Group : uint8_t {
    Extra = 1, ///< quads added through AddQuad
    Ui = 2,    ///< VrUi's: panels, menus, keyboard, stats, cursors
    Toast = 3,
    Hint = 4,
    Label = 5,
    Badge = 6,
    Top = 7, ///< over the controller models: ctrlfront's two eye quads, beams, dots
};

struct Key {
    Group group = Group::Ui;
    uint64_t swapchain = 0;
    int32_t width = 0, height = 0; ///< the rect's size (an atlas region's kind)
    uint32_t ordinal = 0;
    bool operator==(const Key&) const = default;
};

/// A collapsed (absent) quad's size, times its own.
inline constexpr float kCollapse = 1e-5f;

/// The list's context for Map::Begin (a new one starts a new list): the panels' context, whether a
/// game's screens are live, the top screen's path, and how many screen layers come before the
/// slots (a game starting or stopping, Motion Clarity's backing coming or going: the slots after
/// them would all move anyway). AzaharFrame projdef: the path too, so every switch between the
/// quads and the projection layer starts a new list, also when the screens' layers keep their
/// number (Motion Clarity off, or the normal view: 2 -> 2): with the count alone the old list
/// lived on with the models' destroyed swapchain in a slot (XR_ERROR_HANDLE_INVALID in 590,
/// XR_ERROR_LAYER_INVALID in 590b).
inline uint32_t ContextKey(uint32_t panels, bool screens_live, bool projection,
                           std::size_t screen_layers) {
    const uint32_t n = static_cast<uint32_t>(std::min<std::size_t>(screen_layers, 15));
    return ((panels * 2 + (screens_live ? 1u : 0u)) * 2 + (projection ? 1u : 0u)) * 16 + n;
}

/// `Quad` is anything copyable; `collapse(Quad&)` shrinks one in place.
template <typename Quad>
class Map {
public:
    struct Out {
        Quad quad;
        Group group = Group::Ui;
        bool present = false; ///< false: a collapsed placeholder
    };

    /// Starts a frame. A different `context` (or the first frame) forgets every slot.
    void Begin(uint32_t context) {
        if (!started_ || context != context_) {
            slots_.clear();
            unreserved_.clear(); // integ13: a new list: every key may be reserved again
            context_ = context;
            started_ = true;
            resets_++;
        }
        frame_++;
        counts_.clear();
        last_index_ = -1;
        last_group_ = Group::Extra;
        for (Slot& s : slots_) {
            s.present = false;
        }
    }

    /// integ13: after Begin, before the frame's Add calls: a slot for this key (as Add takes it,
    /// ordinal 0) from now on, at the end of its group, `placeholder` (collapsed) until its quad
    /// first shows. Nothing when the key has a slot, or when its reserved slot went over the
    /// limit in this list.
    void Reserve(Group group, uint64_t swapchain, int32_t width, int32_t height,
                 const Quad& placeholder) {
        const Key key{group, swapchain, width, height, 0};
        for (const Slot& s : slots_) {
            if (s.key == key) {
                return;
            }
        }
        for (const Key& k : unreserved_) {
            if (k == key) {
                return;
            }
        }
        std::size_t at = 0;
        while (at < slots_.size() && slots_[at].key.group <= group) {
            at++;
        }
        Slot slot{key, placeholder};
        slot.seen = frame_;
        slot.reserved = true;
        slots_.insert(slots_.begin() + static_cast<std::ptrdiff_t>(at), slot);
        added_++;
    }

    /// The frame's quads, in the order they are drawn (groups in ascending order).
    void Add(Group group, uint64_t swapchain, int32_t width, int32_t height, const Quad& quad) {
        Key key{group, swapchain, width, height, 0};
        uint32_t& n = Count(key);
        key.ordinal = n++;
        if (group != last_group_) {
            last_index_ = -1;
            last_group_ = group;
        }
        int found = -1;
        for (std::size_t i = 0; i < slots_.size(); i++) {
            if (slots_[i].key == key) {
                found = static_cast<int>(i);
                break;
            }
        }
        if (found < 0) {
            // After the slot of the quad before it in this frame (its group), else at the start of
            // its group (after every slot of a lower group).
            std::size_t at = 0;
            if (last_index_ >= 0) {
                at = static_cast<std::size_t>(last_index_) + 1;
            } else {
                while (at < slots_.size() && slots_[at].key.group < group) {
                    at++;
                }
            }
            slots_.insert(slots_.begin() + static_cast<std::ptrdiff_t>(at), Slot{key, quad});
            found = static_cast<int>(at);
            added_++;
        }
        Slot& s = slots_[static_cast<std::size_t>(found)];
        s.quad = quad;
        s.present = true;
        s.seen = frame_;
        last_index_ = found;
    }

    /// AzaharFrame projdef: drops every slot of `swapchain` (destroyed: retired_swapchains.h), so
    /// no collapsed placeholder carries a dead handle into xrEndFrame. The others keep their
    /// order; a quad of a new swapchain gets a new slot. Returns how many went.
    std::size_t Forget(uint64_t swapchain) {
        const auto gone = [swapchain](const auto& k) { return k.swapchain == swapchain; };
        const std::size_t before = slots_.size();
        std::erase_if(slots_, [&](const Slot& s) { return gone(s.key); });
        std::erase_if(unreserved_, gone);
        const std::size_t n = before - slots_.size();
        forgotten_ += n;
        if (n) {
            last_index_ = -1; // an index into the old list
        }
        return n;
    }

    /// The list for this frame: every slot, absent ones collapsed. At most `max_slots` (the
    /// runtime's limit less what else the frame carries): the slots absent longest are dropped
    /// first (for good: a key seen again gets a new slot); integ13: a reserved slot only when no
    /// other absent slot is left (then it is not reserved again in this list).
    template <typename Collapse>
    void End(std::size_t max_slots, std::vector<Out>& out, Collapse&& collapse) {
        while (slots_.size() > max_slots) {
            std::size_t victim = slots_.size();
            for (const bool reserved : {false, true}) {
                uint64_t oldest = frame_ + 1;
                for (std::size_t i = 0; i < slots_.size(); i++) {
                    if (!slots_[i].present && slots_[i].reserved == reserved &&
                        slots_[i].seen < oldest) {
                        oldest = slots_[i].seen;
                        victim = i;
                    }
                }
                if (victim < slots_.size()) {
                    break;
                }
            }
            if (victim == slots_.size()) {
                break; // only present quads left: the caller's limit cuts the rest
            }
            if (slots_[victim].reserved) {
                unreserved_.push_back(slots_[victim].key);
            }
            slots_.erase(slots_.begin() + static_cast<std::ptrdiff_t>(victim));
            dropped_++;
        }
        out.clear();
        for (const Slot& s : slots_) {
            Out o{s.quad, s.key.group, s.present};
            if (!s.present) {
                collapse(o.quad);
            }
            out.push_back(o);
        }
    }

    std::size_t Size() const {
        return slots_.size();
    }
    /// flash2: the slots of the groups before `group` (absent ones too). What the frame carries
    /// before that group's quads: Size() also counts that group's own slots from earlier frames,
    /// so a budget for it taken from Size() shrank by what it had placed before (the Top group
    /// in a menu: its beams and the controllers' eye quads left out with room to spare).
    std::size_t SizeBefore(Group group) const {
        return static_cast<std::size_t>(std::count_if(
            slots_.begin(), slots_.end(), [group](const Slot& s) { return s.key.group < group; }));
    }
    /// Totals since the start, for the log: slots made, dropped over the limit, context resets.
    uint64_t Added() const {
        return added_;
    }
    uint64_t Dropped() const {
        return dropped_;
    }
    uint64_t Resets() const {
        return resets_;
    }
    /// AzaharFrame projdef: slots dropped by Forget (their swapchain destroyed).
    uint64_t Forgotten() const {
        return forgotten_;
    }

private:
    struct Slot {
        Key key;
        Quad quad;
        bool present = false;
        uint64_t seen = 0;
        bool reserved = false; ///< integ13: made by Reserve
    };
    uint32_t& Count(const Key& key) {
        for (auto& [k, n] : counts_) {
            if (k == key) {
                return n;
            }
        }
        counts_.push_back({key, 0});
        return counts_.back().second;
    }

    std::vector<Slot> slots_;
    std::vector<std::pair<Key, uint32_t>> counts_;
    std::vector<Key> unreserved_; ///< integ13: reserved keys dropped over the limit, this list
    uint32_t context_ = 0;
    bool started_ = false;
    uint64_t frame_ = 0;
    int last_index_ = -1;
    Group last_group_ = Group::Extra;
    uint64_t added_ = 0, dropped_ = 0, resets_ = 0, forgotten_ = 0;
};

} // namespace FrameXr::LayerSlots
