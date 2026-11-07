# Azahar for Frame controller mapping (input)

Source of truth for docs/CONTROLS.md. CitraVR's mapping is kept, with the Steam Frame's real D-pad
as the 3DS D-pad, with one change: L and R are on the bumpers, and the grips only grab the
screens (CitraVR's trigger drag moved to the grips, so the triggers are ZL and ZR everywhere
except on the bottom screen). Every row below is implemented in `mapper.cpp` and
`default_bindings.h`; the tests in `src/tests/frame_xr/` check them.

The Steam Frame controllers are a gamepad split in two. Right controller: A, B, X, Y, Menu, stick,
trigger, grip, bumper. Left controller: D-pad, View, stick, trigger, grip, bumper. The left
`/input/system` button is never bound (it belongs to SteamVR's dashboard).

## Buttons and sticks (while no pointer gesture is in progress)

| Frame control | 3DS | Notes |
| --- | --- | --- |
| A (bottom), B (right), X (left), Y (top) | B, A, Y, X | By position, as on the 3DS (A right, B bottom, X top, Y left). The setting `vr_face_buttons` ("Face Buttons", `MapperConfig::face_buttons_by_position`) 1 gives by label: A, B, X, Y. See below. |
| Left bumper | L | |
| Right bumper | R | |
| Left grip, right grip (squeeze) | (none) | Grab a screen, see below. Pressed above 0.60, released below 0.40. |
| Left trigger | ZL | Always, except on the visible bottom screen (touch), see below. Pressed above 0.55, released below 0.40. |
| Right trigger | ZR | Same. |
| Left stick | Circle Pad | Radial deadzone 0.12, rest rescaled. |
| Right stick | C-Stick | Same. |
| D-pad up / down / left / right | D-Pad up / down / left / right | The Frame's real D-pad (replaces CitraVR's thumbrest-touch plus stick trick). |
| Menu | Start | A tap: Start, sent as Menu is let go (for 100 ms). Held 1 s: the in-game menu, see below; the game never sees that hold. |
| View | Select | Hold 1.5 s instead stops the game, see below. |
| Left stick click, tap | (none) | Hides or shows the screen that is not in focus (`toggle_other_screen`, on release). |
| Left stick click, held 1 s | Home | The pad's `StickClickL`, bound to Home, is pulsed for 100 ms when the second is up (`stick_click_hold_ns`, `start_pulse_ns`). The game sees nothing before. |
| Right stick click, tap | (none) | Swaps the screens (`swap_screens`, on release). |
| Right stick click, held 1 s | (none) | Resets the screen positions (`reset_panels`, once per hold). |

A stick click is a `ClickGesture` (`click_gesture.h`): a tap acts when it is let go, a hold acts once at
1 s and its release does nothing. A click that starts while its stick is pushed past half way
(`reset_click_max_deflection`) is ignored whole, tap and hold, so free camera cannot swap, hide or reset
by accident. The right click is still reported to the pad as `StickClickR` (unbound by default).

| Left bumper + right bumper + View | (none) | Only in games that hide the bottom screen (Hide Bottom Screen, immersive mode): shows or hides it. That View press never reaches the game. Everywhere else the game sees L, R and Select. |

Select follows View at once, so a tap is a normal Select. While the stop hold is running the game
sees Select held; when the 1.5 s are up the game sees Select released and the game stops.

### Face buttons

The Frame's face buttons are labelled and placed like an Xbox controller's (A bottom, B right, X
left, Y top). The 3DS has A right, B bottom, X top, Y left. The default (`vr_face_buttons` 0,
"By Position (Nintendo)") maps by position:

| Frame button | Position | 3DS |
| --- | --- | --- |
| A | bottom | B |
| B | right | A |
| X | left | Y |
| Y | top | X |

"By Label (Xbox)" (`vr_face_buttons` 1) maps A to A, B to B, X to X, Y to Y. The swap is done by the
mapper, so the pad's `Control::A`, `B`, `X`, `Y` (named "Frame A" ... in the Controls dialog) are
the controls in the positions of the 3DS A, B, X and Y, and `default_bindings.h` binds them to the
3DS A, B, X and Y as before. The setting is read every frame (`InputBridge::Process`) and is
global. The game picker and the other places that read the raw `InputFrame` (`controls.cpp`:
A = select, B = previous page) are not game input and do not swap.

## Motion (gyroscope and accelerometer)

`motion.h`. The controller chosen by `vr_motion_controls` (0 off, 1 right, 2 left; default right)
drives the 3DS's sensors through the `frame_xr` motion device (`engine:frame_xr`, shared state in
`input_common/frame_xr.h`), written once per display frame by the `InputBridge` and read by the HID
service. VR start-up replaces the profile's default motion device (`motion_emu`, the mouse) with
it; any other device the user picked is kept.

The 3DS axes are those of Azahar's other motion devices (`motion_emu.cpp`; the Android sensors in
`ndk_motion.cpp`): right handed, x toward the 3DS's left, y out of the screen, z toward the top of
the 3DS. The accelerometer is the direction of gravity in these axes, in g (a 3DS lying flat,
screen up, reads (0, -1, 0)); the gyroscope is the angular rate about them in degrees per second.

The controller is held as a window: its aim ray (-Z) goes into the screen. Controller (OpenXR aim
pose) to 3DS axes: x3 = -x, y3 = z, z3 = y. So the ray pointing level with the controller upright
reads gravity (0, 0, -1) (a 3DS held up facing you); the ray pointing down is a 3DS lying flat
(0, -1, 0). The gyroscope is the rotation between two consecutive aim orientations, in the
controller's own axes, divided by the time between their display times, then converted the same
way. Rates under 1.5 deg/s read as zero, they are capped at 2000 deg/s, and a gap of more than
100 ms between two frames restarts the tracking. The base space is assumed to be gravity aligned
(+Y up), as OpenXR's LOCAL space is. Hand movement through the room is not sent (no linear
acceleration).

## Pointing at the screens

The pointer is the controller's aim ray (the pose's -Z axis). The hand that pulled its trigger last
is the pointing hand and owns the hover dot. Only a screen that is shown can be hit (a hidden
bottom screen, or the top screen in immersive mode, is skipped); the nearer of the two wins.

**Trigger.** What a trigger does is decided when it is pulled:

| Trigger pulled while the ray hits | Result | Until release |
| --- | --- | --- |
| the bottom screen (nearest) | Touch: a stylus tap at the pixel under the ray (320x240). Moving the ray drags the touch. Leaving the screen clamps the touch to the edge and keeps it down. | The trigger is not ZL/ZR. |
| the top screen, or nothing | ZL (left) or ZR (right). Sweeping the ray over a screen afterwards changes nothing. | Held as ZL/ZR. |

- Only one touch at a time. A second trigger pulled on the bottom screen while the first is busy
  does nothing (and is not ZL/ZR either).
- The hover dot shows only over the bottom screen, and on the grabbed screen during a drag.
- The dot is 0.01 m + 0.003 m per metre of distance wide (CitraVR's scale rule).

**Grip.** A grip squeezed while the ray hits a screen grabs it (decided at the squeeze; a grip
that was already down when the ray reached the screen grabs nothing). The grip does not reach the
game.

| Grip squeezed while the ray hits | Result | Until release |
| --- | --- | --- |
| the top screen | Drag the screen. It moves on the sphere around you (same distance, always turned to face you). The point you grabbed stays under the ray, so the screen does not jump. | Dragging. |
| the bottom screen (shown) | The same, grabbed by the bottom screen: both screens move together by the amount the grabbed point moves (the bottom screen follows the top one rigidly). | Dragging. |
| nothing | Nothing. | |

- One drag at a time. A trigger (ZL/ZR or touch) works during a drag.
- While dragging, that hand's stick Y moves the screens nearer or further (full deflection is
  3 m/s, away = stick up, never nearer than 1 m, never further than 10 m, ignored below 0.5
  deflection). That hand's stick does not reach the game while the drag lasts; the other hand's
  does.
- Horizontal lock is on by default as in CitraVR: dragging never moves the screens sideways (the
  target's x is zeroed). `MapperConfig::horizontal_lock`.
- The top screen goes no nearer than 1 m (`kNearestTopZ` in `controls.cpp`) and no further than
  10 m. The bottom screen follows it: 0.75 m lower, 0.5 m nearer, tilted back 45 degrees.
- A drag is refused (the screens stay) when the target is nearer than 1 m.

**Bottom screen toggle.** In games that hide the bottom screen on their own (the per-game Hide
Bottom Screen setting, immersive mode), Left Bumper + Right Bumper held and View pressed shows or
hides it (IMMERSIVE.md). The mapper reports `toggle_bottom` once, keeps that View press away from
the game until it is released, and does not start the stop-hold timer for it. View held first and
bumpers added later is plain Select. With the toggle disabled (every other game) the game sees L, R
and Select as pressed.

**Screen focus** (`screen_view.h`, used by `controls.cpp`; the mapper only reports the clicks). The
placement has a main slot (where the top screen starts) and a secondary slot (where the bottom screen
starts). With the bottom screen in focus, `ScreenView::Layout` gives the mapper the bottom screen in the
main slot (same height, 4:3) and the top screen in the secondary slot (that slot's height, the top
screen's aspect), so touch (`panels.bottom`, 320x240 pixels from u and v), ZL/ZR (`panels.top`) and the
dot follow the new places without any change in the mapper. The mapper's drag target is "its" top
screen's; `ScreenView::MainDragTarget` turns it into the same movement of the main slot. The bumper
chord's `toggle_bottom`, the left click and the swap go through `ScreenView`, which keeps one "bottom
screen shown" flag with Immersive in games it manages and a flag of its own otherwise.

**VR / flat chord.** Left Bumper + Right Bumper + View + Menu, in any order once both bumpers are
held, switches between VR and flat (`toggle_vr`, once per chord). So that it can share View with
the bottom screen toggle, View pressed with both bumpers held is held back from the game for up to
`chord_grace_ns` (150 ms): Menu down by then (or already down) makes it the switch; otherwise it
becomes the bottom screen toggle (when enabled) or Select (late; a tap shorter than the wait is
sent as a 100 ms pulse). Menu with both bumpers is never a problem: Start is only sent when Menu
is let go. Once the switch fires, View, Menu and both bumpers stay away from the game until each
is let go, and Menu neither sends Start nor opens the in-game menu. The game sees L and R from the
bumper press to the switch (holding them back would delay every L and R). Outside a game (the home
panel, the in-game menu, flat without a game) `VrUi::ModeChord` finds the four itself.

After any switch, `VrUi` arms `ChordLatch` (`chord_latch.h`) in `RequestToggleVr` and again when
the new mode begins (`Create`, `DestroyXr`): the four buttons read as up for everything (mapper,
`ModeChord`, panels) until all four were seen up, no earlier than 1 s after the first frame of the
new mode. Waiting for the release alone was not enough: in flat, SDL opens Steam Input's gamepad
anew after the session ends, its first read is all up, and the still-held chord then came in as a
fresh press and switched back to VR 0.3 s later.

**Fast forward.** View pressed first (with no bumper down, as for Quick Save), then the Right
Trigger: `toggle_fast_forward`, once per pull and at most once a second (`quick_cooldown_ns`).
That pull is neither ZR nor a touch until the trigger is let go, and View stays away from the game
(no Select, no stop hold) until it is let go. The Left Trigger with View is plain ZL. The Qt side
keeps the state: Azahar's turbo speed (`SetTurboEnabled`) and the "Fast Forward" label
(`FrameXr::SetFastForwardShown`, a row of the toast atlas in `hud.cpp`), off again when the game
stops, the in-game menu opens (flat: Menu held pauses) or the mode switches.

## Stopping the game

Hold View for 1.5 s: the mapper reports `stop_game` once and a `stop_hold_progress` of 0..1 while
holding (logged, no bar is drawn). The frame loop calls ShutdownGame and shows the home panel again.
(CitraVR had an Exit button in its menu ribbon.)

## Flat (no session)

The VR / flat switch ends the session (`xrRequestExitSession`, then `xrDestroySession` at
EXITING); the instance and the Vulkan device stay, so a new session later uses the same device.
In flat the XR thread runs `VrUi::FlatFrame` about 120 times a second: citra_qt reads Steam Input's
gamepad with SDL (`citra_qt/frame_mode.cpp`) into the same InputFrame and the same mapper, with
both screens marked not visible (no touch, no grab; the triggers are ZL and ZR) and no bottom
screen toggle. Menu held 1 s pauses or continues (Emulation > Pause), View held 1.5 s stops the
game, the chord goes back to VR. Buttons held when the mode changes (the chord itself) stay away
from the game until let go. The input profile is the Frame one in both modes; the launcher's
"Flat Gamepad" profile (SDL) is only for a run without XR.

## The home panel (no game running)

Without a game, Azahar's own Qt window (game list, menus, dialogs) is mirrored on a panel 1.6 m
wide, 2 m ahead (`frame_xr/qt_mirror`, `citra_qt/frame_mirror.cpp`). The controllers drive it like
a mouse; double-click a game to start it. `AZAHARFRAME_VR_PICKER=1` shows the old picker instead.
The bindings are SteamOS-like and listed in one place at the top of `qt_mirror/mirror_panel.cpp`.

| Frame control | On the panel |
| --- | --- |
| Aim ray | Mouse pointer (hover opens submenus, shows tooltips). The hand that pulled its trigger or grip on the panel last points; if it points elsewhere, the other hand's ray is used. |
| Trigger of the pointing hand, or A | Left button (press above 0.55, release below 0.40). Hold to drag. |
| Right stick click, short | Right click, sent when let go: context menus (a game's Properties has its per-game settings). |
| Right stick click, held 1 s | The panel back where it was placed (in front). No right click then. |
| B | Back (Escape): closes a menu or a dialog. |
| Grip on the panel | Move the panel around you (it keeps its distance and turns to face you). |
| Stick Y (either) | Mouse wheel, up to 12 notches a second; nothing below 0.2. |
| Y | Shows or hides the headset keyboard (below). |
| X, bumpers | Nothing. |

A press keeps its point while the ray moves less than 1.2 cm, and a second press within 450 ms
of the release lands on the same point, so double clicks work with a shaking hand. Stick clicks
are ignored while that stick is pushed past half way.

## The headset keyboard

Moonlight XR's Compact keyboard (same author), US QWERTY, under the window (`frame_xr/keyboard`,
`citra_qt/frame_mirror.cpp`). It shows only with the window (home panel or in-game menu): 0.55 of
the window's width, its top edge 0.035 widths under the window's bottom edge and 0.02 widths
nearer. Its place is kept against the window's bottom edge in window widths, so it follows the
window and sits the same under the in-game menu.

| Frame control | On the keyboard |
| --- | --- |
| Y | Show or hide (Moonlight XR's default for `app:keyboard`; Y is free on the window). Opens on the small letters. |
| Trigger on a key (either hand), or A | Press the key under that ray (A: the pointing ray). One key per press, no repeat. |
| Trigger or grip on the bar (the pill under it) | Move it around you (keeps its distance, turns to face you); that hand's stick Y pushes it away or pulls it nearer, 1 m/s at full tilt, 0.4 to 4 m. |
| Trigger on nothing (not the keyboard, its bar or the window) | Hide. |
| Shift, Ctrl, Alt, Win | Tap: for the next key. Held 500 ms: locked until tapped again. A locked Shift keeps the capitals. |

- A key goes to Qt as a press and a release (`qt_handleKeyEvent`) with its text, to the open
  popup, else the modal dialog, else the topmost dialog, else the main window, whose focused
  widget gets it. Ctrl, Alt and Win go as modifiers; a capital or a shifted character also
  carries Shift.
- A left click let go on an editable line, spin box, combo box or text box that then has the
  keys opens the keyboard. It closes when no such widget has the keys any more (checked after
  each key and every 250 ms), unless Y opened it.
- The window does not see the hands on the keyboard or its bar (unless they hold a press that
  began on the window), nor presses that began there, nor A while it presses a key.
- Keys are drawn on the CPU (1120 x 448) and uploaded when what they show changes; the ring on
  the key under the ray (it swells while held) and the bar's pill are quads of their own. The
  labels are Noto Sans Regular from `/usr/share/fonts/noto` (or `AZAHARFRAME_KEYBOARD_FONT`, or
  `art/NotoSans-Regular.ttf`); without it the picker's font.

## The in-game menu

Menu held 1 s in a game pauses the emulation (as Emulation > Pause) and shows the same panel in
front of the game's screens, which stay visible behind a dark veil. The panel goes 1.2 m ahead
(nearer if the screens are nearer), as wide as the home panel looks. It is driven as above, so
Emulation > Configure, a game's Properties and the Emulation menu all work; settings that apply
live (resolution, filters) apply at once, the VR ones at the next game start.

- Hidden again, and the game resumed, by: Menu held 1 s again, B with no menu or dialog open, or
  Emulation > Continue. View held 1.5 s still stops the game; Emulation > Stop does too.
- The game never sees the Menu hold (`menu_toggle` from the mapper; no Start), and while the
  panel shows the game sees no controller input. Buttons still held as the menu closes stay away
  from the game until they are let go.
- Immersive modes 2 and 3 pause while it shows (normal screens), as with the bottom screen shown.

## Not yet

- The headset keyboard's AZERTY and QWERTZ maps and its Full (tenkeyless) layout, which
  Moonlight XR has.
- When the ray leaves the panel, the last hover stays highlighted (Qt has no public leave event).

## Where CitraVR's behaviour is not reproduced

- The menu ribbon (Select, Start, Exit, performance stats, settings, positional menu) that the
  left Menu button opened. Select, Start and Exit are now buttons; settings are in the in-game
  menu (Azahar's window, Menu held 1 s); stats are the XR core's.
- The positional menu paused the emulation while screens were arranged and could also drag the
  bottom screen from the ribbon. Here a grip drags either screen (the pair moves together), and it
  never pauses the game.
- CitraVR dragged the top screen with the trigger, and its L and R were the grips. Here the grips
  drag and the triggers are ZL/ZR, so that a trigger pulled while aiming at the top screen (the
  usual case in single-screen games such as Ocarina of Time 3D with Ocarina ReFramed) stays ZL/ZR.
  L and R moved to the bumpers.
- CitraVR forwarded the sticks to the game even while dragging; here the dragging hand's stick is
  held back, because it drives the depth.
- CitraVR switched the pointing hand with either trigger and used one shared "cursor visible"
  flag that also blocked both ZL and ZR. Here each hand decides on its own.
- The thumbrest-touch D-pad (Quest) and stick-click stall markers (a debugging aid) are gone.
- The horizontal-lock toggle has no menu button yet; it is a mapper option.
- No haptics (nothing in the mapping needs them).
- CitraVR's left Menu button toggled the bottom screen in immersive mode; the left bumper is L
  now, so the toggle is the bumper chord with View (IMMERSIVE.md).
- A profile saved by an earlier build has L and R on the grips; at VR start they move to the
  bumpers (`MigrateGripBindings`, only when they are still the old Frame defaults).

## Default input profile (Qt Controls dialog)

`InputCommon::FrameXr::GetDefaultProfile()` gives these ParamPackage strings (control ids are
`FrameXr::Input::Control` / `AnalogControl` in `controls.h`):

| 3DS | Param string |
| --- | --- |
| A, B, X, Y | `engine:frame_xr,button:0` .. `button:3` |
| Up, Down, Left, Right | `engine:frame_xr,button:4` .. `button:7` (D-pad up, down, left, right) |
| L, R | `engine:frame_xr,button:12` (left bumper), `button:13` (right bumper) |
| ZL, ZR | `engine:frame_xr,button:10` (left trigger), `button:11` (right trigger) |
| Start, Select | `engine:frame_xr,button:14` (Menu), `button:15` (View) |
| Home | `engine:frame_xr,button:16` (left stick click) |
| Circle Pad | `engine:frame_xr,analog:0` (left stick) |
| C-Stick | `engine:frame_xr,analog:1` (right stick) |
| Debug, Gpio14, Power | empty |
| Motion | `engine:frame_xr` (set at VR start-up in place of the default `motion_emu`, see Motion) |

The Controls dialog shows these as "Frame A", "Frame Left Grip", "Frame Menu", and so on, and its
"press a button" capture works for the Frame controls (pollers). Unused engine inputs: grips
(`button:8`, `button:9`, the screen grab also fires) and right stick click (`button:17`, the reset
gesture also fires).
