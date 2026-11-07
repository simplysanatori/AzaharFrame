# Controls

Azahar for Frame keeps CitraVR's layout and uses the Steam Frame controllers' real D-pad where CitraVR needed a trick on the Quest. The emulator sees a 3DS with a New 3DS's C-Stick, ZL and ZR.

<!-- VERIFY: the whole table in the headset, every row. The rows follow src/frame_xr/input/MAPPING.md and default_bindings.h. -->

## Buttons

| Steam Frame | 3DS | Notes |
|---|---|---|
| **A**, **B**, **X**, **Y** (right controller) | A, B, X, Y | |
| Left bumper | L | |
| Right bumper | R | |
| Left trigger | ZL | Also when you aim at the top screen. On the bottom screen it is a touch instead |
| Right trigger | ZR | Same |
| Left and right grip | Grab a screen and move it | Nothing reaches the game. See Pointing |
| Left stick | Circle Pad | |
| Right stick | C-Stick | |
| Left D-pad | D-pad | On the Quest, CitraVR used the thumbrest and a stick for this |
| **Menu** (right controller) | Start | Hold for 1 second to open the in-game menu, see below. The game does not see that hold |
| **View** (left controller) | Select | Hold for 1.5 seconds to stop the game and go back to Azahar's window |
| Left stick click | Home | |
| Right stick click | Resets the screen positions | Nothing reaches the game. Ignored while the stick is pushed more than half way, so free camera cannot reset by accident |
| Left bumper + right bumper + **View** | Shows or hides the bottom screen | Only in games that hide it, see below. The game does not see that press |
| Left bumper + right bumper + **View** + **Menu** | Switches between VR and Flat | See below. The game sees no Select or Start |

The left Steam button belongs to SteamVR and is never bound.

Grips press at 60 % and let go at 40 %, triggers press at 55 % and let go at 40 %, and both sticks have a 12 % dead zone.

While you hold **View**, the game sees Select held. When the 1.5 seconds are up the game sees it released and stops.

A tap of **Menu** reaches the game as Start when you let go.

## Pointing

Each controller has a laser. The hand that pulled its trigger last owns the dot, which shows over the bottom screen.

| Aim at | Trigger | Grip | Stick of that hand |
|---|---|---|---|
| Bottom screen | Touch. Hold and move to drag the touch | Move both screens | While you move them: nearer and farther |
| Top screen | ZL or ZR, as everywhere in the game | Move both screens. They stay the same distance from you and turn to face you | While you move them: nearer and farther. It does not reach the game then |

The trigger is only a touch when the laser is on the bottom screen when you pull it. Anywhere else it is ZL or ZR, and stays that for as long as you hold it. A second trigger pulled on the bottom screen while it is already being touched does nothing. A grip grabs the screen under the laser when you squeeze it; a grip squeezed with the laser on neither screen does nothing.

The top screen goes no nearer than 1 m, and no further than 10 m. Moving them never moves the screens sideways. The bottom screen follows the top one, 0.75 m lower and 0.5 m nearer, tilted back. The screens keep their place when you let go. The right stick click puts both back where they start.

## Azahar's window

When no game runs, Azahar's own window floats in front of you: the game list, the menus and every dialog. The laser is the mouse. Double-click a game to start it, and use **Emulation > Configure...** for the settings. When a game stops, the window comes back.

| Control | What it does |
|---|---|
| Trigger or **A** | Left click. Hold it to drag |
| Right stick click | Right click, for example on a game for **Properties** |
| Right stick click, held for 1 second | Brings the window back in front of you |
| **B** | Back: closes a menu or a dialog |
| Grip aimed at the window | Moves the window. It stays the same distance from you and turns to face you |
| Either stick, up or down | Scrolls |

**X**, **Y** and the bumpers do nothing on the window. There is no typing in VR yet, so a text field needs a keyboard.

## The in-game menu

Hold **Menu** for 1 second in a game. The game pauses, its screens dim, and Azahar's window shows in front of them, with the same controls as above. **Emulation > Configure...**, the game's **Properties** and the **Emulation** menu all work here. Settings such as the resolution apply at once; the VR settings apply the next time a game starts.

To go back to the game, hold **Menu** for 1 second again, press **B** with no menu or dialog open, or select **Emulation > Continue**. Immersive mode shows the normal screens while the menu is open.

## VR and Flat

Press both bumpers, **View** and **Menu** together to switch between VR and Flat, in a game or not. The game keeps running. In Flat, Azahar's window shows the game on the Frame's screen, as a normal app, and the game draws one eye only; back in VR the 3D comes back. The next start uses the mode you switched to last.

The buttons mean the same in both modes. In Flat the controllers reach Azahar as Steam Input's gamepad, so there is no laser: the triggers are ZL and ZR, the mouse is the touch screen, and the Frame's pointer is the mouse for Azahar's menus. Holding **Menu** for 1 second pauses or continues the game, and holding **View** for 1.5 seconds stops it, as in VR.

Hold both bumpers first. **View** with both bumpers waits a moment for **Menu** before it does anything else (the bottom screen, or Select). The game sees L and R until the switch, then nothing of the four until you let go.

## Hiding the bottom screen

Two things hide the bottom screen: immersive mode, and the **Hide Bottom Screen** setting of a game (properties, **Graphics** tab, **VR** group). A hidden bottom screen cannot be touched or grabbed, and the top screen can still be grabbed with a grip unless immersive mode is on.

| Control | What it does |
|---|---|
| Left bumper + right bumper + **View** | Shows or hides the bottom screen. Hold both bumpers, then press View. Every game starts with it hidden. The game sees none of the three |
| Right stick click | Also re-centres the view on your head in immersive mode (same half-way rule) |
| Triggers | ZL and ZR everywhere while the bottom screen is hidden |

The top screen cannot be grabbed in immersive mode. More in [src/frame_xr/IMMERSIVE.md](../src/frame_xr/IMMERSIVE.md).

## Ocarina of Time 3D with Ocarina ReFramed

The Single Screen edition of Ocarina ReFramed (by Hyrule Hysteria, GameBanana 695604) puts the whole HUD, pause menu and options on the top screen. Turn on **Hide Bottom Screen** for the game. The mod reads the 3DS buttons below; the shortcuts that need ZL and ZR need New 3DS mode, which is on by default.

| What it does | 3DS | Steam Frame |
|---|---|---|
| Open the Items menu | Select | **View**, a quick tap. A hold of 1.5 s stops the game |
| Close the menus | Select, Start or B | **View**, **Menu** or **B** |
| Item I, Item II | D-pad left, D-pad down | D-pad left, D-pad down |
| Fairy (Navi) and Look Up, Ocarina | D-pad up, D-pad right | D-pad up, D-pad right |
| Show or hide the minimap | ZL | Left trigger |
| Free camera | C-Stick | Right stick |
| Camera speed up or down | L + R + D-pad up or down | Both bumpers + D-pad up or down |
| Invert camera axes | L + R + D-pad left or right | Both bumpers + D-pad left or right |
| HUD size (75, 100, 125 %, off) | L + R + ZR, held | Both bumpers + right trigger, held. The HUD size is also in Options |
| Manual jump (Options > Manual jump) | ZR, or X (the X item moves to ZR) | Right trigger, or **X** (the X item moves to the right trigger) |
| Pause menu: pages | L and R | Left and right bumper |
| Pause menu: move, choose, back | D-pad or Circle Pad, A, B | D-pad or left stick, **A**, **B** |
| Options menu: columns | L and R | Left and right bumper |

The mod's HUD is a controller-neutral, Switch-style diamond of the items, so it does not name ZL or ZR. The mod does the 3DS touch buttons for you from the D-pad and View. If you ever need the real bottom screen, press both bumpers and **View**.

## Ocarina of Time 3D with Single Screen Experience

Single Screen Experience (by M-1, GameBanana 695893) is the mod I recommend for Ocarina of Time 3D. It puts the HUD and menus on the top screen and adds a 60 FPS option, a 16:9 option, a field of view setting, a draw distance slider and a free camera on the right stick. Turn on **Hide Bottom Screen** for the game.

- If you turn on its 16:9 option, set **Screen Shape** to **Widescreen (16:9)** in the game's properties (**Graphics** tab, **VR** group). Without it the picture looks squeezed.
- Leave the mod's FOV at its default. If you change it, set **Immersive Game FOV** to the same value so immersive mode keeps the world still when you turn your head.
- If it stutters, turn off the mod's 60 FPS option first.
- Keep real-time shadows off, and draw distance at 100 to 150 %.

The buttons are the same as in the table above. Both bumpers and **View** bring back the bottom screen if you ever need it.

## Changing the layout

The layout is a controls profile in Azahar's **Emulation > Configure > Controls** window, where the Frame's controls are named **Frame A**, **Frame Left Bumper**, **Frame Menu** and so on. The first time VR starts, the buttons and sticks of the current profile are replaced with this layout, unless one of them already is a Frame control. After that your changes stay. A profile saved by an earlier version, with L and R on the grips, gets them moved to the bumpers (the grips only grab the screens now). Keep a PC profile of your own under another name if you also use the window with a keyboard.

SDL game controllers still work, and keyboard and mouse work in the desktop window.

## Rumble

None.
