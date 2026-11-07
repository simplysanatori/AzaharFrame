# Immersive mode

CitraVR's immersive mode on the Steam Frame. The game draws a wider view than the 3DS screen
shows, and in the Full modes its camera turns and moves with your head, so you stand inside the
game instead of looking at a screen. It works with very few games: CitraVR's view-matrix
heuristics are made for The Legend of Zelda: Ocarina of Time 3D and Majora's Mask 3D. Other games
often show broken geometry, a HUD in the wrong place, or nothing changes at all.

## Modes

Setting `vr_immersive_mode` (CitraVR's key, section `VR`), per application in the Qt window
(Graphics tab, VR group):

| Value | Name in the Qt window | Game | Top screen |
| --- | --- | --- | --- |
| 0 | Off | normal | the normal screens |
| 1 | Wide (3DoF) | draws 3x wider and higher (`gl_Position.xy /= 3`) | 0.75 m ahead, fixed in the room, 1.88 x 1.13 m for the 3DS screen shape. Its middle third (the game's own frame) has the angular size of the normal top screen |
| 2 | Full (6DoF): Ocarina of Time, Majora's Mask | 1.0x to 1.4x wider (see Placement); view matrix register 4 follows the head (rotation and position) | head-locked, 0.75 m ahead, both eyes on one quad, fitted into the view |
| 3 | Full (6DoF): Other Games | as 2, register found by CitraVR's heuristic (or `vr_si_mode_register_offset`); stereo from the eye indicator register | head-locked, each eye's quad centred on that eye, fitted into the view. The game is shown a 3D depth of about 0: the two eyes come from your head |

The other keys, also per application: `vr_immersive_positional_game_scaler` (Depth Factor
Multiplier: 0 = 1x, 1 = 10x, 2 = 100x; how far the camera moves with your head, times the 3D
slider / 10), `vr_si_mode_register_offset` (View Matrix Register, -1 = Auto, mode 3 only),
`vr_immersive_eye_indicator` (Eye Indicator Register, e.g. `87,2`, empty = Auto, mode 3 only).

While the setting is not 0, the internal resolution is two steps higher (CitraVR
`vr_main.cpp:402`), at most 8x (`AdjustResolution` in `video_core/vr_immersive.h`).
Modes 2 and 3 need hardware shaders (Graphics, Enable Hardware Shader; on by default).

## Controls

| Control | In immersive mode |
| --- | --- |
| Left Bumper + Right Bumper + View | Shows or hides the bottom screen (CitraVR's left Menu button; the left bumper is L here, so it is a chord). Hold both bumpers, then press View. Every game starts with it hidden. The game does not see that View press. In modes 2 and 3, showing it pauses immersive mode (normal screens, the game camera stops following the head) until you hide it again; in mode 1 it shows below the middle of the wide screen and nearer than it, so it is drawn over it. |
| Grip (either hand) | Holds the immersive screen: move the hand up or down and the screen follows (the point under the laser stays under it). The stick of that hand: up moves it farther, down nearer; right makes it bigger, left smaller (past half way, as in the normal placement). Let go and the placement is saved for this game. The grip, and that hand's stick, do not reach the game meanwhile. |
| Right Stick Click | Also re-centres: the direction you face and where your head is become the game camera's rest pose (modes 2, 3), and the wide screen moves in front of you (mode 1). SteamVR's own re-centre does the same. With a grip holding the immersive screen, it also goes back to its default placement (the saved one is forgotten). |
| Triggers | ZL / ZR (a trigger is a touch only on a shown bottom screen). |

While immersive mode is paused (modes 2 and 3 with the bottom screen shown or the in-game menu open),
the grips move the normal screens as usual.

## Hide Bottom Screen (without immersive mode)

Setting `vr_hide_bottom_screen` (section `VR`, per game in the Qt window: Properties, **Graphics**
tab, **VR** group, **Hide Bottom Screen**) draws only the top screen in the normal placement.
It is read when a game starts, and the game starts with the bottom screen hidden. The bumper chord
above brings it back (and hides it again); nothing else changes: the top screen can be grabbed
with a grip, the right stick click resets the placement, the triggers are ZL/ZR.

The two compose: with immersive mode on, hiding the bottom screen changes nothing (immersive mode
hides it already) and the chord behaves as described above. With the chord the bottom screen shows
for touch-only moments, in mode 1 and with the setting alone at its normal place.

## Screen Shape and Immersive Game FOV

Two more per-game settings, in the same **VR** group, both read when a game starts:

- `vr_top_aspect`, **Screen Shape**: 0 = 3DS (5:3, the 400x240 of the screen), 1 = Widescreen (16:9),
  2 = Ultrawide (21:9). For games and mods that render widescreen into the 400x240 top screen
  (Ocarina of Time 3D with the Single Screen Experience mod draws a squeezed 16:9 picture and
  expects the emulator to stretch it). The renderer is not touched; only the quad is wider. The
  normal top screen keeps its height (1 m) and its place, 2 m ahead, and gets wider; the bottom
  screen stays below it as before.
- `vr_immersive_game_fov`, **Immersive Game FOV**: the game camera's vertical angle in degrees,
  30 to 110, default 60 (Ocarina of Time's). It sizes the head-locked screen of modes 2 and 3. The
  environment variable `AZAHARFRAME_IMMERSIVE_FOV` still wins when it is set.

In the immersive modes the quad has to show the game's picture at the angle the game drew it, so
its aspect must be the aspect of the game's projection. The vertical half angle comes from the FOV
setting and the horizontal one from the shape: `tan(h / 2) = aspect x tan(v / 2)`. In modes 2 and 3
the quad is `2 x d x 1.4 x tan(v / 2)` high and that times the aspect wide (60 degrees with
Widescreen: 0.81 m high, 1.44 m wide, 110 degrees across). In mode 1 the width is the normal top
screen's angular size times 3, and the height is the width over the aspect. If the shape in
Azahar for Frame and the mod's own aspect option disagree, the picture is stretched, and in modes
2 and 3 the world drifts against your head turns.

## Placement

`frame_xr/immersive_layout.{h,cpp}` (unit tested in `src/tests/frame_xr/immersive_layout.cpp`), saved
per game and mode group in `immersive-placement.txt` in the user directory (`<title id> head|wide
<pitch> <distance> <size>`; written when a grip lets go, read when a game starts).

In modes 2 and 3 the world only stays put when a head turn moves the picture by the angle the game
camera turned. Each of the three things a grip changes keeps that:

- **Distance** scales the quad with it, so its angle does not change. In mode 2 it moves the flat
  picture's stereo depth; in mode 3 (each eye's quad on that eye) nothing visible changes.
- **Pitch.** The quad turns about the eye, and the same rotation is added to the head rotation the
  game camera gets (camera = head x pitch, `GameCameraRotation`), so the camera looks where the
  screen is: the world stays where it was and you see it through a lower window. In mode 2 the
  register's rotation is replaced by that rotation; in mode 3 it multiplies the game's own view
  matrix. Both take it as a plain rotation. Yaw would work the same way; the gesture keeps the
  normal placement's horizontal lock, so it is always 0.
- **Size.** First the periphery factor (`gl_Position.xy /= factor`) moves between 1.0 and CitraVR's
  1.4: the game's view widens with the quad, so the world stays locked and only more or less of the
  edges shows. Past those limits the quad is scaled against the game's view, and the head rotation
  given to the game is scaled by `gain` = game angle / screen angle (`ScaleRotation`), which keeps the
  picture moving with the head at the middle of the screen. The cost of a gain other than 1: a big
  head turn turns the game camera `gain` times as far (more or less of the game world than you
  turned), and the edges of a planar picture move a little against the head. The gain stays within
  0.6 to 1.8 (past that the screen keeps its size and the world drifts a little), and the size
  within 0.6 to 1.5 times the default.

The default fits the whole quad, its edges and so the game's HUD included, inside the part of the
view both eyes see, from the runtime's per-eye FOV (logged once: `xr: immersive: both eyes see ...`)
minus 2.5 degrees on every side, centred 5 degrees below the middle of that view. If the view has
room for more than 1.4, the factor stays 1.4. If the game's own frame does not fit (a wide game FOV
or a narrow view), the factor is 1.0 and the gain is above 1. The line `xr: immersive mode N on` logs
the factor, the screen's angles, its pitch and the gain.

Mode 1 is fixed in the room, so its pitch and distance are a plain move (the size scales with the
distance) and its size is free.

Numbers to tweak, all in `immersive_layout.h`: `kDefaultPitchDeg` (-5), `kDefaultDistance` (0.75 m),
`kFitMarginDeg` (2.5), `kFactorMax` (1.4), `kMinSize` / `kMaxSize`, the gesture rates.

## Layer order

Quad layers are composited in the order they are submitted, with no depth test, so a farther
screen submitted after a nearer one is drawn through it. `Immersive::Screens` reports which screen
is farther (`bottom_first`) and `app.cpp` submits that one first. A hidden screen is not submitted
at all.

## How it works

Three threads, and the rasterizer is only touched by the one that draws:

1. **XR thread** (`immersive.cpp`, a frame callback, every display frame): reads the setting and
   the located head pose, takes it relative to a reference (head position and heading when the
   game started or at the last re-centre), and builds CitraVR's `SetVRData` arguments as
   `vr_main.cpp` `UpdateImmersiveModeIfNeeded` did: the inverse of the camera rotation (the head's,
   with the screen's pitch and gain, see Placement), the inverse head position rotated into the
   camera frame times `10^multiplier x (3D slider / 10)`, factor 3.0 (mode 1) or 1.0 to 1.4. Not immersive (setting 0, no game, bottom screen shown in modes 2/3): CitraVR's "off"
   values (mode 1, factor 1, identity). It publishes them into a seqlock of atomics
   (`video_core/vr_immersive.h`, one writer).
2. **Emulation thread** (`GPU::VBlankCallback`, right after `SwapBuffers`, once per guest frame):
   `VideoCore::Immersive::ApplyPending` reads the newest snapshot and calls
   `RasterizerInterface::SetVRData` (again after a new rasterizer, one per game boot).
   `SetVRData` marks the PICA uniforms dirty so the patched view matrix is uploaded with the new
   pose even when the game did not touch its uniforms.
3. **XR thread, layers** (`app.cpp` asks `Immersive::Screens`): the top screen's space, per-eye
   pose and size, and whether the bottom screen is shown. Modes 2/3 use a quad in
   `XR_REFERENCE_SPACE_TYPE_VIEW`, so SteamVR keeps it locked to the head at display time.

CitraVR called `SetVRData` from its OpenXR thread while the emulation thread drew with the same
fields; that race is gone.

## Differences from CitraVR

- **No cylinder.** SteamVR has no `XR_KHR_composition_layer_cylinder`, so the top screen is a
  flat quad. Modes 2/3: a head-locked quad in VIEW space instead of CitraVR's 0.5 m cylinder
  re-posed from the head pose each frame. Mode 1: a flat quad fixed in the room instead of the
  wrap-around cylinder (a flat quad cannot wrap around you).
- **Size of the head-locked screen.** CitraVR's mode 2 is a cylinder 0.5 m around the head, 55 x
  1.4 = 77 degrees across and about 44 degrees high (the 5:3 shape), centred at eye level
  (`GameSurfaceLayer.cpp`). That is smaller than the game's 1.4x view (107 x 78 degrees for a camera
  60 degrees high), so its world moves with the head a little. Earlier builds here drew the quad at
  the full 107 x 78 degrees to keep the world still, which is more than the Frame shows: the top of
  the picture and the HUD were cut off, and the screen sat high. Here it is fitted into the view
  and placed lower, without losing the world lock (see Placement), and the grips move it.
  The Immersive Game FOV setting sets the game camera's angle, and `AZAHARFRAME_IMMERSIVE_FOV` overrides it.
- **Mode 3 eye offset** from the located eye poses (half your IPD), not a fixed 32.5 mm.
- **Reference pose.** CitraVR used Quest's LOCAL space, whose origin is the head at start. SteamVR's
  LOCAL origin can be anywhere, and the head position is multiplied by 5 or more, so the pose is
  taken relative to the head when the game starts (and at each re-centre).
- **Bottom screen** in mode 1 shows below the wide screen's middle, at 0.7 times its distance, with
  the normal bottom screen's angular size (CitraVR moved it to 1 m below the top screen). It is
  nearer than the wide screen and submitted after it, so it is never drawn through it.
- The `vr_immersive_mode > 90` register search mode (thumbrest touch steps `uoffset`) is not
  carried over; `uoffset` is always -1.
- The four `vr_*` settings are now `SwitchableSetting`s so they can be set per application in the
  Qt window (CitraVR's Android app only had the mode per game).
- `vr_immersive_mode_factor` defaults to 1 (`shader_uniforms.h`); it was 0 until CitraVR's
  Android frontend set it every frame, which made the vertex shader divide by zero elsewhere.

## Tuning without a rebuild

Environment variables, read when the XR session starts (logged as `xr: immersive: ...`):

| Variable | Default | Meaning |
| --- | --- | --- |
| `AZAHARFRAME_IMMERSIVE_FOV` | unset | the game camera's vertical angle in degrees (20..120); sets the head-locked screen size and wins over the Immersive Game FOV setting (default 60, 30..110) |
| `AZAHARFRAME_IMMERSIVE_DISTANCE` | 0.75 | the default distance in metres (0.3..3) for a game without a saved placement; sizes scale with it, so only stereo depth changes |
| `AZAHARFRAME_IMMERSIVE_LOCAL` | 0 | 1: place the head-locked screen in LOCAL space from the located head pose each frame, as CitraVR did (fallback if VIEW-space quads misbehave) |

## Trying it

Set the mode in the Qt window: Properties of a game, **Graphics** tab, **VR** group, **Immersive Mode**
(the same group is under **Emulation > Configure > Graphics** for all games). Start the game from the
picker. Without the window, the global config (`qt-config.ini` in the user directory) takes:

    [VR]
    vr_immersive_mode\default=false
    vr_immersive_mode=2

What to look for:

- Mode 2 in Ocarina of Time 3D: the top screen fills most of your view, a little below eye level, with
  its edges and the HUD inside the view, and stays in front of your eyes; there is no bottom screen. Turn your head slowly: the game camera turns with it and the world stays
  put. If the world swims along with your head, the screen is too small: raise
  **Immersive Game FOV** (try 70, 80; a mod that changes the game's FOV needs the same value here). If it swims against your head, lower it (50).
  <!-- VERIFY: the default of 60 degrees in the headset -->
- Leaning and crouching move the camera a little (Depth Factor Multiplier 1x).
- Squeeze a grip and move the hand down: the screen goes lower and the world stays where it was.
  Push the stick of that hand up: the screen goes farther (mode 2) without changing size. Push it
  right: bigger. Let go, stop the game, start it again: the screen is where you left it.
  <!-- VERIFY: the default placement and the grip gesture in the headset -->
- Left Bumper + Right Bumper + View brings the normal screens and touch back; the chord again returns to immersive mode.
  Right Stick Click while looking 45 degrees to the side makes that direction straight ahead.
- Mode 1: a big screen in front of you, fixed in the room, the middle of it about the size of the
  normal top screen. Mode 3 (Other Games) also works on Ocarina of Time 3D, after a while (the
  heuristic needs about 5000 uniform uploads).

Put the variables on their own lines in `session.env` in `~/.local/share/AzaharFrame` when you start
from the Steam library. Headless runs with the mock runtime (`tools/mockrun.sh`) check the layers
and the log lines (`xr: immersive: ...`, `xr: immersive mode 2 on ...`); that needs no headset.
