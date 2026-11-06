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
| 1 | Wide (3DoF) | draws 3x wider and higher (`gl_Position.xy /= 3`) | 0.5 m ahead, fixed in the room, 1.25 x 0.75 m. Its middle third (the game's own frame) has the angular size of the normal top screen |
| 2 | Full (6DoF): Ocarina of Time, Majora's Mask | 1.4x wider; view matrix register 4 follows the head (rotation and position) | head-locked, 0.5 m ahead, both eyes on one quad |
| 3 | Full (6DoF): Other Games | as 2, register found by CitraVR's heuristic (or `vr_si_mode_register_offset`); stereo from the eye indicator register | head-locked, each eye's quad centred on that eye. The game is shown a 3D depth of about 0: the two eyes come from your head |

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
| Left Bumper | Shows or hides the bottom screen (CitraVR's left Menu button). Every game starts with it hidden. In modes 2 and 3, showing it pauses immersive mode (normal screens, the game camera stops following the head) until you hide it again; in mode 1 the bottom screen shows in its normal place, over the wide screen. |
| Right Stick Click | Also re-centres: the direction you face and where your head is become the game camera's rest pose (modes 2, 3), and the wide screen moves in front of you (mode 1). SteamVR's own re-centre does the same. |
| Triggers | ZL / ZR while the bottom screen is hidden (nothing to point at). The top screen cannot be dragged. |

The left bumper is still a frame_xr input in the Controls dialog; if you bind it to a 3DS button
there, it does both.

## How it works

Three threads, and the rasterizer is only touched by the one that draws:

1. **XR thread** (`immersive.cpp`, a frame callback, every display frame): reads the setting and
   the located head pose, takes it relative to a reference (head position and heading when the
   game started or at the last re-centre), and builds CitraVR's `SetVRData` arguments exactly as
   `vr_main.cpp` `UpdateImmersiveModeIfNeeded` did: the inverse head rotation, the inverse head
   position rotated into the head frame times `10^multiplier x (3D slider / 10)`, factor 3.0 or
   1.4. Not immersive (setting 0, no game, bottom screen shown in modes 2/3): CitraVR's "off"
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
- **Size of the head-locked screen.** It shows the game's view at the angle the game drew it, so
  that turning your head turns the game camera by the same angle as the picture moves and the
  world stays put: 2 x 0.5 m x 1.4 x tan(60 / 2) = 0.81 m high (78 degrees), 1.35 m wide, for a
  game camera 60 degrees high (Ocarina of Time's). `AZAHARFRAME_IMMERSIVE_FOV` sets that angle.
  `AZAHARFRAME_IMMERSIVE_FOV=28` gives the normal top screen's size times 1.4.
- **Mode 3 eye offset** from the located eye poses (half your IPD), not a fixed 32.5 mm.
- **Reference pose.** CitraVR used Quest's LOCAL space, whose origin is the head at start. SteamVR's
  LOCAL origin can be anywhere, and the head position is multiplied by 5 or more, so the pose is
  taken relative to the head when the game starts (and at each re-centre).
- **Bottom screen** in mode 1 stays at its normal place (CitraVR moved it to 1 m below the top
  screen), so touch works as usual.
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
| `AZAHARFRAME_IMMERSIVE_FOV` | 60 | the game camera's vertical angle in degrees (20..120); sets the head-locked screen size |
| `AZAHARFRAME_IMMERSIVE_DISTANCE` | 0.5 | metres to the top screen (0.2..5); sizes scale with it, so only stereo depth changes |
| `AZAHARFRAME_IMMERSIVE_LOCAL` | 0 | 1: place the head-locked screen in LOCAL space from the located head pose each frame, as CitraVR did (fallback if VIEW-space quads misbehave) |

## Trying it

Set the mode in the Qt window: Properties of a game, **Graphics** tab, **VR** group, **Immersive Mode**
(the same group is under **Emulation > Configure > Graphics** for all games). Start the game from the
picker. Without the window, the global config (`qt-config.ini` in the user directory) takes:

    [VR]
    vr_immersive_mode\default=false
    vr_immersive_mode=2

What to look for:

- Mode 2 in Ocarina of Time 3D: the top screen fills most of your view and stays in front of your eyes;
  there is no bottom screen. Turn your head slowly: the game camera turns with it and the world stays
  put. If the world swims along with your head, the screen is too small: raise
  `AZAHARFRAME_IMMERSIVE_FOV` (try 70, 80). If it swims against your head, lower it (50).
  <!-- VERIFY: the default of 60 degrees in the headset -->
- Leaning and crouching move the camera a little (Depth Factor Multiplier 1x).
- If the depth is uncomfortable, try `AZAHARFRAME_IMMERSIVE_DISTANCE=1.0`.
- Left Bumper brings the normal screens and touch back; Left Bumper again returns to immersive mode.
  Right Stick Click while looking 45 degrees to the side makes that direction straight ahead.
- Mode 1: a big screen in front of you, fixed in the room, the middle of it about the size of the
  normal top screen. Mode 3 (Other Games) also works on Ocarina of Time 3D, after a while (the
  heuristic needs about 5000 uniform uploads).

Put the variables on their own lines in `session.env` in `~/.local/share/AzaharFrame` when you start
from the Steam library. Headless runs with the mock runtime (`tools/mockrun.sh`) check the layers
and the log lines (`xr: immersive: ...`, `xr: immersive mode 2 on ...`); that needs no headset.
