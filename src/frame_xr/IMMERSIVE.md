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
| 2 | Full (6DoF): Ocarina of Time, Majora's Mask | the game's own view (1.1x to 1.4x wider with Wider View, see Placement); view matrix register 4 follows the head (rotation and position) | head-locked, 3 m ahead, both eyes on one quad, the world at its own scale (see Placement) |
| 3 | Full (6DoF): Other Games | as 2, register found by CitraVR's heuristic (or `vr_si_mode_register_offset`); stereo from the eye indicator register | head-locked, each eye's quad centred on that eye, fitted into the view. The game is shown a 3D depth of about 0: the two eyes come from your head |

The other keys, also per application: `vr_immersive_positional_game_scaler` (Depth Factor
Multiplier: 0 = 1x, 1 = 10x, 2 = 100x; how far the camera moves with your head, times the 3D
slider / 10, and in mode 3 how far apart the two eyes are in the game's units: the game's world
scale), `vr_si_mode_register_offset` (View Matrix Register, -1 = Auto, mode 3 only),
`vr_immersive_eye_indicator` (Eye Indicator Register, e.g. `87,2`, empty = Auto, mode 3 only).

While the setting is not 0, the internal resolution is two steps higher (CitraVR
`vr_main.cpp:402`), at most 8x (`AdjustResolution` in `video_core/vr_immersive.h`).
Modes 2 and 3 need hardware shaders (Graphics, Enable Hardware Shader; on by default).

## Controls

| Control | In immersive mode |
| --- | --- |
| Left Bumper + Right Bumper + View | Shows or hides the bottom screen (CitraVR's left Menu button; the left bumper is L here, so it is a chord). Hold both bumpers, then press View. Every game starts with it hidden. The game does not see that View press. In modes 2 and 3, showing it pauses immersive mode (normal screens, the game camera stops following the head) until you hide it again; in mode 1 it shows below the middle of the wide screen and nearer than it, so it is drawn over it. |
| Left Stick Click, tap | Shows or hides the bottom screen, the same switch as the chord (screen focus, [docs/CONTROLS.md](../../docs/CONTROLS.md#swapping-the-screens)). |
| Right Stick Click, tap | Swaps the screens: the bottom screen becomes the main one. Immersive mode pauses (all three modes, `Immersive::SetBottomFocused`) until you tap again. |
| Grip (either hand) | Holds the immersive screen: move the hand up or down and the screen follows (the point under the laser stays under it). The stick of that hand: up moves it farther, down nearer; right makes it bigger, left smaller (past half way, as in the normal placement). Let go and the placement is saved for this game. The grip, and that hand's stick, do not reach the game meanwhile. Stick Click taps do nothing meanwhile (`Immersive::HoldingScreen`). A grab shows the hint chip low in front of you (`VrUi::ShowGripHint`, at most once per 10 s). |
| D-Pad, while both grips are down and one holds the screen | AzaharFrame immersive2. Right makes the screen narrower, left wider (Narrow Screen); down makes it lower, up taller (Narrow Height); 5 % per press, held: again after 0.4 s, then every 0.1 s, 0 to 30 %. Live, saved for this game, toast "Narrow Screen: 15%" or "Narrow Height: 5%" (`Immersive::StepNarrow`). Grip + D-pad with one grip keeps the offset and Wider View below; the second grip only chooses which. |
| D-Pad, while a grip holds the screen | Left / right nudge the global Immersive Offset 0.5 degrees per press (held: again after 0.4 s, then every 0.1 s) and show "Offset Right 1.5°" (etc.) as a toast for 2 s; the vertical offset is set only in the settings. Up / down step Wider View for this game (Full modes), "Wider View: 1.2x". The D-pad does not reach the game while the grip holds the screen, nor a direction pressed then until it is let go (`Input::ScreenRig::DpadNudger`, `Immersive::NudgeOffset`). See Placement. |
| Right Stick Click, held for 1 s | Also re-centres: the direction you face and where your head is become the game camera's rest pose (modes 2, 3), and the wide screen moves in front of you (mode 1). The immersive screen keeps its size, distance and pitch unless a grip holds it; then it goes back to its default placement and its saved one is forgotten (`Immersive::OnResetPanels`). **Reset Position** in the menu always puts it back. SteamVR's own recenter (hold the Steam button) only re-centres: the screen keeps its size, distance and pitch, so recentring again changes nothing (`Immersive::OnRecentred`; the rest pose is taken from the first head pose located in the moved space, at or after the event's `changeTime`). |
| Triggers | ZL / ZR (a trigger is a touch only on a shown bottom screen). |

The rest pose (`Layout::RestPose`) is taken when immersive mode starts, again each time it
resumes after a pause, and on a re-centre; never from the render size. AzaharFrame immersive,
after the owner's report that a resolution change (Internal Resolution, Dynamic Resolution, a
game's own option) left the character off to one side with the camera swinging round it: those
changes are made in a pause, and the old rest pose belonged to where the player faced and sat
before it. After turning or leaning in the pause the game camera kept that turn, and the head's
offset, times the game's world scale, moved the camera sideways and round the character. Mock
runs that change Internal Resolution mid-game with the head still (`AZAHARFRAME_TEST_RESOLUTION`)
keep the picture exactly where it was in Ocarina of Time 3D and Super Mario 3D Land.

While immersive mode is paused (modes 2 and 3 with the bottom screen shown or the in-game menu open,
any mode while the bottom screen is in focus),
the grips move the normal screens as usual.

## Hide Bottom Screen (without immersive mode)

Setting `vr_hide_bottom_screen` (section `VR`, per game in the Qt window: Properties, **Graphics**
tab, **VR** group, **Hide Bottom Screen**) draws only the top screen in the normal placement.
It is read when a game starts, and the game starts with the bottom screen hidden. The bumper chord
above brings it back (and hides it again); nothing else changes: the top screen can be grabbed
with a grip, the right stick click (held for 1 s) resets the placement, the triggers are ZL/ZR.

The two compose: with immersive mode on, hiding the bottom screen changes nothing (immersive mode
hides it already) and the chord behaves as described above. With the chord the bottom screen shows
for touch-only moments, in mode 1 and with the setting alone at its normal place.

## Screen Shape and Immersive Game FOV

Two more per-game settings, in the same **VR** group, both read every frame while a game runs (`OnFrame`), so a change in Configure or Properties shows at once (logged as "top screen shape ... (changed)"):

- `vr_top_aspect`, **Screen Shape**: 0 = 3DS (5:3, the 400x240 of the screen), 1 = Widescreen (16:9),
  2 = Ultrawide (21:9), 3 = Standard (4:3), 4 = 3:2, 5 = 16:10. For games and mods that render widescreen into the 400x240 top screen
  (Ocarina of Time 3D with the Single Screen Experience mod draws a squeezed 16:9 picture and
  expects the emulator to stretch it). The renderer is not touched; only the quad is wider. The
  normal top screen keeps its height (1 m) and its place, 2 m ahead, and gets wider; the bottom
  screen stays below it as before. The last three match game cheats that change the game's own
  aspect: Screen Shape must match what the game renders, or the picture stretches.
- `vr_immersive_game_fov`, **Immersive Game FOV**: the game camera's vertical angle in degrees,
  30 to 110, default 60. It sizes the head-locked screen of modes 2 and 3 until the game's own
  angle is measured (below), and in games where it is not found. The environment variable
  `AZAHARFRAME_IMMERSIVE_FOV` wins over both when it is set.

In the immersive modes the quad has to show the game's picture at the angle the game drew it (or
that angle over the gain, see Placement), so its aspect must be the aspect of the game's projection.
The vertical half angle comes from the FOV setting and the horizontal one from the shape:
`tan(h / 2) = aspect x tan(v / 2)`. In mode 1 the width is the normal top
screen's angular size times 3, and the height is the width over the aspect. If the shape in
Azahar for Frame and the mod's own aspect option disagree, the picture is stretched, and in modes
2 and 3 the world drifts against your head turns, unless the camera is measured.

**Measured camera** (modes 2 and 3; AzaharFrame depth). The game's projection matrix sits in the
four registers just before its view matrix: 86 to 89 in Nintendo's own games (view matrix 90),
0 to 3 in Ocarina of Time 3D and Majora's Mask 3D (view matrix 4). In the draws immersive mode
patches, the emulation thread reads the vertical angle and the shape from it
(`ProjectionFromRows` in `video_core/vr_view_matrix.h`: a perspective matrix, 10 to 150 degrees
high, 1.2 to 3 times as wide), takes the one most draws used in each quarter second
(`ProjectionTracker`) and hands it to the XR thread (`g_measured_projection` in
`video_core/vr_immersive.h`). The head-locked screen is then sized with the game's real angle and
shape instead of the two settings, so the world stays put through camera changes, cutscenes and
zooms, and with any widescreen mod, whatever Screen Shape says. The XR thread splits it in two
(`CameraFilter` in `immersive_layout.h`, AzaharFrame immersive):

- the **live angle**, which the gain follows: taken once two measurements in a row agree on it,
  nothing changes within 1.5 degrees (Jabu-Jabu's Belly in Ocarina of Time 3D changes its
  projection every frame);
- the **screen's camera**, which sizes and shapes the screen: its angle changes only once a new one
  held for 2 seconds (8 measurements: a new scene, a cutscene, a menu), its shape only at the same
  vertical angle (a widescreen mod or the game's own shape option widen the camera at the same
  height). An aim or a dash (a zoom, sometimes with another shape) therefore zooms the picture on
  a screen that keeps its size and shape, as on the 3DS, and the gain keeps the world locked at
  the middle meanwhile. Earlier the screen followed every zoom: its shape changed while aiming.

Angles stay within the setting's 30 to 110 degrees: a 10 degree close-up in a cutscene would push
the gain past its limit, so there the world moves with the head a little instead. Logged as
`xr: immersive: game camera measured 45.0 degrees high, 1.667 wide` (the screen's camera; the live
angle at debug level). Screen Shape still shapes the normal top screen. **Match Game Camera** (`vr_immersive_match_camera`, global and per game, on
by default, live) turns it off: the emulation thread then reads no registers at all
(`g_measure_camera`, `MeasureDraw`) and the two settings are used as they are;
`AZAHARFRAME_IMMERSIVE_MEASURE=0` forces it off.

## Placement

`frame_xr/immersive_layout.{h,cpp}` (unit tested in `src/tests/frame_xr/immersive_layout.cpp`), saved
per game and mode group in `immersive-placement.txt` in the user directory (`<title id> screen|wide
<pitch> <distance> <size>`; written when a grip lets go, read when a game starts).

In modes 2 and 3 the world only stays put when a head turn moves the picture by the angle the game
camera turned. Each of the three things a grip changes keeps that:

- **Distance** scales the quad with it, so its angle does not change. In mode 2 it moves the flat
  picture's stereo depth; in mode 3 (each eye's quad on that eye) nothing visible changes.
- **Pitch.** The quad turns about the eye, and the same rotation is added to the head rotation the
  game camera gets (camera = head x pitch, `GameCameraRotation`), so the camera looks where the
  screen is: the world stays where it was and you see it through a lower window. In mode 2 the
  register's rotation is replaced by that rotation; in mode 3 it multiplies the game's own view
  matrix. Both take it as a plain rotation. The grip gesture keeps the normal placement's
  horizontal lock, so its yaw is always 0; the Immersive Offset below adds one.
- **Size.** The quad is scaled against the game's view, and the head's yaw and pitch given to the
  game are scaled by `gain` = game angle / screen angle (tangents; `ScaleYawPitch`: the head
  rotation split into yaw, pitch and roll, the roll kept, since a picture turned in its plane keeps
  its angle at any scale). That keeps the picture moving with the head at the middle of the screen.
  The game draws past its own frame only by the periphery factors (Wider View across, and up and
  down as much as the screen's shape needs, below); past them the picture is magnified. Where a
  game's scenery runs out, that part is black (below the floor of a Super Mario 3D Land level). The cost of a gain other than 1: a big head turn turns the game camera `gain` times
  as far, and away from the middle the planar picture moves a little against the head: at an angle
  `e` from the middle, by `(1 + gain^2 tan^2 e) / (1 + tan^2 e)` times the turn (1.25 at gain 1.3
  and 37 degrees, 0.56 at gain 0.5 and 50 degrees). The gain stays within 0.4 to 2.0 (past that the
  screen keeps its size and the world drifts a little), and the size within 0.6 to 2.5 times the
  default.

**The default is the world at its own scale** (AzaharFrame immersive2, after the owner's headset
test of the view-filling default of the 1.2.0 test builds: "way too close", and Mario Kart 7
"stretched"). At size 1 the screen is as many degrees across as the game draws: its camera's
width times the Wider View factor, so the gain is 1, nothing is magnified, and a head turn moves
the picture exactly with the head everywhere on it, not only at its middle. The view-filling
default magnified a 55 degree game 1.6 to 2.5 times (gain 0.4 to 0.6): a perspective picture is
only right at its middle, and 35 degrees to the side it was drawn 1.4 to 1.6 times taller than
wide. Majora's Mask 3D with the owner's own settings (gain 1.03, Wider View 1.4x, Narrow Screen 10 %,
3 m, mode 2) was "perfect"; those are the defaults now (`kDefaultHeadDistance`, the settings'
defaults). The screen takes the view's shape, not the picture's (`DefaultCover`: what either eye
sees, from the runtime's per-eye FOV, logged once as `xr: immersive: both eyes see ..., either
eye ...`; on the Frame about 126 x 112 degrees), so the game draws further up and down to match
(`HeadLocked::factor_y` = factor x picture aspect x screen height / width, at least the factor and
at most `kMaxFactorY`, 2.0; 1.26 times the factor on the Frame for 5:3): the picture has the same
scale both ways and is not stretched. The vertex shader divides clip x by `factor_y` and clip y by
the factor (the top screen is drawn turned by 90 degrees, so clip x is up and down). A 2D vertex
(w = 1) on or past the edge of the view keeps its place: full-screen 2D passes cover the whole
picture. Shrunk with the rest, a full-screen mask cut Ocarina of Time 3D's world off at the game's
own frame and Majora's Mask 3D's copy of the last frame nested smaller copies of itself; the HUD,
inside the view, still shrinks with the world. The screen is never bigger than filling what either
eye sees (1 degree past it, `kFillMarginDeg`, at least +-35 degrees high): a very wide game camera
is shrunk to that (gain above 1). It is centred on the middle of the view (5 degrees below straight
ahead on the Frame). The line `xr: immersive mode N on` logs the factor, the screen's angles, its
pitch, both gains and the size. A preset can make a game's default bigger or smaller
(`screen_size`: Mario Kart 7 0.9).

The angle the screen is sized with: the game's own **Immersive Game FOV** when it has one (a
preset's measured play camera, or the player's per-game value), so the screen keeps one size
through cutscenes and menus while the gain follows the measured angle; otherwise the settled
measurement (Match Game Camera) or the setting.

Two per-game settings change this (**VR** group, logged as
`xr: immersive: Wider View ..., Narrow Screen ..., Narrow Height ...`):

- **Wider View** (`vr_immersive_wider_view_level`, per game: Off, 1.1x to 1.4x, 1.4x by default;
  read every frame): the periphery factor across (`gl_Position.y /= factor`) is the level (up and
  down `factor_y`, which grows with it). The default size grows with the factor, so a level
  changed by grip + D-pad up or down or in Properties keeps the picture's scale and the gain: only
  more or less of the game shows around it. The old switch (`vr_immersive_wider_view`) is
  converted when a config is read: on becomes the level nearest to what it gave at the saved size
  (0.78 x size, 1.4x when no size is saved), off becomes Off; a global switch never set keeps the
  default. Mock runs of 1.2.0 at 1.4x: Ocarina of Time 3D, Majora's Mask 3D and Super Mario 3D
  Land draw correctly, but Super Mario 3D Land's levels are black below the floor.
- **Narrow Screen** (`vr_immersive_squeeze`, called Squeeze Width in 1.0.0; AzaharFrame
  immersive2: percent, 0 to 30 in 5 % steps, 10 by default, read every frame; 1, 2 and 3 are the
  10 % steps of earlier builds, `Layout::NarrowPercent`): the quad keeps its height and is that much
  narrower than the picture, and the yaw gets its own gain, `gain_x = base gain / (1 - narrow)`, so
  the middle stays locked both ways. The HUD at the sides moves in without changing the vertical
  size. The cost: the world looks that much thinner, a left-right turn turns the game camera more
  than an up-down turn (a diagonal turn is a little off), the stereo depth in mode 3 is that much
  flatter, a head roll cannot be matched exactly in a squeezed picture (kept 1:1), and the edges
  move a little more against the head (the formula above with `gain_x`). `gain_x` is clamped to
  `kMaxGain` (2.0) like the gain. It costs nothing to draw, and needs no restart: only the quad's
  shape and the gains change, nothing in the game (earlier builds read it when the game started).
  The VR menu's row offers Off, 10 %, 20 % and 30 %; the Qt window every 5 %; both grips + D-pad
  left or right every 5 %.
- **Narrow Height** (`vr_immersive_squeeze_y`, AzaharFrame immersive2, per game, percent, 0 by
  default, live; both grips + D-pad up or down only): the same up and down. The quad is that much
  lower at the same width, the pitch gain is `base gain / (1 - narrow height)`; the world looks
  that much flatter.

Saved sizes are relative to that default. The file's `world` lines are written by this build;
`screen` and `view` lines (1.2.0 test builds) and `head` lines (1.0, 1.1) had sizes against other
defaults:
their pitch and distance are kept and the size becomes the game's default, logged as `placement
(earlier build's size replaced by the default)` and saved as a `world` line. The owner's
`screen` lines were all at the smallest size (0.6 times the view-filling one), about gain 1: the
new default is what they had.

**Immersive Offset** (`vr_immersive_offset_x`, `_y`, degrees right and up, global only, -10 to 10 in
0.5 steps; `LensOffset` in `immersive_layout.h`). The Frame's lenses sit a little differently for
each person, so one player sees the screen's right edge and not its left. Grip + D-pad turns the
solved quad about the eye by that yaw and pitch, after the per-game placement, and the same
rotation goes into the game camera (`QuadRotation`, used by both `HeadLockedQuadPose` and
`GameCameraRotation`), exactly as the pitch above: the world stays where it is and only the window
onto it moves, the angle is the same at every distance and size, the gains still apply to the head
turn alone, and the size is fitted without it (nudging never resizes the screen). A re-centre
only resets the rest pose, so it keeps the offset; **Reset Position** in the menu puts it back to
the middle with everything else. Mode 1 turns its wide screen (and the bottom
screen under it) about the rest pose by the same angles. It is global, not per game: it is about
the player's eyes. Putting a game's screen back (right stick click while holding) keeps it;
**Immersive Offset** in the VR settings shows it and resets it. The XR thread writes the setting
and asks the Qt side to save the config (`FrontendHooks::vr_setting_changed`, saved half a
second after the last press); it is read every frame and logged as `xr: immersive offset`.

Mode 1 is fixed in the room, so its pitch and distance are a plain move (the size scales with the
distance) and its size is free.

Numbers to tweak, all in `immersive_layout.h`: `kComfortHalfWidthDeg` (37), `kDefaultPitchDeg` (-5),
`kDefaultHeadDistance` (3 m, modes 2 and 3), `kDefaultDistance` (0.75 m, mode 1), `kFitMarginDeg`
(2.5), `kFactorMax` (1.4), `kMinSize` / `kMaxSize` (0.6, 2.5), `kMinGain` / `kMaxGain` (0.4, 2.0),
`kNarrowStepPercent`, `kMaxNarrowPercent`, the gesture rates.

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
   camera frame times `10^multiplier x (3D slider / 10)`, factor 3.0 (mode 1), 1.0 (modes 2, 3) or
   1.0 to 1.4 with Wider View. Not immersive (setting 0, no game, bottom screen shown in modes 2/3): CitraVR's "off"
   values (mode 1, factor 1, identity). It publishes them into a seqlock of atomics
   (`video_core/vr_immersive.h`, one writer).
2. **Emulation thread**, once per game frame (AzaharFrame immersive, `FrameLatch` in
   `video_core/vr_immersive.h`): when the game swaps the top screen's framebuffer
   (`GPU::SetBufferSwap`, the end of a frame), or after 4 VBlanks without a swap,
   `VideoCore::Immersive::ApplyPending` reads the newest snapshot and calls
   `RasterizerInterface::SetVRData` (again after a new rasterizer, one per game boot). Every draw
   of one game frame, both eyes, gets the same head pose. It used to change at every VBlank (60 a
   second), and a game at 30 frames a second (Ocarina of Time 3D, Majora's Mask 3D) draws one
   frame across two VBlanks: the draws after the VBlank got a newer pose than those before, so
   the two eyes, and parts of one eye's picture, came from different head poses, and objects
   doubled whenever the head moved.
   `SetVRData` marks the PICA uniforms dirty so the patched view matrix is uploaded with the new
   pose even when the game did not touch its uniforms.
3. **XR thread, layers** (`app.cpp` asks `Immersive::Screens`): the top screen's space, per-eye
   pose and size, and whether the bottom screen is shown. Modes 2/3 use a quad in
   `XR_REFERENCE_SPACE_TYPE_VIEW`, so SteamVR keeps it locked to the head at display time.

CitraVR called `SetVRData` from its OpenXR thread while the emulation thread drew with the same
fields; that race is gone.

## Differences from CitraVR

- **No cylinder in immersive mode.** The immersive screen is a flat quad (earlier builds found no
  `XR_KHR_composition_layer_cylinder` in SteamVR; it is there, and the normal screens use it for
  **Screen Curve**, see `input/screen_rig.h`). Modes 2/3: a head-locked quad in VIEW space instead
  of CitraVR's 0.5 m cylinder re-posed from the head pose each frame. Mode 1: a flat quad fixed in
  the room instead of the wrap-around cylinder. The immersive placement and its world-stays-put
  maths are made for a flat picture; Screen Curve leaves the immersive screen flat.
- **Size of the head-locked screen.** CitraVR's mode 2 is a cylinder 0.5 m around the head, 55 x
  1.4 = 77 degrees across and about 44 degrees high (the 5:3 shape), centred at eye level
  (`GameSurfaceLayer.cpp`). That is smaller than the game's 1.4x view (107 x 78 degrees for a camera
  60 degrees high), so its world moves with the head a little. Earlier builds here drew the quad at
  the full 107 x 78 degrees to keep the world still, which is more than the Frame shows: the top of
  the picture and the HUD were cut off, and the screen sat high. Here it is fitted to a comfortable
  width and placed lower, without losing the world lock (see Placement), and the grips move it.
  The game draws its own view, not CitraVR's 1.4x wider one, unless Wider View is on.
  The Immersive Game FOV setting sets the game camera's angle, and `AZAHARFRAME_IMMERSIVE_FOV` overrides it.
- **Mode 3 eye offset** from the located eye poses (half your IPD), not a fixed 32.5 mm.
- **Reference pose.** CitraVR used Quest's LOCAL space, whose origin is the head at start. SteamVR's
  LOCAL origin can be anywhere, and the head position is multiplied by 5 or more, so the pose is
  taken relative to the head when the game starts (and at each re-centre).
- **Bottom screen** in mode 1 shows below the wide screen's middle, at 0.7 times its distance, with
  the normal bottom screen's angular size (CitraVR moved it to 1 m below the top screen). It is
  nearer than the wide screen and submitted after it, so it is never drawn through it.
- **Mode 2 patches only perspective draws** (`HasPerspectiveProjection` in
  `video_core/vr_view_matrix.h`, AzaharFrame immersive). CitraVR skipped only the HUD (by one
  value of its orthographic projection, `f[0][3] == 1`); other screen-space passes got the head
  pose too. A pass that draws an earlier picture again, turned and moved with the head, fed into
  itself frame after frame (copies of the picture shrinking towards the middle, the owner's
  screenshot in the Great Deku Tree), and what it no longer covered kept old pixels (streaks). Now
  the projection in registers 0 to 3 must be a perspective one.
- **Mode 3 patches only world cameras** (`video_core/vr_view_matrix.h`). CitraVR changed the
  register (rotation, head position and the eye offset) in every draw. Draws that keep other data
  there were corrupted: in Mario Kart 7 the register holds other numbers in many 2D and menu draws,
  and the race start was washed out white. Now a draw is patched only when the register's 3x3 is
  a rotation other than the identity (a 2D layout camera looks straight down -Z).
  Mario Kart 7's menus remain odd in modes 2 and 3: each menu is a small 3D set built for the
  3DS's fixed view, so when the camera turns with the head (or with the screen's pitch) its edges
  show, and the white behind them slides into the picture. Nothing can fill that in; Off or Wide
  is the clean choice for the menus.
- **Mode 3 eye indicator with a known register.** CitraVR searched for the view matrix register
  and the eye indicator together, so a register from a preset or the setting skipped both, and
  both eyes drew the same picture (the game itself sees a 3D slider of 0.01). Here a known
  register only ends the register search; the eye indicator is still searched for, outside that
  register's matrix, unless **Eye Indicator Register** is set. The log names both once the search
  ends (`immersive mode 3: view matrix register 90 (set), eye indicator 87,2 (found)` in Super
  Mario 3D Land). The presets now set the eye indicator too, so the 3D is there from the first
  frame (the search needs 5000 uniform uploads first). Super Mario 3D Land's preset also sets the
  Depth Factor Multiplier to 100x: at 1x its eyes were 0.65 units apart in a world seen from
  about 1500 units, a third of a 3DS pixel of difference (see Tuned games).
- The `vr_immersive_mode > 90` register search mode (thumbrest touch steps `uoffset`) is not
  carried over; `uoffset` is always -1.
- The four `vr_*` settings are now `SwitchableSetting`s so they can be set per application in the
  Qt window (CitraVR's Android app only had the mode per game).
- `vr_immersive_mode_factor` defaults to 1 (`shader_uniforms.h`); it was 0 until CitraVR's
  Android frontend set it every frame, which made the vertex shader divide by zero elsewhere.
- AzaharFrame immersive: `vr_immersive_mode_factor_y` (up and down, `SetVRFactorY`) next to it,
  and full-screen 2D vertices are not divided (`VrFactor` in `glsl_shader_gen.cpp`). CitraVR
  divided `gl_Position.xy` of every vertex by one factor.

## Tuned games

Measured in mock runs of each game's play (AzaharFrame depth, 1.2.0) with the uniform registers
dumped: the camera's angle and shape from its projection, and the 3DS's own eye distance at full
3D (the difference between the two eyes' view matrices, in the game's units). In mode 3 the eye
distance here is 6.5 cm x 3D Depth / 10 x 10^multiplier game units, and the head moves the camera
by the same scale, so the **Depth Factor Multiplier** is the game's world scale. It is picked so
that at a 3D Depth of 90 the eyes are about as far apart as the 3DS's at full 3D, the depth the
game was made for; 3D Depth then turns it down. The presets (`frame_xr/presets.cpp`) set these
for a game whose properties have no value of their own.

| Game | Mode, registers | Camera (measured) | 3DS eyes at full 3D | Multiplier | Notes |
| --- | --- | --- | --- | --- | --- |
| Super Mario 3D Land | 3, view 90, eye 87,2 | 45 degrees, 5:3, title and levels | 58 units in a level | 100x (58.5) | at Wider View 1.4x black below the floor in mock runs; the owner plays it at 1.4x |
| Mario Kart 7 | 3, view 90, eye 87,2 | 52 racing, 55 in the fly-by before a race, 40 and 16 in menus | 3.4 | 10x (5.9) | menus stay odd in the Full modes (Differences from CitraVR) |
| Ocarina of Time 3D | 2 (view 4) | 55 in play (54.6 to 55.4: Jabu-Jabu's Belly wobbles), 35 to 70 in cutscenes, 5:3 without mods | (one flat picture) | 10x | leaning moves the camera; Link is about 56 units tall, so 10x at 3D Depth 30 is life size |
| Majora's Mask 3D | 2 (view 4) | 7 to 70 in the opening cutscenes, 30 to 55 measured in play (the owner's headset log; the real camera stays at 55), 5:3 | (one flat picture) | 10x | same engine and scale as Ocarina of Time 3D; the Project Restoration mod keeps the camera; Match Game Camera off in its preset |
| Asphalt 3D | 3, view 8 | not measured | | 1x | CitraVR's notes only |

### The immersive screen per game

AzaharFrame immersive2, on the Frame (its view from the headset log), at the defaults and presets
(a game's own values win). Narrow Height is Off and the screen's pitch is the middle of the view
in all of them. Size 1 is the world at its own scale; the angles are the screen's.

| Game | Screen Shape | Narrow Screen | Wider View | Up and down | Distance | Size |
| --- | --- | --- | --- | --- | --- | --- |
| Any game (default) | 3DS (5:3), or as measured | 10 % | 1.4x | 1.76x (1.26x Wider View) | 3 m | 1 (95 x 85 degrees at 55) |
| Ocarina of Time 3D | 3DS (5:3); with SSE as measured | 10 % | 1.4x | 1.76x; 16:9 1.88x; widest 2.0x | 3 m | 1 (95 x 85; 16:9 99 x 89; widest 111 x 92) |
| Majora's Mask 3D | 3DS (5:3) | 10 % | 1.4x | 1.76x | 3 m | 1 (95 x 85) |
| Super Mario 3D Land | 3DS (5:3) | 10 % | 1.4x | 1.77x | 3 m | 1 (82 x 73) |
| Mario Kart 7 | 3DS (5:3) | Off | 1.4x | 1.77x | 3 m | 0.9 (91 x 76, gain 1.11) |

Mario Kart 7 looked stretched at the view-filling size (gain 0.40 to 0.53) and with Narrow
Screen 20 %; the vertical extension itself is not a stretch (the same scale both ways). Its
preset puts the HUD where Narrow Screen 10 % did (about 36 degrees to each side) by making the
whole screen 10 % smaller instead, so the world keeps its shape. In mode 3 the distance changes
nothing visible; in mode 2 it is the flat picture's stereo depth.

Checked and left without a preset: Pushmo (a 9 degree telephoto camera on the puzzles, so the
head-locked window is 9 x 15 degrees; Off or Wide), Fire Emblem Awakening and Fates (menus and a
22.6 degree map camera; in Fates no draw passes the world-camera test, so nothing turns), Fire
Emblem Echoes (its world map works in mode 3 with view 90 and eye 94,1 at 30 degrees; not tried
in battle or the dungeons), Tetris Ultimate (no world camera found), Stella Glow (the title
needs Start; not reached), and homebrew without a 3D camera.

### Ocarina of Time 3D with a widescreen mod

Single Screen Experience (SSE) draws Ultrawide, 16:9, 4:3 or 5:3 by changing the projection's
width and keeping its height (Hor+): with SSE and a save set to its widest shape the camera
measured 54.7 degrees high and 2.22 wide (20:9; 5:3 x 4/3), without the mod 55 degrees and 5:3.
In immersive mode the measured camera sizes the screen, so the mod's shape needs nothing here.
**Screen Shape** still shapes the normal top screen (the bottom screen shown, immersive mode off):

| Mod's shape | Camera | Screen Shape | Immersive Game FOV |
| --- | --- | --- | --- |
| 5:3 (or no mod) | 55 x 1.667 (measured) | 3DS (5:3) | 55 |
| 16:9 | 55 x 1.778 | Widescreen (16:9) | 55 |
| Ultrawide | 55 x 2.22 (measured) | Ultrawide (21:9), 5 % too wide | 55 |
| 4:3 | 55 x 1.333 | Standard (4:3) | 55 |

The FOV setting is only used until the first measurement; leave the mod's own FOV at its default
or the measurement follows it anyway.

### Kid Icarus: Uprising: no immersive mode

AzaharFrame immersive: Kid Icarus: Uprising (every region, `ImmersiveOffered` in
`frame_xr/presets.h`) always runs in the normal view. Its Immersive Mode is set to Off when it
starts (logged as `xr: immersive mode is not offered for this game: Off`), the VR menu shows the
setting as not available for this game, and its Properties disable it. The cause, from the
owner's headset test (shots and their effects came apart from the reticle): the game aims with a
reticle it draws on the screen in 2D and shoots along its own camera. In the Full modes the world
is drawn from a camera turned with the head, which the game does not know about, so the shots fly
off where the unturned camera looked while the reticle stays on the screen; and the 2D reticle
has no stereo depth (the game is shown a 3D slider of about 0) while the shots do. Nothing in the
drawing can join them again: the reticle cannot be told from the rest of the HUD. It is not Match
Game Camera (it measures perspective draws only, the HUD's projection is orthographic) and not
the eye offset (both eyes' world draws get it). Its preset is gone.

## Tuning without a rebuild

Environment variables, read when the XR session starts (logged as `xr: immersive: ...`):

| Variable | Default | Meaning |
| --- | --- | --- |
| `AZAHARFRAME_IMMERSIVE_FOV` | unset | the game camera's vertical angle in degrees (20..120); sets the head-locked screen size and wins over the Immersive Game FOV setting (default 60, 30..110) |
| `AZAHARFRAME_IMMERSIVE_DISTANCE` | 3 (0.75 in mode 1) | the default distance in metres (0.3..3) for a game without a saved placement, all modes; sizes scale with it, so only stereo depth changes |
| `AZAHARFRAME_IMMERSIVE_LOCAL` | 0 | 1: place the head-locked screen in LOCAL space from the located head pose each frame, as CitraVR did (fallback if VIEW-space quads misbehave) |
| `AZAHARFRAME_TEST_RESOLUTION` | unset | `<seconds>:<factor>`: sets Internal Resolution that long after the game started, once (mock runs) |
| `AZAHARFRAME_IMMERSIVE_WIDER` | unset | 0 to 4: the Wider View level, over the setting (comparing levels in mock runs) |
| `AZAHARFRAME_IMMERSIVE_MEASURE` | 1 | 0: size the head-locked screen from the settings only, not from the game's measured projection |

## Trying it

Set the mode in the Qt window: Properties of a game, **Graphics** tab, **VR** group, **Immersive Mode**
(the same group is under **Emulation > Configure > Graphics** for all games). Start the game from
Azahar's window. Without the window, the global config (`qt-config.ini` in the user directory) takes:

    [VR]
    vr_immersive_mode\default=false
    vr_immersive_mode=2

What to look for:

- Mode 2 in Ocarina of Time 3D: the top screen fills most of your view, a little below eye level, with
  its edges and the HUD inside the view, and stays in front of your eyes; there is no bottom screen. Turn your head slowly: the game camera turns with it and the world stays
  put. The log line `game camera measured ... degrees high` shows the angle in use. Before it
  shows, **Immersive Game FOV** is used: if the world swims along with your head then, raise it;
  against your head, lower it.
- Leaning and crouching move the camera a little (Depth Factor Multiplier 1x).
- Squeeze a grip and move the hand down: the screen goes lower and the world stays where it was.
  Push the stick of that hand up: the screen goes farther (mode 2) without changing size. Push it
  right: bigger. Let go, stop the game, start it again: the screen is where you left it.
- Left Bumper + Right Bumper + View brings the normal screens and touch back; the chord again returns to immersive mode.
  Right Stick Click, held for 1 s, while looking 45 degrees to the side makes that direction straight ahead.
- Mode 1: a big screen in front of you, fixed in the room, the middle of it about the size of the
  normal top screen. Mode 3 (Other Games) also works on Ocarina of Time 3D, after a while (the
  heuristic needs about 5000 uniform uploads).

Put the variables on their own lines in `session.env` in `~/.local/share/AzaharFrame` when you start
from the Steam library. Headless runs with the mock runtime (`tools/mockrun.sh`) check the layers
and the log lines (`xr: immersive: ...`, `xr: immersive mode 2 on ...`); that needs no headset.
