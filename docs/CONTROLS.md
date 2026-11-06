# Controls

<!-- VERIFY: every row is checked against src/frame_xr/input/MAPPING.md and in the headset before the release. -->

Azahar for Frame keeps CitraVR's layout and uses the Steam Frame controllers' real D-pad where CitraVR needed a trick on the Quest. The emulator sees a 3DS with a New 3DS's C-Stick, ZL and ZR. The layout is the default **VR** controls profile in Azahar's **Emulation > Configure > Controls** window, so you can change any of it there.

## Buttons

| Steam Frame | 3DS | Notes |
|---|---|---|
| **A**, **B**, **X**, **Y** | A, B, X, Y | |
| Left grip | L | |
| Right grip | R | |
| Left trigger | ZL | Not while you aim at a screen |
| Right trigger | ZR | Not while you aim at a screen |
| Left stick | Circle Pad | |
| Right stick | C-Stick | |
| Left D-pad | D-pad | On the Quest, CitraVR used the thumbrest and a stick for this |
| **Menu** | Start | |
| **View** | Select | Hold for 1.5 seconds to stop the game and go back to the picker |
| Left stick click | Home | |
| Right stick click | Resets the screen positions | |
| Bumpers | Not mapped | They can be bound in the Controls window |

The left Steam button belongs to SteamVR and is never bound.

## Pointing

Each controller has a laser. A dot shows where it hits a screen. The hand that pulled its trigger last owns the dot.

| Aim at | Trigger | Stick |
|---|---|---|
| Bottom screen | Touch. Hold and move to drag | |
| Top screen | Hold to move the screen. It stays the same distance from you and turns to face you | Up and down: nearer and farther. That stick does not reach the game while you drag |
| The game picker | Select a game | Scroll |

What a trigger does is decided when you pull it. If the laser is on neither screen, it is ZL or ZR, and stays that for as long as you hold it.

The screens keep their place when you let go. The right stick click puts both back where they start.

## Immersive mode

| Control | What it does |
|---|---|
| Left bumper | Shows or hides the bottom screen. Every game starts with it hidden |
| Right stick click | Also re-centres the view on your head |
| Triggers | ZL and ZR while the bottom screen is hidden |

More in [src/frame_xr/IMMERSIVE.md](../src/frame_xr/IMMERSIVE.md).

## Other controllers

SDL game controllers still work, and keyboard and mouse work in the desktop window. Use **Emulation > Configure > Controls** as on a PC.

## Rumble

None. <!-- VERIFY: delete if rumble gets added -->
