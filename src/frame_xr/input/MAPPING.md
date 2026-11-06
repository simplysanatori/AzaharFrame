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
| A, B, X, Y | A, B, X, Y | Same names. |
| Left bumper | L | |
| Right bumper | R | |
| Left grip, right grip (squeeze) | (none) | Grab a screen, see below. Pressed above 0.60, released below 0.40. |
| Left trigger | ZL | Always, except on the visible bottom screen (touch), see below. Pressed above 0.55, released below 0.40. |
| Right trigger | ZR | Same. |
| Left stick | Circle Pad | Radial deadzone 0.12, rest rescaled. |
| Right stick | C-Stick | Same. |
| D-pad up / down / left / right | D-Pad up / down / left / right | The Frame's real D-pad (replaces CitraVR's thumbrest-touch plus stick trick). |
| Menu | Start | |
| View | Select | Hold 1.5 s instead stops the game, see below. |
| Left stick click | Home | |
| Right stick click | (none) | Resets the screen positions (once per click). Ignored while the stick is pushed past half way, so free camera cannot reset by accident. |
| Left bumper + right bumper + View | (none) | Only in games that hide the bottom screen (Hide Bottom Screen, immersive mode): shows or hides it. That View press never reaches the game. Everywhere else the game sees L, R and Select. |

Select follows View at once, so a tap is a normal Select. While the stop hold is running the game
sees Select held; when the 1.5 s are up the game sees Select released and the game stops.

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

## Stopping the game

Hold View for 1.5 s: the mapper reports `stop_game` once and a `stop_hold_progress` of 0..1 while
holding (logged, no bar is drawn). The frame loop calls ShutdownGame and shows the picker.
(CitraVR had an Exit button in its menu ribbon.)

## Where CitraVR's behaviour is not reproduced

- The menu ribbon (Select, Start, Exit, performance stats, settings, positional menu) that the
  left Menu button opened. Select, Start and Exit are now buttons; settings are in the Qt window;
  stats are the XR core's.
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

The Controls dialog shows these as "Frame A", "Frame Left Grip", "Frame Menu", and so on, and its
"press a button" capture works for the Frame controls (pollers). Unused engine inputs: grips
(`button:8`, `button:9`, the screen grab also fires) and right stick click (`button:17`, the reset
gesture also fires).
