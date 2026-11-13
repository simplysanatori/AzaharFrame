# Changelog

## 1.2.0 (November 2026)

A Mods tab, controls you can edit for any controller, mouse and pointer controls, screens you can detach, a keyboard in the headset, autosave and crash protection.

### What's Different From 1.1

- Changes on the in-game menu's **Settings**, **Controls** and **Position** tabs now apply to the running game by default. Choose **Global** to change them for every game.
- **Settings** is in three sections: **General**, **Graphics** and **Immersive**.
- Homebrew has its own tab in the in-game menu.
- Fast Forward keeps the sound at its normal pitch. **Fast Forward Audio** (**Sped Up**, **Normal Pitch**, **Normal Speed (Experimental)**, **Muted**) is on the VR **Settings** tab and in **Configure > Audio**, and can be set for one game. **Normal Speed (Experimental)** plays the sound at its real speed and skips ahead to keep up; the sound can rise and fall in volume as it does.
- **Preload Custom Textures** is off by default, and turning it on now asks first.
- Save states from 1.1.1 are dimmed. They still load with **Load Anyway**.
- Azahar runs under a memory limit. If a texture pack fills memory, Azahar closes instead of freezing the Frame.

### Mods tab

- New **Mods** tab in the VR menu, with **Cheats**, **Mods** and **Textures**.
- **Cheats**: **Download** fetches the game's cheats from Sharkive and from my own repository.
- Cheats, patches, mods and textures known to work on Azahar for Frame (or made by me) carry a verified badge.
- Cheats made for a different version of the game show which version. The version is the one the game itself shows, so Mario Kart 7 Ver. 1.2 and Fire Emblem Echoes 1.1 are no longer mistaken for other versions. A version I don't know yet shows as **Update Installed** or the cartridge's revision.
- Code patches are matched to the game's own code. The European Fire Emblem Echoes with its update gets the 1.1 patches, not the 1.0 Ghosting Shader Fix, which makes the picture flash there. True 60 FPS says it can't be combined with the Ghosting Shader Fix.
- Cheats, mods and texture packs from outside sources link to their author's page, for credit.
- **Mods**: **Install Mods** lists mods for the game and downloads them straight from their authors' pages on GitHub, GameBanana, MEGA or Google Drive.
- Each mod shows its author, version, downloads and date on one line.
- When two mods conflict, you choose which one to use.
- Texture packs have their own section in the VR menu. **Download Packs** downloads packs from their original sources, all within the app.

### Texture packs

- Several packs can be on at once, like Modern Pokemon Sprites on top of 5X Redrawn UI in Pokémon X and Y. I put the parts of every pack that's on in one list, **Enabled Textures**. When two change the same texture, the higher one wins, and **Move Up** and **Move Down** set the order. A pack made to go on top of another goes above it by itself.
- **Customize** on a pack turns its parts on or off, such as characters or backgrounds. When two parts change the same texture, the one higher in the list wins. **Move Up** and **Move Down** set the order.
- Packs that come in variants install as one pack. You pick the variant on its **Customize** page.
- **Optimize for Frame** converts a PNG pack to ASTC, which the Frame's GPU reads as it is. It is now on a pack's **Customize** page in the headset too.
- The converter is built in, so it needs no compiler. It uses Arm's astcenc.
- **Remove Original** deletes the PNG pack once the optimized copy is in use.
- A PNG pack over 1 GB suggests **Optimize for Frame** once.
- In Flat, **Customize Texture Pack...** is in the game list's right-click menu, with the game's packs and their order at the top.

### Crash protection

- Azahar runs under a memory limit. A texture pack that fills memory closes Azahar instead of freezing the Frame. The next start says **Azahar Ran Out of Memory**.
- **Running Low on Memory** shows how many GB Azahar uses of its limit as it gets close.
- Near the limit, the game pauses and offers **Turn Off Custom Textures**, **Resume** or **Stop Game**.
- Turning on **Preload Custom Textures** asks first, with the pack's size, the memory in use and Azahar's limit. A pack too large to preload loads as you play instead.
- Holding **Menu** or **View** for 3 seconds stops a game that takes too long to start.

### Save states

- **About Save States** says it once: save states are NOT a replacement for saving in the game.
- A state loads in the version that made it. States from 1.1.1 show **From 1.1.1**, dimmed.
- **Load Anyway** still loads one. It backs up the game's save data and extdata first. If the game then closes unexpectedly within 10 minutes of play, the next start offers **Restore Save Data**.
- **Restore** backs up your current save data first, so newer progress is never lost.
- **Undo Load** takes back a load from a slot or an autosave, until the game closes.
- A state that can't be read stops the game with a message instead of crashing it.
- A state or Quick Save that can't be written shows **Couldn't Save State**. The game keeps running, and the slot keeps the state it had.

### Autosave

- **Autosave** makes save states while you play. It is off by default. Turn it on in **Settings** > **General**.
- It saves every 5 minutes, when you open the menu and when you close the game. Each game keeps its newest 3. Both can be set per game.
- **Load State** has an **Autosave** card with the newest one, and a list of the others.
- "Autosaved" shows above the top screen, or as a short message when that corner is out of view. **Couldn't Autosave** tells you when one fails.
- Autosaves never replace a slot or the Quick Save.
- The autosave made when you open the menu has a spot of its own. It doesn't count toward the newest 3, so opening the menu never removes an older autosave.
- An autosave pauses the game for about 0.2 to 0.5 seconds.
- Autosaves wait while memory is nearly full, because writing one needs about 0.9 GB more for a moment.
- In Azahar's window, autosaves are at the end of **Emulation > Load State**. The settings are in **Emulation > Configure > General** and in a game's **Properties**.

### Settings

- **Settings**, **Controls** and **Position** have a **Global | Per-Game** switch at the top right. The menu opens on **Per-Game** while a game runs. Choose **Global** to change every game.
- Pointing at **Per-Game** while it is picked shows how many values on the tab are set for this game, for example "2 Per-Game Values Set".
- **Use Global Values for All**, in the **⋮** menu next to the switch, puts the tab back on the global values.
- **Bottom Screen Reticle**, **Reticle Speed**, **At Edge**, **Touchpad** and **Motion Controls** can be set per game, so Kid Icarus: Uprising can have the reticle while Super Mario 3D Land doesn't. Kid Icarus: Uprising starts with **Bottom Screen Reticle** on **Headset**, **C-Stick at Edge** on and **Motion Controls** off. These are only starting values: anything you set for the game, or for all games, stays.

### Crash reports

- If Azahar closed unexpectedly, the next start says so, with the reason and the game.
- **What Were You Doing?** takes a note, typed on the VR keyboard.
- **See What's Sent** shows the whole report. **Save Report** saves it as a zip. **Send Report** sends it to me.
- Nothing is sent unless you tap **Send Report**, and then only that one report. It goes through Sentry, which I've set not to store IP addresses. Your user name and home folder are removed. The log can include the names and folders of your games.
- **Report a Problem** sends any other problem the same way. It is in **Settings** > **General** > **Help** in VR, and under **Help** in Azahar's window, next to **About Azahar for Frame**.

### Homebrew

- The new **Homebrew** tab builds Pokémon Emerald 3DS from your own Pokémon Emerald ROM, and plays it.
- **Check for Updates** looks for a new release at once.
- If a newer Emerald 3DS release changes how its data is built so that Azahar for Frame can't build it itself, it asks first. **Build With Emerald 3DS's Builder** then runs the release's own builder in a protected sandbox that can only read your ROM and write its own folder. It remembers a yes for that version; Not Now asks again next time.

### Controls

- **Edit Controls** shows a New 3DS XL.
- Use several controllers at once, for example the left Frame controller and a mouse as a pointer.
- **Bottom Screen Reticle**: a controller or your head aims at the bottom screen, for games that aim with the touch screen. That controller shows no beam while it aims.
- **Reticle Speed**: how far the reticle moves when you turn. At the default 2.0x your head turns half as far as before to cross the screen, about 15 degrees side to side.
- **C-Stick at Edge**: hold the reticle near an edge of the bottom screen and the touch lifts while the C-Stick pushes that way. With the C-Stick & Gyro mod for Kid Icarus: Uprising, the camera turns, the same way the mod turns it when C-Stick aiming pushes past its aiming box.
- **Aim Only While Pressed** keeps the reticle in the middle until the trigger touches. **Right Stick Moves Touch** lets the right stick move the touch point too, instead of being the C-Stick.

### Mouse and Pointer Controls

- **Bottom Screen**: the mouse touches and drags on the bottom screen, for games that aim with the touch screen, such as Kid Icarus: Uprising.
- **Pointer**: a 3D pointer for the menu and screens, adapted from [Frametop](https://github.com/Frametop/frametop), so you can use the app outside a game.
- Hold left and right click for 3 seconds to switch modes.
- Middle click opens the VR menu.

### Screens

- **Detach Screens**: Off, Secondary or Both. Detached screens keep their place per game.
- **Touchpad**: a gamepad's touchpad works as the bottom screen.

### Game keyboard

- A keyboard opens in the headset when a game asks for text. Thanks to [NicholasDavis5](https://github.com/NicholasDavis5) for reporting it ([#1](https://github.com/simplysanatori/AzaharFrame/issues/1)).
- When a game asks for a Mii, one is picked automatically. A future update may let you use your own Miis or create one.

### Immersive View upgrades

- **Detect Game FOV** reads the game camera's field of view, so cutscenes, zooms and widescreen mods stay correct. I gave it three choices: **Always** follows the camera while you play, **Once** measures it once and saves it for that game, and **Off** uses the Immersive Game FOV you set. You can change it in the VR menu or in Azahar's settings.
- New presets for Super Mario 3D Land, Mario Kart 7, Ocarina of Time 3D and Majora's Mask 3D, tuned for the Frame's wider field of view compared with Quest headsets. They are the settings I play with: resolution, texture filter, 3D Depth, Horizontal Scale, Vertical Scale, Render Margin and where the screen sits. A value you set yourself always wins.
- The immersive screen now has the game's own shape. **Render Margin** adds the same margin on every side, and at **Off** you see exactly the game's picture. Before, the game drew a quarter more up and down than across, so a 5:3 game came out as 4:3 and full-screen pictures such as Ocarina of Time 3D's item screen were stretched. When a game zooms in to aim, the screen keeps its size and shape.
- **Screen Shape** has a new **Automatic** choice. I made it follow the shape the game's camera draws with, 4:3, 3:2, 5:3, 16:10, 16:9 or 21:9, so a mod or cheat that changes the picture's shape looks right without setting it yourself.
- Faraway scenery no longer doubles in Ocarina of Time 3D and Majora's Mask 3D. The 3D was too deep for a screen this big: at 3D Depth 30, Ocarina of Time 3D put distant scenery further apart than your eyes. In **Full (6DoF): Ocarina of Time, Majora's Mask** I now limit the 3D to the deepest that still lines up, worked out from the screen's size and distance and your IPD, and it follows them as you change them. Your 3D Depth setting stays as you set it. When the limit applies, the 3D Depth message says so, for example **3D Depth: 30% (Limited to 8%)**.
- In the same mode, the game's own menu screens, such as Ocarina of Time 3D's item screen, show at the game's own size, without Render Margin's border of the paused world around them. The margin comes back as you return to the game.
- Both eyes now always see the same moment of the game.
- SteamVR's recenter and the right stick hold keep the immersive screen's size and distance. Reset Position in the menu puts everything back, the Immersive Offset too.
- Immersive mode recenters on where you face each time it resumes after the menu or the bottom screen, so the character stays in the middle after you change settings such as the resolution.
- Render Margin no longer cuts off Ocarina of Time 3D's world at the edge of the game's own picture, or repeats the last frame in Majora's Mask 3D.
- Kid Icarus: Uprising starts in the normal view: its aiming does not work with a head-turned camera. Its preset sets only the graphics and where the screen sits. You can still turn Immersive Mode on for it.
- Super Mario 3D Land now has proper stereoscopic 3D in immersive view.
- I replaced **Narrow Screen** with **Horizontal Scale** and added **Vertical Scale**. Each goes from -30% to +30% in 5% steps, so the immersive screen can be larger than the picture as well as smaller, and both start at 0%. A game where you set Narrow Screen keeps its screen: Narrow Screen 10% becomes Horizontal Scale -10%. In VR, hold the immersive screen with both grips and press D-pad right or left for wider or narrower, up or down for taller or shorter.
- **Wider View** is now called **Render Margin**. It adds the same margin on every side, and **Off** shows exactly the game's own picture.
- The game's two eyes now follow your IPD as the headset measures it, so turning the IPD dial during play takes effect at once. Before, **Full (6DoF): Other Games** always used 65 mm. The last IPD read is kept for the next start, and only if none was ever read does a message ask you to turn the IPD dial.

### Smaller changes

- The 3DS microphone works through the Frame's microphone, with Azahar's microphone input on **Auto**.
- While a game waits for its shaders, a small **Preparing Shaders** badge with a progress ring shows in the top-right corner of the main screen, or at the top right of what you see in immersive view. I made it fade in and out and stay up through short gaps between stalls, so it no longer pops up in the middle of your view every few seconds when async shaders are off. In Flat it is in the status bar.
- In VR, Azahar's window has a **FLAT** button on its status bar.
- In VR and Game Mode, the game list's **Open** entries, **File** > **Open Azahar Folder** and **Open Log Folder** now open Azahar's own file dialog, where the desktop file manager never showed. It lists files with size and date, and can rename, delete and make folders.
- Messages in the headset stay for 5 seconds.
- Installing needs no Python: the Steam entry and its art, the dock icon, the controller models and Emerald 3DS's build are part of the program.
- The bundled 7-Zip is now 26.06 and LibreSSL 4.4.1. Downloads stop when they grow well past their expected size.

### Bug fixes

- Fixed a freeze when switching between VR and Flat.
- Fixed Azahar's menus no longer responding after some messages in VR.
- Fixed a crash when loading a save state after a long pause with the microphone in use.
- Fixed a crash when starting a game with a downloaded texture pack that has no options, such as 5X Redrawn UI HD in Pokémon X and Y.
- **Install Mods** no longer stays on **Loading Mods**. It asks again, and after 20 seconds shows **Couldn't Load Mods** with **Retry**.
- A game's **Properties** no longer shows a stray Turbo Speed Limit label.
- From Azahar: a freeze when a controller connects or disconnects, signature checks for encrypted content, keyboard navigation, pop-ups on screens with a different DPI, the default hotkeys and a local wireless fix.

## 1.1.1 (November 2026)

Sharper screens at high resolution, the game list on the controller, 3D depth per game, better stats placement and head motion controls.

- At 5x resolution and above, the screens are sharp edge to edge. Valve's foveation layer lowered the detail away from where you looked; Azahar for Frame now turns it off.
- The game list works with the controller, in VR and in Flat. The D-pad or the left stick moves through the games, and **A** pressed twice quickly starts the one you're on.
- Double clicks with the beam are reliable.
- **3D Depth** is on the in-game menu's **Settings** tab, from 0% to 100%, for every game or just this one. It changes the 3D at once.
- The stats appear in front of you and stay there, never behind a screen. Grip them to move them; the stick sets their distance and size.
- **Stats Position** can put the stats on your left wrist instead. Turn your wrist toward your face to see them.
- **Motion Controls** can use the headset: turn and tilt your head to aim.

## 1.1.0 (November 2026)

A new in-game menu, curved screens, a side-by-side layout and live controls for immersive mode.

### The in-game menu

- Holding **Menu** opens a new menu with five tabs: Main, Position, Stats, Controls and Settings. It follows CitraVR's layout, in the style of Android XR.
- **Main**: Continue, Quick Save, Quick Load, Save State and Load State (10 slots, with the time each was saved), Screenshot, Restart, Switch to Flat and Close Game.
- **Position**: Reset Position, Swap Screens, Hide Bottom Screen, Screen Shape, Screen Layout, Screen Curve and Immersive Offset.
- **Stats**: the frame rates and frame times from just before the pause.
- **Controls**: the controller diagram, in game and for Azahar's window.
- **Settings**: the VR settings, for every game or just this one. Rest the beam on a setting to see what it does.
- **Azahar's Window** shows Azahar's own window over the game, for everything else. **B** goes back to the menu.

### The screens

- **Screen Curve**: the main screen can bend toward you like a curved monitor, from Off to 90° in 15° steps. The smaller screen stays flat, and touch still lands where the beam points.
- Hold the screens with a grip and press D-pad up or down to change the curve. D-pad left or right still nudges them. To move them up or down, move your hand.
- **Screen Layout**: **Side by Side** puts the smaller screen to the right of the main one, facing you. **Stacked** keeps it below. Hold the left stick click for 1 second to switch.
- In Flat, the same hold cycles **Large Screen**, **Side by Side** and **Default**. So does a grip held with D-pad left or right.

### Immersive mode

- **Wider View** now has steps per game: Off, 1.1x, 1.2x, 1.3x and 1.4x. It shows more of the game around the edges.
- Hold the immersive screen with a grip and press D-pad up or down to change it as you play. The screen grows with the view, so the picture keeps its scale.
- D-pad left or right still shifts the screen for your lenses. To raise or lower it, move your hand.
- **Squeeze Width** is now **Narrow Screen**, with a new 30% step.

### Controls

- Holding the right stick click for 1 second is the one reset. It puts the screens back and, in immersive mode, re-centres the view.
- Quick Load needs the left bumper held for half a second, so a brush of the bumper never loads.
- Grabbing the screens shows a short hint of what the D-pad and stick do.

### Fixed

- In Flat, dialogs such as Configure opened squeezed, with OK and Cancel off the bottom of the screen. They now fill the big screen and scroll.

## 1.0.0 (November 2026)

The first release of Azahar for Frame: Nintendo 3DS games in VR, running natively on the Valve Steam Frame. It is the Azahar emulator with the VR modes of amwatson's CitraVR.

### Compared with CitraVR through Lepton and similar forks

- It runs natively on the Frame. There is no Android layer, APK or adb.
- It starts in under a second.
- Far less work per frame. In Ocarina of Time 3D at 120 Hz with a 4K texture pack: 0.6 ms of CPU and 1.5 ms of GPU time, against 4.8 ms and 2.7 ms.
- Less work for the chip means better battery life.
- Texture packs load as ASTC with no stutter. CitraVR loads PNGs and stalls while it decodes them.

### The emulator on the Frame

- A native OpenXR app in the Frame's SteamVR, at 120 Hz, or 90 Hz if you prefer.
- The release brings its own Qt and libraries. It runs on SteamOS Stable, Beta and Preview.
- Play in VR on 3D screens, in immersive VR with head tracking, or flat on SteamVR's big screen.
- Settings, saves, keys and shader caches are in one folder, `~/.local/share/AzaharFrame`.

### The screens

- Grip a screen to move both. The stick of that hand moves them nearer or farther, and makes them bigger or smaller, up to 3 times: big enough to fill the whole view.
- While you hold them, the D-pad nudges them half a degree at a time, for a big screen with an edge just out of view.
- Tap the right stick click to swap the screens. Hold it for a second to put them back.
- Tap the left stick click to hide or show the other screen.
- **Hide Bottom Screen**, per game, for games that do not need it. Both bumpers and View bring it back.
- **Screen Shape**, per game: 5:3, 16:9, 21:9, 4:3, 3:2 or 16:10, for mods and cheats that change the game's aspect. A change shows at once.
- **Background**: black, or passthrough to see the room. The in-game menu has a button for it.

### Immersive mode

- Grip the immersive screen and move your hand to raise or lower it. The stick of that hand moves it nearer or farther, and makes it bigger or smaller.
- The screen position is remembered for each game.
- SteamVR's recenter (hold the Steam button) also re-centres the immersive view.
- The default size keeps the HUD within the clear centre of the lenses. You can make the screen up to 2.5 times bigger.
- While you hold it, the D-pad shifts it half a degree at a time. **Immersive Offset** is global, since it corrects for small differences in each user's IPD.
- **Wider View**, per game, shows more of the game around a bigger screen.
- **Squeeze Width**, per game, makes the screen 10 or 20% narrower, to bring HUD elements at the sides into view.
  - In future updates I may extend this, or add game-specific cheats or mods that move the HUD.
- **Immersive Game FOV**, per game, for mods that change the game's field of view. A change shows at once.
- Mario Kart 7, Super Mario 3D Land and Asphalt 3D start in Full (6DoF) mode, set up for each game.

### Controls

- The Steam Frame controllers, with the real D-pad.
- The face buttons go by position, as on a 3DS. **Face Buttons** switches them to by label.
- A trigger aimed at the bottom screen is a touch. Anywhere else it is ZL or ZR. Each hand decides on its own.
- Hold View, then the right bumper, to Quick Save. Hold View, then the left bumper, to Quick Load.
- Hold View, then pull the right trigger, to turn Fast Forward on or off. "Fast Forward" or "Normal Speed" shows for 2 seconds.
- Hold Menu for a second to open the in-game menu: Azahar's window over the paused game. It has Quick Save, Quick Load and Passthrough buttons.
- Hold View for 1.5 seconds to stop the game.
- The right controller is the 3DS's gyroscope, for games that aim by moving the 3DS.
- A **Controls** panel shows both controllers, with a line from each button to what it does. It has a page for the game and one for Azahar's window.
- A **Stats** panel shows the frame rate, the emulation speed and the frame times.
- The Frame controllers show in your hands, with soft beams that fade when you put them down.
- The layout is a controls profile in Azahar's window, so you can change it.

### VR and Flat

- Press both bumpers, View and Menu to switch between VR and a flat window. The game keeps running.
- In Flat, Azahar's window sits on SteamVR's big screen, the theater. It stays there when you close the dashboard.
- In Flat the controllers work as in VR, with the same holds and combos. A tap of View or Menu is still Select or Start.
- In Flat the window is 16:9, so it fills the big screen with no bars at the sides.
- In Flat, hold Menu for a second to open the Flat menu: the game pauses and the window leaves Fullscreen with its menu bar, so the pointer reaches Azahar's menus and settings. Hold Menu again, press B or select Emulation > Continue to go back.
- In Flat the stick clicks change the window's layout. Tap the right one to swap the screens and the left one to show only one screen. Hold the right one to go back to Large Screen, one big screen and one small, which is where Flat starts. VR keeps your own layout.
- The next start uses the mode you used last.

### Texture packs

- **Optimize Texture Pack for Frame** in the game list converts a PNG pack to ASTC, so it loads with no stutter.
- **Use Original Texture Pack** switches it back to how it was before.
- Packs that ship DDS or KTX files keep their own mip levels.

### Install

- An installer that adds the library entry with its art, names the SteamVR dock entry, and sets SteamVR to 120 Hz with no motion smoothing. It sets them again at every launch.
- `install.sh --dry-run` shows what it would do first.
- Installing again updates the same entry. Your settings, saves and keys stay.
- It installs and updates through Frametanium and FrameDrop too.
- An uninstaller, with `--purge` for your data.

### From Azahar and CitraVR

- The emulation, the game list and the settings are Azahar 2127.0-alpha4's, and the 3D screens and immersive modes are CitraVR's. Thanks to the Azahar team and amwatson.
