# Controls

Azahar for Frame keeps CitraVR's layout and uses the Steam Frame controllers' real D-pad where CitraVR needed a trick on the Quest. The emulator sees a 3DS with a New 3DS's C-Stick, ZL and ZR.

<!-- VERIFY: the whole table in the headset, every row. The rows follow src/frame_xr/input/MAPPING.md and default_bindings.h. -->

## Buttons

| Steam Frame | 3DS | Notes |
|---|---|---|
| **A** (bottom), **B** (right), **X** (left), **Y** (top) on the right controller | B, A, Y, X | By position, as on the 3DS, where A is on the right and B is below it. **Face Buttons** in the VR settings changes it to by label |
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
| Left stick click, tap | Hides or shows the screen that is not in focus | Nothing reaches the game. See Swapping the screens |
| Left stick click, held for 1 second | Nothing | A tap hides or shows the screen only when you let go, so a hold does nothing |
| Right stick click, tap | Swaps the screens: the other screen becomes the main one | Nothing reaches the game. Tap again to swap back. See Swapping the screens |
| Right stick click, held for 1 second | Resets the screen positions | Nothing reaches the game. A stick click is ignored (tap and hold) while that stick is pushed more than half way, so free camera cannot swap or reset by accident |
| Left bumper + right bumper + **View** | Shows or hides the bottom screen | Only in games that hide it, see below. The game does not see that press |
| Left bumper + right bumper + **View** + **Menu** | Switches between VR and Flat | See below. The game sees no Select or Start |
| **View**, then the right bumper | Quick Save | Hold **View** first, then press the bumper. The game sees none of the three. See Quick Save and Quick Load |
| **View**, then the left bumper | Quick Load | The same |
| **View**, then the right trigger | Fast Forward on or off | Hold **View** first, then pull the trigger. The game sees neither Select nor ZR. See Fast Forward |

The left Steam button belongs to SteamVR and is never bound.

The Steam Frame buttons are labelled like an Xbox controller, and the 3DS is not: its A is on the right, B at the bottom, X at the top and Y on the left. So by default each button does what the 3DS button in the same place does. In the picker, **A** and **B** are not game buttons and keep their names (see below). If you would rather have A as A, set **Face Buttons** to **By Label (Xbox)**, see Settings.

Grips press at 60 % and let go at 40 %, triggers press at 55 % and let go at 40 %, and both sticks have a 12 % dead zone.

While you hold **View**, the game sees Select held. When the 1.5 seconds are up the game sees it released and stops.

A tap of **Menu** reaches the game as Start when you let go.

A tap of a stick click acts when you let go, so a hold never also taps: a hold of the left stick click does nothing, and the right one resets only after a full second.

Because **View** also starts Quick Save, Quick Load and Fast Forward, the game sees it a moment late: Select starts 0.15 seconds after you press **View** (a quick tap is Select for 0.1 seconds, sent when you let go). Holding it for 1.5 seconds still stops the game, counted from when Select starts.

## Quick Save and Quick Load

Hold **View**, then press the right bumper to Quick Save or the left bumper to Quick Load. It is the same Quick Save slot as **Emulation > Quick Save** and **Quick Load** in Azahar's window. A short message in front of you says "Quick Saved" or "Quick Loaded" (or "No Quick Save" when there is nothing to load). The game sees neither bumper nor Select, and the stop timer starts over.

- **View** has to go down first, with no bumper already held. Bumpers first, then **View**, is still the bottom screen button.
- Pressing the other bumper while the first is held does the other action, and never the bottom screen button.
- A second press within a second does nothing, so a bounce cannot load twice.
- It works in a game in VR and in Flat. In the in-game menu the same two actions are buttons in the row at the top, see below.

<!-- VERIFY: the combo and the message in the headset -->

## Fast Forward

Hold **View**, then pull the right trigger to turn Fast Forward on: the game runs at Azahar's turbo speed (**Turbo Speed Limit** in **Emulation > Configure > General**, 200 % unless you changed it). "Fast Forward" shows in front of you for 2 seconds, then fades. Do the same again to turn it off; "Normal Speed" shows for 2 seconds. It also goes off when the game stops, when the in-game menu opens (in Flat: when holding **Menu** pauses), and when you switch between VR and Flat.

- The same rules as Quick Save: **View** has to go down first, with no bumper held, and a second pull within a second does nothing.
- The game sees neither Select nor ZR, and the stop timer starts over, so holding **View** while you pull never stops the game. Aimed at the bottom screen, that pull is not a touch either.
- The left trigger with **View** held stays ZL.
- It works in a game in VR and in Flat. In Flat there is no label; the speed in the status bar shows it.

<!-- VERIFY: Fast Forward and its label in the headset -->

## Pointing

Each controller has a laser. The hand that pulled its trigger last owns the dot, which shows over the bottom screen.

| Aim at | Trigger | Grip | Stick of that hand |
|---|---|---|---|
| Bottom screen | Touch. Hold and move to drag the touch | Move both screens | While you move them: nearer and farther |
| Top screen | ZL or ZR, as everywhere in the game | Move both screens. They stay the same distance from you and turn to face you | While you move them: nearer and farther. It does not reach the game then |

The trigger is only a touch when the laser is on the bottom screen when you pull it. Anywhere else it is ZL or ZR, and stays that for as long as you hold it. A second trigger pulled on the bottom screen while it is already being touched does nothing. A grip grabs the screen under the laser when you squeeze it; a grip squeezed with the laser on neither screen does nothing.

The top screen goes no nearer than 1 m, and no further than 10 m. Moving them never moves the screens sideways. The bottom screen follows the top one, 0.75 m lower and 0.5 m nearer, tilted back. The screens keep their place when you let go. Hold the right stick click for 1 second to put both back where they start.

## Swapping the screens

<!-- VERIFY: the swap, the hide, the touch in both focuses and immersive mode in the headset. -->

The screens have two places: the main one, where the top screen starts (2 m ahead, 1 m high), and the second one below it, where the bottom screen starts (tilted back, smaller). The screen in the main place is the one in focus. Every game starts with the top screen in focus and the screens placed as above.

| Control | What it does |
|---|---|
| Right stick click, tap | The other screen becomes the main one. The bottom screen takes the main place at the same height as the top screen had (so its pixels are the same size), and the top screen goes to the second place, with the bottom screen's tilt and offsets. Tap again to swap back. The move is instant |
| Left stick click, tap | Hides or shows the screen that is not in focus: the bottom screen while the top one is in focus, the top screen while the bottom one is in focus |
| Right stick click, held for 1 second | Resets the screen positions. The focus and what is hidden stay as they are |

- Moving the screens with a grip works in both focuses and moves the main place and the second place together. Wherever you leave them, both focuses use them.
- Touch works on the bottom screen in either focus, at its new size and place. The trigger aimed at the top screen is ZL or ZR in either place.
- The screen in focus is always shown. A hidden bottom screen is shown when it takes the focus, and hidden again when the focus goes back to the top screen. Hiding the top screen lasts until the focus changes: the top screen is shown again whenever it is the main one.
- The bottom screen is hidden at the start in games with **Hide Bottom Screen** on, and in immersive mode. There the left stick click, with the top screen in focus, does what both bumpers + **View** does (they are one switch: either one shows it and either one hides it again). With the bottom screen in focus, both bumpers + **View** hides the bottom screen and puts the top screen back in focus.
- In every other game the left stick click hides and shows the bottom screen on its own, and both bumpers + **View** is still L, R and Select for the game.
- Immersive mode pauses while the bottom screen is in focus, as it does when the bottom screen shows, and picks up again when you swap back.
- Every new game starts with the top screen in focus.

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
| **Y** | Shows or hides the keyboard, see below |

**X** and the bumpers do nothing on the window.

### The keyboard

<!-- VERIFY: the keyboard rows in the headset, and the font (Noto Sans from SteamOS). -->

The keyboard is the one from my Moonlight XR. It hangs under the window and types into the field that has the keys. Click a text field and it opens by itself; it closes again when the field loses the keys, unless you opened it with **Y**. It goes where the window goes, and never shows in a game, where **Y** is the 3DS's Y.

| Control | What it does |
|---|---|
| Trigger on a key, or **A** | Presses the key. Either hand can type |
| **?123**, **ABC** | Switch between the letters and the symbols |
| **Fn** (on the symbols) | Esc, Tab, F1 to F12, Insert, Delete, Home, End, Page Up and Down, the arrows, Ctrl, Alt and Win |
| Shift, Ctrl, Alt, Win | A tap holds it for the next key. Hold it for half a second to lock it (it turns blue), tap it again to let go. A locked Shift keeps the capitals |
| Grip or trigger on the bar under it | Moves the keyboard. The stick of that hand pushes it away or pulls it nearer |
| **Hide**, **Y**, or a trigger pulled at nothing | Hides it |

## The in-game menu

Hold **Menu** for 1 second in a game. The game pauses, its screens dim, and Azahar's window shows in front of them, with the same controls as above. **Emulation > Configure...**, the game's **Properties** and the **Emulation** menu all work here. Settings such as the resolution apply at once; the VR settings apply the next time a game starts.

A row of buttons shows at the top of the window, under Azahar's menu bar, only while the in-game menu is open in VR: **Quick Save**, **Quick Load** and **Passthrough**. **Quick Save** and **Quick Load** do what the Emulation menu's items do, and say so in the status bar at the bottom. **Passthrough** shows the room behind the screens instead of black, and stays as you leave it (see Settings in the user guide).

To go back to the game, hold **Menu** for 1 second again, press **B** with no menu or dialog open, or select **Emulation > Continue**. Immersive mode shows the normal screens while the menu is open.

## The controls help, the controllers and the stats

<!-- VERIFY: the help panel, the two buttons, the beams, the controller models and the stats panel in the headset. They are built and mock-tested only. -->

Two buttons, **Controls** and **Stats**, sit above the top left corner of Azahar's window, on the home window and in the in-game menu. Point at one and pull the trigger or press **A**.

**Controls** opens a panel to the left of the window that lists what every Frame control does: in a game, on Azahar's window and in the in-game menu, with the holds and the button chords. The names come from the controls profile you use, so a button you changed in **Emulation > Configure > Controls** shows its new job, and one you cleared shows **Nothing**. The face buttons follow the **Face Buttons** setting. The part that applies where you are is marked **Now**. The panel opens by itself the first time VR starts, and when the in-game menu opens in a game (it closes with the menu again, unless you opened it yourself). Once you have closed it with the **X** or the **Controls** button, the in-game menu leaves it closed until you open it again. Close it with the **X** in its corner or with the **Controls** button again.

**Stats** shows a small panel under the top screen while a game runs: the game's frame rate, the emulation speed, the frame time and its peak, the slow frames, the headset's frame rate and its late frames, how busy the CPU is and the GPU's clock. In immersive mode it hangs low in front of you instead. The **Show Stats** setting is the same switch, and the button changes the setting.

Each controller shows as a Frame controller in your hand, with its buttons, triggers and sticks moving as you press them. The laser is a soft beam from its tip. On Azahar's window and the panels it ends where it points, and pointing at nothing shows a short faint stub. In a game the beam shows only on the way to the dot over the bottom screen. A controller you have not moved or pressed for 4 seconds fades its beam and dot, and after 30 seconds the model fades too. Pick it up and they come back. A controller that loses tracking for more than a quarter of a second hides.

The models are made from the ones SteamVR has for the Frame. The installer makes them, and when they are missing the launcher makes them in the background on the next start, so the first start can be without them. Turn them off with **Show Controllers**.

## VR and Flat

Press both bumpers, **View** and **Menu** together to switch between VR and Flat, in a game or not. The game keeps running. In Flat, Azahar's window shows the game on the Frame's screen, as a normal app, and the game draws one eye only; back in VR the 3D comes back. The next start uses the mode you switched to last.

SteamVR shows the window on its big screen (the theater), so it stays when you close the dashboard; when you switch back to VR, the window goes back into the dashboard first, which takes a few seconds. A chord still held after a switch does nothing until you let go of all four buttons, at least one second later, so one press switches once.

The buttons mean the same in both modes. In Flat the controllers reach Azahar as Steam Input's gamepad, so there is no laser: the triggers are ZL and ZR, the mouse is the touch screen, and the Frame's pointer is the mouse for Azahar's menus. Holding **Menu** for 1 second pauses or continues the game, and holding **View** for 1.5 seconds stops it, as in VR.

Hold both bumpers first. **View** with both bumpers waits a moment for **Menu** before it does anything else (the bottom screen, or Select). The game sees L and R until the switch, then nothing of the four until you let go.

## Hiding the bottom screen

Two things hide the bottom screen: immersive mode, and the **Hide Bottom Screen** setting of a game (properties, **Graphics** tab, **VR** group). A hidden bottom screen cannot be touched or grabbed, and it is never drawn. The top screen can still be grabbed with a grip unless immersive mode is on.

| Control | What it does |
|---|---|
| Left bumper + right bumper + **View** | Shows or hides the bottom screen. Hold both bumpers, then press View. Every game starts with it hidden. The game sees none of the three |
| Right stick click, held for 1 second | Also re-centres the view on your head in immersive mode (same half-way rule) |
| Left stick click, tap | Shows or hides the bottom screen, the same switch as the bumper chord. See Swapping the screens |
| Triggers | ZL and ZR everywhere while the bottom screen is hidden |

## Moving the immersive screen

In immersive mode the grips move the immersive screen instead of the normal screens. The world stays where it is while you move it.

| Control | What it does |
|---|---|
| Grip, then move the hand up or down | Moves the screen up or down. Squeeze anywhere, the laser does not have to be on the screen |
| Stick of that hand, up or down | Moves the screen farther or nearer. It keeps its size in your view |
| Stick of that hand, right or left | Makes the screen bigger or smaller |
| Right stick click while holding the screen | Puts the screen back where it starts. It does not swap the screens while you hold one, and the left stick click does not hide one. Held for 1 second it also re-centres the view and resets the normal screens |

The screen starts with its sides, and the HUD there, inside the sharp middle of the lenses. Bigger only enlarges the game's picture; it shows more around it only with **Wider View** on (see the [User Guide](USER_GUIDE.md#immersive-mode)). The screen stays where you leave it, for each game. The grip and that hand's stick do not reach the game while you hold the screen. While the bottom screen shows or is in focus (or the in-game menu is open), the grips move the normal screens as usual. More in [src/frame_xr/IMMERSIVE.md](../src/frame_xr/IMMERSIVE.md).
<!-- VERIFY: the immersive screen's default place and the grip gesture in the headset -->

## Ocarina of Time 3D with Ocarina ReFramed

The Single Screen edition of Ocarina ReFramed (by Hyrule Hysteria, GameBanana 695604) puts the whole HUD, pause menu and options on the top screen. Turn on **Hide Bottom Screen** for the game. The mod reads the 3DS buttons below; the shortcuts that need ZL and ZR need New 3DS mode, which is on by default.

| What it does | 3DS | Steam Frame |
|---|---|---|
| Open the Items menu | Select | **View**, a quick tap. A hold of 1.5 s stops the game |
| Close the menus | Select, Start or B | **View**, **Menu** or **A** |
| Item I, Item II | D-pad left, D-pad down | D-pad left, D-pad down |
| Fairy (Navi) and Look Up, Ocarina | D-pad up, D-pad right | D-pad up, D-pad right |
| Show or hide the minimap | ZL | Left trigger |
| Free camera | C-Stick | Right stick |
| Camera speed up or down | L + R + D-pad up or down | Both bumpers + D-pad up or down |
| Invert camera axes | L + R + D-pad left or right | Both bumpers + D-pad left or right |
| HUD size (75, 100, 125 %, off) | L + R + ZR, held | Both bumpers + right trigger, held. The HUD size is also in Options |
| Manual jump (Options > Manual jump) | ZR, or X (the X item moves to ZR) | Right trigger, or **Y** (the X item moves to the right trigger) |
| Pause menu: pages | L and R | Left and right bumper |
| Pause menu: move, choose, back | D-pad or Circle Pad, A, B | D-pad or left stick, **B**, **A** |
| Options menu: columns | L and R | Left and right bumper |

The mod's HUD is a controller-neutral, Switch-style diamond of the items, so it does not name ZL or ZR. The mod does the 3DS touch buttons for you from the D-pad and View. If you ever need the real bottom screen, press both bumpers and **View**.

## Ocarina of Time 3D with Single Screen Experience

Single Screen Experience (by M-1, GameBanana 695893) is the mod I recommend for Ocarina of Time 3D. It puts the HUD and menus on the top screen and adds a 60 FPS option, a 16:9 option, a field of view setting, a draw distance slider and a free camera on the right stick. Turn on **Hide Bottom Screen** for the game.

- If you turn on its 16:9 option, set **Screen Shape** to **Widescreen (16:9)** in the game's properties (**Graphics** tab, **VR** group). Without it the picture looks squeezed. **Screen Shape** also has **4:3**, **3:2** and **16:10**, which match game cheats that change the game's own aspect; it must match what the game renders, or the picture stretches.
- Leave the mod's FOV at its default. If you change it, set **Immersive Game FOV** to the same value so immersive mode keeps the world still when you turn your head.
- If it stutters, turn off the mod's 60 FPS option first.
- Keep real-time shadows off, and draw distance at 100 to 150 %.

The buttons are the same as in the table above. Both bumpers and **View** bring back the bottom screen if you ever need it.

## Settings

Four settings of the **VR** group (**Emulation > Configure > Graphics**, global only, not per game) belong to the controllers.

| Setting | Choices | What it does |
|---|---|---|
| **Face Buttons** | **By Position (Nintendo)** (default), **By Label (Xbox)** | How the Frame's A, B, X and Y map to the 3DS. By Position matches the 3DS layout. Read every frame |
| **Motion Controls** | **Off**, **Right Controller** (default), **Left Controller** | Uses a controller as the 3DS's gyroscope and accelerometer. Going from Off to a controller takes effect the next time VR starts. Switching between the controllers, or back to Off, takes effect at once |
| **Show Controllers** | on (default) or off | Shows the Frame controllers in VR. They hide when you put them down. Takes effect at once |
| **Show Stats** | off (default) or on | Shows the stats panel under the top screen while a game runs. The **Stats** button above Azahar's window switches it too |

## Motion controls

Some games aim by moving the 3DS, like the first-person items in Ocarina of Time 3D (the slingshot, the bow, the hookshot). The 3DS has a gyroscope and an accelerometer for that, and in VR the controller you chose in **Motion Controls** stands in for them. Hold it as if it were the 3DS in front of you, with its beam pointing into the screen. Turn it left and right to turn like the 3DS, and tilt it up and down to tilt it. The tilt of the beam also tells the game which way is down. The game is not told about the movement of your hand through the room, only about how the controller turns and which way it hangs.

The same controller still points at the screens, so moving it to aim also moves its beam. Choose the controller you do not use for pointing in **Motion Controls** if that gets in the way. A game that does not use the sensors is not affected, and **Off** leaves the 3DS lying flat and still. A motion device you chose yourself in **Emulation > Configure > Controls > Motion/Touch** is left alone.

<!-- VERIFY: gyro aiming in the headset (direction of each axis, how fast it turns, the beam held level or tilted) -->

## Playing in a window (Flat)

The same library entry can also run Azahar as a flat window on the Frame's virtual screen, with other apps usable beside it. It starts in the mode you used last (VR the first time), kept in the file `mode` in `~/.local/share/AzaharFrame` as the single word `vr` or `flat`. To start in Flat once without changing that, add `AZAHARFRAME_FLAT=1` to `session.env` in the same folder (`0` forces VR); from a terminal, `azaharframe.sh --flat` or `--vr` does the same. To switch while it runs, press both bumpers, **View** and **Menu** together, see [VR and Flat](#vr-and-flat).

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

The mouse is the touch screen: point at the bottom screen and click. The Frame's controller pointer acts as the mouse. Every button can be changed in **Emulation > Configure > Controls**, and your changes in **Flat Gamepad** stay; it is only added when it is missing. The layout of the screens, 3D and everything else in Azahar's settings are the ones you already have.

## Changing the layout

The layout (apart from Face Buttons, see Settings) is a controls profile in Azahar's **Emulation > Configure > Controls** window, where the Frame's controls are named **Frame A**, **Frame Left Bumper**, **Frame Menu** and so on. The first time VR starts, the buttons and sticks of the current profile are replaced with this layout, unless one of them already is a Frame control. After that your changes stay. The face buttons there are the 3DS-named ones, so they follow the **Face Buttons** setting: with By Position, **Frame A** is the button in the 3DS's A place, which is the Frame's **B**. A profile saved by an earlier version, with L and R on the grips, gets them moved to the bumpers (the grips only grab the screens now). Keep a PC profile of your own under another name if you also use the window with a keyboard.

SDL game controllers still work, and keyboard and mouse work in the desktop window.

## Rumble

None.
