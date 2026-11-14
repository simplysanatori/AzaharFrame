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

    /// The list for this frame: every slot, absent ones collapsed. At most `max_slots` (the
    /// runtime's limit less what else the frame carries): the slots absent longest are dropped
    /// first (for good: a key seen again gets a new slot).
    template <typename Collapse>
    void End(std::size_t max_slots, std::vector<Out>& out, Collapse&& collapse) {
        while (slots_.size() > max_slots) {
            std::size_t victim = slots_.size();
            uint64_t oldest = frame_;
            for (std::size_t i = 0; i < slots_.size(); i++) {
                if (!slots_[i].present && slots_[i].seen < oldest) {
                    oldest = slots_[i].seen;
                    victim = i;
                }
            }
            if (victim == slots_.size()) {
                break; // only present quads left: the caller's limit cuts the rest
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

private:
    struct Slot {
        Key key;
        Quad quad;
        bool present = false;
        uint64_t seen = 0;
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
    uint32_t context_ = 0;
    bool started_ = false;
    uint64_t frame_ = 0;
    int last_index_ = -1;
    Group last_group_ = Group::Extra;
    uint64_t added_ = 0, dropped_ = 0, resets_ = 0;
};

} // namespace FrameXr::LayerSlots
