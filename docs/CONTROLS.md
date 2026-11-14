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
| Left and right grip | Move the screens | See Pointing |
| Left stick | Circle Pad | |
| Right stick | C-Stick | |
| Left D-pad | D-pad | On the Quest, CitraVR used the thumbrest and a stick for this. While a grip holds a screen, the D-pad nudges that screen (left, right) or changes its curve (up, down) instead |
| **Menu** (right controller) | Start | Hold for 1 second to open the in-game menu |
| **View** (left controller) | Select | Hold for 1.5 seconds to stop the game and go back to Azahar's window. **Stopping...** shows half way; let go before the end to keep playing |
| **Menu** or **View**, while a game starts | | Hold for 3 seconds to stop it and go back to Azahar's window, for a start that takes too long (a texture preload) |
| Left stick click, tap | Hide or Show Other Screen | See Swapping the screens |
| Left stick click, held for 1 second | Screen Layout | See Screen layouts |
| Right stick click, tap | Swap Screens | The Secondary Screen becomes the Primary Screen. Tap again to swap back |
| Right stick click, held for 1 second | Reset Screens and View | |
| Both bumpers, then **View** | Show or Hide Bottom Screen | Only in games that hide it, see below |
| Both bumpers + **View** + **Menu** | Switch between VR and Flat | |
| Hold **View**, then the right bumper | Quick Save | When you let go of the bumper |
| Hold **View**, then hold the left bumper for half a second | Quick Load | |
| Hold **View**, then the right trigger | Fast Forward on or off | When you let go of the trigger |

Shortcuts never reach the game. The left Steam button belongs to SteamVR and is never bound.

The Frame's buttons are labelled like an Xbox controller, and the 3DS's are not: its A is on the right, B at the bottom, X at the top and Y on the left. So by default each button does what the 3DS button in the same place does. If you would rather have A as A, set **Face Buttons** to **By Label (Xbox)**, see Settings.

A tap of **Menu** is Start and a tap of **View** is Select, sent when you let go. A stick click pushed more than half way is ignored, so the free camera cannot swap or reset by accident. The exact timings are in [TECHNICAL.md](TECHNICAL.md#controls).

## Quick Save and Quick Load

Hold **View**, then press the right bumper to Quick Save, or hold the left bumper for half a second to Quick Load. The half second is there so a brush of the bumper never throws away your progress. It is the same slot as **Emulation > Quick Save** and **Quick Load** in Azahar's window, and the in-game menu has both buttons too. A short message in front of you says "Quick Saved" or "Quick Loaded", or "No Quick Save" when there is nothing to load. Press **View** first: bumpers first, then **View**, is the bottom screen button. It works in VR and in Flat.

## Fast Forward

Hold **View**, then pull and let go of the right trigger to turn Fast Forward on. The game runs at Azahar's turbo speed: **Turbo Speed Limit** in **Emulation > Configure > General**, 200 % unless you changed it. "Fast Forward" shows in front of you, and doing it again shows "Normal Speed". The sound stays at its normal pitch while it runs. **Fast Forward Audio**, on the VR **Settings** tab (General) and in **Emulation > Configure > Audio**, picks **Sped Up** (the game's own sound, faster and higher), **Normal Pitch** (the default), **Normal Speed (Experimental)** (real speed, skipping ahead to keep up; the sound can rise and fall in volume) or **Muted**. It also goes off when the game stops, when a menu opens and when you switch between VR and Flat. In Flat the speed in the status bar shows it.

## Pointing

Each controller has a beam. The hand that pulled its trigger last owns the dot, which shows over the bottom screen.

| Aim at | Trigger | Grip | Stick of that hand |
|---|---|---|---|
| Bottom screen | Touch. Hold and move to drag the touch | Move both screens | While you move them: nearer and farther (up, down), bigger and smaller (right, left) |
| Top screen | ZL or ZR, as everywhere in the game | Move both screens. They keep their distance from you and turn to face you | While you move them: nearer and farther, bigger and smaller. It does not reach the game then |
| Stats panel (In Front) | ZL or ZR, nothing behind it is touched | Move the stats. They keep their distance from you and turn to face you | While you move them: nearer and farther, bigger and smaller. It does not reach the game then |

The trigger is a touch only when the beam is on the bottom screen as you pull it; anywhere else it is ZL or ZR for as long as you hold it. A grip grabs the screen under the beam.

The top screen starts 2 m ahead at eye height. It can be made up to 3 times bigger, which fills the whole view, and down to half size. A bigger screen gets less 3D depth, so the distant scenery stays easy on the eyes.

While a grip holds the screens, D-pad left or right nudges them 0.5 degrees per press around you. Hold it to keep going. They keep their distance and turn to face you, and a label shows **Moved Right** (or Left) for 2 seconds. Use it when a big screen's edge is just out of view. To move them up or down, move the hand that holds them.

D-pad up or down, while a grip holds the screens, makes the Primary Screen more or less curved, one step per press: Off, 15°, 30°, 45°, 60°, 75° and 90°. A label shows the new value, for example **Screen Curve: 45°**, and it is kept for every game. The Secondary Screen stays flat. It is the **Screen Curve** setting ([SETTINGS.md](SETTINGS.md#screen-curve)).

When you take hold of the screens, a small hint under them lists what the D-pad and the stick do. The D-pad does not reach the game while you hold the screens.

The bottom screen follows the top one, tilted back, just under its bottom edge. Each game keeps the screens where you leave them. Hold the right stick click for 1 second to put both back. To move them apart, see Detaching the screens.

## Detaching the screens

**Detach Screens** in the in-game menu's **Position** tab lets the screens move apart. It is kept for every game.

| Choice | Primary Screen | Secondary Screen |
|---|---|---|
| **Off** | A grip on either screen moves both | Follows the Primary Screen |
| **Secondary** | Moves as with **Off** | Moves on its own |
| **Both** | Moves on its own | Moves on its own |

- A screen that moves on its own goes wherever you carry it with the grip, all around you, and turns to face you, like the stats panel. While you hold it, the stick of that hand moves it nearer or farther and makes it bigger or smaller (half to 3 times its size), and D-pad left or right nudges it around you.
- After a swap the Secondary Screen is the top screen.
- D-pad up or down changes the Screen Curve only while you hold the Primary Screen. The hint shows under the screen you hold.
- Touch works on the bottom screen wherever you put it.
- Hold the right stick click for 1 second to put both back together in front of you, at their normal size. **Detach Screens** stays as it is. **Off**, or a new Screen Layout, puts the Secondary Screen back in its place next to the Primary Screen.
- Each game keeps its own screen positions. Immersive mode is not affected.

## Swapping the screens

The screens have two places. The Primary Screen is where the top screen starts. The Secondary Screen is smaller: below it, tilted back, or to its right with **Side by Side** (see Screen layouts). The screen in the Primary Screen's place is the one in focus. Every game starts with the top screen in focus.

| Control | What it does |
|---|---|
| Right stick click, tap | The bottom screen becomes the Primary Screen, at the top screen's height, and the top screen becomes the Secondary Screen. Tap again to swap back |
| Left stick click, tap | Hides or shows the screen that is not in focus |
| Right stick click, held for 1 second | Resets the screen positions, and in immersive mode recentres the view. The immersive screen keeps its size unless you hold it with a grip. The focus and what is hidden stay as they are |

- A grip moves both places together, in either focus, unless **Detach Screens** is on (see Detaching the screens).
- Touch works on the bottom screen in either place. The trigger aimed at the top screen is ZL or ZR in either place.
- The screen in focus always shows. A hidden bottom screen shows while it is in focus, and hides again when the top screen takes the focus back. A hidden top screen shows again whenever it is the Primary Screen.
- In games with **Hide Bottom Screen** on, and in immersive mode, the bottom screen starts hidden. There the left stick click and both bumpers + **View** are one switch: either one shows it, and either one hides it again. With the bottom screen in focus, both bumpers + **View** hides it and puts the top screen back in focus.
- In every other game the left stick click hides and shows the bottom screen, and both bumpers + **View** is L, R and Select for the game.
- Immersive mode pauses while the bottom screen is in focus, and picks up again when you swap back.

## Screen layouts

Hold the left stick click for 1 second to change the layout. In Flat, a grip held with D-pad right or left also goes to the next or the previous layout. The stick clicks work in every layout.

| Control | VR | Flat |
|---|---|---|
| Left stick click, held for 1 second | **Stacked** or **Side by Side** | **Large Screen**, **Side by Side**, **Default**, and round again |
| Grip + D-pad right or left | Nudge the screens | Next or previous layout |
| Right stick click, tap | Swap Screens | Swap Screens |
| Left stick click, tap | Hide or Show Other Screen | **Single Screen** on or off |
| Right stick click, held for 1 second | Reset Screens and View, in the same layout | **Large Screen**, not swapped |

In VR, **Stacked** puts the Secondary Screen below the Primary Screen, tilted back. **Side by Side** puts it to the right of the Primary Screen, at the same distance and height, facing you. Only the Primary Screen curves. A label shows the new layout, and it is kept. **Screen Layout** in the in-game menu's Position tab is the same setting. In Flat, the layout's name shows over the game for a moment.

## Azahar's window

When no game runs, Azahar's own window floats in front of you: the game list, the menus and every dialog. The beam is the mouse. Double-click a game to start it, or choose it with the D-pad and press **A** twice. Use **Emulation > Configure...** for the settings. When a game stops, the window comes back.

| Control | What it does |
|---|---|
| Trigger or **A** | Left click. Hold it to drag |
| D-pad or left stick, up or down | Moves to the previous or next game in the list. Hold to keep going |
| **A** twice quickly | Starts the game you moved to |
| Right stick click | Right click, for example on a game for **Properties** |
| Right stick click, held for 1 second | Brings the window back in front of you |
| **B** | Back: closes a menu or a dialog |
| Grip aimed at the window | Moves the window. It keeps its distance from you and turns to face you |
| Right stick, up or down | Scrolls. With a menu or dialog open, the left stick scrolls too |
| **Y** | Shows or hides the keyboard, see below |

**X** and the bumpers do nothing on the window. The D-pad, the left stick and **A** twice only move in the game list while no menu or dialog is open.

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

Hold **Menu** for 1 second in a game. The game pauses, its screens dim, and the menu shows in front of them, with seven tabs down its left side:

- **Main**: **Continue**, **Quick Save**, **Quick Load**, **Screenshot**, **Save State** and **Load State** (10 slots each, with the time each was saved; a state from another version shows **From** and its version, dimmed, and asks before it loads), **Restart**, **Azahar's Window**, **Switch to Flat** and **Close Game**. **Load State** also has the **Autosave** card above the slots: tap it to load the newest autosave, or, when the game keeps more than one, to pick one from the list. After a slot or an autosave loads, **Undo Load** at the bottom goes back to the moment before. With Autosave off, the card's **Settings** goes to the Saves rows.
- **Position**: **Reset Position**, **Swap Screens**, **Hide Bottom Screen**, **Screen Layout**, **Detach Screens**, **Screen Shape**, **Screen Curve** and the **Immersive Offset**.
- **Stats**: the numbers of the last second before the pause, and the **Show Stats** switch.
- **Controls**: the drawings of both controllers, in a game and on Azahar's window, **Edit Controls**, **Touchpad**, **Bottom Screen Reticle**, **Reticle Speed** and **At Edge** (see Editing the controls). **Touchpad** and the reticle's rows can be set per game; the button layouts in **Edit Controls** are the same in every game.
- **Mods**: **Cheats**, **Mods** and **Textures** for the running game (see Cheats, mods and textures).
- **Homebrew**: Pokémon Emerald 3DS. **Install** asks for your Pokémon Emerald ROM in Azahar's window and builds the game from it. Once it is installed, **Play** starts it, and **Update** shows when a newer release is out. Otherwise **Check for Updates** asks GitHub at once, and **Reinstall** builds the game again. The progress shows on the row.
- **Settings**: the VR settings, in three sections: **General** (Background, Window Scale, Face Buttons, Motion Controls, Show Controllers, Fast Forward Speed, Fast Forward Audio, the **Saves** group with **Autosave**, **Every** and **Keep**, **More Settings** and **Switch to Flat**), **Graphics** (3D Depth, Internal Resolution, Dynamic Resolution, Texture Filter, Async Shader Compilation) and **Immersive** (Immersive Mode, Render Margin, Immersive Game FOV, Detect Game FOV, Horizontal Scale, Vertical Scale, World Scale, Follow Screen Size). The **Global | Per-Game** switch at the top right (on **Controls** and **Position** too) picks this game only or every game. The menu opens on **Per-Game** each time while a game runs, so a change is for that game unless you pick **Global**; the first change makes the game's own settings. Settings that are the same in every game, such as **Background** and **Dynamic Resolution**, always change for every game. Pointing at **Per-Game** while it is picked shows how many values on the tab (on **Settings**, the section) are set for this game. **Use Global Values for All**, in the **⋮** menu next to the switch, puts them back on the global values. A greyed-out row tells you why when you point at it. The menu remembers the last section until the app closes. **More Settings** opens **Emulation > Configure...**.

Point and pull the trigger or press **A**. Either stick scrolls a long list, faster the further you push it and the longer you hold it; a quick tap moves one row. You can also hold the trigger on a list and drag it, or grab the scrollbar on the right and drag it, or click above or below it to jump a page. Rest the beam on a setting for a moment to see what it does. **Hide Bottom Screen** applies the next time the game starts; the rest apply at once.

**Azahar's Window** shows the window itself, with the same controls as above, for everything else: the game's **Properties**, the **Emulation** menu and any dialog, which also brings the window up by itself. Press **B** with nothing open there to go back to the menu.

To go back to the game, select **Continue**, press **B**, or hold **Menu** for 1 second again. Holding **View** for 1.5 seconds stops the game. The Full immersive modes show the normal screens while the menu is open.

## Cheats, mods and textures

The in-game menu's **Mods** tab has three lists, chosen at its top. My own cheats and fixes carry a verified badge after their name. Point and pull the trigger, as in the rest of the menu; the right stick scrolls. Cheats made for another version of the game are in **For Another Version** at the end of the Cheats list; point at it and pull the trigger to open or close it.

| List | What is there |
|---|---|
| **Cheats** | **Download** fetches the game's cheats from Sharkive and my 3ds-game-fixes repository. A switch per cheat, applied at once and kept. A red chip such as **v1.0** marks a cheat made for another version of the game. **Azahar's Window** opens Azahar's **Cheats** window to add or edit your own |
| **Mods** | What is installed for the game, with a switch and **Remove**. **Install Mods** lists the mods known for the game, with the author, version, downloads and the month it was last updated when they are known; **Install** downloads one from its author's page. Two mods that change the same files are never installed over each other without asking. **Created by** opens the author's page in Steam's browser |
| **Textures** | **Custom Textures**, the game's packs (or **Off**), **Download Packs** and **Preload**. **Download Packs** lists packs for the game from GitHub, GameBanana and Google Drive; **Download**, **Cancel** and **Delete** act on one. **Customize** opens a pack's page: its parts and variants, **Move Up** and **Move Down** (a part higher in the list wins), **Optimize for Frame** for a PNG pack, and **Use Original** and **Remove Original** once it is optimized |

After a change to mods or textures, **Restart the Game to Apply** shows with a **Restart** button. More in the [user guide](USER_GUIDE.md#the-mods-tab).

## Editing the controls

**Edit Controls** on the in-game menu's **Controls** tab opens a drawing of a New 3DS XL with a chip on every 3DS button and stick. Each chip names what drives that 3DS input on the device chosen at the bottom.

| Control | What it does |
|---|---|
| A device chip at the bottom | Picks what you edit: **Frame Controllers**, a connected gamepad, **Mouse** or **Keyboard** |
| A chip on the drawing | Listens: **Press a Button…** shows. Press the device's button for that 3DS input. A button that already had a job swaps with it |
| A chip on the Circle Pad or the C-Stick | **Push the Stick Up…**, then **Push the Stick Right…**. The two pushes set the stick and its directions |
| **Cancel** | Stops listening |
| **Reset to Default** | Puts the device's layout back |
| **Done** | Back to the Controls tab |

The Frame controllers' layout is your controls profile (see Changing the layout). A gamepad's layout is kept per kind of gamepad, in `user/controller-mappings.txt`. A new gamepad starts with its buttons in the 3DS's places: the face buttons by position, the bumpers L and R, the triggers ZL and ZR, the sticks the Circle Pad and the C-Stick.

### Gamepads

A Bluetooth or USB gamepad plays in VR alongside the Frame controllers. Pair it in SteamOS. Both can be used at the same time.

### Touchpad

A gamepad's touchpad is the bottom screen. The pad covers the whole screen, and a dot shows where your finger is. **Touchpad** on the **Controls** tab sets how it touches:

- **Tap for Input** (the default): a finger on the pad touches the screen.
- **Click for Input**: pressing the pad touches it, and holding it down drags. The finger alone only moves the dot.

Turn Steam Input off for that controller, or Steam keeps the touchpad. On a touchpad's Edit Controls page, the drawing's bottom screen shows the dot live, and **Recentre at Edge** is there too (see below). A hidden bottom screen takes no touch.

### Bottom Screen Reticle

**Bottom Screen Reticle** on the **Controls** tab (**Off**, **Left Controller**, **Right Controller**, **Headset**) aims at the bottom screen without pointing at it, like a stylus, for games that aim with the touch screen, such as Kid Icarus: Uprising. It works in a game with the menu closed.

- Where the controller points when it starts is the middle of the screen. Point further to move the dot; past an edge, the dot stays on the edge.
- Its trigger touches, and held, it drags. Its grip recentres the middle where it points now. SteamVR's recentre and the right stick click held for 1 second do too.
- While it aims, that controller draws no beam. In the in-game menu, Azahar's window and the keyboard it is a normal pointer.
- **Headset**: your head aims, and the trigger of either controller touches.
- **Reticle Speed**, under it, sets how far the dot moves for a turn of your head or the controller: 0.5x to 4.0x, one value for both. At the default 2.0x your head turns about 15 degrees from edge to edge and 11 from top to bottom (30 and 22.5 at 1.0x); a controller turns 20 and 15.
- **Bottom Screen Reticle**, **Reticle Speed**, **At Edge** and the switches below can be set per game with the **Global | Per-Game** switch at the top of the tab. Kid Icarus: Uprising starts with **Headset** and **C-Stick at Edge** on unless you set them yourself.
- **At Edge**, under it: **Follow** (the default) keeps the dot on the edge. **Snap to Middle** lifts the touch at the edge and touches again in the middle, like a stylus swipe.
- **C-Stick at Edge**: while the trigger touches near an edge, the touch lifts and the C-Stick pushes toward that edge, more the closer you are to it. Move back from the edge and the touch comes back; lift the trigger and both let go. In Kid Icarus: Uprising with the C-Stick & Gyro mod this turns the camera, as the mod does when C-Stick aiming pushes past its aiming box.
- **Aim Only While Pressed**: the dot stays in the middle until the trigger touches, and goes back when you let go.
- **Right Stick Moves Touch**: the right stick moves the touch point too, and is not the C-Stick while the reticle aims. Off, the right stick stays the C-Stick.

### The mouse

A mouse works in VR. **Mouse Mode** on its Edit Controls page has two modes:

- **Bottom Screen**: the mouse moves a cursor on the bottom screen. The left button touches and drags. With **Recentre at Edge** on, a drag that runs into an edge lets go and touches again in the middle.
- **Pointer**: the mouse moves a pointer around you that works like the right controller's beam, on the menu, Azahar's window and the screens. Moving your head leaves it where it is.

| Mouse | What it does |
|---|---|
| Left and right button, held together for 3 seconds | Switches **Mouse Mode**. **Mouse: Pointer** or **Mouse: Bottom Screen** shows |
| Left button | Touch |
| Right button | A |
| Back, Forward | L, R |
| Middle button | Opens and closes the in-game menu |
| Wheel | Scrolls the menu's lists |

Edit Controls changes what each button and the wheel do. Holding the left and right buttons to switch modes always works.

With Frametop installed, Frametop holds the mouse while it is not paused. It pauses itself while a VR game runs, by default. If the mouse does nothing in VR, pause Frametop.

### Playing with a keyboard

A USB or Bluetooth keyboard plays in VR alongside the Frame controllers, gamepads and the mouse. It starts with Azahar's own keyboard layout, so a game plays as it does in Azahar on a PC:

| Key | 3DS |
|---|---|
| A, S, Z, X | A, B, X, Y |
| T, G, F, H | D-Pad up, down, left, right |
| Q, W | L, R |
| 1, 2 | ZL, ZR |
| M, N | Start, Select |
| B | Home |
| Arrow keys | Circle Pad |
| I, K, J, L | C-Stick |
| D, held | Both sticks pushed halfway |
| Esc | Opens and closes the in-game menu |

Keys count by where they are on the keyboard, not by its language layout. On the **Keyboard** Edit Controls page, choose a chip and press a key (**Press a Key…**). The Circle Pad and the C-Stick take four keys in turn: up, down, left, right. **Open Menu** and **Modifier** are the chips at the top.

While a game's keyboard is open, the keys type into it and never reach the game. The desktop keeps the keyboard too.

## The game keyboard and the Mii Selector

When a game asks for text, such as a name, a keyboard opens in front of you over the paused game, with the game's own hint, length limit and buttons.

| Control | What it does |
|---|---|
| Trigger or **A** on a key | Types it. Backspace repeats while held |
| **B** | Backspace |
| Shift | One capital; again: caps lock; again: off |
| **?123** | Digits and symbols |
| The game's buttons | Answer the game, such as **OK** or **Cancel** |

A keyboard connected to the Frame types into it too. The screens leave immersive mode while it is open. Stopping the game closes it.

When a game asks you to choose a Mii, the Mii it suggests is chosen, and its name shows. In Flat both are Azahar's own dialogs.

## The controls help, the controllers and the stats

Two buttons, **Controls** and **Stats**, sit above the top left corner of Azahar's window, on the home window and when the in-game menu shows Azahar's window. Point at one and pull the trigger or press **A**.

**Controls** opens a panel beside the window with drawings of the two Frame controllers and a line from each button to what it does, in a game and on Azahar's window, following your controls profile and **Face Buttons**. It opens by itself the first time VR starts, until you close it with its **X** or the **Controls** button. The in-game menu has the same drawings on its **Controls** tab.

**Stats** shows a small panel while a game runs. It shows the game's frame rate, the emulation speed, the frame time and its peak, the slow frames, the headset's frame rate and late frames, the CPU load and the GPU clock. The **Show Stats** setting is the same switch. Where it shows is **Stats Position** (see [SETTINGS.md](SETTINGS.md#stats-position)):

- **In Front**: it appears about 1 m ahead of you, a little below eye level, and stays put, in every mode. Point at it and squeeze a grip to move it; while you hold it, the stick of that hand moves it nearer or farther (up or down) and makes it bigger or smaller (sideways). Nothing you do with it reaches the game or the screens behind it. Turning it off and on, or holding the right stick click for 1 second, brings it back in front of you.
- **On Wrist**: it rides on the left controller like a watch and shows when you turn your wrist toward your face.

Each controller shows as a Frame controller in your hand, with its buttons, triggers and sticks moving as you press them. The beam is soft and comes from its tip. On Azahar's window and the panels it ends where it points; pointing at nothing shows a short, faint stub. In a game the beam shows only on the way to the dot over the bottom screen. A controller you leave still for 4 seconds fades its beam and dot, and after 30 seconds its model fades too. Pick it up and they come back. A controller that loses tracking for more than a quarter of a second hides.

The models are made from SteamVR's own Frame models when you install. If they are missing, the launcher makes them in the background and they show from the next start. Turn them off with **Show Controllers**.

## VR and Flat

Press both bumpers, **View** and **Menu** together to switch between VR and Flat, in a game or not. The game keeps running. In Flat, Azahar's window shows the game on SteamVR's big screen (the theatre), as a normal app, and stays there when you close the dashboard. Back in VR the 3D comes back; the window first goes back into the dashboard, which takes a few seconds. The next start uses the mode you switched to last.

Hold both bumpers first, then press **View** and **Menu**.

In Flat there are two more ways back to VR: the **VR** button in the status bar of Azahar's window, next to the renderer and volume buttons, and, with no game running, holding **Menu** for 1 second.

In VR, Azahar's window has a **FLAT** button in the same place, and the in-game menu has **Switch to Flat**.

## Hiding the bottom screen

Two things hide the bottom screen: immersive mode, and a game's **Hide Bottom Screen** setting (**Properties**, **Graphics** tab, **VR** group). A hidden bottom screen cannot be touched or grabbed, and it is not drawn. The top screen can still be grabbed with a grip unless immersive mode is on.

| Control | What it does |
|---|---|
| Both bumpers, then **View** | Shows or hides the bottom screen |
| Left stick click, tap | The same switch, with the top screen in focus. See Swapping the screens |
| Right stick click, held for 1 second | In immersive mode it also recentres the view on where you look. The screen keeps its size, as with SteamVR's recentre (hold the Steam button). **Reset Position** in the menu puts the screen back |
| Triggers | ZL and ZR everywhere while the bottom screen is hidden |

## Moving the immersive screen

In immersive mode the grips move the immersive screen instead of the normal screens. The world stays where it is while you move it.

| Control | What it does |
|---|---|
| Grip, then move the hand up or down | Moves the screen up or down. Squeeze anywhere; the beam does not have to be on the screen |
| Stick of that hand, up or down | Moves the screen farther or nearer. It keeps its size in your view. In the Full modes this is **World Scale**: farther makes the world bigger, nearer smaller. A label shows the value, for example **World Scale: 45%**. Kept for this game |
| Stick of that hand, right or left | Makes the screen bigger or smaller. In the Full modes with **Follow Screen Size** on (the default), the world grows or shrinks with it, and a label shows **Follow Screen Size: On** |
| D-pad left or right, while holding the screen | Nudges the screen 0.5 degrees per press, to fit your lenses. Hold it to keep going. A label shows the offset, for example **Offset Right 1.5°** |
| D-pad up or down, while holding the screen | Render Margin, Full modes: shows more or less of the game around the edges, Off to 1.4x, and the screen grows or shrinks with it. A label shows the value, for example **Render Margin: 1.2x**. Kept for this game |
| Both grips, then D-pad right or left | Full modes: makes the screen wider or narrower, 5% per press, -30% to +30% (**Horizontal Scale**). A label shows the value, for example **Horizontal Scale: -10%**. Kept for this game, at once |
| Both grips, then D-pad up or down | Full modes: makes the screen taller or shorter in the same way (**Vertical Scale**), for example **Vertical Scale: +5%** |
| Right stick click, held for 1 second | While you hold the screen: puts it back where it starts, recentres the view and resets the normal screens. Without a grip it only recentres, and the screen keeps its size. A tap does nothing while you hold the screen |

Taking hold shows the same kind of hint as for the normal screens, low in front of you.

The D-pad offset is for your eyes, not for a game: it is the same in every game and every immersive mode, and it stays when you put a game's screen back. **Reset Position** in the menu puts it back to the middle. It goes up to 10 degrees each way. The world stays where it is, only the screen moves. The **Immersive Offset** setting shows it and puts it back to the middle.

The screen starts 3 m ahead in the middle of your view, showing the game at its own scale: nothing is magnified, so the world keeps its shape and stays put wherever you look on it. It is as wide as the picture (**Horizontal Scale** 0%), with **Render Margin** at 1.4x. With both grips down, D-pad left and right always make the screen narrower or wider; with one grip they nudge it. It can grow to 2.5 times its size or shrink to 0.6 times. Bigger only enlarges the game's picture; it shows more around it only with **Render Margin** (see the [user guide](USER_GUIDE.md#immersive-mode)). Each game keeps the screen where you leave it. The grip, that hand's stick and the D-pad do not reach the game while you hold the screen. While immersive mode is paused (the bottom screen in focus, or in the Full modes the bottom screen shown or the in-game menu open), the grips move the normal screens. More in [src/frame_xr/IMMERSIVE.md](../src/frame_xr/IMMERSIVE.md).

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

Some games aim by moving the 3DS, like the first-person items in Ocarina of Time 3D (the slingshot, the bow, the hookshot). In VR the controller you chose in **Motion Controls** is the 3DS's gyroscope and accelerometer. Hold it as if it were the 3DS in front of you, with its beam pointing into the screen. Turn it left and right to turn, and tilt it up and down to tilt. The tilt of the beam also tells the game which way is down. The game is told how the controller turns and which way it hangs, not how your hand moves through the room. Choose **Headset** to aim with your head instead: turn and tilt it as you would the 3DS, with looking straight ahead as the 3DS held upright in front of you, and both controllers point as usual. **Motion Controls** can be set per game. Kid Icarus: Uprising starts with it off, because the C-Stick & Gyro mod's gyro aim moved the reticle whenever you moved.

The same controller still points at the screens, so aiming also moves its beam. If that gets in the way, choose the controller you do not point with. A game that does not use the sensors is not affected, and **Off** leaves the 3DS lying flat and still. A motion device you chose yourself in **Emulation > Configure > Controls > Motion/Touch** is left alone.

## Playing in a window (Flat)

The same library entry can also run Azahar as a flat window on SteamVR's big screen, with other apps usable beside it. It starts in the mode you used last, VR the first time. That mode is kept in the file `mode` in `~/.local/share/AzaharFrame`, as the single word `vr` or `flat`. To start in Flat once without changing it, add `AZAHARFRAME_FLAT=1` to `session.env` in the same folder (`0` forces VR). From a terminal, `azaharframe.sh --flat` or `--vr` does the same. To switch while it runs, see [VR and Flat](#vr-and-flat). If a run in Flat ends without a clean quit (a crash, or the system closing it), the next launch starts in VR, once, and the one after that is Flat again.

In Flat the Frame controllers reach Azahar as a gamepad from Steam Input, and Azahar maps it the same way as in VR: the same controls profile, the same buttons, holds and chords. A tap of **View** is still Select and a tap of **Menu** is still Start, so the game's own menus work.

The differences:

- There is no beam. The mouse is the touch screen: point at the bottom screen and click. The Frame's controller pointer acts as the mouse.
- Before a game starts, the D-pad or the left stick moves through the game list, and **A** twice quickly starts the game, as in VR.
- The triggers are always ZL and ZR. A grip held with D-pad right or left changes the layout; the D-pad does not reach the game meanwhile.
- The stick clicks change the window's screen layout. A tap of the right stick click swaps the screens. A tap of the left one shows only the main screen (**Single Screen**), and the next tap brings the other one back. Holding the right one for 1 second goes back to **Large Screen**, not swapped. Holding the left one for 1 second goes to the next layout: **Large Screen**, **Side by Side**, **Default** (see [Screen layouts](#screen-layouts)).
- Holding **Menu** for 1 second opens the Flat menu instead of the in-game menu, see below.
- Holding **Menu** for 3 seconds asks **Switch to VR?**, in a game or in the game list. **A** is **Yes**, **B** is **No**, and the buttons can be clicked too. In a game the Flat menu opens on the way, at 1 second; **No** goes back to the game, in Fullscreen if it was. Let go before 3 seconds and nothing more happens. The **VR** button in the status bar, next to the renderer and volume buttons, switches with one click.
- Only a tap of **Menu**, under 0.3 seconds, reaches the game as Start. A longer press never pauses or unpauses the game, even if you let go before 1 second.

Azahar's window is 16:9 in Flat, the shape of the big screen, so it fills it with no bars at the sides. Flat starts with the **Large Screen** layout: one big screen and the other one small beside it, which suits the wide screen. Flat keeps its own layout. The stick clicks and **View > Screen Layout** change it while you play in Flat, and the layout in the settings stays as it is for VR. **View > Fullscreen** works in Flat only: in VR the in-game menu always shows the whole window, with its menus.

Holding **View** for 1.5 seconds stops the game. Quick Save, Quick Load, Fast Forward and the switch to VR work as in VR.

### The Flat menu

Hold **Menu** for 1 second in a game in Flat. The game pauses and Azahar's window leaves Fullscreen, with its menu bar and status bar, so the pointer reaches **File**, **Emulation**, **View** and the rest, **Emulation > Configure Current Application...** included. The window stays on the big screen, 16:9.

To go back to the game, hold **Menu** for 1 second again and let go (held on to 3 seconds, it asks **Switch to VR?** instead), press **B** with no menu or dialog open, or select **Emulation > Continue**. The window goes back to Fullscreen if it was. **B** with a menu or dialog open closes that first. Holding **View** for 1.5 seconds stops the game.

Switching between VR and Flat with the in-game menu or the Flat menu open closes it, and the game goes on.

If VR cannot start at all (no SteamVR, or a renderer other than Vulkan), Azahar runs in its window with a plain controls profile of its own, **Flat Gamepad**, by position like the 3DS, without the holds and chords. The launcher adds it once, when it is missing.

## Changing the layout

The VR layout (apart from Face Buttons) is a controls profile in **Emulation > Configure > Controls**. There the Frame's controls are named **Frame A**, **Frame Left Bumper**, **Frame Menu** and so on. The first time VR starts, the buttons and sticks of the current profile get this layout, unless one of them already is a Frame control. After that your changes stay. The face buttons there are the 3DS-named ones, so they follow **Face Buttons**: with By Position, **Frame A** is the button in the 3DS's A place, which is the Frame's **B**. Keep a profile of your own under another name if you also use the window with a keyboard.

In VR, gamepads, the mouse and a keyboard have their own layouts, set in **Edit Controls** (see Editing the controls). In Flat, SDL game controllers work as in Azahar, and so do a keyboard and mouse on the window.

## Rumble

None.
