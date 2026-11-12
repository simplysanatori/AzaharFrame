# Settings

These are the settings Azahar for Frame adds to Azahar, and the few it changes. Everything else is Azahar's own.

## Where they are

The VR settings are in the **VR** group at the bottom of the **Graphics** tab.

- **Global** settings are in **Emulation > Configure**. They apply to every game, also when you open it from the in-game menu.
- **Per game** settings are also in a game's **Properties**: right-click the game in the list. A value set there wins over the global one for that game.

While a game with its own value runs, that row is greyed out in **Configure**.

**Detach Screens** and **Touchpad Touch** are only in the in-game menu. **Mouse Mode** and **Recentre at Edge** belong to a device and are on its **Edit Controls** page; they are kept in `user/controller-mappings.txt`.

## At a glance

| Setting | Choices | Default | Scope | Takes effect |
|---|---|---|---|---|
| [Hide Bottom Screen](#hide-bottom-screen) | on or off | off | global and per game | when the game starts |
| [Face Buttons](#face-buttons) | **By Position (Nintendo)**, **By Label (Xbox)** | By Position (Nintendo) | global | at once |
| [Background](#background) | **Black**, **Passthrough** | Black | global | at once |
| [Window Scale](#window-scale) | **100%**, **150%**, **200%** | 150% | global | when you press **OK** |
| [Motion Controls](#motion-controls) | **Off**, **Right Controller**, **Left Controller**, **Headset** | Right Controller | global | at once, or next VR start from Off |
| [Show Controllers](#show-controllers) | on or off | on | global | at once |
| [Show Stats](#show-stats) | on or off | off | global | at once |
| [Stats Position](#stats-position) | **In Front**, **On Wrist** | In Front | global | at once |
| [Screen Layout](#screen-layout) | **Stacked**, **Side by Side** | Stacked | global | at once |
| [Detach Screens](#detach-screens) | **Off**, **Secondary**, **Both** | Off | global | at once |
| [Touchpad Touch](#touchpad-touch) | **Off**, **Touch**, **Click** | Off | global | at once |
| [Mouse Mode](#mouse-mode) | **Bottom Screen**, **Pointer** | Bottom Screen | the mouse | at once |
| [Recentre at Edge](#recentre-at-edge) | on or off | on | the mouse, each touchpad | at once |
| [Screen Curve](#screen-curve) | **Off**, **15°**, **30°**, **45°**, **60°**, **75°**, **90°** | Off | global | at once |
| [3D Depth](#3d-depth) | 0% to 100%, in 5% steps | 50% | global and per game | at once |
| [Screen Shape](#screen-shape) | **3DS (5:3)**, **Widescreen (16:9)**, **Ultrawide (21:9)**, **Standard (4:3)**, **3:2**, **16:10** | 3DS (5:3) | global and per game | at once |
| [Immersive Mode](#immersive-mode) | **Off**, **Wide (3DoF)**, **Full (6DoF): Ocarina of Time, Majora's Mask**, **Full (6DoF): Other Games** | Off | global and per game | — |
| [Immersive Offset](#immersive-offset) | up to 10° each way, in 0.5° steps | centred | global | at once |
| [Immersive Game FOV](#immersive-game-fov) | 30° to 110° | 60° | global and per game | at once |
| [Match Game Camera](#match-game-camera) | on or off | on | global and per game | at once |
| [Wider View](#wider-view) | **Off**, **1.1x**, **1.2x**, **1.3x**, **1.4x** | Off | global and per game | at once |
| [Narrow Screen](#narrow-screen) | **Off**, **10%**, **20%**, **30%** | Off | global and per game | when the game starts |
| [Depth Factor Multiplier](#depth-factor-multiplier) | **1x**, **10x**, **100x** | 1x | global and per game | at once |
| [View Matrix Register](#view-matrix-register) | **Auto**, 0 to 92 | Auto | global and per game | — |
| [Eye Indicator Register](#eye-indicator-register) | empty for auto, or a register and index such as `87,2` | empty | global and per game | — |

## Hide Bottom Screen

Shows only the top screen. Use it for games that don't need the bottom screen, and for single-screen mods such as Ocarina ReFramed or Single Screen Experience for Ocarina of Time 3D.

While you play, both bumpers + **View**, or a tap of the left stick click, shows or hides it. A hidden bottom screen can't be touched or grabbed.

## Face Buttons

- **By Position (Nintendo)**: each Frame button does what the 3DS button in the same place does. The Frame's **A** is the 3DS's B.
- **By Label (Xbox)**: **A** is A, **B** is B, and so on.

Pick **By Label** if you'd rather follow the letters than the layout.

## Background

What shows behind the screens. **Passthrough** shows your room through the headset's cameras, when SteamVR offers it to apps. If it doesn't, the background stays black.

The in-game menu has the same choice on its **Settings** tab, and your choice is kept.

## Window Scale

How large Azahar's window looks in VR. Larger makes the text, buttons and list rows easier to read and to hit with the beam, and shows less of the window at once. Flat mode always uses 100%.

## Motion Controls

Makes one controller, or your head, the 3DS's gyroscope and accelerometer, for games that aim by moving the 3DS. The slingshot, bow and hookshot in Ocarina of Time 3D are examples. Hold the controller like a 3DS, with its beam pointing into the screen. With **Headset**, turn and tilt your head instead; looking straight ahead is the 3DS held upright in front of you, and both controllers keep pointing normally.

The same controller still points at the screens. If that gets in the way, choose the hand you don't point with. **Off** leaves the 3DS lying flat and still. Going from **Off** to another choice takes effect the next time VR starts.

## Show Controllers

Shows the Frame controllers in your hands, with their buttons and sticks moving. A controller you put down fades out after 30 seconds.

## Show Stats

Shows a small panel while a game runs: the game's frame rate, the emulation speed, the frame times, the headset's frame rate, the CPU load and the GPU clock. The **Stats** button above Azahar's window is the same switch.

## Stats Position

Where the stats show. The in-game menu has the same choice on its **Stats** tab, under **Show Stats**.

- **In Front**: the panel appears about 1 m ahead of where you look, a little below eye level, and stays there. It is never placed behind a screen: if a screen is nearer, it goes just in front of it. Grab it with a grip to move it; while you hold it, that hand's stick moves it nearer or farther (up or down) and makes it bigger or smaller (sideways). Turning the stats off and on, a new game or a re-centre puts it in front of you again. It hides while the in-game menu is open.
- **On Wrist**: the panel sits on the left controller like a watch. It shows only when you turn your wrist toward your face, and hides when the left controller loses tracking.

## Screen Layout

Where the Secondary Screen sits in VR. **Stacked**: below the Primary Screen, tilted back. **Side by Side**: to its right, at the same distance and height, facing you. In VR, hold the left stick click for 1 second to switch. Flat has its own layouts, see [CONTROLS.md](CONTROLS.md#screen-layouts).

## Detach Screens

Lets the screens in VR move apart. **Off**: a grip moves both screens together, and the Secondary Screen follows the Primary Screen. **Secondary**: the Primary Screen moves as with **Off**, and the Secondary Screen moves on its own. **Both**: each screen moves on its own. A screen that moves on its own goes anywhere around you with the grip and turns to face you; the stick of that hand sets its distance and size. After a swap the Secondary Screen is the top screen. Holding the right stick click for 1 second puts both back together; **Off** or a new Screen Layout puts the Secondary Screen back in its place. Each game keeps its own screen positions. It is in the in-game menu's **Position** tab. See [CONTROLS.md](CONTROLS.md#detaching-the-screens).

## Touchpad Touch

Makes a gamepad's touchpad the bottom screen in VR. The pad covers the whole screen, and a dot shows where your finger is. **Touch**: a finger on the pad touches the screen. **Click**: pressing the pad touches it, and the finger only moves the dot. Turn Steam Input off for that controller, or Steam keeps the touchpad. It is on the in-game menu's **Controls** tab, and on a touchpad's **Edit Controls** page. See [CONTROLS.md](CONTROLS.md#touchpad-touch).

## Mouse Mode

What a mouse does in VR. **Bottom Screen**: it moves a cursor on the bottom screen, and the left button touches. **Pointer**: it moves a pointer around you that works like the controller's beam. Hold the left and right buttons together for 3 seconds to switch. See [CONTROLS.md](CONTROLS.md#the-mouse).

## Recentre at Edge

For the mouse in **Bottom Screen** mode and for touchpads. A drag that runs into an edge of the bottom screen lets go and touches again in the middle, so you can keep turning a camera that follows the touch screen.

## Screen Curve

Bends the Primary Screen toward you, like a curved monitor: the top screen, or the bottom one when you swap them. The Secondary Screen stays flat. The picture keeps its size and shape. Immersive mode stays flat. In VR, hold the screens with a grip and press D-pad up or down. A label shows the new value, for example **Screen Curve: 45°**, and it is kept. The steps are **Off** (flat) and 15° to 90° in 15° steps: the angle the screen's width spans. In the menu's **Position** tab, pick it from the list.

## Screen Shape

The shape of the top screen in VR, and of the screen in immersive mode. The screen keeps its height and its width follows the shape. The bottom screen stays as it is.

- **3DS (5:3)**: the 3DS's own shape. Right for almost every game.
- **Widescreen (16:9)** and **Ultrawide (21:9)**: for mods that draw a widescreen picture.
- **Standard (4:3)**, **3:2** and **16:10**: for cheats and mods that change the game's own aspect.

Match the shape the game or mod draws, or the picture stretches. A change shows at once, also from the in-game menu, so you can match a mod's setting while you play.

## 3D Depth

How deep the 3D looks in a game. 0% is flat. It is on the in-game menu's **Settings** tab, as a slider: **Global** for every game, **Per-Game** for this game only. The global value is also Azahar's own **Depth** under **Stereoscopy** on the **Enhancements** tab of **Emulation > Configure**. A game's **Properties** show its value greyed out; change it in the in-game menu.

A screen you make bigger than its start size gets less depth on top of this. In **Full (6DoF): Other Games** the game's own 3D is off and the two eyes are set apart in the game by your own eye distance times this setting (with the [Depth Factor Multiplier](#depth-factor-multiplier)), so it still sets how deep the 3D looks. In both Full modes it also sets how far the game camera moves with your head, with the [Depth Factor Multiplier](#depth-factor-multiplier), in 10% steps: below 10% the camera stays still when you move your head.

## Immersive Mode

Puts you inside the game instead of in front of a floating screen.

- **Off**: the normal 3DS screens.
- **Wide (3DoF)**: the game draws a wider view, on a big screen fixed in the room.
- **Full (6DoF): Ocarina of Time, Majora's Mask**: the screen stays in front of your eyes, and the game camera turns and moves with your head.
- **Full (6DoF): Other Games**: the same for other games. It finds the game camera while you play.

Many games show graphical glitches in immersive mode. Set it per game, in **Properties**. Mario Kart 7, Super Mario 3D Land and Asphalt 3D start in **Full (6DoF): Other Games** unless you set something else.

The bottom screen starts hidden. Both bumpers + **View** shows it. Holding the right stick click for 1 second re-centres the view, and so does SteamVR's own recenter (hold the Steam button). Moving and sizing the screen: [CONTROLS.md](CONTROLS.md#moving-the-immersive-screen). More: [user guide](USER_GUIDE.md#immersive-mode).

## Immersive Offset

Shifts the immersive screen a little left or right to fit your lenses. Use it if you can see one side of the screen but not the other. It is the same in every game and every immersive mode. In VR, hold the immersive screen with a grip and press D-pad left or right. Each press moves it half a degree. **Reset** in this group puts it back in the middle.

## Immersive Game FOV

The game camera's vertical field of view, in degrees. In the Full modes with Match Game Camera on, the immersive screen reads the real angle from the game while it runs, so this is only used until then, or in a game where it is not found. Then set it to the game's angle, so the world stays still when you turn your head. A change shows at once.

## Match Game Camera

Full modes only. On, the immersive screen takes the game camera's angle and shape from the game while it runs; off, it uses Immersive Game FOV and Screen Shape as they are, and nothing is read from the game.

## Wider View

Shows more of the game around the edges. Some games leave them empty. Full modes only. In VR, hold the immersive screen with a grip and press D-pad up or down; the screen grows with it.

## Narrow Screen

Narrows the screen so the side HUD moves into view. Full modes only. The game looks that much thinner.

## Depth Factor Multiplier

Full modes only. The game's world scale: how far the game camera moves when you move your head and, in **Full (6DoF): Other Games**, how far apart your eyes are in the game, together with the 3D depth. A game with a large world needs a larger value: Super Mario 3D Land 100x, Mario Kart 7 and Kid Icarus: Uprising 10x (their presets).

## View Matrix Register

**Full (6DoF): Other Games** only. The shader register that holds the game's camera. **Auto** finds it while you play. Mario Kart 7, Super Mario 3D Land and Kid Icarus: Uprising use 90, and Asphalt 3D uses 8. Change it only for a game that already has a known value.

## Eye Indicator Register

**Full (6DoF): Other Games** only. The register and index that tell the two eyes apart, such as `87,2` (Mario Kart 7, Super Mario 3D Land, Kid Icarus: Uprising). Leave it empty to find it while you play; that takes a little while, and until then both eyes see the same picture.

## What changes from Azahar

- Azahar for Frame starts with the **Dark** theme. **Emulation > Configure > General > UI > Theme** changes it.
- The first time VR starts, the current controls profile gets the Frame layout. After that your changes stay. See [CONTROLS.md](CONTROLS.md#changing-the-layout).
- In VR, the Single Screen and Separate Windows layouts change to Default, and a global 3D depth of 0 becomes 50%. A game's own 3D Depth stays as you set it, 0% too.
- Fast Forward runs at **Turbo Speed Limit** in **Emulation > Configure > General**.
- Flat mode has its own screen layout, **Large Screen** at first, in a 16:9 window. See [CONTROLS.md](CONTROLS.md#playing-in-a-window-flat).
