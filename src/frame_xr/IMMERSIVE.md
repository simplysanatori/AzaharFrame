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
angle at debug level). Screen Shape still shapes the normal top screen.

**Detect Game FOV** (named Match Game Camera before; `vr_immersive_match_camera_mode`, global and
per game, live; AzaharFrame fovonce) is Off, Once or Always (in the code `MatchGameCamera::Off`,
`SetOnce`, `Always`). The pure parts are in `immersive_layout.h` (`FovEstimate`, `FovFollow`,
`FovSlew`, `FovEase`, `CameraAngles`, unit tested), and they work in time and per displayed frame,
not in degrees fitted to a game, so they hold for any game and any settings: what they give is used
exactly as a typed Immersive Game FOV would be.

- **Off:** the emulation thread reads no registers at all (`g_measure_camera`, `MeasureDraw`) and
  Immersive Game FOV and Screen Shape are used as they are.
- **Always** (the default, the earlier "on"): the measured camera as above, followed calmly. The
  live angle to follow is the median of the last five measurements (`FovFollow`), so a new angle
  wins with the third in a row (about half a second, however the game spaces them): a flash, a
  one-off reading or a menu that is back sooner starts nothing. The screen's angle and the live one then move toward
  it by at most 0.1 degrees per displayed frame (`FovSlew`; 12 degrees a second at 120 Hz, a
  10 degree cut in about 0.8 s, 40 degrees in about 3.5 s), and the step itself changes by at most
  a 24th of that a frame, so it starts and stops gently: no kick at either end, the same both ways,
  and a camera that comes back mid-way is followed back the same way. A gradual change (Mario
  Kart 7 widening with speed, a few degrees a second) is followed about half a second behind.
  Before this, a cut moved the gain in one frame ("the whole view jumps"); the half-second ease of
  the first 1.2.0 builds still showed as a jump. With the slew a longer hold is not needed: one that
  does win is never stepped, only slewed.
- **Once:** the camera is measured while a game in a Full mode is on view (the menu closed),
  and the screen meanwhile uses the settings, as Off. `FovEstimate` counts a measured angle only
  once it held for 2 s (each measurement within 1.5 degrees of the one before, none missing for
  1 s), so cuts, Z-targeting, menus and flashes are left out; ortho and HUD projections never
  reach it (`ProjectionFromRows` takes perspective matrices only), and angles outside the
  setting's 30 to 110 degrees never count. Of the time that counted, the 3-degree band that held
  the most is the game's angle: taken after 12 s if it holds 60 % of it, or after 20 s with 40 %
  (a game with long cutscenes at other angles). Its time-weighted mean, rounded to a degree, is
  written as the game's own Immersive Game FOV with `vr_immersive_fov_measured` (per game; the Qt
  side saves the game's file through `MenuSetting::MeasuredGameFov`), eased in, a toast says
  `Game FOV Detected: N°`, and nothing is measured for that game again: it is an ordinary
  per-game value the player can change. With no value after 60 s of measuring, the setting stays,
  the log and a toast (`Couldn't Detect Game FOV · Kept N°`) say so, and the next start of
  the game tries again. Choosing Once again in the VR menu (also while it is picked) or newly
  in the game's Properties clears `vr_immersive_fov_measured` and measures anew. Logged as
  `xr: immersive: Detect Game FOV Once: the game camera is N degrees high ...`.

A config from before has only the old switch `vr_immersive_match_camera`: on becomes Always, off
Off, globally and per game (`FrameConfig::MigratedMatchCamera`, tested in frame_qt_tests); the
old key is no longer written. Presets set the three-way value (Super Mario 3D Land Always, Mario
Kart 7 and the Zelda games Off). `AZAHARFRAME_IMMERSIVE_MEASURE=0` forces it off, and
`AZAHARFRAME_IMMERSIVE_FOV_TRACE=1` logs the angles each frame while they move (each measurement,
the angle followed, the slewed screen and live angles), for mocks.

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
defaults). The screen has the picture's own shape (AzaharFrame oot), and Wider View widens the
game's picture by the same factor both ways (`HeadLocked::factor_y` = factor): at Off the game is
asked for exactly its own picture, whatever the game's shape, mode or view. From AzaharFrame
immersive2 until then the screen took the view's shape (`DefaultCover`; on the Frame about 126 x
112 degrees), so the game drew 1.25 times further up and down than across at every level, Off
included: a 5:3 game came out as a 4:3 picture ("it says 5:3, but it is not 5:3"), and full-screen
2D (below) was stretched up and down against the HUD drawn on it (Ocarina of Time 3D's item screen
with the Single Screen Experience mod). The screen now leaves the view's top and bottom edges
showing where the picture is wider than the view. The vertex shader divides clip x by `factor_y`
and clip y by the factor (the top screen is drawn turned by 90 degrees, so clip x is up and down),
so the picture has the same scale both ways and is not stretched. A 2D vertex
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
measurement (Detect Game FOV Always) or the setting. Always slews it in, Once eases in the value
it wrote.

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
  immersive2: percent, 0 to 30 in 5 % steps, Off by default (AzaharFrame oot; 10 % before), read every frame; 1, 2 and 3 are the
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
- **Mode 3 eye offset** from your IPD as SteamVR locates the eyes (half of it each way, times the
  Depth Factor), not a fixed 32.5 mm, and the head-locked screens use the same value. It follows
  the IPD dial during play: a change settles in about 0.4 s and glides there, while frame-to-frame
  jitter and frames without tracked eyes are ignored (`frame_xr/ipd_smoother.h`). The log says
  `xr: IPD 63.4 mm (from the headset)` once, then on each change of more than 0.5 mm. The settled
  value is saved globally (`vr_last_ipd_mm`, only when it moves more than 0.5 mm) and used at the
  next start until SteamVR gives the eyes again, else 65 mm. Only when nothing was ever saved and
  no value arrives within 10 s of the session's start does a toast say "Couldn't Read Your IPD ·
  Turn the IPD Dial to Set It", then "IPD Set: 63 mm" once one arrives; read normally, nothing.
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
distance here is your IPD x 3D Depth / 10 x 10^multiplier game units, and the head moves the camera
by the same scale, so the **Depth Factor Multiplier** is the game's world scale. It is picked so
that at a 3D Depth of 90 the eyes are about as far apart as the 3DS's at full 3D, the depth the
game was made for; 3D Depth then turns it down. The presets (`frame_xr/presets.cpp`) set these
for a game whose properties have no value of their own. The presets were tuned with a 65 mm IPD;
at another IPD the eyes are that IPD x the same factor apart, so the world's scale is true to your
headset rather than to the 65 mm the numbers below assume.

| Game | Mode, registers | Camera (measured) | 3DS eyes at full 3D | Multiplier | Notes |
| --- | --- | --- | --- | --- | --- |
| Super Mario 3D Land | 3, view 90, eye 87,2 | 45 degrees, 5:3, title and levels | 58 units in a level | 100x (58.5) | at Wider View 1.4x black below the floor in mock runs; the owner plays it at 1.4x |
| Mario Kart 7 | 3, view 90, eye 87,2 | 52 racing, 55 in the fly-by before a race, 40 and 16 in menus | 3.4 | 10x (5.9) | menus stay odd in the Full modes (Differences from CitraVR) |
| Ocarina of Time 3D | 2 (view 4) | 55 in play (54.6 to 55.4: Jabu-Jabu's Belly wobbles), 35 to 70 in cutscenes, 5:3 without mods | (one flat picture) | 10x | leaning moves the camera; Link is about 56 units tall, so 10x at 3D Depth 30 is life size |
| Majora's Mask 3D | 2 (view 4) | 7 to 70 in the opening cutscenes, 30 to 55 measured in play (the owner's headset log; the real camera stays at 55), 5:3 | (one flat picture) | 10x | same engine and scale as Ocarina of Time 3D; the Project Restoration mod keeps the camera; Detect Game FOV Off in its preset |
| Asphalt 3D | 3, view 8 | not measured | | 1x | CitraVR's notes only |

AzaharFrame presets: the table above is what was measured. The values the presets ship are the
owner's own tuned per-game settings of 2026-11-12 (`frame_xr/presets.cpp`; each applies only
where a game has no value of its own and the global one is still the default): Mario Kart 7 FOV
40, Wider View 1.3x, Narrow Screen 30 % and Height 15 %, Internal Resolution 3x, ScaleForce, the
screen 4.95 degrees below the middle, 0.37 m, size 2.02; Super Mario 3D Land 10x, 3D Depth 100,
4x, screen 7.15 degrees below the middle, size 1.06; Kid Icarus: Uprising (normal view; Immersive stays Off by default, the player can turn it on) Internal Resolution 4x and Bicubic only; Ocarina of Time 3D and Majora's Mask 3D (every region, identical) FOV 55, Match
Game Camera off, 3D Depth 30 (no Narrow Screen of their own since AzaharFrame oot), screen 9.21 degrees below the middle, 3 m.

### The immersive screen per game

AzaharFrame immersive2, on the Frame (its view from the headset log), at the defaults and presets
(a game's own values win). Narrow Height is Off and the screen's pitch is the middle of the view
in all of them. Size 1 is the world at its own scale; the angles are the screen's.

| Game | Screen Shape | Narrow Screen | Wider View | Up and down | Distance | Size |
| --- | --- | --- | --- | --- | --- | --- |
| Any game (default) | 3DS (5:3), or as measured | Off (10 % before AzaharFrame oot) | 1.4x | 1.4x (1.76x before AzaharFrame oot) | 3 m | 1 (95 x 85 degrees at 55) |
| Ocarina of Time 3D | 3DS (5:3); with SSE as measured | Off (1 % before) | 1.4x | 1.4x (1.76x to 2.0x before) | 3 m | 1 (95 x 85; 16:9 99 x 89; widest 111 x 92) |
| Majora's Mask 3D | 3DS (5:3) | Off (1 % before) | 1.4x | 1.4x (1.76x before) | 3 m | 1 (95 x 85) |
| Super Mario 3D Land | 3DS (5:3) | Off (10 % before) | 1.4x | 1.4x (1.77x before) | 3 m | 1 (82 x 73) |
| Mario Kart 7 | 3DS (5:3) | Off | 1.4x | 1.4x (1.77x before) | 3 m | 0.9 (91 x 76, gain 1.11) |

The Size column's heights are from before AzaharFrame oot (the view's shape); the screen is now
as high as the picture's shape gives at that width. The widths were worked out at Narrow Screen 10 %, the default before
AzaharFrame oot; at Off (now the default, and the Zelda presets no longer set 1 %, the 10 % step)
the screen is about a tenth wider.

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

### Kid Icarus: Uprising: starts in the normal view

AzaharFrame immersive: Kid Icarus: Uprising (every region, a mode 0 preset in
`frame_xr/presets.cpp`) starts in the normal view: its preset keeps Immersive Mode Off and sets
nothing, so the player can still turn it on from the VR menu or its Properties, and
a global value the player chose wins as for any preset. Nothing is locked. The cause, from the
owner's headset test (shots and their effects came apart from the reticle): the game aims with a
reticle it draws on the screen in 2D and shoots along its own camera. In the Full modes the world
is drawn from a camera turned with the head, which the game does not know about, so the shots fly
off where the unturned camera looked while the reticle stays on the screen; and the 2D reticle
has no stereo depth (the game is shown a 3D slider of about 0) while the shots do. Nothing in the
drawing can join them again: the reticle cannot be told from the rest of the HUD. It is not Match
Game Camera (it measures perspective draws only, the HUD's projection is orthographic) and not
the eye offset (both eyes' world draws get it).

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
| `AZAHARFRAME_IMMERSIVE_ANCHOR` | 1 | 0: the Full modes' screen locked to the head at display time, as up to 1.2 (see below) |
| `AZAHARFRAME_IMMERSIVE_ANCHOR_MAX_MS` | 250 | 50..2000: a picture further behind the display than this (the game stopped drawing) goes back to the head |
| `AZAHARFRAME_IMMERSIVE_ANCHOR_EYE` | 1 | 0: the anchored screen at the picture's head pose, position too, as in 1.2.0 builds before it (slides against its scenery as the eyes move with a turn) |

## Head turns and the picture (AzaharFrame doubling)

The game camera follows the head pose the XR thread published when the game frame began
(`FrameLatch`), so a picture is right for that pose. Up to 1.2 the Full modes' screen was locked to
the head at display time: the picture came on view 25 to 70 ms later (a frame of the game, the GPU,
the compositor's lead) and stayed for 2 display frames (60 a second) or 4 (30 a second), all the
while moving with the head. On a quick head turn the world swung along with the head and jumped
back at every new picture: a double image in every game, at any resolution (the owner's report of
2026-11-12; Ocarina of Time 3D in the mock, at 54 degrees a second: pictures 62 ms old on average,
the head 2.1 degrees on, up to 4.5).

Now the picture carries its head pose (`VRData::pose`, `ContentTrack` in `video_core/vr_immersive.h`:
the frame that ended at a latch was drawn with the pose applied at the latch before), the renderer
tags the XR images with it (`XrPresentTarget::BeginFrame`), the release hands it back
(`Runtime::ReleaseFrame`, `ShownContentPose`), and `Immersive::Screens` places the screen in LOCAL
space where the head was (`AnchorPose`). SteamVR re-projects that layer for the head at display time,
so the world stays where it is between pictures; only the screen's edges move. A picture drawn
before SteamVR's last recenter, or more than `AZAHARFRAME_IMMERSIVE_ANCHOR_MAX_MS` behind, goes back
to the head. Once a second while a Full mode is on, the log says how old the pictures were, how far
the head turned since, how many frames the screen was anchored, and how many display frames each
picture stayed (`xr: immersive: 121 frames, 30 pictures, picture drawn 62.4 ms ...`).

AzaharFrame turns: a real head turns about the neck, so the eyes move sideways with every turn
(about 8 to 10 cm ahead of it). A screen left at the picture's head pose, position too, slid against
the scenery it shows as if that scenery were as near as the screen: r / distance of the turn, 22 %
at Mario Kart 7's 0.37 m (3 % at 3 m), back at every new picture, so the remaining double image on
very fast turns. `Layout::AnchoredQuadPose` keeps the picture's orientation about where the eye is
now (`AZAHARFRAME_IMMERSIVE_ANCHOR_EYE`, default 1): far scenery is seen exactly where it was drawn,
as in a rotational reprojection, and the game camera follows the head's position on the next picture
as before. A second line each second says how far far scenery on the anchored screen was from where
its picture was drawn, and how far it moved from one display frame to the next
(`xr: immersive: far scenery on the anchored screen ...`). The rest of a fast turn's slip is the
gain: the game camera turns `gain` times the head, which keeps the world still at the screen's middle
only when the gain is the game's real angle over the screen's (Detect Game FOV Always, or an
Immersive Game FOV that is the game's real angle, such as the one Once measured).

AzaharFrame turns2: the same for any settings. In mode 2 the one quad both eyes see was kept about
the middle of the head, so each eye, half the IPD to the side, was off by half the IPD / distance of
the turn: 1 % at 3 m, the presets' distance, but 11 % at 0.3 m. Each eye's quad is now kept about
that eye in both Full modes (`Layout::AnchoredFullQuad`, the quad `Immersive::Screens` shows), so
each eye sees its screen exactly as when the picture was drawn, in the same directions. Nothing in it
depends on the distance, size, pitch, Immersive Offset, Immersive Game FOV or Screen Shape, Match Game
Camera, Wider View, Narrow Screen or Height, a zoom, the view or the IPD: a unit test sweeps 600 random
combinations of them (both Full modes) with four random head turns each (yaw, pitch, roll about a
neck behind and below the eyes, and a lean) and checks every point of the screen from each eye: the
largest error is 0.0001 degrees, and with the head where the picture was drawn the placement is the
one before (`the anchored screen keeps far scenery still for any settings`). The per-second line
measures from each eye as located against where that eye saw the picture's middle when it was
drawn; before, mode 2 measured from the middle of the head, which hid that error.

AzaharFrame oot: the tag follows the world, not the swap. A game frame's swap is not always the
frame its world was drawn in: Ocarina of Time 3D swaps 30 times a second, and 60 with the owner's
single-screen mod, which still has to be shown to draw a new world at each. A game that draws its
world every other frame and only its HUD in between, or copies the last picture into the
framebuffer before it draws the next world, got the pose of the latch before on pictures whose
world was drawn one game frame earlier: on a head turn the anchored world stepped by one game
frame's turn and back, a double image the far-scenery line could not see, as it trusts the tag.
`ContentTrack` now notes the pose of every draw through the patched world camera (`OnWorldDraw`,
from the rasterizers' draws), which pose's world each display transfer or texture copy took to its
address (`OnTransfer`, `GPU::MemoryTransfer`), and at the top screen's swap tags the picture with
the newest copy into the framebuffer shown (`OnTopScreenSwap`). Where nothing was copied into it in
the last three latches (a game that draws straight into its framebuffer) or at the VBlank fallback,
the earlier rule stays. It needs no setting or game value. Once a second the log says
`xr: immersive: N top screen swaps, ... without a world draw, ... without a copy into a
framebuffer; the picture's world drawn with the latch's pose ..., an earlier one ... (the head ...
degrees on ...)`. `AZAHARFRAME_IMMERSIVE_WORLD_TAG=0` tags by the latch again (A/B runs);
`AZAHARFRAME_IMMERSIVE_FRAME_TRACE=1` adds the world draws of each swap in order and the distinct
world projections each second.

Two more switches with it, for every mode (docs/TECHNICAL_NOTES.md): both eyes' top images in one
swapchain (`AZAHARFRAME_XR_PACKED`) and one game frame's screens released together
(`AZAHARFRAME_XR_SET_RELEASE`), so the two eyes never come from different game frames.

## The game's own 3D on the screen (AzaharFrame stereo)

Mode 2 shows the game's own two eyes on one screen: the game shears each eye's projection by the
3D slider it is shown (3D Depth / 100), so its far scenery is that far apart in the two pictures,
and the screen magnifies it. The rasterizer reads where each world draw's projection puts a point
at infinity across the picture (`FarAcrossFromRows`, `video_core/vr_view_matrix.h`); each top
screen frame's draws fall in one group per eye (`StereoFrame`): two with the game's 3D on, one
when it draws its world once (Ocarina of Time 3D's item screen: the paused world, mono, behind
it). Once a second the emulation thread logs `xr: immersive stereo: N top screen swaps with the
game's two eyes, ... with its world drawn once, ... with no world camera; ... in the 3D format;
far scenery X % of the picture's width apart`, and the XR thread puts that on the screen
(`StereoFarOnQuad`, `immersive_layout.h`: the picture is 1/factor of the screen's width, mode 3's
eye quads are already the IPD apart): `xr: stereo far N deg, eyes allow M deg (...)`, with a
warning when the eyes would have to diverge (more than 0.1 degrees past parallel). Measured in mocks with the owner's settings (Ocarina of Time 3D: screen 105
degrees wide at 3 m, IPD 64 mm): 3D Depth 10, 30 and 50 give 1.17, 3.44 and 5.86 % (the far
scenery 1.8, 5.2 and 8.9 degrees apart against 1.22 allowed); the game reads the slider live (a
change shows within a second). Majora's Mask 3D at 50 %: 3.78 %, 3.9 degrees. In mode 3 (slider
0.01) Mario Kart 7 and Kid Icarus show 0.05 to 0.07 %. `AZAHARFRAME_TEST_DEPTH="17:10,19:50"`
sets 3D Depth at those seconds (mocks).

**The stereo cap (mode 2 only, while 3D Depth is above 0).** The game's 3D is linear in the
slider, so the emulation thread also measures its far width per slider unit (`FarPerSlider`: frames
with two eyes, the slider held for 4 swaps, the median of the last 9). Each displayed frame the XR
thread works out the largest slider at which the far scenery is still 0.05 degrees inwards of
parallel (`StereoSliderLimit`), from the live terms only: that measure, the screen's width
(its angle and the factor), its distance and the IPD. The game is shown min(3D Depth / 100,
that limit) (`StereoCap`, `common/frame_vr_depth.h` `frame_vr_stereo_cap`, read by HID and the
shared page): a higher limit counts only once it is half a percent above the one in use, and the
slider follows a moved limit at most 4 (down) or 0.25 (up) a second, so it never jitters; a change
of 3D Depth itself goes through at once, up to the limit. The 3D Depth setting is never rewritten,
and the head-motion scale (10^multiplier x (3D Depth / 10)) keeps using it, never the capped
slider. Modes 0, 1 and 3 are unchanged. The log says `xr: 3D Depth N % limited to M % so far
scenery never diverges (...)` when it takes effect or moves by half a percent, and `no longer
limited` when it lifts. With the owner's screen (105 degrees wide at 3 m, IPD 69.9 mm) Ocarina of
Time 3D's limit is 7.7 %.

**Menu screens at the game's own size (mode 2, a heuristic).** Wider View (Render Margin) makes the
game draw past its frame, and its own full-screen 2D (Ocarina of Time 3D's item screen) is then
shown smaller, with the margin's world around it. While all of these hold, the game draws with
factor 1 (no margin), eased in and out over 250 ms (`MenuExact`, `immersive_layout.h`): mode 2;
the slider the game is shown is above 0; the game drew two eyes earlier since it started (a quick
save loaded in it keeps that; another game starts afresh); and for
4 top screen swaps in a row its world is drawn for one eye only, or not at all (`WorldOnceRun`,
`video_core/vr_view_matrix.h`). Measured: Ocarina of Time 3D's item screen draws its paused world
once, while gameplay, standing still included, draws two eyes in every frame; Mario Kart 7, Kid
Icarus: Uprising and Majora's Mask 3D (its title and cutscenes) never trigger it (Mario Kart 7's
menus draw one eye but never after two; Super Mario 3D Land's title draws its world on every other
swap only). It reads the game's draws, not its screens, so a game could show something else with
its world drawn once (a 2D cutscene over a paused world); then that is shown at the game's size
too. The log says `xr: immersive: the game shows a screen of its own (its world drawn once): at
its own size, Wider View N eased to 1.0x` and `the game's world in both eyes again`.
`AZAHARFRAME_IMMERSIVE_MENU_EXACT=0` turns it off (always the margin, as before), for A/B tests.

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
