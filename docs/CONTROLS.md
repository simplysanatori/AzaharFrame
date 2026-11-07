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
| **Menu** (right controller) | Start | |
| **View** (left controller) | Select | Hold for 1.5 seconds to stop the game and go back to the picker |
| Left stick click | Home | |
| Right stick click | Resets the screen positions | Nothing reaches the game. Ignored while the stick is pushed more than half way, so free camera cannot reset by accident |
| Left bumper + right bumper + **View** | Shows or hides the bottom screen | Only in games that hide it, see below. The game does not see that press |

The left Steam button belongs to SteamVR and is never bound.

Grips press at 60 % and let go at 40 %, triggers press at 55 % and let go at 40 %, and both sticks have a 12 % dead zone.

While you hold **View**, the game sees Select held. When the 1.5 seconds are up the game sees it released and stops.

## Pointing

Each controller has a laser. The hand that pulled its trigger last owns the dot, which shows over the bottom screen.

| Aim at | Trigger | Grip | Stick of that hand |
|---|---|---|---|
| Bottom screen | Touch. Hold and move to drag the touch | Move both screens | While you move them: nearer and farther |
| Top screen | ZL or ZR, as everywhere in the game | Move both screens. They stay the same distance from you and turn to face you | While you move them: nearer and farther. It does not reach the game then |
| The game picker | Select a game | | Pages |

The trigger is only a touch when the laser is on the bottom screen when you pull it. Anywhere else it is ZL or ZR, and stays that for as long as you hold it. A second trigger pulled on the bottom screen while it is already being touched does nothing. A grip grabs the screen under the laser when you squeeze it; a grip squeezed with the laser on neither screen does nothing.

The top screen goes no nearer than 1 m, and no further than 10 m. Moving them never moves the screens sideways. The bottom screen follows the top one, 0.75 m lower and 0.5 m nearer, tilted back. The screens keep their place when you let go. The right stick click puts both back where they start.

## The game picker

| Control | What it does |
|---|---|
| Trigger or **A** | Starts the game under the dot. Also presses Rescan, Previous and Next |
| **B** | Previous page |
| Either stick | Right or down: next page. Left or up: previous page. Hold it to keep turning |

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

## Playing in a window (Flat)

The same library entry can also run Azahar as a flat window on the Frame's virtual screen, with other apps usable beside it. It starts in the mode you used last (VR the first time), kept in the file `mode` in `~/.local/share/AzaharFrame` as the single word `vr` or `flat`. To start in Flat once without changing that, add `AZAHARFRAME_FLAT=1` to `session.env` in the same folder (`0` forces VR); from a terminal, `azaharframe.sh --flat` or `--vr` does the same. Switching with a button chord while it runs is coming <!-- VERIFY: chord (both bumpers + View + Menu) -->.

In Flat the Frame controllers reach the game as a gamepad from Steam Input, and Azahar uses its own controls profile for them, **Flat Gamepad**, added the first time Flat starts. The profile that VR uses is not touched, and the one in use follows the mode. The buttons are placed like the 3DS, by position:

| Frame, as a gamepad | 3DS |
|---|---|
| Left stick | Circle Pad |
| Right stick | C-Stick |
| D-pad | D-pad |
| **A** (bottom) | B |
| **B** (right) | A |
| **X** (left) | Y |
| **Y** (top) | X |
| Left and right bumper | L and R |
| Left and right trigger | ZL and ZR |
| **Menu** | Start |
| **View** | Select |
| Left stick click | Home |

The mouse is the touch screen: point at the bottom screen and click. The Frame's controller pointer acts as the mouse. Every button can be changed in **Emulation > Configure > Controls**, and your changes in **Flat Gamepad** stay; it is only added when it is missing. The layout of the screens, 3D and everything else in Azahar's settings are the ones you already have.

## Changing the layout

The layout is a controls profile in Azahar's **Emulation > Configure > Controls** window, where the Frame's controls are named **Frame A**, **Frame Left Bumper**, **Frame Menu** and so on. The first time VR starts, the buttons and sticks of the current profile are replaced with this layout, unless one of them already is a Frame control. After that your changes stay. A profile saved by an earlier version, with L and R on the grips, gets them moved to the bumpers (the grips only grab the screens now). Keep a PC profile of your own under another name if you also use the window with a keyboard.

SDL game controllers still work, and keyboard and mouse work in the desktop window.

## Rumble

None.
