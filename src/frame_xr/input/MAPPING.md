# Azahar for Frame controller mapping (input)

Source of truth for docs/CONTROLS.md. CitraVR's mapping is kept, with the Steam Frame's real D-pad
as the 3DS D-pad. Every row below is implemented in `mapper.cpp` and
`default_bindings.h`; the tests in `src/tests/frame_xr/` check them.

The Steam Frame controllers are a gamepad split in two. Right controller: A, B, X, Y, Menu, stick,
trigger, grip, bumper. Left controller: D-pad, View, stick, trigger, grip, bumper. The left
`/input/system` button is never bound (it belongs to SteamVR's dashboard).

## Buttons and sticks (while no pointer gesture is in progress)

| Frame control | 3DS | Notes |
| --- | --- | --- |
| A, B, X, Y | A, B, X, Y | Same names. |
| Left grip (squeeze) | L | Pressed above 0.60, released below 0.40. |
| Right grip (squeeze) | R | Same. |
| Left trigger | ZL | Only when not pointing at a screen, see below. Pressed above 0.55, released below 0.40. |
| Right trigger | ZR | Same. |
| Left stick | Circle Pad | Radial deadzone 0.12, rest rescaled. |
| Right stick | C-Stick | Same. |
| D-pad up / down / left / right | D-Pad up / down / left / right | The Frame's real D-pad (replaces CitraVR's thumbrest-touch plus stick trick). |
| Menu | Start | |
| View | Select | Hold 1.5 s instead stops the game, see below. |
| Left stick click | Home | |
| Right stick click | (none) | Resets the screen positions (once per click). |
| Left bumper, right bumper | (none) | Unbound in v1. They are frame_xr inputs, so they can be bound in the Controls dialog. |

Select follows View at once, so a tap is a normal Select. While the stop hold is running the game
sees Select held; when the 1.5 s are up the game sees Select released and the game stops.

## Pointing at the screens

The pointer is the controller's aim ray (the pose's -Z axis). The hand that pulled its trigger last
is the pointing hand and owns the cursor; if it points at nothing, the other hand's cursor shows.
What a trigger does is decided when it is pulled, from what that hand's ray hits at that moment
(the nearer of the two screens wins):

| Trigger pulled while the ray hits | Result | Until release |
| --- | --- | --- |
| the bottom screen | Touch: a stylus tap at the pixel under the ray (320x240). Moving the ray drags the touch. Leaving the screen clamps the touch to the edge and keeps it down. | The trigger is not ZL/ZR. |
| the top screen | Drag the top screen. It moves on the sphere around you (same distance, always turned to face you). The point you grabbed stays under the ray, so the screen does not jump. | The trigger is not ZL/ZR. |
| neither screen | ZL (left) or ZR (right). Sweeping the ray over a screen afterwards changes nothing. | Held as ZL/ZR. |

- Only one touch and one top-screen drag at a time. A second trigger pulled on a screen while the
  first is busy does nothing (and is not ZL/ZR either).
- While dragging the top screen, that hand's stick Y moves it nearer or further (full deflection is
  3 m/s, away = stick up, never nearer than 1 m, never further than 10 m, ignored
  below 0.5 deflection). That hand's stick does not reach the game while the drag lasts.
- Horizontal lock is on by default as in CitraVR: dragging never moves the top screen sideways
  (the target's x is zeroed). `MapperConfig::horizontal_lock`.
- The top screen goes no nearer than 1 m (`kNearestTopZ` in `controls.cpp`) and no further than
  10 m. The bottom screen follows it: 0.75 m lower, 0.5 m nearer, tilted back 45 degrees.
- The cursor is a dot at the hit point, 0.01 m + 0.003 m per metre of distance wide (CitraVR's
  scale rule). It shows only while a ray is on a screen or a touch/drag is in progress.
- A drag is refused (the screen stays) when the target is nearer than 1 m (CitraVR: 0.5 m; the
  bottom screen sits 0.5 m nearer than the top one).

## Stopping the game

Hold View for 1.5 s: the mapper reports `stop_game` once and a `stop_hold_progress` of 0..1 while
holding (logged, no bar is drawn). The frame loop calls ShutdownGame and shows the home panel again.
(CitraVR had an Exit button in its menu ribbon.)

## The home panel (no game running)

Without a game, Azahar's own Qt window (game list, menus, dialogs) is mirrored on a panel 1.6 m
wide, 2 m ahead (`frame_xr/qt_mirror`, `citra_qt/frame_mirror.cpp`). The controllers drive it like
a mouse; double-click a game to start it. `AZAHARFRAME_VR_PICKER=1` shows the old picker instead.

| Frame control | On the home panel |
| --- | --- |
| Aim ray | Mouse pointer (hover opens submenus, shows tooltips). The hand that pulled its trigger or grip on the panel last points; if it points elsewhere, the other hand's ray is used. |
| Trigger, or A | Left button (press above 0.55, release below 0.40). |
| Grip, or X | Right button: context menus (game Properties for per-game settings). |
| B | Escape: closes a menu or a dialog. |
| Stick Y (either) | Mouse wheel, up to 12 notches a second; nothing below 0.2. |
| Bumper on the panel | Drag the panel around you (it keeps its distance and turns to face you). |
| Right stick click | Panel back 2 m ahead. |

A press keeps its point while the ray moves less than 1.2 cm, and a second press within 450 ms
of the release lands on the same point, so double clicks work with a shaking hand.
There is no text entry yet (OpenXR has no system keyboard); text fields need the desktop.

## Where CitraVR's behaviour is not reproduced

- The menu ribbon (Select, Start, Exit, performance stats, settings, positional menu) that the
  left Menu button opened. Select, Start and Exit are now buttons; settings are in the Qt window;
  stats are the XR core's.
- The positional menu paused the emulation while screens were arranged and could also drag the
  bottom screen from the ribbon. Here only the top screen is dragged, and it never pauses the
  game. The bottom screen moves only with a reset (the XR core may let it follow the top screen).
- CitraVR forwarded the sticks to the game even while dragging; here the dragging hand's stick is
  held back, because it drives the depth.
- CitraVR switched the pointing hand with either trigger and used one shared "cursor visible"
  flag that also blocked both ZL and ZR. Here each hand decides on its own.
- The thumbrest-touch D-pad (Quest) and stick-click stall markers (a debugging aid) are gone.
- The horizontal-lock toggle has no menu button yet; it is a mapper option.
- No haptics (nothing in the mapping needs them).
- Bumpers are free in the default profile, but the left bumper also shows or hides the bottom
  screen in immersive mode (IMMERSIVE.md).

## Default input profile (Qt Controls dialog)

`InputCommon::FrameXr::GetDefaultProfile()` gives these ParamPackage strings (control ids are
`FrameXr::Input::Control` / `AnalogControl` in `controls.h`):

| 3DS | Param string |
| --- | --- |
| A, B, X, Y | `engine:frame_xr,button:0` .. `button:3` |
| Up, Down, Left, Right | `engine:frame_xr,button:4` .. `button:7` (D-pad up, down, left, right) |
| L, R | `engine:frame_xr,button:8` (left grip), `button:9` (right grip) |
| ZL, ZR | `engine:frame_xr,button:10` (left trigger), `button:11` (right trigger) |
| Start, Select | `engine:frame_xr,button:14` (Menu), `button:15` (View) |
| Home | `engine:frame_xr,button:16` (left stick click) |
| Circle Pad | `engine:frame_xr,analog:0` (left stick) |
| C-Stick | `engine:frame_xr,analog:1` (right stick) |
| Debug, Gpio14, Power | empty |

The Controls dialog shows these as "Frame A", "Frame Left Grip", "Frame Menu", and so on, and its
"press a button" capture works for the Frame controls (pollers). Unused engine inputs: bumpers
(`button:12`, `button:13`) and right stick click (`button:17`, the reset gesture also fires).
