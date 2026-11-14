# Settings

These are the settings Azahar for Frame adds to Azahar, and the few it changes. Everything else is Azahar's own.

## Where they are

The VR settings are in the **VR** group at the bottom of the **Graphics** tab.

- **Global** settings are in **Emulation > Configure**. They apply to every game, also when you open it from the in-game menu.
- **Per game** settings are also in a game's **Properties**: right-click the game in the list. A value set there wins over the global one for that game.

While a game with its own value runs, **Configure** still changes that row's global value; the game keeps its own, and pointing at the box says so.

The in-game menu's **Settings** tab has three sections, **General**, **Graphics** and **Immersive**. The **Settings**, **Controls** and **Position** tabs have a **Global | Per-Game** switch at the top right, which opens on **Per-Game** while a game runs. **Global** changes every game. Pointing at **Per-Game** while it is picked shows how many values on the tab are set for this game (on **Settings**, in this section), and **Use Global Values for All**, in the **⋮** menu next to it, puts them back on the global values.

**Detach Screens**, **Touchpad**, **Bottom Screen Reticle**, **Reticle Speed**, **At Edge**, **C-Stick at Edge**, **Aim Only While Pressed** and **Right Stick Moves Touch** are only in the in-game menu. **Mouse Mode** and **Recentre at Edge** belong to a device and are on its **Edit Controls** page; they are kept in `user/controller-mappings.txt`.

## At a glance

| Setting | Choices | Default | Scope | Takes effect |
|---|---|---|---|---|
| [Hide Bottom Screen](#hide-bottom-screen) | on or off | off | global and per game | when the game starts |
| [Face Buttons](#face-buttons) | **By Position (Nintendo)**, **By Label (Xbox)** | By Position (Nintendo) | global | at once |
| [Background](#background) | **Black**, **Passthrough** | Black | global | at once |
| [Window Scale](#window-scale) | **100%**, **150%**, **200%** | 150% | global | when you press **OK** |
| [Motion Controls](#motion-controls) | **Off**, **Right Controller**, **Left Controller**, **Headset** | Right Controller (Kid Icarus: Uprising: Off) | global and per game | at once, or next VR start from Off |
| [Show Controllers](#show-controllers) | on or off | on | global | at once |
| [Controllers in Front of Screen](#controllers-in-front-of-screen) | **Off**, **Primary**, **Secondary**, **Both** | Both | global | at once |
| [Show Stats](#show-stats) | on or off | off | global | at once |
| [Stats Position](#stats-position) | **In Front**, **On Wrist** | In Front | global | at once |
| [Screen Layout](#screen-layout) | **Stacked**, **Side by Side** | Stacked | global | at once |
| [Detach Screens](#detach-screens) | **Off**, **Secondary**, **Both** | Off | global | at once |
| [Touchpad](#touchpad) | **Tap for Input**, **Click for Input** | Tap for Input | global and per game | at once |
| [Bottom Screen Reticle](#bottom-screen-reticle) | **Off**, **Left Controller**, **Right Controller**, **Headset** | Off (Kid Icarus: Uprising: Headset) | global and per game | at once |
| [Reticle Speed](#reticle-speed) | 0.5x to 4.0x, in 0.1x steps | 2.0x | global and per game | at once |
| [At Edge](#at-edge) | **Follow**, **Snap to Middle** | Follow | global and per game | at once |
| [C-Stick at Edge](#c-stick-at-edge) | on or off | off (Kid Icarus: Uprising with the C-Stick mod: on) | global and per game | at once |
| [Aim Only While Pressed](#aim-only-while-pressed) | on or off | off | global and per game | at once |
| [Right Stick Moves Touch](#right-stick-moves-touch) | on or off | off | global and per game | at once |
| [Mouse Mode](#mouse-mode) | **Bottom Screen**, **Pointer** | Bottom Screen | the mouse | at once |
| [Recentre at Edge](#recentre-at-edge) | on or off | on | the mouse, each touchpad | at once |
| [Autosave](#autosave) | on or off | off | global and per game | at once |
| [Every](#autosave) | **1**, **2**, **5**, **10**, **15** minutes | 5 | global and per game | at once |
| [Keep](#autosave) | 1 to 10 | 3 | global and per game | at once |
| [Screen Curve](#screen-curve) | **Off**, **15°**, **30°**, **45°**, **60°**, **75°**, **90°** | Off | global | at once |
| [3D Depth](#3d-depth) | 0% to 100%, in 5% steps | 50% (Full modes: 100%) | global and per game | at once |
| [Screen Shape](#screen-shape) | **3DS (5:3)**, **Widescreen (16:9)**, **Ultrawide (21:9)**, **Standard (4:3)**, **3:2**, **16:10**, **Automatic** | 3DS (5:3) | global and per game | at once |
| [Immersive Mode](#immersive-mode) | **Off**, **Wide (3DoF)**, **Full (6DoF): Ocarina of Time, Majora's Mask**, **Full (6DoF): Other Games** | Off | global and per game | — |
| [Immersive Offset](#immersive-offset) | up to 10° each way, in 0.5° steps | centred | global | at once |
| [Immersive Game FOV](#immersive-game-fov) | 30° to 110° | 60° | global and per game | at once |
| [Detect Game FOV](#detect-game-fov) | **Off**, **Once**, **Always** | Always | global and per game | at once |
| [Render Margin](#render-margin) | **Off**, **1.1x**, **1.2x**, **1.3x**, **1.4x** | 1.4x | global and per game | at once |
| [Horizontal Scale](#horizontal-scale-and-vertical-scale) | -100% to +100%, in 5% steps | 0% | global and per game | at once |
| [Vertical Scale](#horizontal-scale-and-vertical-scale) | -100% to +100%, in 5% steps | 0% | global and per game | at once |
| [World Scale](#world-scale) | 10% to 400%, 100% by default | 100% | global and per game | eased in |
| [Follow Screen Size](#follow-screen-size) | on or off | on | global and per game | eased in |
| [Motion Clarity](#motion-clarity) | on or off | on | global and per game | at once |
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

It can be set for one game on the in-game menu's **Settings** tab, in **General**, with **Per-Game** picked, or in the game's **Properties**. In the global settings it sets the value the other games use; a game with its own value keeps it. Kid Icarus: Uprising starts with **Off**, because its C-Stick & Gyro mod aims with the gyroscope and the reticle moved whenever you did. Turn it on for that game if you want gyro aiming.

## Show Controllers

Shows the Frame controllers in your hands, with their buttons and sticks moving. A controller you put down fades out after 30 seconds.

## Controllers in Front of Screen

Shows the controllers, and a weapon in your hand, in front of a screen when they are nearer to you than it, so a hand held up in front of a screen stays in front and one reaching behind it goes behind. **Primary** and **Secondary** name the screens as **Detach Screens** does (after a swap the Secondary Screen is the top screen); the screen left out covers the controllers wherever they are. **Both** does it for both screens. **Off** keeps the controllers behind every screen and menu, as before. With any choice but **Off**, the menus, the keyboard and the stats go by distance too, so a controller pointing at a menu shows in front of it; messages never cover the controllers, and neither does the immersive screen (what it shows is far away) unless the choice leaves it out. It is on the in-game menu's **Settings** tab, under **Show Controllers**, while that is on.

## Show Stats

Shows a small panel while a game runs: the game's frame rate, the emulation speed, the frame times, the headset's frame rate, the CPU load and the GPU clock. The **Stats** button above Azahar's window is the same switch.

## Stats Position

Where the stats show. The in-game menu has the same choice on its **Stats** tab, under **Show Stats**.

- **In Front**: the panel appears about 1 m ahead of where you look, a little below eye level, and stays there. It is never placed behind a screen: if a screen is nearer, it goes just in front of it. Grab it with a grip to move it; while you hold it, that hand's stick moves it nearer or farther (up or down) and makes it bigger or smaller (sideways). Turning the stats off and on, a new game or a recentre puts it in front of you again. It hides while the in-game menu is open.
- **On Wrist**: the panel sits on the left controller like a watch. It shows only when you turn your wrist toward your face, and hides when the left controller loses tracking.

## Screen Layout

Where the Secondary Screen sits in VR. **Stacked**: below the Primary Screen, tilted back. **Side by Side**: to its right, at the same distance and height, facing you. In VR, hold the left stick click for 1 second to switch. Flat has its own layouts, see [CONTROLS.md](CONTROLS.md#screen-layouts).

## Detach Screens

Lets the screens in VR move apart. **Off**: a grip moves both screens together, and the Secondary Screen follows the Primary Screen. **Secondary**: the Primary Screen moves as with **Off**, and the Secondary Screen moves on its own. **Both**: each screen moves on its own. A screen that moves on its own goes anywhere around you with the grip and turns to face you; the stick of that hand sets its distance and size. After a swap the Secondary Screen is the top screen. Holding the right stick click for 1 second puts both back together; **Off** or a new Screen Layout puts the Secondary Screen back in its place. Each game keeps its own screen positions. It is in the in-game menu's **Position** tab. See [CONTROLS.md](CONTROLS.md#detaching-the-screens).

## Touchpad

How a gamepad's touchpad touches the bottom screen in VR. The pad covers the whole screen, and a dot shows where your finger is. **Tap for Input**: a finger on the pad touches the screen. **Click for Input**: pressing the pad touches it, and holding it down drags. Turn Steam Input off for that controller, or Steam keeps the touchpad. It is on the in-game menu's **Controls** tab, and on a touchpad's **Edit Controls** page. See [CONTROLS.md](CONTROLS.md#touchpad).

## Bottom Screen Reticle

A controller, or your head with **Headset**, aims at the bottom screen like a stylus, for games that aim with the touch screen. Its trigger touches and its grip recentres. While it aims in a game, that controller draws no beam. It is on the in-game menu's **Controls** tab. Kid Icarus: Uprising starts with **Headset** unless you set it yourself, for that game or for all games. See [CONTROLS.md](CONTROLS.md#bottom-screen-reticle).

## Reticle Speed

How far the reticle moves when you turn your head or the controller, from where aiming started or was recentred. One value for the head and the controllers. At 1.0x your head turns 30 degrees to cross the bottom screen and 22.5 degrees from top to bottom, and a controller 40 and 30. At the default 2.0x it is half that: about 15 by 11 degrees for your head. Higher is faster; past an edge the dot stays on the edge. Shown under **Bottom Screen Reticle** while it is on.

## At Edge

What the reticle does at the edge of the bottom screen. **Follow**: the dot stays on the edge. **Snap to Middle**: the touch lifts and starts again in the middle. Shown under **Bottom Screen Reticle** while it is on.

## C-Stick at Edge

While the reticle touches near an edge of the bottom screen, the touch lifts and the C-Stick pushes toward that edge instead: part of the way where the edge band starts, all the way at the edge. Kid Icarus: Uprising with the C-Stick & Gyro mod ignores the C-Stick while the screen is touched, so this is what turns the camera there, the same way it turns when the mod's C-Stick aiming pushes past its aiming box. The band is the same turn of your head or controller at any **Reticle Speed**: about 3 degrees. Move back from the edge and the touch comes back where the reticle is; lift the trigger and both let go. While it is on, the reticle stays on the edge as with **At Edge** on **Follow**, whatever **At Edge** is set to. Made for Kid Icarus: Uprising with Darkshade's C-Stick & Gyro mod. Kid Icarus: Uprising starts with it on once a C-Stick mod (that one or kiu-cstick-aim) is installed for the game. Shown under **Bottom Screen Reticle** while it is on.

## Aim Only While Pressed

The reticle stays in the middle of the bottom screen and moves only while the trigger touches. Let go and it goes back to the middle. Shown under **Bottom Screen Reticle** while it is on.

## Right Stick Moves Touch

The right stick moves the bottom screen's touch point too, together with your head or controller, and the game no longer gets it as the C-Stick. Full tilt crosses the screen in about a second. Off, the right stick is the C-Stick as usual. With **Aim Only While Pressed** on, it moves the point only while the trigger touches. Shown under **Bottom Screen Reticle** while it is on.

## Autosave

Save states made while you play, separate from the 10 slots and Quick Save. **Every** is the minutes of play between autosaves, and **Keep** how many each game keeps. They are in the in-game menu's **Settings** > **General**, in **Emulation > Configure > General** and in a game's **Properties**. See the [user guide](USER_GUIDE.md#autosave).

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
- **Automatic**: follows the shape the game's camera draws with: 4:3, 3:2, 5:3, 16:10, 16:9 or 21:9. A new shape is used once it has held for 2 seconds, and the screen eases into it. A game with a widescreen mod from the **Mods** tab starts at that mod's shape. This works with **Detect Game FOV** set to **Off** too.

Match the shape the game or mod draws, or the picture stretches. A change shows at once, also from the in-game menu, so you can match a mod's setting while you play.

## 3D Depth

How deep the 3D looks in a game. 0% is flat. It is on the in-game menu's **Settings** tab, as a slider: **Global** for every game, **Per-Game** for this game only. The global value is also Azahar's own **Depth** under **Stereoscopy** on the **Enhancements** tab of **Emulation > Configure**. A game's **Properties** show its value greyed out; change it in the in-game menu.

Outside the Full modes, a screen you make bigger than its start size gets less depth.

The Full modes have their own 3D Depth: **Immersive 3D Depth** in Azahar's window, and the same slider in the in-game menu while a Full mode is on. At 100%, the default, the farthest scenery is at infinity and never past it, so it never doubles, whatever the screen's size or distance. Below 100%, **Full (6DoF): Ocarina of Time, Majora's Mask** brings faraway scenery nearer, and **Full (6DoF): Other Games** keeps it far and flattens everything. A game whose 3D Depth was 0% starts at 0%.

## Immersive Mode

Puts you inside the game instead of in front of a floating screen.

- **Off**: the normal 3DS screens.
- **Wide (3DoF)**: the game draws a wider view, on a big screen fixed in the room.
- **Full (6DoF): Ocarina of Time, Majora's Mask**: the screen stays in front of your eyes, and the game camera turns and moves with your head.
- **Full (6DoF): Other Games**: the same for other games. It finds the game camera while you play.

Many games show graphical glitches in immersive mode. Set it per game, in **Properties**. Mario Kart 7, Super Mario 3D Land and Asphalt 3D start in **Full (6DoF): Other Games** unless you set something else.

The bottom screen starts hidden. Both bumpers + **View** shows it. Holding the right stick click for 1 second recentres the view, and so does SteamVR's own recentre (hold the Steam button). Moving and sizing the screen: [CONTROLS.md](CONTROLS.md#moving-the-immersive-screen). More: [user guide](USER_GUIDE.md#immersive-mode).

## Immersive Offset

Shifts the immersive screen a little left or right to fit your lenses. Use it if you can see one side of the screen but not the other. It is the same in every game and every immersive mode. In VR, hold the immersive screen with a grip and press D-pad left or right. Each press moves it half a degree. **Reset** in this group puts it back in the middle.

## Immersive Game FOV

The game camera's vertical field of view, in degrees. In the Full modes with Detect Game FOV set to Always, the immersive screen reads the real angle from the game while it runs, so this is only used until then, or in a game where it is not found. With Once, it is set for you from the game's camera. Otherwise set it to the game's angle, so the world stays still when you turn your head. A change shows at once.

## Detect Game FOV

Called Match Game Camera in earlier versions. Full modes only. **Always** reads the game camera's angle and shape from the game while it runs. A new angle is followed once it has held for half a second, and the view eases into it a little at a time, so a camera cut never makes it jump. **Once** measures the game's camera once while you play, for about 15 seconds, leaving out cuts and cutscenes, and sets that game's Immersive Game FOV to match. A message shows the value. From then on that value is used as if you had set it, and you can change it. Choose Once again to measure again. If no steady angle is found within a minute, the value stays as it was and a message says so. **Off** uses Immersive Game FOV as set and reads no angle from the game. With **Screen Shape** on **Automatic**, the shape is still read from the game. Settings from earlier versions keep their choice: on is Always, off is Off.

## Render Margin

Called Wider View in earlier versions. Renders extra picture around the 3DS screen's edges so more of the game shows. Some games leave the edges empty. **Off** shows exactly the game's own picture. Full modes only. The margin is the same on every side. In **Full (6DoF): Ocarina of Time, Majora's Mask**, the game's own menu screens, such as Ocarina of Time 3D's item screen, show at the game's own size, without the margin. In VR, hold the immersive screen with a grip and press D-pad up or down; the screen grows with it. A label shows the value, for example **Render Margin: 1.2x**.

## Horizontal Scale and Vertical Scale

Make the immersive screen wider or narrower (**Horizontal Scale**) and taller or shorter (**Vertical Scale**) than the game's picture. Full modes only. Above 0% the screen is larger, below 0% smaller: a narrower screen brings the side HUD into view, and the game looks that much thinner. Past -30% each step shrinks it a little less, so -100% leaves about a quarter of the screen. Turning your head still moves the world as much as the screen, so the middle of the picture stays in place. In VR, hold the immersive screen with both grips and press D-pad right (wider) or left (narrower), up (taller) or down (shorter). A label shows the value, for example **Horizontal Scale: -10%**. Each is kept for the game. They replace Narrow Screen and Narrow Height: a game where you set those keeps its screen, so Narrow Screen 10% is now Horizontal Scale -10%. Mario Kart 7's preset starts at -30% and -15%.

## World Scale

Full modes only. How big the game world feels: below 100% the world is smaller, like a diorama, and above it larger. 100% is the default. In VR, hold the immersive screen with a grip and push the stick up or down: the screen moves farther or nearer, keeping its size in your view, and the world grows or shrinks with it. A label shows the value, for example **World Scale: 45%**. It is kept for the game.

At every value, your head moves the game camera as far as the 3D shows. A game set up in an earlier version keeps its look; in **Full (6DoF): Ocarina of Time, Majora's Mask** its head movement now matches the 3D. It replaces **Depth Factor Multiplier**.

## Follow Screen Size

Full modes only. When you resize the immersive screen (grip and stick left or right), the world grows or shrinks with it. Off keeps the world's size: a bigger screen then moves nearer.

## Motion Clarity

Full modes only. Shows each game picture once, for one refresh of the headset, and black until the next one. Turning your head, the world stays sharp and single instead of doubling. The screen is dimmer: at 60 fps it is lit half the time, which looks about three quarters as bright, and some people notice a slight flicker at the edges of their view. Games that run under 60 fps, a game that pauses or stalls, and the VR menu show as before.

## View Matrix Register

**Full (6DoF): Other Games** only. The shader register that holds the game's camera. **Auto** finds it while you play. Mario Kart 7 and Super Mario 3D Land use 90, and Asphalt 3D uses 8. Change it only for a game that already has a known value.

## Eye Indicator Register

**Full (6DoF): Other Games** only. The register and index that tell the two eyes apart, such as `87,2` (Mario Kart 7, Super Mario 3D Land). Leave it empty to find it while you play; that takes a little while, and until then both eyes see the same picture.

## What changes from Azahar

- Azahar for Frame starts with the **Dark** theme. **Emulation > Configure > General > UI > Theme** changes it.
- The first time VR starts, the current controls profile gets the Frame layout. After that your changes stay. See [CONTROLS.md](CONTROLS.md#changing-the-layout).
- In VR, the Single Screen and Separate Windows layouts change to Default, and a global 3D depth of 0 becomes 50%. A game's own 3D Depth stays as you set it, 0% too. The Full modes have their own 3D Depth (100% unless the game's was 0%).
- Fast Forward runs at **Turbo Speed Limit** in **Emulation > Configure > General**.
- Flat mode has its own screen layout, **Large Screen** at first, in a 16:9 window. See [CONTROLS.md](CONTROLS.md#playing-in-a-window-flat).
