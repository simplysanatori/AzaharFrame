// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <cstdint>
#include "frame_xr/input/input_frame.h"
#include "frame_xr/input/types.h"

// AzaharFrame padsui
// The mouse in VR, pure logic (no evdev, OpenXR or settings): the source reads the mouse's
// running totals (MouseRaw), MouseLogic turns them into one frame of input (MouseFrame: the
// motion, the inputs held for the game, Open Menu, the mode-switch chord) and, in Pointer mode,
// MousePointer (mouse_pointer.h) into a ray that stands in for a controller's.
//
// Two modes (the mouse's mapping, Edit Controls' Mouse Mode):
//   Bottom Screen  the mouse moves a cursor on the bottom screen (relative, clamped to it); the
//                  touch input (Left Click by default) touches there, held it drags; with
//                  Recentre at Edge, a drag that runs into an edge lifts and touches again at
//                  the middle, so a turn or an aim can go on (touchpad_touch.h).
//   Pointer        the mouse turns a ray about you (yaw and pitch, fixed in the room) that
//                  works the menu, the screens and the grab bars as a controller's beam does:
//                  the touch input is its trigger. Picking up the right controller hands it
//                  back; moving the mouse on purpose takes it again.
// Left and Right Click held together for 3 s switch the mode (pressed within 80 ms of each other:
// both are let go for the game at once). The other inputs are the 3DS
// buttons of the mapping, and Open Menu (Middle Click by default) opens and closes the menu.
namespace FrameXr::Input {

/// The inputs a mouse gives (a mouse mapping's values).
enum class MouseInput : int {
    LeftClick,
    RightClick,
    MiddleClick,
    Back,
    Forward,
    WheelUp,
    WheelDown,
    Count,
};
constexpr int kNumMouseInputs = static_cast<int>(MouseInput::Count);
/// "Left Click", "Right Click", "Middle Click", "Back", "Forward", "Wheel Up", "Wheel Down"; ""
/// for anything else.
const char* MouseInputName(int input);

/// The buttons as the kernel has them: left, right, middle, side (Back), extra (Forward).
constexpr int kNumMouseButtons = 5;

/// A source's mouse as running totals since it started, so each reader takes its own deltas.
struct MouseRaw {
    bool present = false; ///< a mouse is connected
    int64_t x = 0;        ///< counts, right positive
    int64_t y = 0;        ///< counts, down positive
    int64_t wheel_up = 0; ///< notches
    int64_t wheel_down = 0;
    std::array<bool, kNumMouseButtons> buttons{};
};

enum class MouseMode : uint8_t { BottomScreen = 0, Pointer = 1 };

/// Cursor Speed: five steps, the middle one Normal. It scales the mouse's counts in both modes
/// (and a relative cursor's motion in TouchpadTouch).
constexpr int kNumCursorSpeeds = 5;
constexpr int kCursorSpeedNormal = 2;
/// How much faster than Normal: 0.5, 0.7, 1, 1.4, 2.
float CursorSpeedScale(int speed);
/// Bottom Screen: 3DS pixels per count (0.3 at Normal).
float MouseScreenGain(int speed);
/// Pointer: degrees per count (Frametop's 0.03 at Normal).
float MousePointerGain(int speed);
/// Which hand's slot of the InputFrame the Pointer's ray takes: the right one when its
/// controller is off or asleep, else the left one when that is, else the right one (shared with
/// its controller: the one used last has it). A controller in the other hand keeps its own ray.
int MousePointerHand(const InputFrame& in);

/// One display frame of the mouse.
struct MouseFrame {
    bool present = false;
    Vec2f motion; ///< counts since the last frame (right, down)
    /// The inputs held for the game (and the touch, and Open Menu): after the chord and after a
    /// release (what is held then stays away until let go); a wheel notch is a short press.
    std::array<bool, kNumMouseInputs> held{};
    bool open_menu = false; ///< the Open Menu input went down
    /// Left and Right Click held together: how far (0..1) the hold that switches the mode is.
    float chord = 0.0f;
    bool switch_mode = false; ///< the hold completed this frame
    bool used = false;        ///< moved or pressed this frame
    bool disabled = false;    ///< a mouse is there, with Use This Device off
    /// From the mouse's mapping (the hub fills them in).
    MouseMode mode = MouseMode::BottomScreen;
    int speed = kCursorSpeedNormal;
    bool recentre = true;
    int touch_input = static_cast<int>(MouseInput::LeftClick); ///< -1: none
    bool Touch() const {
        return touch_input >= 0 && touch_input < kNumMouseInputs &&
               held[static_cast<std::size_t>(touch_input)];
    }
};

class MouseLogic {
public:
    /// Left and Right Click go to the game at once; the other one going down within this long
    /// is the chord: both are let go for the game at that moment (it may have seen a tap shorter
    /// than this) and stay away until let go.
    static constexpr int64_t kChordWindowNs = 80'000'000;
    /// The chord held this long switches the mode.
    static constexpr int64_t kChordHoldNs = 3'000'000'000;
    /// A wheel notch presses for kNotchOnNs, then lets go for kNotchOffNs before the next one.
    static constexpr int64_t kNotchOnNs = 50'000'000;
    static constexpr int64_t kNotchOffNs = 40'000'000;
    static constexpr int kMaxNotches = 4;

    /// `open_menu_input` is the mapping's Open Menu input (-1 none).
    MouseFrame Update(const MouseRaw& raw, int open_menu_input, int64_t now_ns);
    /// Everything held now stays away until let go (the menu closed, a new game, a button just
    /// learnt in Edit Controls).
    void Release();
    /// One input (a MouseInput) stays away until let go.
    void Swallow(int input);

private:
    struct Notches {
        int64_t queued = 0;
        bool on = false;
        int64_t until = 0;
    };
    bool Step(Notches& n, int64_t added, int64_t now);

    bool seen_ = false;
    MouseRaw last_;
    std::array<bool, kNumMouseButtons> swallowed_{};
    std::array<int64_t, 2> down_since_{}; ///< Left, Right: when the press began, 0 none
    bool chord_ = false;
    bool chord_fired_ = false;
    int64_t chord_since_ = 0;
    Notches up_;
    Notches down_;
    bool open_menu_prev_ = false;
    bool release_wheel_ = false;
};

/// The mouse's ray in place of hand `hand` of `in`: its aim, the touch input as the trigger, and
/// (`stick_y`, when not 0) the wheel as the stick, for the menu's lists. The controller's other
/// inputs stay. Marks the hand as the mouse's.
void ApplyMousePointer(InputFrame& in, int hand, const Posef& aim, bool trigger, float stick_y);

} // namespace FrameXr::Input
