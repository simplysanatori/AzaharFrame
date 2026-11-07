# Controls

Azahar for Frame keeps CitraVR's layout and uses the Steam Frame controllers' real D-pad where CitraVR needed a trick on the Quest. The emulator sees a New 3DS, with its C-Stick, ZL and ZR.

## Buttons

| Steam Frame | 3DS | Notes |
|---|---|---|
| **A** (bottom), **B** (right), **X** (left), **Y** (top) on the right controller | B, A, Y, X | By position, as on the 3DS, where A is on the right and B is below it. **Face Buttons** in the VR settings changes it to by label |
| Left bumper | L | |
| Right bumper | R | |
| Left trigger | ZL | Aimed at the bottom screen it is a touch instead |
| Right trigger | ZR | The same |
| Left and right grip | Move the screens | Nothing reaches the game. See Pointing |
| Left stick | Circle Pad | |
| Right stick | C-Stick | |
| Left D-pad | D-pad | On the Quest, CitraVR used the thumbrest and a stick for this. While a grip holds a screen, the D-pad nudges that screen instead |
| **Menu** (right controller) | Start | Hold for 1 second to open the in-game menu. The game does not see that hold. A bumper or **View** pressed during the hold cancels it |
| **View** (left controller) | Select | Hold for 1.5 seconds to stop the game and go back to Azahar's window |
| Left stick click, tap | Hide or Show Other Screen | Nothing reaches the game. See Swapping the screens |
| Left stick click, held | Nothing | There is no Home button in VR |
| Right stick click, tap | Swap Screens | The other screen becomes the main one. Tap again to swap back. Nothing reaches the game |
| Right stick click, held for 1 second | Reset Screens | Nothing reaches the game |
| Both bumpers, then **View** | Show or Hide Bottom Screen | Only in games that hide it, see below. The game does not see that press |
| Both bumpers + **View** + **Menu** | Switch between VR and Flat | The game sees no Select or Start |
| Hold **View**, then the right bumper | Quick Save | When you let go of the bumper. The game sees none of the three |
| Hold **View**, then the left bumper | Quick Load | The same |
| Hold **View**, then the right trigger | Fast Forward on or off | When you let go of the trigger. The game sees neither Select nor ZR |

The left Steam button belongs to SteamVR and is never bound.

The Frame's buttons are labelled like an Xbox controller, and the 3DS's are not: its A is on the right, B at the bottom, X at the top and Y on the left. So by default each button does what the 3DS button in the same place does. If you would rather have A as A, set **Face Buttons** to **By Label (Xbox)**, see Settings.

Grips press at 60 % and let go at 40 %. Triggers press at 55 % and let go at 40 %. Both sticks have a 12 % dead zone.

A tap of **Menu** reaches the game as Start when you let go.

A stick click acts when you let go, so a hold never also taps. The right one resets after a full second. A stick click is ignored while that stick is pushed more than half way, so the free camera cannot swap or reset by accident.

**View** also starts Quick Save, Quick Load and Fast Forward, so the game sees it a moment late. Select starts 0.15 seconds after you press **View**. A quick tap is a 0.1 second Select, sent when you let go. Holding **View** for 1.5 seconds stops the game, counted from when Select starts. While you hold it the game sees Select held, and it sees Select let go when the game stops.

## Quick Save and Quick Load

Hold **View**, then press the right bumper to Quick Save or the left bumper to Quick Load. It happens when you let go of the bumper, with **View** still held. It is the same slot as **Emulation > Quick Save** and **Quick Load** in Azahar's window. A short message in front of you says "Quick Saved" or "Quick Loaded", or "No Quick Save" when there is nothing to load. The game sees neither bumper nor Select, and the stop timer starts over.

- **View** has to go down first, with no bumper held. Bumpers first, then **View**, is the bottom screen button.
- Pressing the other bumper or **Menu** while the bumper is held cancels it, so forming the VR / Flat switch never saves or loads. So does letting go of **View** first.
- A second press within a second does nothing, so a bounce cannot load twice.
- It works in a game in VR and in Flat. The in-game menu has **Quick Save** and **Quick Load** buttons too, see below.

## Fast Forward

Hold **View**, then pull and let go of the right trigger to turn Fast Forward on. The game runs at Azahar's turbo speed: **Turbo Speed Limit** in **Emulation > Configure > General**, 200 % unless you changed it. "Fast Forward" shows in front of you for 2 seconds. Do the same again to turn it off, and "Normal Speed" shows for 2 seconds. It also goes off when the game stops, when the in-game menu or the Flat menu opens and when you switch between VR and Flat.

- **View** has to go down first, with no bumper held. A bumper or **Menu** pressed while you pull cancels it. A second pull within a second does nothing.
- The game sees neither Select nor ZR, and the stop timer starts over. Aimed at the bottom screen, that pull is not a touch either.
- The left trigger with **View** held stays ZL.
- It works in a game in VR and in Flat. In Flat there is no message; the speed in the status bar shows it.

## Pointing

Each controller has a beam. The hand that pulled its trigger last owns the dot, which shows over the bottom screen.

| Aim at | Trigger | Grip | Stick of that hand |
|---|---|---|---|
| Bottom screen | Touch. Hold and move to drag the touch | Move both screens | While you move them: nearer and farther (up, down), bigger and smaller (right, left) |
| Top screen | ZL or ZR, as everywhere in the game | Move both screens. They keep their distance from you and turn to face you | While you move them: nearer and farther, bigger and smaller. It does not reach the game then |

The trigger is a touch only when the beam is on the bottom screen as you pull it. Anywhere else it is ZL or ZR, for as long as you hold it. A second trigger pulled on the bottom screen while it is already touched does nothing. A grip grabs the screen under the beam when you squeeze it. A grip squeezed with the beam on neither screen does nothing.

The top screen starts 2 m ahead at eye height, 1 m tall. It goes no nearer than 1 m and no further than 10 m. It can be made up to 3 times bigger, which fills the whole view from where it starts, and down to half size. Making it bigger is the way to fill the view: the screens never come nearer than 1 m and 0.5 m. A screen bigger than its start size gets that much less 3D depth (a third at 3 times), so the two eyes' pictures of the distant scenery stay as far apart as at the start size and still fuse. Moving the screens with the hand never moves them sideways.

While a grip holds the screens, the D-pad nudges them 0.5 degrees per press around you, left, right, up or down. Hold it to keep going. They keep their distance and turn to face you, and a label shows **Moved Right** (or Left, Up, Down) for 2 seconds. Use it when a big screen's edge is just out of view. The D-pad does not reach the game while you hold the screens, nor a direction you pressed then until you let go of it.

The bottom screen follows the top one, 0.5 m nearer and tilted back 45 degrees, just under the top screen's bottom edge (0.75 m below its middle at the start size). It keeps its own size when the top screen grows, and moves down with that edge. The screens stay where you leave them. Hold the right stick click for 1 second to put both back.

## Swapping the screens

The screens have two places. The main one is where the top screen starts. The second one is below it, where the bottom screen starts, tilted back and smaller. The screen in the main place is the one in focus. Every game starts with the top screen in focus.

| Control | What it does |
|---|---|
| Right stick click, tap | The other screen becomes the main one. The bottom screen takes the main place at the top screen's height, and the top screen goes to the second place. Tap again to swap back |
| Left stick click, tap | Hides or shows the screen that is not in focus |
| Right stick click, held for 1 second | Resets the screen positions. The focus and what is hidden stay as they are |

- A grip moves both places together, in either focus.
- Touch works on the bottom screen in either place. The trigger aimed at the top screen is ZL or ZR in either place.
- The screen in focus always shows. A hidden bottom screen shows while it is in focus, and hides again when the top screen takes the focus back. A hidden top screen shows again whenever it is the main one.
- In games with **Hide Bottom Screen** on, and in immersive mode, the bottom screen starts hidden. There the left stick click and both bumpers + **View** are one switch: either one shows it, and either one hides it again. With the bottom screen in focus, both bumpers + **View** hides it and puts the top screen back in focus.
- In every other game the left stick click hides and shows the bottom screen, and both bumpers + **View** is L, R and Select for the game.
- Immersive mode pauses while the bottom screen is in focus, and picks up again when you swap back.

## Azahar's window

When no game runs, Azahar's own window floats in front of you: the game list, the menus and every dialog. The beam is the mouse. Double-click a game to start it, and use **Emulation > Configure...** for the settings. When a game stops, the window comes back.

| Control | What it does |
|---|---|
| Trigger or **A** | Left click. Hold it to drag |
| Right stick click | Right click, for example on a game for **Properties** |
| Right stick click, held for 1 second | Brings the window back in front of you |
| **B** | Back: closes a menu or a dialog |
| Grip aimed at the window | Moves the window. It keeps its distance from you and turns to face you |
| Either stick, up or down | Scrolls |
| **Y** | Shows or hides the keyboard, see below |

**X** and the bumpers do nothing on the window.

### The keyboard

The keyboard is the one from my Moonlight XR. It hangs under the window and types into the field that has the keys. Click a text field and it opens. It closes again when the field loses the keys, unless you opened it with **Y**. It goes where the window goes, and never shows in a game, where **Y** is the 3DS's X.

| Control | What it does |
|---|---|
| Trigger on a key, or **A** | Presses the key. Either hand can type |
| **?123**, **ABC** | Switch between the letters and the symbols |
| **Fn** (on the symbols) | Esc, Tab, F1 to F12, Insert, Delete, Home, End, Page Up and Down, the arrows, Ctrl, Alt and Win |
| Shift, Ctrl, Alt, Win | A tap holds it for the next key. Hold it for half a second to lock it, and tap it again to let go. A locked Shift keeps the capitals |
| Grip or trigger on the bar under it | Moves the keyboard. The stick of that hand pushes it away or pulls it nearer |
| The hide key, **Y**, or a trigger pulled at nothing | Hides it |

## The in-game menu

Hold **Menu** for 1 second in a game. The game pauses, its screens dim, and Azahar's window shows in front of them, with the same controls as above. **Emulation > Configure...**, the game's **Properties** and the **Emulation** menu all work here. Settings such as the resolution apply at once, and so do **Screen Shape** and **Immersive Game FOV**. **Hide Bottom Screen**, **Wider View** and **Squeeze Width** apply the next time the game starts.

A row of buttons shows under Azahar's menu bar while the in-game menu is open: **Quick Save**, **Quick Load** and **Passthrough**. **Quick Save** and **Quick Load** do what the Emulation menu's items do, and say so in the status bar. **Passthrough** shows the room behind the screens instead of black, and stays as you leave it.

To go back to the game, hold **Menu** for 1 second again, press **B** with no menu or dialog open, or select **Emulation > Continue**. Holding **View** for 1.5 seconds stops the game. The Full immersive modes show the normal screens while the menu is open.

## The controls help, the controllers and the stats

Two buttons, **Controls** and **Stats**, sit above the top left corner of Azahar's window, on the home window and in the in-game menu. Point at one and pull the trigger or press **A**.

**Controls** opens a panel to the left of the window. It shows drawings of the two Frame controllers, from the top and from the front, with a line from each button, stick, bumper, trigger and grip to what it does. Two tabs pick the page: **In Game** and **Azahar's Window** (which also has the in-game menu). The holds and the chords sit under the button they start from, for example **Hold View +** under View. The page that applies where you are opens first. The drawings are made from the controller models on your Frame; if they are missing, the panel shows the same controls as a list. The names come from your controls profile: a button you changed in **Emulation > Configure > Controls** shows its new job, and one you cleared shows **Nothing**. The face buttons follow the **Face Buttons** setting. In the list, the part that applies where you are is marked. The panel opens by itself the first time VR starts, and when the in-game menu opens, and closes with the menu. Once you close it with the **X** in its corner or the **Controls** button, the in-game menu leaves it closed until you open it again.

**Stats** shows a small panel under the top screen while a game runs. It shows the game's frame rate, the emulation speed, the frame time and its peak, the slow frames, the headset's frame rate and late frames, the CPU load and the GPU clock. In immersive mode it hangs low in front of you. The **Show Stats** setting is the same switch.

Each controller shows as a Frame controller in your hand, with its buttons, triggers and sticks moving as you press them. The beam is soft and comes from its tip. On Azahar's window and the panels it ends where it points; pointing at nothing shows a short, faint stub. In a game the beam shows only on the way to the dot over the bottom screen. A controller you leave still for 4 seconds fades its beam and dot, and after 30 seconds its model fades too. Pick it up and they come back. A controller that loses tracking for more than a quarter of a second hides.

The models are made from SteamVR's own Frame models when you install. If they are missing, the launcher makes them in the background and they show from the next start. Turn them off with **Show Controllers**.

## VR and Flat

Press both bumpers, **View** and **Menu** together to switch between VR and Flat, in a game or not. The game keeps running. In Flat, Azahar's window shows the game on SteamVR's big screen (the theater), as a normal app, and stays there when you close the dashboard. Back in VR the 3D comes back; the window first goes back into the dashboard, which takes a few seconds. The next start uses the mode you switched to last.

Hold both bumpers first. **View** with both bumpers held waits a moment for **Menu** before it is the bottom screen button or Select. The game sees L and R until the switch, then nothing of the four until you let go. After a switch the chord does nothing until you let go of all four buttons, and at least one second has passed, so one press switches once.

## Hiding the bottom screen

Two things hide the bottom screen: immersive mode, and a game's **Hide Bottom Screen** setting (**Properties**, **Graphics** tab, **VR** group). A hidden bottom screen cannot be touched or grabbed, and it is not drawn. The top screen can still be grabbed with a grip unless immersive mode is on.

| Control | What it does |
|---|---|
| Both bumpers, then **View** | Shows or hides the bottom screen. The game sees none of the three |
| Left stick click, tap | The same switch, with the top screen in focus. See Swapping the screens |
| Right stick click, held for 1 second | In immersive mode it also re-centres the view on where you look. SteamVR's recenter (hold the Steam button) does the same |
| Triggers | ZL and ZR everywhere while the bottom screen is hidden |

## Moving the immersive screen

In immersive mode the grips move the immersive screen instead of the normal screens. The world stays where it is while you move it.

| Control | What it does |
|---|---|
| Grip, then move the hand up or down | Moves the screen up or down. Squeeze anywhere; the beam does not have to be on the screen |
| Stick of that hand, up or down | Moves the screen farther or nearer. It keeps its size in your view |
| Stick of that hand, right or left | Makes the screen bigger or smaller |
| D-pad, while holding the screen | Nudges the screen 0.5 degrees per press, to fit your lenses: left, right, up or down. Hold it to keep going. A label shows the offset, for example **Offset Right 1.5°**, for 2 seconds |
| Right stick click, tap, while holding the screen | Puts the screen back where it starts. It does not swap the screens while you hold one, and the left stick click does not hide one |
| Right stick click, held for 1 second, while holding the screen | Also re-centres the view and resets the normal screens |

The D-pad offset is for your eyes, not for a game: it is the same in every game and every immersive mode, and it stays when you put a game's screen back. It goes up to 10 degrees each way. The world stays where it is, only the screen moves. The **Immersive Offset** setting shows it and puts it back to the middle.

The screen starts a little below eye level, with its sides, and the HUD there, inside the sharp middle of the lenses. It can grow to 2.5 times that size. Bigger only enlarges the game's picture; it shows more around it only with **Wider View** on (see the [user guide](USER_GUIDE.md#immersive-mode)). Each game keeps the screen where you leave it. The grip, that hand's stick and the D-pad do not reach the game while you hold the screen. While immersive mode is paused (the bottom screen in focus, or in the Full modes the bottom screen shown or the in-game menu open), the grips move the normal screens. More in [src/frame_xr/IMMERSIVE.md](../src/frame_xr/IMMERSIVE.md).

## Ocarina of Time 3D with Ocarina ReFramed

The Single Screen edition of Ocarina ReFramed (by Hyrule Hysteria, GameBanana 695604) puts the whole HUD, pause menu and options on the top screen. Turn on **Hide Bottom Screen** for the game. The mod reads the 3DS buttons below. The shortcuts that need ZL and ZR need New 3DS mode, which is on by default.

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

The mod's HUD is a Switch-style diamond of the items, so it does not name ZL or ZR. The mod does the 3DS touch buttons for you from the D-pad and View. If you ever need the real bottom screen, press both bumpers and **View**.

## Ocarina of Time 3D with Single Screen Experience

Single Screen Experience (by M-1, GameBanana 695893) is the mod I recommend for Ocarina of Time 3D. It puts the HUD and menus on the top screen and adds a 60 FPS option, a 16:9 option, a field of view setting, a draw distance slider and a free camera on the right stick. Turn on **Hide Bottom Screen** for the game.

- If you turn on its 16:9 option, set **Screen Shape** to **Widescreen (16:9)** in the game's properties (**Graphics** tab, **VR** group). Without it the picture looks squeezed. **Screen Shape** also has **Standard (4:3)**, **3:2** and **16:10**, which match game cheats that change the game's own aspect. It must match what the game renders, or the picture stretches.
- Leave the mod's FOV at its default. If you change it, set **Immersive Game FOV** to the same value, so immersive mode keeps the world still when you turn your head.
- If it stutters, turn off the mod's 60 FPS option first.
- Keep real-time shadows off, and draw distance at 100 to 150 %.

The buttons are the same as in the table above. Both bumpers and **View** bring back the bottom screen if you ever need it.

## Settings

The VR settings are in the **VR** group at the bottom of the **Graphics** tab. Every one is explained in [SETTINGS.md](SETTINGS.md).

## Motion controls

Some games aim by moving the 3DS, like the first-person items in Ocarina of Time 3D (the slingshot, the bow, the hookshot). In VR the controller you chose in **Motion Controls** is the 3DS's gyroscope and accelerometer. Hold it as if it were the 3DS in front of you, with its beam pointing into the screen. Turn it left and right to turn, and tilt it up and down to tilt. The tilt of the beam also tells the game which way is down. The game is told how the controller turns and which way it hangs, not how your hand moves through the room.

The same controller still points at the screens, so aiming also moves its beam. If that gets in the way, choose the controller you do not point with. A game that does not use the sensors is not affected, and **Off** leaves the 3DS lying flat and still. A motion device you chose yourself in **Emulation > Configure > Controls > Motion/Touch** is left alone.

## Playing in a window (Flat)

The same library entry can also run Azahar as a flat window on SteamVR's big screen, with other apps usable beside it. It starts in the mode you used last, VR the first time. That mode is kept in the file `mode` in `~/.local/share/AzaharFrame`, as the single word `vr` or `flat`. To start in Flat once without changing it, add `AZAHARFRAME_FLAT=1` to `session.env` in the same folder (`0` forces VR). From a terminal, `azaharframe.sh --flat` or `--vr` does the same. To switch while it runs, see [VR and Flat](#vr-and-flat).

In Flat the Frame controllers reach Azahar as a gamepad from Steam Input, and Azahar maps it the same way as in VR: the same controls profile, the same buttons, holds and chords. A tap of **View** is still Select and a tap of **Menu** is still Start, so the game's own menus work.

The differences:

- There is no beam. The mouse is the touch screen: point at the bottom screen and click. The Frame's controller pointer acts as the mouse.
- The triggers are always ZL and ZR, and the grips do nothing.
- The stick clicks change the window's screen layout. A tap of the right stick click swaps the screens. A tap of the left one shows only the main screen (**Single Screen**), and the next tap brings the other one back. Holding the right one for 1 second goes back to **Large Screen**, not swapped.
- Holding **Menu** for 1 second opens the Flat menu instead of the in-game menu, see below.

Azahar's window is 16:9 in Flat, the shape of the big screen, so it fills it with no bars at the sides. Flat starts with the **Large Screen** layout: one big screen and the other one small beside it, which suits the wide screen. Flat keeps its own layout. The stick clicks and **View > Screen Layout** change it while you play in Flat, and the layout in the settings stays as it is for VR. **View > Fullscreen** works in Flat only: in VR the in-game menu always shows the whole window, with its menus.

Holding **View** for 1.5 seconds stops the game. Quick Save, Quick Load, Fast Forward and the switch to VR work as in VR, and the game sees the same of their buttons as in VR (see [Buttons](#buttons)).

### The Flat menu

Hold **Menu** for 1 second in a game in Flat. The game pauses and Azahar's window leaves Fullscreen, with its menu bar and status bar, so the pointer reaches **File**, **Emulation**, **View** and the rest, **Emulation > Configure Current Application...** included. The window stays on the big screen, 16:9.

To go back to the game, hold **Menu** for 1 second again, press **B** with no menu or dialog open, or select **Emulation > Continue**. The window goes back to Fullscreen if it was. **B** with a menu or dialog open closes that first. The game sees none of these buttons while the menu is open. Holding **View** for 1.5 seconds stops the game.

Switching between VR and Flat with the in-game menu or the Flat menu open closes it, and the game goes on.

If VR cannot start at all (no SteamVR, or a renderer other than Vulkan), Azahar runs in its window with a plain controls profile of its own, **Flat Gamepad**, by position like the 3DS, without the holds and chords. The launcher adds it once, when it is missing.

## Changing the layout

The VR layout (apart from Face Buttons) is a controls profile in **Emulation > Configure > Controls**. There the Frame's controls are named **Frame A**, **Frame Left Bumper**, **Frame Menu** and so on. The first time VR starts, the buttons and sticks of the current profile get this layout, unless one of them already is a Frame control. After that your changes stay. The face buttons there are the 3DS-named ones, so they follow **Face Buttons**: with By Position, **Frame A** is the button in the 3DS's A place, which is the Frame's **B**. Keep a profile of your own under another name if you also use the window with a keyboard.

SDL game controllers still work, and so do a keyboard and mouse on the window.

## Rumble

None.
