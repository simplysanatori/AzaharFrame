# Controls

Azahar for Frame keeps CitraVR's layout and uses the Steam Frame controllers' real D-pad where CitraVR needed a trick on the Quest. The emulator sees a 3DS with a New 3DS's C-Stick, ZL and ZR.

<!-- VERIFY: the whole table in the headset, every row. The rows follow src/frame_xr/input/MAPPING.md and default_bindings.h. -->

## Buttons

| Steam Frame | 3DS | Notes |
|---|---|---|
| **A**, **B**, **X**, **Y** (right controller) | A, B, X, Y | |
| Left grip | L | |
| Right grip | R | |
| Left trigger | ZL | Not while you aim at a screen |
| Right trigger | ZR | Not while you aim at a screen |
| Left stick | Circle Pad | |
| Right stick | C-Stick | |
| Left D-pad | D-pad | On the Quest, CitraVR used the thumbrest and a stick for this |
| **Menu** (right controller) | Start | |
| **View** (left controller) | Select | Hold for 1.5 seconds to stop the game and go back to the picker |
| Left stick click | Home | |
| Right stick click | Resets the screen positions | Nothing reaches the game |
| Left and right bumper | Not mapped | They can be bound in the Controls window |

The left Steam button belongs to SteamVR and is never bound.

Grips press at 60 % and let go at 40 %, triggers press at 55 % and let go at 40 %, and both sticks have a 12 % dead zone.

While you hold **View**, the game sees Select held. When the 1.5 seconds are up the game sees it released and stops.

## Pointing

Each controller has a laser. A dot shows where it hits a screen. The hand that pulled its trigger last owns the dot.

| Aim at | Trigger | Stick of that hand |
|---|---|---|
| Bottom screen | Touch. Hold and move to drag the touch | |
| Top screen | Hold to move the screen. It stays the same distance from you and turns to face you | Up and down: nearer and farther. It does not reach the game while you drag |
| The game picker | Select a game | Pages |

What a trigger does is decided when you pull it. If the laser is on neither screen, it is ZL or ZR, and stays that for as long as you hold it. A second trigger pulled on a screen that is already being touched or dragged does nothing.

The top screen goes no nearer than 1 m, and no further than 10 m. Dragging never moves it sideways. The bottom screen follows it, 0.75 m lower and 0.5 m nearer, tilted back. The screens keep their place when you let go. The right stick click puts both back where they start.

## The game picker

| Control | What it does |
|---|---|
| Trigger or **A** | Starts the game under the dot. Also presses Rescan, Previous and Next |
| **B** | Previous page |
| Either stick | Right or down: next page. Left or up: previous page. Hold it to keep turning |

## Immersive mode

| Control | What it does |
|---|---|
| Left bumper | Shows or hides the bottom screen. Every game starts with it hidden |
| Right stick click | Also re-centres the view on your head |
| Triggers | ZL and ZR while the bottom screen is hidden |

The top screen cannot be dragged in immersive mode. More in [src/frame_xr/IMMERSIVE.md](../src/frame_xr/IMMERSIVE.md).

## Changing the layout

The layout is a controls profile in Azahar's **Emulation > Configure > Controls** window, where the Frame's controls are named **Frame A**, **Frame Left Grip**, **Frame Menu** and so on. The first time VR starts, the buttons and sticks of the current profile are replaced with this layout, unless one of them already is a Frame control. After that your changes stay. Keep a PC profile of your own under another name if you also use the window with a keyboard.

SDL game controllers still work, and keyboard and mouse work in the desktop window.

## Rumble

None.
