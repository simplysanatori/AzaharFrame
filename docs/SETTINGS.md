# Settings

These are the settings Azahar for Frame adds to Azahar, and the few it changes. Everything else is Azahar's own.

## Where they are

The VR settings are in the **VR** group at the bottom of the **Graphics** tab.

- **Global** settings are in **Emulation > Configure**. They apply to every game, also when you open it from the in-game menu.
- **Per game** settings are also in a game's **Properties**: right-click the game in the list. A value set there wins over the global one for that game.

While a game with its own value runs, that row is greyed out in **Configure**.

## At a glance

| Setting | Choices | Default | Scope | Takes effect |
|---|---|---|---|---|
| [Hide Bottom Screen](#hide-bottom-screen) | on or off | off | global and per game | when the game starts |
| [Face Buttons](#face-buttons) | **By Position (Nintendo)**, **By Label (Xbox)** | By Position (Nintendo) | global | at once |
| [Background](#background) | **Black**, **Passthrough** | Black | global | at once |
| [Window Scale](#window-scale) | **100%**, **150%**, **200%** | 150% | global | when you press **OK** |
| [Motion Controls](#motion-controls) | **Off**, **Right Controller**, **Left Controller** | Right Controller | global | at once, or next VR start from Off |
| [Show Controllers](#show-controllers) | on or off | on | global | at once |
| [Show Stats](#show-stats) | on or off | off | global | at once |
| [Screen Curve](#screen-curve) | **Off**, **Gentle**, **Medium**, **Strong**, **Max** | Off | global | at once |
| [Screen Shape](#screen-shape) | **3DS (5:3)**, **Widescreen (16:9)**, **Ultrawide (21:9)**, **Standard (4:3)**, **3:2**, **16:10** | 3DS (5:3) | global and per game | at once |
| [Immersive Mode](#immersive-mode) | **Off**, **Wide (3DoF)**, **Full (6DoF): Ocarina of Time, Majora's Mask**, **Full (6DoF): Other Games** | Off | global and per game | — |
| [Immersive Offset](#immersive-offset) | up to 10° each way, in 0.5° steps | centred | global | at once |
| [Immersive Game FOV](#immersive-game-fov) | 30° to 110° | 60° | global and per game | at once |
| [Wider View](#wider-view) | on or off | off | global and per game | when the game starts |
| [Squeeze Width](#squeeze-width) | **Off**, **10%**, **20%** | Off | global and per game | when the game starts |
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

The in-game menu has a **Passthrough** button for the same switch, and your choice is kept.

## Window Scale

How large Azahar's window looks in VR. Larger makes the text, buttons and list rows easier to read and to hit with the beam, and shows less of the window at once. Flat mode always uses 100%.

## Motion Controls

Makes one controller the 3DS's gyroscope and accelerometer, for games that aim by moving the 3DS. The slingshot, bow and hookshot in Ocarina of Time 3D are examples. Hold the controller like a 3DS, with its beam pointing into the screen.

The same controller still points at the screens. If that gets in the way, choose the hand you don't point with. **Off** leaves the 3DS lying flat and still. Going from **Off** to a controller takes effect the next time VR starts.

## Show Controllers

Shows the Frame controllers in your hands, with their buttons and sticks moving. A controller you put down fades out after 30 seconds.

## Show Stats

Shows a small panel under the top screen while a game runs: the game's frame rate, the emulation speed, the frame times, the headset's frame rate, the CPU load and the GPU clock. The **Stats** button above Azahar's window is the same switch.

## Screen Curve

Bends the two screens toward you, like a curved monitor. The picture keeps its size and shape. Gentle, Medium, Strong and Max wrap the top screen's width round 15°, 30°, 45° and 60°; Strong to Max wraps a screen at its start place evenly around you. Immersive mode stays flat. In VR, hold the screens with a grip and press D-pad up or down. A label shows the new value, for example **Screen Curve: Medium**, and it is kept.

## Screen Shape

The shape of the top screen in VR, and of the screen in immersive mode. The screen keeps its height and its width follows the shape. The bottom screen stays as it is.

- **3DS (5:3)**: the 3DS's own shape. Right for almost every game.
- **Widescreen (16:9)** and **Ultrawide (21:9)**: for mods that draw a widescreen picture.
- **Standard (4:3)**, **3:2** and **16:10**: for cheats and mods that change the game's own aspect.

Match the shape the game or mod draws, or the picture stretches. A change shows at once, also from the in-game menu, so you can match a mod's setting while you play.

## Immersive Mode

Puts you inside the game instead of in front of a floating screen.

- **Off**: the normal 3DS screens.
- **Wide (3DoF)**: the game draws a wider view, on a big screen fixed in the room.
- **Full (6DoF): Ocarina of Time, Majora's Mask**: the screen stays in front of your eyes, and the game camera turns and moves with your head.
- **Full (6DoF): Other Games**: the same for other games. It finds the game camera while you play.

Many games show graphical glitches in immersive mode. Set it per game, in **Properties**. Mario Kart 7, Super Mario 3D Land and Asphalt 3D start in **Full (6DoF): Other Games** unless you set something else.

The bottom screen starts hidden. Both bumpers + **View** shows it. Holding the right stick click for 1 second re-centres the view, and so does SteamVR's own recenter (hold the Steam button). Moving and sizing the screen: [CONTROLS.md](CONTROLS.md#moving-the-immersive-screen). More: [user guide](USER_GUIDE.md#immersive-mode).

## Immersive Offset

Shifts the immersive screen a little left, right, up or down to fit your lenses. Use it if you can see one side of the screen but not the other. It is the same in every game and every immersive mode. In VR, hold the immersive screen with a grip and press the D-pad. Each press moves it half a degree. **Reset** in this group puts it back in the middle.

## Immersive Game FOV

The game camera's vertical field of view, in degrees. Change it only if a mod changes the game's FOV, and set it to the same value. Then the world stays still when you turn your head. A change shows at once.

## Wider View

Full modes only. When you make the immersive screen bigger, it also shows more of the game around its own picture, up to 1.4 times, as in CitraVR. With it off, a bigger screen only enlarges the picture.

Some games stop drawing their scenery just past their own view, so the edges show black or missing scenery. Super Mario 3D Land does. Turn it on per game, for games that look right with it.

## Squeeze Width

Full modes only. Makes the immersive screen narrower at the same height, so the HUD at the sides moves into view. The game looks that much thinner. Use **10%** or **20%** when the HUD sits too far out to read.

## Depth Factor Multiplier

Full modes only. How far the game camera moves when you move your head, together with the 3D depth. Some games need a larger value.

## View Matrix Register

**Full (6DoF): Other Games** only. The shader register that holds the game's camera. **Auto** finds it while you play. Mario Kart 7 and Super Mario 3D Land use 90, and Asphalt 3D uses 8. Change it only for a game that already has a known value.

## Eye Indicator Register

**Full (6DoF): Other Games** only. The register and index that tell the two eyes apart, such as `87,2`. Leave it empty to find it while you play.

## What changes from Azahar

- Azahar for Frame starts with the **Dark** theme. **Emulation > Configure > General > UI > Theme** changes it.
- The first time VR starts, the current controls profile gets the Frame layout. After that your changes stay. See [CONTROLS.md](CONTROLS.md#changing-the-layout).
- In VR, the Single Screen and Separate Windows layouts change to Default, and a 3D depth of 0 becomes 50%.
- Fast Forward runs at **Turbo Speed Limit** in **Emulation > Configure > General**.
- Flat mode has its own screen layout, **Large Screen** at first, in a 16:9 window. See [CONTROLS.md](CONTROLS.md#playing-in-a-window-flat).
