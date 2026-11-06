# Controls

> **DRAFT.** This is the layout decided for the first release. The final mapping is written by the input code in `src/frame_xr/input/MAPPING.md`; this table is replaced from it before the release. <!-- VERIFY: replace with MAPPING.md -->

CitraFrame keeps CitraVR's layout and uses the Steam Frame controllers' real D-pad where CitraVR needed a trick on the Quest. The emulator sees a 3DS with a New 3DS's C-Stick, ZL and ZR. The layout is the default **VR** controls profile in Azahar's **Emulation > Configure > Controls** window, so you can change any of it there.

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
| **View** | Select | Hold for 1.5 seconds to go back to the game picker |
| Bumpers | Not mapped | |

## Pointing

Each controller has a laser. A dot shows where it hits a screen.

| Aim at | Trigger | Stick |
|---|---|---|
| Bottom screen | Touch. Hold and move to drag | |
| Top screen | Hold to move the screen with the controller | Up and down: nearer and farther |
| The game picker | Select a game | Scroll |

The screens keep their place when you let go. The reset command puts both back where they start. <!-- VERIFY: how reset is bound -->

## Home and recentre

<!-- VERIFY: Home and recentre bindings are taken from CitraVR (vr_main.cpp) and mapped to Menu and View in the input code. Fill in from MAPPING.md. -->

## Other controllers

SDL game controllers still work, and keyboard and mouse work in the desktop window. Use **Emulation > Configure > Controls** as on a PC.

## Rumble

<!-- VERIFY: v1 has no rumble unless lane C adds it; delete this section if it is not in MAPPING.md. -->
