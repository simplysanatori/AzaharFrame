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
| Left grip, right grip (squeeze) | (none) | Grab a screen, see below. Pressed above 0.60, released below 0.40. In Flat (`grip_dpad_cycles_layout`) a grip held + D-pad right or left is `cycle_layout` +1 or -1, and the D-pad stays away from the game until let go. |
| Left trigger | ZL | Always, except on the visible bottom screen (touch), see below. Pressed above 0.55, released below 0.40. |
| Right trigger | ZR | Same. |
| Left stick | Circle Pad | Radial deadzone 0.12, rest rescaled. |
| Right stick | C-Stick | Same. |
| D-pad up / down / left / right | D-Pad up / down / left / right | The Frame's real D-pad (replaces CitraVR's thumbrest-touch plus stick trick). |
| Menu | Start | A tap: Start, sent as Menu is let go (for 100 ms). Held 1 s: the in-game menu, see below; the game never sees that hold. |
| View | Select | Hold 1.5 s instead stops the game, see below. |
| Left stick click, tap | (none) | Hides or shows the screen that is not in focus (`toggle_other_screen`, on release). |
| Left stick click, held 1 s | (none) | `cycle_layout`: the next Screen Layout (VR: Stacked or Side by Side; Flat: Large Screen, Side by Side, Default). The release hides nothing. Azahar has no HOME Menu without system files, so there is no Home binding. |
| Right stick click, tap | (none) | Swaps the screens (`swap_screens`, on release). |
| Right stick click, held 1 s | (none) | Resets the screen positions (`reset_panels`, once per hold). |

A stick click is a `ClickGesture` (`click_gesture.h`): a tap acts when it is let go, a hold acts once at
1 s and its release does nothing. A click that starts while its stick is pushed past half way
(`reset_click_max_deflection`) is ignored whole, tap and hold, so free camera cannot swap, hide or reset
by accident. The right click is still reported to the pad as `StickClickR` (unbound by default).

| Left bumper + right bumper + View | (none) | Only in games that hide the bottom screen (Hide Bottom Screen, immersive mode): shows or hides it. That View press never reaches the game. Everywhere else the game sees L, R and Select. |

View pressed with no bumper down is held back for `chord_grace_ns` (150 ms), since a bumper or the
Right Trigger may follow (Quick Save, Quick Load, Fast Forward). After that Select follows View; a
tap shorter than the wait is sent late as a 100 ms pulse. While the stop hold is running the game
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

`motion.h`. The controller chosen by `vr_motion_controls` (0 off, 1 right, 2 left, 3 headset; default right; AzaharFrame kiu: per game too, Kid Icarus: Uprising's Controls preset starts it Off, AzaharFrame kiugyro: Right Controller when the C-Stick & Gyro mod is installed; the headset uses the located view-space head pose, `InputFrame::head`, through `HeadAsMotionSource`)
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
acceleration). AzaharFrame kiugyro: **Gyro Sensitivity** (`vr_gyro_sensitivity`, per game too,
0.2x to 4.0x, default 1.0x) multiplies the gyroscope in every game; the accelerometer is not
scaled.

## Aiming (Gyro Aim in the code; AzaharFrame kiugyro)

Settings, General, under Motion Controls: **Aiming** Off, Direct Aim, Gyro Aim (`vr_gyro_aim`, per
game too; stored as before: 0 Off, 1 Gyro Aim, 2 Direct Aim, so older builds read the same
value; the code calls Direct Aim "Light Gun" and Gyro Aim "Gyro"; `Menu::AimingIndexOf`),
**Recoil**, **Gyro Sensitivity**, and with Direct Aim **Turn Zone** (`vr_light_gun_turn_zone`, 0
to 30 % of the screen, default 10 %) and **Turn Speed** (`vr_light_gun_turn_speed`, 0.2x to
4.0x, default 1.0x). Made for Kid Icarus: Uprising with Darkshade's C-Stick & Gyro mod (v172), in
any of the game's own control schemes (AzaharFrame kiufix2); its Controls preset starts
Aiming at Direct Aim when the mod is installed (`ControlsPreset::gyro_aim_with_mod`). `gyro_aim.h`,
`kiu_scan.h`, `kiu_probe.h`; the disassembly is in handoff/reports/37/kiugyro/design.md.

- **What the mod reads:** once per game frame, the latest HID gyroscope entry: z as the
  reticle's horizontal rate, x as its vertical rate (deg/s), less a 0.25 deg/s dead zone, at
  most 360; into the same synthetic touch drag as its C-Stick, so on the ground the game's aim
  box turns the camera when the drag runs past it. A held touch stops it.
- **The controller:** Motion Controls' own, or the right controller when that is Off.
- **Gyro:** the ray's yaw rate (about the room's vertical) goes to 3DS z, left positive (a turn to
  the right is negative z, as Motion Controls and a real 3DS read it), and its pitch rate to 3DS
  x, down positive, times Gyro Sensitivity, replacing Motion Controls' gyroscope meanwhile.
  Rolling the controller sends nothing. AzaharFrame kiufix: the first build sent right as
  positive z, so the reticle went left when the hand went right (the owner's headset check,
  2026-11-14); a unit test now holds Gyro Aim to Motion Controls' axes.
- **The game's controls** (AzaharFrame kiufix2): Direct Aim and Gyro Aim run in any of them.
  The disassembly (design.md fact 1) read the mod's gyro as its gyro scheme's only, and the first
  kiufix build would not aim otherwise, with a toast and a Mods tab line asking for the gyro
  scheme; the owner's headset check of 2026-11-14 aimed with the mod's gyro with the game's
  controls on C-Stick, so the gate, the toast and the line are gone. The game's scheme is still
  read with the weapon (`Kiu::ControlScheme`, the options object of the shots record, +0x163E8
  bits 21 to 23) and logged as it changes (`xr: Kid Icarus: the game's controls: scheme N`).
- **The C-Stick** (AzaharFrame kiufix2): with the game on C-Stick the mod moves the reticle with
  the stick and the gyro together, so the stick still aims and turns the camera with Aiming on.
  Direct Aim would undo it (every poll it sends the rate back to the pointer, and against a stick
  pushing past the box it pulled back), so while the right stick (`AnalogControl::StickR`, the
  C-Stick in the default profile, after the mapper's dead zone and the other gamepads;
  `CStickTakesAim`) is pushed, Direct Aim pauses as for its grip (`LightGunFrame::stick`) and the
  stick has the reticle to itself, Turn Zone included. Let go, or after the grip's pause,
  Direct Aim eases back in rather than snapping under the pointer: what `LightGunLoop` sends,
  the Turn Zone's push too, is scaled from 0 to 1 over 300 ms of game time (a smoothstep,
  `LightGunLoop::kEaseInNs`), and it learns nothing until then, so the stick's moves are never
  taken for the gyro's. The C-Stick is untouched (never zeroed or rebound); Gyro Aim adds the
  hand's turn to the stick's and has no pause. With the trace switch the reading's line ends in
  `C-Stick steers`, `paused` or `easing back in`.
- **Light Gun:** where the ray hits the top screen (`LightGunTargetFor`, the screen as drawn,
  `PanelGeometry::top`; a screen that cannot be pointed at, such as the immersive one, gives no
  target) is where the reticle goes. On the emulation thread, at each HID gyroscope poll
  (~101 Hz; `InputCommon::FrameXr::MotionState::SetGyroOverride`), the probe reads the reticle
  from the game, read only (the mod's state block + 0x20 -> the reticle object: +0xA4/+0xA8 the
  reticle, +0xE0/+0xE8 the screen's size, +0xE4/+0xEC its centre, +0x81 the ground box), and
  `LightGunLoop` sends the rate that brings it under the pointer: a Smith predictor (one game
  frame of latency) and a proportional term (two game frames), in emulated time, with the gain
  (pixels per degree, sign included) learnt from rest-to-rest moves and flipped when the reticle
  runs the other way; the HID service's slow-down stretch is divided out. On the ground the target
  is kept in the game's box (x 0.7 to 1.3 of the centre, y 0.4 to 1.25); the **Turn Zone** at the
  screen's sides (and top and bottom) adds a push past the box, depth 0 to 1 across the zone, one
  screen width (height) a second at the edge times Turn Speed, which the mod turns into a camera
  turn. In the air (no box) nothing is pushed. When the reticle cannot be read (not Kid Icarus,
  the mod's code patterns not found, no reticle, a value out of
  range) Light Gun uses Gyro instead and says why once in the log (`xr: Light Gun: Gyro instead:
  ...`); `xr: Light Gun: reticle found` when it reads it again, and every 5 s the reading, the
  target, the gain and the game frame.
- **Laser sight** (with Laser Sight on; AzaharFrame kiuguns): with Direct Aim, while its ray is on
  the top screen (AzaharFrame kiufix: whether or not the reticle is read from the game; it waited
  for that before, and showed nothing at all when Direct Aim had fallen back), the controller's
  beam is red and ends in a small red
  dot (`LaserDotSize`: half the pointer dot's) where the ray meets the screen, which is where the
  loop steers the reticle (on the ground, pointing past the box, the two part: the reticle waits
  at the box). The beam stays at full strength while the hand is still. A red copy of the beam
  art and the dot are in the vis atlas (`kLaserBeamRect`, `kLaserDotRect`), drawn once: no GPU
  work in a game. Otherwise the usual white beam (`Input::LaserSight`, `Vis::Visuals::SetLaser`).
- **Calibrate Aim** (AzaharFrame kiufix; Direct Aim, optional, per game): the Controls page's
  Calibrate Aim row, Start, closes the menu; toasts ask for the top screen's centre, then its
  top-left and bottom-right corners, each a press of the aiming controller's grip (the reticle
  waits meanwhile). The centre alone gives an offset; with the corners a line is fitted per axis
  (least squares: offset and scale), u' = a + b u, applied to Light Gun's target and to the laser's
  dot (`AimCalibration`, `FitCalibration`, `AimCalibrationFlow`, `PointOnQuad`). A point that
  would give an offset past half the screen or a scale outside 0.5 to 2 is asked again. Kept in
  `config/custom/<title id>.aim` (one line, `v1 au bu av bv scaled`), not a settings key, so an
  older build neither reads nor drops it; Reset deletes it; the menu opening stops a calibration
  where it is (what was saved stays). Computed from the player's own pointing, never a preset.
- **Grip:** the Gyro Aim controller's grip held (above 0.60, let go below 0.40) pauses it (Gyro:
  zero rate; Light Gun: the reticle stays) and grabs nothing; letting go does not jump (Gyro
  starts again from the hand's new place; Light Gun goes back to the pointer). The other
  controller's grip still grabs, moves and sizes the screens (World Scale, grip + stick).
  Not with Headset (no grip).
- **Fire:** while Gyro Aim is on, a trigger that is ZL/ZR (not touching the bottom screen) also
  presses the Left Bumper's control (L in the default profile): the game fires with L, so the
  aiming controller's trigger fires, and the left trigger too. The mod also writes ir:rst's
  ZL/ZR into the game's held buttons (its poll at 0xD33C7C), so ZL/ZR reach the game as well.
- **Recoil** (`vr_light_gun_recoil`, per game too, on by default; shown under Gyro Aim while it
  is not Off): a pulse on the aiming controller's haptics for each shot (full amplitude, 40 ms,
  the runtime's frequency, as Moonlight XR's light gun). The game's own shots are not read: a
  shot is the fire press, then one every 125 ms while fire is held (`Recoil::kRecoilPaceNs`, one
  constant for a weapon's rate later), and a press after 1.5 s without firing gets a 90 ms pulse
  (a heuristic for the charged shot). Never in menus or with Gyro Aim off. The haptic output is
  suggested with the bindings, and without it if the runtime refuses it (`Actions::Pulse`).
  AzaharFrame kiuguns: in Kid Icarus: Uprising the pulses follow the game's own shots instead
  (`ShotRecoil`, `kiu_weapon.h`): the save's shots-fired record (`kiu_scan.h ScanForShots`,
  [0xD636C4] + 0x16454 EUR and USA, [0xD6365C] + 0x16454 JPN), read about once a game frame on
  the emulation thread; a read with new shots is one pulse (60 ms for several), a shot after
  1.5 s without one keeps the 90 ms pulse (the game marks no charged shot that was found); a
  drop or a jump of the record (a save loaded) is none. Unreadable: the timed pulses
  (`xr: Recoil: on the fire button (timed)`).
- **Weapon in Hand** (`vr_light_gun_weapon`, per game too, on by default; shown under Recoil;
  AzaharFrame kiuguns): in Kid Icarus: Uprising the aiming controller's model is the weapon Pit
  holds (`kiu_weapon.h`: the game's getWeaponId record, slot 0 in single player only, settled
  for 0.5 s; the models from the player's own game, `core/frame_weapon_models.h`, fitted by
  `vis/weapon_fit.h`). The controller is drawn until the weapon is ready, for no weapon, with
  another fighter in the records (multiplayer) and in menus. Every class's muzzle (the json's
  `muzzleNode_` + `muzzleOffset_`) is on the aim ray: held classes are turned so it is, worn ones
  raised onto it, the orbitars' middle is on the hand's line; a bow shoots from the hand.
- **Muzzle Flash** (`vr_light_gun_flash`, per game too, on by default; shown under Weapon in
  Hand while it is on; AzaharFrame kiuflash): a flash at the weapon's muzzle on each of the
  game's own shots (the same shots record as Recoil, read for either; `vis/muzzle_flash.h`). The
  weapon's own flash from its `character/effect.bcres` (rapid fire's, or the charged shot's after
  Recoil's 1.5 s pause), made with the weapon (`core/frame_weapon_models.h` ConvertFlash) as up to
  four cards per kind: faces turned to the eye a little ahead of the muzzle, fans along the ray.
  Added to what is behind, 70 ms (charged 100 ms), fading as it grows a little. A weapon whose
  file has no flash borrows its class's first, in neutral colours. Recoil and Laser Sight are
  untouched by it. Test switch (live): `AZAHARFRAME_KIU_FLASH=size,ms` (a size factor, a
  duration).
- **Laser Sight** (`vr_light_gun_laser`, per game too, off by default; shown with Direct Aim;
  AzaharFrame kiuguns): on, the red laser sight above, from the weapon's muzzle when the weapon
  is drawn (`ControllerModels::WeaponMuzzle`), else from the controller; off, the aiming hand has
  no beam or dot over the top screen with Direct Aim, or with the weapon in hand (the game's
  reticle shows the aim; the weapon is on the same ray). The bottom screen, menus and the room
  keep the beam (`Input::AimBeamFor`). Test switches (live): `AZAHARFRAME_KIU_FIT_<CLASS>` (BLADE, STAFF,
  CLAWS, BOW, PALM, CLUB, CANNON, ORBITARS, ARM) `=dx,dy,dz,yaw,pitch,roll,scale` (cm, degrees,
  a factor), `AZAHARFRAME_KIU_WEAPON=<id>` (0 to 107: show that weapon), and
  `AZAHARFRAME_KIU_WEAPON_GLOW=1` (draw the effect cards too).
- **Trigger:** while Gyro Aim is on, the Bottom Screen Reticle steps aside (its setting is kept;
  the shelved C-Stick at Edge with it), so the trigger is ZL/ZR, which the mod fires with, and never a touch,
  which would stop the mod's aim. The controller keeps its beam; pointed at the bottom screen its
  trigger touches there as usual.

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
  deflection), and its stick X makes them bigger (right) or smaller (`top_scale_stick`; the scale
  changes by e^(0.5 x seconds) at full deflection, 0.5 to 3 times; above 1 the games get the 3D
  depth setting divided by it, `ScreenRig::DepthScale`, `common/frame_vr_depth.h`). That hand's stick does not
  reach the game while the drag lasts; the other hand's does.
- While dragging, the D-pad turns the pair 0.5 degrees per press about the head (same distance,
  turned to face you; held: again after 0.4 s, then every 0.1 s; at most 60 degrees from straight
  ahead and never nearer than 1 m ahead) and a "Moved Right" (etc.) toast shows for 2 s. The
  D-pad does not reach the game during the drag, nor a direction pressed then until it is let go.
  The mapper takes the grab point again from the moved screens (`Mapper::ReanchorDrag`), so the
  hand carries on from there. `input/screen_rig.h`.
- Horizontal lock is on by default as in CitraVR: dragging never moves the screens sideways (the
  target keeps the top screen's x: 0 unless the D-pad moved it). `MapperConfig::horizontal_lock`.
- AzaharFrame move, **Detach Screens** (`vr_move_screens`, read every frame by `controls.cpp`):
  the mapper reports the held screen as drawn and the hand (`drag_panel`, `drag_hand`), and
  `ScreenView::InMainSlot` gives the slot. Off: as above. Secondary: the main slot as above, the
  second slot free. Both: both free. A free slot is moved by `ScreenRig::FreeGrab` (as the stats
  panel: round the eyes on its sphere, facing them, the stick sets its distance 0.5 to 10 m and its
  own scale 0.5 to 3), D-pad left and right turn it about the eyes (`NudgeFree`), up and down are
  the Screen Curve only while the main slot is held. The first grab in Secondary or Both detaches the
  second slot where it is drawn (`ScreenRig::Detach`); the reset, Off and a new Screen Layout
  link it again. Placement per game: `screen-placement.txt` (`ScreenRig::PlacementFile`).
- The top screen goes no nearer than 1 m (`ScreenRig::kNearestTopZ`) and no further than 10 m.
  The bottom screen follows it, 0.5 m nearer, tilted back 45 degrees, 0.25 m under the top
  screen's bottom edge (0.75 m below its middle at size 1). It keeps its own size.
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
bumper press to the switch (holding them back would delay every L and R). The four going down in
any other order (View first, say) also switch, on the frame the last one goes down, unless that
View press already toggled the bottom screen. Outside a game (the home
panel, the in-game menu, flat without a game) `VrUi::ModeChord` finds the four itself.

After any switch, `VrUi` arms `ChordLatch` (`chord_latch.h`) in `RequestToggleVr` and again when
the new mode begins (`Create`, `DestroyXr`): the four buttons read as up for everything (mapper,
`ModeChord`, panels) until all four were seen up, no earlier than 1 s after the first frame of the
new mode. Waiting for the release alone was not enough: in flat, SDL opens Steam Input's gamepad
anew after the session ends, its first read is all up, and the still-held chord then came in as a
fresh press and switched back to VR 0.3 s later.

**Quick Save, Quick Load.** View pressed first (no bumper down), then one bumper: `quick_save`
(right) or `quick_load` (left) when that bumper is let go with View still held (or both let go in
the same frame), at most once a second. The other bumper or Menu going down while the bumper is
held cancels it (a chord being formed), and so does letting go of View first. The bumper and View
stay away from the game until let go either way.

**Menu hold.** A bumper or View going down while Menu is held cancels the hold: no in-game menu,
and no Start when Menu is let go (`menu_cancelled_`). In the in-game menu, the hold that closes it
is cancelled the same way (`VrUi::MenuFrame`).

**Fast forward.** View pressed first (with no bumper down, as for Quick Save), then the Right
Trigger: `toggle_fast_forward` when the trigger is let go with View still held, once per pull and
at most once a second (`quick_cooldown_ns`). A bumper or Menu going down meanwhile cancels it.
That pull is neither ZR nor a touch until the trigger is let go, and View stays away from the game
(no Select, no stop hold) until it is let go. The Left Trigger with View is plain ZL. The Qt side
keeps the state: Azahar's turbo speed (`SetTurboEnabled`) and the "Fast Forward" and
"Normal Speed" toasts (`FrameXr::SetFastForwardShown`, rows of the toast atlas in `hud.cpp`, 2 s each), off again when the game
stops, the in-game menu or the Flat menu opens or the mode switches.

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
screen toggle. Menu held 1 s opens the Flat menu (`VrUi::FlatMenuFrame`: the game paused, the
window out of Fullscreen with its menu bar; no mapper while it shows, Menu held again or B closes
it, B with a Qt menu or dialog open sends it Escape), View held 1.5 s stops the game, the chord
goes back to VR. Buttons held when the mode changes (the chord itself) stay away
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
| Circle Pad | `engine:frame_xr,analog:0` (left stick) |
| C-Stick | `engine:frame_xr,analog:1` (right stick) |
| Home, Debug, Gpio14, Power | empty |
| Motion | `engine:frame_xr` (set at VR start-up in place of the default `motion_emu`, see Motion) |

The Controls dialog shows these as "Frame A", "Frame Left Grip", "Frame Menu", and so on, and its
"press a button" capture works for the Frame controls (pollers). Unused engine inputs: grips
(`button:8`, `button:9`, the screen grab also fires) and right stick click (`button:17`, the reset
gesture also fires).

## Touchpad Touch (AzaharFrame touchpad)

Settings tab Touchpad Touch: Off, Touch or Click. A gamepad touchpad (the one Edit Controls chose, else the one touched last, else the first controller that has one; the right pad when it has two) maps absolutely onto the bottom screen, with a dot where the touch is. Touch: finger contact touches. Click: the pad click touches and contact only moves the dot. The mouse over the render window (a touchpad that Steam Input maps to the mouse) maps the same way over the window, and its left button touches in both modes; its dot fades 2 s after the pointer rests. The beam, the pad and the mouse share one touch: the last to touch wins, the owner's release ends it, and a source that stayed down must let go before it touches again. A hidden bottom screen has no pad or mouse touch. Logic: `touchpad_touch.*`; feeds: the gamepad hub (`gamepads.*`) and `FrameXr::GetMouseState`.

Recentre at Edge (AzaharFrame padsui; Edit Controls, per device, on by default): a touch dragged into the screen's edge (within 2% of it) lifts and, 50 ms later, touches again at the middle; the pad then moves the touch by its motion from there until the finger lets go, so a turn or an aim can go on. Kept with the pad's mapping (`recentre=0` when off).

## Other gamepads in VR (AzaharFrame pads)

In VR, with a game running and the in-game menu closed, every SDL gamepad (Bluetooth or USB, with
or without Steam Input) joins the Frame controllers. Its inputs are taken by position (`PadInput`,
SDL's button order then the two triggers; a Nintendo pad that SDL reports by label is turned back),
its own 3DS mapping turns them into 3DS buttons and sticks, and those press the Frame controls the
controls profile binds to the same 3DS buttons (`ProfileRoutes`, pushed by the Qt side with the
profile): buttons ORed, the stick pushed further wins. So a 3DS button the profile does not give a
Frame control cannot be pressed from a gamepad either. Default mapping: the right button A, the
bottom B, the top X, the left Y (by label with Face Buttons: By Label), shoulders L and R, triggers
ZL and ZR, Start and Back Start and Select, the D-pad, the left stick the Circle Pad, the right stick
the C-Stick. Mappings are per model in `user/controller-mappings.txt` (key: SDL's GUID, or
`steam:<vendor>:<product>` behind Steam Input; found again by vendor, product and name when the
pad switches between the two). The Frame's own pad as SDL sees it (Valve's vendor id, not one of
Valve's gamepads) is never merged; a pad that only ever presses with the Frame (Steam's echo of it)
is dropped after three presses (`EchoGuard`). The menu's API is `devices.h`; `AZAHARFRAME_PADS=0`
turns the merge off. Tests: `src/tests/frame_xr/gamepads.cpp`.

## The mouse in VR (AzaharFrame padsui)

A mouse is read from its event node (relative counts, so it works with no window under it) and listed in Edit Controls as Mouse once plugged in. It is found in `/proc/bus/input/devices` by what it can do (`ClassifyInputDevices`): relative X and Y and a left button, on any bus, not Valve's; a Bluetooth mouse through uhid has no `mouseN` handler, so handlers do not count. A keyboard's mouse interface (a node of a device that also has a keyboard with LEDs, such as a Keychron's "Mouse" node for its mouse keys) is read but counts, clicks included, only once it reports motion, which holds for the run. The list is read again every 2 s and when a node stops reading, so a Bluetooth mouse that sleeps and reconnects (a new event node) comes back by itself. Each pointer-like device gets one log line whenever the list changes (`xr: mouse: "MX Master 3 Mouse" (Bluetooth 046d:b023, event8): a mouse`; no addresses). Its mapping is the line `mouse` in `user/controller-mappings.txt`: `buttons=` holds `MouseInput`s (0 Left Click, 1 Right Click, 2 Middle Click, 3 Back, 4 Forward, 5 Wheel Up, 6 Wheel Down) by 3DS button, and `touch=`, `menu=`, `mode=`, `speed=` (Cursor Speed, 0 to 4), `recentre=0` and `use=0` are written only when not the default. Default: Right Click A, Back L, Forward R, Left Click touches, Middle Click Open Menu, Bottom Screen, Cursor Speed Normal (the middle of five steps: 0.5, 0.7, 1, 1.4 and 2 times), Recentre at Edge on, used. A wheel notch is a 50 ms press. Lines without the new keys read as before.

- **Bottom Screen:** a cursor of its own on the bottom screen, moved by the counts (0.3 pixels a count at Normal, times Cursor Speed) and kept on it, with the touchpad's dot. The touch input touches there; held, it drags; Recentre at Edge as above (a drag pushed past an edge). The touchpads and the mouse go through one cursor pipeline (`TouchpadTouch::CursorStep`): a source gives its point (a touchpad, absolute) or its motion (the mouse, relative), and the clamp, click to touch, hold to drag and Recentre at Edge are the same code.
- **Pointer:** the mouse turns a ray about you, fixed in the room (yaw and pitch, 0.03 degrees a count), which takes the slot of a hand with no controller awake (the right one first; with both controllers on, the right one's, the one used last has it), so a controller in the other hand keeps its own ray (`MousePointerHand`, `HandInput::mouse`). The menu, every tab, Edit Controls, the dialogs, the keyboard panel, the screens (the touch input touches the bottom screen) and the grab bars take it as a beam. It wakes on a deliberate move (40 counts within a second) where you look; the controller of its slot moved on purpose takes its ray back (the other hand's never does). Cursor Speed scales its 0.03 degrees a count. Its dot is on what it hits, else on the plane of the panel it just left (30 cm past its edge, for the grab bars), else 1.5 m out. The wheel scrolls the menu's lists. Adapted from Frametop's pointer helper (`mouse_pointer.*`).
- **Switching:** Mouse Mode in Edit Controls, or Left and Right Click held together for 3 s (a ring fills round the dot; the toast "Mouse: Pointer" or "Mouse: Bottom Screen"). Left and Right Click go to the game at once; when the other goes down within 80 ms, both are let go for the game there and then (it may have seen a tap that short) and stay away until let go.
- **Open Menu** (Middle Click by default) opens and closes the in-game menu.
- In VR the render window leaves the mouse's clicks alone while a mouse is plugged in (the window's touchpad-as-mouse path then waits too).
- **Edit Controls' "Press a Button…"** (AzaharFrame input2, every device): after 3 s with no answer it counts 3, 2, 1 and closes, the binding as it was (`Remap::Tick`). The pointer's select never becomes the answer: in Pointer mode the mouse's touch input (what clicks the menu) is never reported or kept from the pointer while listening (`BeginListen(id, ignore)`), so Cancel and the chips still click; in Bottom Screen mode it may be bound, but not in the first 300 ms. A press held as listening starts counts only after a release. The Frame's triggers (the beams' select) cannot be bound at all.

With Frametop installed: Frametop's input relay grabs a mouse to drive its own 3D mouse, so Azahar for Frame sees none of its motion. Frametop pauses itself while a VR game runs (Pause while a VR game runs on, the default), and paused, its relay passes the mouse on through its virtual mouse ("frametop virtual mouse"), which Azahar for Frame reads when the real one is quiet. If the mouse does nothing in VR, pause Frametop: its Pause/resume Frametop action (a mouse button, key combination or controller button you mapped), or `input/ft-pause on` in Frametop's folder; `input/ft-pause off` resumes it after.

**Every device at once.** The Frame controllers, every gamepad, the mouse and the keyboard feed the game together: their 3DS buttons press the Frame controls the profile gives those 3DS buttons, ORed, and of two sticks the one pushed further wins. Edit Controls' device row only chooses which device's mapping is edited; each device but the Frame Controllers has Use This Device (`use=0` when off: it reaches nothing). A dot on a device's chip shows it is connected (hollow when not used). The 3DS has one touch: the newest press of the beams, a touchpad or the mouse owns it until it lifts, and a source still down after losing it touches again only after a new press.

Tests: `src/tests/frame_xr/mouse.cpp`, `gamepads.cpp` (every device in one frame), Edit Controls' in `src/tests/frame_xr/remap.cpp`.

## A physical keyboard as a controller (AzaharFrame keys)

A keyboard (USB or Bluetooth) plays the game in VR alongside the other devices, and is listed in Edit Controls as Keyboard once one is connected. Keyboards are found as the game's keyboard finds them (`ClassifyKeyboards`, the same test as `ParseKeyboards`: an event handler and a keyboard's keys, A to Z, Space, Enter and Backspace, not Valve's, so a controller in its keyboard mode is never a keyboard) and read from their event nodes (`KeyboardReader` in `devices.cpp`), every keyboard into one device with one mapping. A keyboard node without LEDs (a mouse's own keyboard node, such as the MX Master's "Keyboard", for its programmable buttons) counts only once a key goes down on it, which holds for the run. The list is read again every 2 s and when a node stops reading, so a Bluetooth keyboard that sleeps and reconnects comes back by itself. A keyboard's mouse interface (a Keychron's "Mouse" node) stays the mouse's, as above. Nothing is grabbed (the desktop keeps the keyboard) and no key is ever logged: one log line per keyboard when the list changes (`xr: keyboard: "Keychron Q1 Pro Keyboard" (Bluetooth 3434:0610, event7)`).

Keys are by Linux key code, not by layout (named by the US key caps: `key_names.h`), codes 1 to 127: every key of a full-size keyboard; the volume, mute and power keys are the system's and are never bound. The default is Azahar's own desktop keyboard (`citra_qt/configuration/config.cpp`'s `default_buttons` and `default_analogs`), so a game plays as it does in the desktop app:

| 3DS | Key | | 3DS | Key |
| --- | --- | --- | --- | --- |
| A | A | | L | Q |
| B | S | | R | W |
| X | Z | | ZL | 1 |
| Y | X | | ZR | 2 |
| D-Pad up / down / left / right | T / G / F / H | | Start | M |
| Circle Pad up / down / left / right | Up / Down / Left / Right Arrow | | Select | N |
| C-Stick up / down / left / right | I / K / J / L | | Home | B |
| Modifier (both sticks halfway while held) | D | | Open Menu | Esc |

Azahar's Debug (O), Gpio14 (P) and Power (V) keys are left out: nothing in VR reaches those 3DS buttons. Home, as on a gamepad, reaches the game only when the controls profile gives it a Frame control. Open Menu is Esc: nothing else uses it in the default, and Azahar binds no 3DS button to it.

- **Sticks from keys** as Azahar's analog from buttons: full tilt along an axis, a diagonal at sqrt(1/2) on each axis, times 0.5 while the modifier is held; up and down together cancel out. Like every device's stick, the one pushed further wins against the Frame's.
- **Typing and playing:** while the game's keyboard is open (`swkbd_shown`, or the game's keyboard reading keyboards), the keys type into it as above and none reaches the game or Open Menu (Esc is its cancel there); what is still held when typing ends stays away from the game until let go. So does what is held as the in-game menu closes, as for every device. A tap shorter than a frame still counts for one frame (the source counts presses).
- **Open Menu** (Esc by default) opens and closes the in-game menu in a game.
- **Edit Controls:** the 3DS drawing with the gamepad's chips (each 3DS button's key, the Circle Pad's and the C-Stick's four keys: "Arrow Keys", or the keys in the order up, left, down, right: "W A S D", "I J K L"), and, in the bar at the top where the title is (the mouse has Mouse Mode there), Open Menu and Modifier, both editable chips; the columns stay the gamepad's, 48 dp targets each. A button listens with "Press a Key…"; a stick takes four keys in turn, "Press a Key for Up…", "…Down…", "…Left…", "…Right…" (`Remap::Press`: each key swaps as a button does; Cancel or the 3, 2, 1 running out before the fourth puts all four back). Any key can be bound, Esc included; a key held as listening starts counts only once pressed again, and the key that answers never reaches the game or Open Menu. Reset to Default and Use This Device as for the other devices.
- **Mapping file:** the line `keyboard` in `user/controller-mappings.txt`, with only what differs from the default, so a later default reaches every key the owner never changed: a 3DS button by its name (`a b x y up down left right l r start select debug gpio14 zl zr home power`), `circle=` and `c_stick=` (up, down, left, right), `modifier=`, `menu=` (key codes, -1 none), `use=0`; the default has no line. Builds before this one skip the line.

Logic: `key_device.*` (pure: `KeyboardLogic`, `KeyStickValue`), `key_names.h`, the hub (`GamepadHub::PollKeyboard`, `Merge`), `VrUi::KeyboardTick` in `controls.cpp`. Tests: `src/tests/frame_xr/keys.cpp`; Edit Controls' in `remap.cpp`.

## Bottom Screen Reticle (AzaharFrame mouse)

Controls tab, under Touchpad Touch: Off, Left Controller, Right Controller, Headset (`vr_bottom_screen_reticle`, global, 0 to 3; Headset is AzaharFrame input2). The chosen controller (its aim pose) or the head aims a point on the bottom screen the way a stylus would, without pointing at the bottom screen: for the reticle of Kid Icarus: Uprising. In a game with the menu closed:

- **Aim is absolute, with edge drift.** The controller's yaw and pitch from a reference direction map onto the screen, which fills a cone 40 degrees wide and 30 high for a controller, 30 by 22.5 for the head (AzaharFrame input2: the mouse's Cursor Speed no longer sets it; at Fast it made the cone 21 degrees, 15 pixels a degree). The yaw counts across the cone at the reference's pitch (times its cosine), so a controller held pointing down moves the point as fast sideways as up and down. Where it points first is the middle. Pointed past an edge, the reference comes along and the point stays on the edge, so turning back moves it at once: nothing to clutch.
- **At Edge** (`vr_reticle_at_edge`, under it, shown only while the reticle is on): Follow (the default) is the edge drift above. Snap to Middle re-zeroes when the aim passes an edge, so the point goes back to the middle; a touch held then lifts at the edge (a release the game sees, for 50 ms and at least one frame; meanwhile the middle stays where it points) and touches again at the middle, and the drag goes on: a stylus's swipe, lift, swipe, which turns the camera in Kid Icarus: Uprising on foot. Another snap waits 200 ms after the touch comes back (or after a snap with no touch); meanwhile the edges drift.
- **Its trigger touches** at the point (pressed above 0.55, let go below 0.40); held, it drags. The trigger is then neither ZL/ZR nor a beam touch. It touches whether the bottom screen is shown or hidden; the dot shows on a shown one. It shares the one touch with the beams, the touchpads and the mouse (the newest press owns it).
- **Its grip re-zeroes** (the middle where it points now) and grabs nothing while the reticle is on; the other controller still grabs. Recentre (SteamVR's, or the right stick click held 1 s) re-zeroes too. Its other buttons keep their mapping.
- **No beam** (AzaharFrame polish): while it drives the reticle the controller draws no beam, puts no pointer dot on the screens and grabs no stats panel (`HandInput::reticle`, set by `ReticleDrivingHand`): it is not a pointer then. Only in a game with the menu closed and the controller tracked; in the in-game menu, Azahar's window (with or without a game) and the keyboard it is a normal pointer with its beam, and its trigger clicks.
- **Headset:** the head's direction aims; the trigger of either controller touches (either held is a touch; both triggers are then neither ZL/ZR nor beam touches); the grips keep their jobs, and only Recentre re-zeroes.
- **C-Stick at Edge** (shelved, AzaharFrame kiugyro: no row in the menu, Gyro Aim replaces it; the key `vr_reticle_edge_cstick` keeps its meaning for older builds, and this build acts on it only with the test switch `AZAHARFRAME_CSTICK_EDGE=1`, live, `TestSwitches::CStickEdgeOn`. What follows is what it does then.) (`vr_reticle_edge_cstick`, AzaharFrame kiu, per game too; earlier builds' Kid Icarus: Uprising Controls preset turned it on when the C-Stick mod was installed; AzaharFrame kiugyro: it starts Gyro Aim instead (catalogue ids `kiu-cstick-*` in `kCStickModIds`, or by hand a `code.bin` or a name with cstick in it; `CStickModInstalled`), and C-Stick at Edge steps aside while Gyro Aim is on): while the trigger touches and the reticle's point is within an edge band, the touch is handed to the C-Stick (AzaharFrame kiu2, `ReticleEdgeHandoff` in `TouchpadTouch`): Touch, Lead (the C-Stick pushed, the touch still down, 50 ms: a tap at the edge still lands), Stick (the touch lifted), Return (out of the band by a quarter of its width more: the touch back where the point is, the C-Stick still pushed for 50 ms), Touch. The push goes on the right stick's analog (`AnalogControl::StickR`, the C-Stick in the default profile), added after the mapper's dead zone and the other gamepads and capped at full tilt: per axis toward that edge, 0.15 where the band starts (past the mod's dead zone) to 1 at the edge. The band is 3 degrees of aim in the reticle's cone (`ReticleEdgeBandFor`: from the source and Reticle Speed), as a fraction of the width and of the height, clamped to 5 to 25 %. Nothing is lifted or pushed when the trigger is let go, during a grip's grab, while another source holds the touch, or with the switch off. While it is on the reticle follows the edges (Follow) whatever At Edge says, so Snap to Middle never takes the point out of the band; the stored setting stays. Why (a Kid Icarus: Uprising ground battle with the C-Stick & Gyro mod, reports/30/kiu2 and reports/30/kiuadv/plan.md): the mod turns the camera when its C-Stick cursor runs past the game's reticle box (yaw, and pitch up to a limit) and reads no C-Stick in a frame with a touch held; a touch held still at the edge turns nothing. The game's touch aim is relative, so the touch coming back carries on from where the C-Stick left the cursor. The stick and the touch overlap at each change, so the game never sees a frame with neither.
- **Aim Only While Pressed** (`vr_reticle_center_until_pressed`) and **Right Stick Moves Touch** (`vr_reticle_stick_moves`, AzaharFrame fixes7; the latter was Stick Moves Reticle): the point stays in the middle until the trigger touches; the right stick moves the touch point too and is not the C-Stick while the reticle is live (`SplitReticleStick`; off, the C-Stick is untouched).
- **Tremor:** a One Euro filter on the yaw and pitch (1.5 Hz at rest, faster as the hand moves: 0.25 Hz more per degree a second, the speed itself filtered at 2 Hz), then a 0.08 degree dead zone that drags the point (it follows once the aim leaves it, without steps). At 20 degrees a second the point trails by about 0.3 degrees (0.6 before input2), and a flick settles in about 17 ms. Tremor at an edge moves the reference at most by its own size (tested).
- **Why not a gyro mouse** (rotation as relative deltas, a grip to lift it): a stylus is absolute and so is the reticle that follows it; absolute aim gives the same point for the same direction every time and never drifts, and a relative mouse needs a dead zone against tremor that also swallows slow, fine corrections.

One-time toasts, once per install (`vr_hint_reticle_shown`, `vr_hint_mouse_shown` in the config, written half a second later): "Trigger Touches · Grip Re-centres" when the reticle is first set to a controller ("Head Aims · Trigger Touches" when that is Headset; shown as the game comes back), "Mouse Ready · Hold Both Buttons for Pointer" when a mouse is first found. Both fit one line of lane menuux's text-sized toast (`toast_fit.h`); the tab menu test checks it.

Logic: `reticle.{h,cpp}` (pure; `ReticleDrivingHand` is the mode), the source `TouchSource::Reticle` in `touchpad_touch.cpp`, the frame loop in `controls.cpp` (`VrUi::Frame` marks the controller, the game frame aims; the mapper's dot and `vis/visuals.cpp`'s beams skip it). Tests: `src/tests/frame_xr/reticle.cpp`, `input_mapper.cpp`.

## A physical keyboard in the game's keyboard (AzaharFrame input2)

In VR no window of the app has the keyboard focus under gamescope, so Qt sees no keys. While the game's keyboard (the 3DS software keyboard applet) is open in VR, `SwkbdPanel` reads every keyboard from its event node (`input/phys_keyboard.*`; found in `/proc/bus/input/devices` by the keys of a keyboard, not Valve's, so a controller in keyboard mode never types; read again every 2 s and when a node stops), and closes them when it closes. Nothing is grabbed. Keys by the US layout with Shift, Caps Lock and Num Lock (from the LEDs when reading starts); Ctrl, Alt or Meta held type nothing. Backspace, Delete, Left, Right, Home and End edit (and repeat); Enter (and keypad Enter) is the confirm button, Shift+Enter a new line in multi-line text, Esc the game's cancel button (none with one button). The panel's own keys work at the same time. While the XR side reads keyboards, the Qt event filter (`citra_qt/applets/swkbd.cpp`) passes no keys on, so nothing types twice. Tests: `src/tests/frame_xr/phys_keyboard.cpp`, `swkbd.cpp`.
