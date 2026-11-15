# Azahar for Frame 1.2.0

A Mods tab, controls you can edit for any controller, mouse and pointer controls, screens you can detach, a keyboard in the headset, autosave and crash protection.

### What's Different From 1.1

- Changes on the in-game menu's **Settings**, **Controls** and **Position** tabs now apply to the running game by default. Choose **Global** to change them for every game.
- **Settings** is in three sections: **General**, **Graphics** and **Immersive**.
- Homebrew has its own tab in the in-game menu.
- **Preload Custom Textures** is off by default, and turning it on now asks first.
- Save states from 1.1.1 are dimmed. They still load with **Load Anyway**.
- Azahar runs under a memory limit. If a texture pack fills memory, Azahar closes instead of freezing the Frame.
- Fast Forward keeps the sound at its normal pitch. **Fast Forward Audio** (**Sped Up**, **Normal Pitch**, **Normal Speed (Experimental)**, **Muted**) is on the VR **Settings** tab and in **Configure > Audio**, and can be set for one game. **Normal Speed (Experimental)** plays the sound at its real speed and skips ahead to keep up; the sound can rise and fall in volume as it does.

### Mods tab

- New **Mods** tab in the VR menu, with **Cheats**, **Mods** and **Textures**.
- **Cheats**: **Download** fetches the game's cheats from Sharkive and from my own repository.
- Cheats, patches, mods and textures known to work on Azahar for Frame (or made by me) carry a verified badge.
- Cheats made for a different version of the game show which version. The version is the one the game itself shows, so Mario Kart 7 Ver. 1.2 and Fire Emblem Echoes 1.1 are no longer mistaken for other versions. A version I don't know yet shows as **Update Installed** or the cartridge's revision.
- I put cheats for another version of the game in a **For Another Version** section at the end of the list, closed until you open it. Its header says how many there are and how many are on.
- Code patches are matched to the game's own code. The European Fire Emblem Echoes with its update gets the 1.1 patches, not the 1.0 Ghosting Shader Fix, which makes the picture flash there. True 60 FPS says it can't be combined with the Ghosting Shader Fix.
- Cheats, mods and texture packs from outside sources link to their author's page, for credit.
- **Mods**: **Install Mods** lists mods for the game and downloads them straight from their authors' pages on GitHub, GameBanana, MEGA or Google Drive.
- Each mod shows its author, version, downloads and date on one line.
- When two mods conflict, you choose which one to use.
- Texture packs have their own section in the VR menu. **Download Packs** downloads packs from their original sources, all within the app.
- **CTGP-7**: with Mario Kart 7 open, the **Mods** tab has a **CTGP-7** section. **Install** checks your Mario Kart 7 (version 1.2 update and save data), downloads CTGP-7's own installer from its official GameBanana page and runs it for you, in about 5 minutes. Afterwards it has **Launch** and **Uninstall**. Nothing of CTGP-7 is included or redistributed. CTGP-7 is made by PabloMK7 and the CTGP-7 team.

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
- The autosave made when you open the menu has a spot of its own. It doesn't count toward the newest 3, so opening the menu never removes an older autosave.
- **Load State** has an **Autosave** card with the newest one, and a list of the others.
- "Autosaved" shows above the Primary Screen, or as a short message when that corner is out of view. **Couldn't Autosave** tells you when one fails.
- Autosaves wait while memory is nearly full, because writing one needs about 0.9 GB more for a moment.
- Autosaves never replace a slot or the Quick Save.
- In Azahar's window, autosaves are at the end of **Emulation > Load State**. The settings are in **Emulation > Configure > General** and in a game's **Properties**.

### Settings

- **Settings**, **Controls** and **Position** have a **Global | Per-Game** switch at the top right. The menu opens on **Per-Game** while a game runs. Choose **Global** to change every game.
- Pointing at **Per-Game** while it is picked shows how many values on the tab are set for this game, for example "2 Per-Game Values Set".
- **Use Global Values for All**, in the **⋮** menu next to the switch, puts the tab back on the global values.
- **Bottom Screen Reticle**, **Reticle Speed**, **At Edge**, **Touchpad** and **Motion Controls** can be set per game, so Kid Icarus: Uprising can have the reticle while Super Mario 3D Land doesn't. Kid Icarus: Uprising starts with **Bottom Screen Reticle** on **Headset** and **Motion Controls** off. These are only starting values: anything you set for the game, or for all games, stays.
- **Accurate Image Copies**, in **Graphics**, has three choices. **Automatic**, the default, fixes glitches only where a game needs it. **On** makes every image copy exact but lowers performance a lot, and **Off** works the way Azahar always has. Pointing at each choice in the headset or in Azahar's window explains it.

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
- **Bottom Screen Reticle**: a controller or your head aims at the Secondary Screen, for games that aim with the touch screen. That controller shows no beam while it aims.
- **Reticle Speed**: how far the reticle moves when you turn. At the default 2.0x your head turns half as far as before to cross the screen, about 15 degrees side to side.
- **Aim Only While Pressed** keeps the reticle in the middle until the trigger touches. **Right Stick Moves Touch** lets the right stick move the touch point too, instead of being the C-Stick.

### Angel's Aim (Alpha)

- Angel's Aim brings Kid Icarus: Uprising into VR with Darkshade's C-Stick & Gyro mod. **Aiming**, on the **Controls** tab, is **Direct Aim**, which puts the reticle where your weapon points, or **Gyro Aim**, which moves it as you turn your hand. The game starts on **Direct Aim** once the mod is installed.
- Pit's equipped weapon is in your hand, held the way Pit holds it, with his fist on the controller's handle. **Weapon Opacity**, **Weapon Size** and **Weapon Scale** change how it looks. Blades, staffs, bows and cannons aim along the weapon as Pit holds it, so a blade aims a little above the controller's line. Clubs aim along your hand from the front of Pit's fist, and claws, palms, arms and orbitars along your hand.
- Each shot shows a flash at the weapon's tip (**Muzzle Flash**, with **Muzzle Flash Size**), and your shots fly from the weapon into the screen (**Projectiles**, with their opacity, size and speed). Each real shot buzzes the controller (**Recoil**).
- With **Direct Aim**: **Laser Sight**, **Keep Reticle on Screen**, **Grip Frees Aim** (**Hold** or **Toggle**), **Turn Zone From**, **Turn Zone**, **Turn Speed**, and **Check Aim**, which shows targets one at a time so you can check your aim.
- The first time you turn **Aiming** on, **Angel's Aim Is On** lists what is on.
- The **Angel's Aim** segment on the **Mods** tab shows the mod it needs and the suggested texture packs, with **Report an Issue** and **Send Report**.

### Mouse and Pointer Controls

- **Secondary Screen**: the mouse touches and drags on the Secondary Screen, for games that aim with the touch screen, such as Kid Icarus: Uprising.
- **Pointer**: a 3D pointer for the menu and screens, adapted from [Frametop](https://github.com/Frametop/frametop), so you can use the app outside a game.
- Hold left and right click for 3 seconds to switch modes.
- Middle click opens the VR menu.

### Screens

- **Detach Screens**: Off, Secondary or Both. Detached screens keep their place per game.
- **Touchpad**: a gamepad's touchpad works as the Secondary Screen.
- Screens are named by slot: **Hide Bottom Screen** is now **Hide Secondary Screen** and **Mouse Mode**'s **Bottom Screen** is now **Secondary Screen**. Your settings are kept.

### Game keyboard

- A keyboard opens in the headset when a game asks for text. Thanks to [NicholasDavis5](https://github.com/NicholasDavis5) for reporting it ([#1](https://github.com/simplysanatori/AzaharFrame/issues/1)).
- When a game asks for a Mii, one is picked automatically. A future update may let you use your own Miis or create one.

### Immersive View upgrades

- **Detect Game FOV** reads the game camera's field of view, so cutscenes, zooms and widescreen mods stay correct. I gave it three choices: **Always** follows the camera while you play, **Once** measures it once and saves it for that game, and **Off** uses the Immersive Game FOV you set. You can change it in the VR menu or in Azahar's settings.
- New presets for Super Mario 3D Land, Mario Kart 7, Ocarina of Time 3D and Majora's Mask 3D, tuned for the Frame's wider field of view compared with Quest headsets. They are the settings I play with: resolution, texture filter, 3D Depth, Horizontal Scale, Vertical Scale, Render Margin and where the screen sits. A value you set yourself always wins.
- The immersive screen now has the game's own shape. At 0% you see exactly the game's picture, and **Render Margin** (with **Stretch**) adds a margin around it. Before, the game drew a quarter more up and down than across, so a 5:3 game came out as 4:3 and full-screen pictures such as Ocarina of Time 3D's item screen were stretched. When a game zooms in to aim, the screen keeps its size and shape.
- **Screen Shape** has a new **Automatic** choice. I made it follow the shape the game's camera draws with, 4:3, 3:2, 5:3, 16:10, 16:9 or 21:9, so a mod or cheat that changes the picture's shape looks right without setting it yourself.
- Faraway scenery doubles less in Ocarina of Time 3D and Majora's Mask 3D. The 3D was too deep for a screen this big: at 3D Depth 30, Ocarina of Time 3D put distant scenery further apart than your eyes. In **Full (6DoF): Ocarina of Time, Majora's Mask** I now limit the 3D to the deepest that still lines up, worked out from the screen's size and distance and your IPD, and it follows them as you change them. Your 3D Depth setting stays as you set it. When the limit applies, the 3D Depth message says so, for example **3D Depth: 30% (Limited to 8%)**.
- In the same mode, the game's own menu screens, such as Ocarina of Time 3D's item screen, show at the game's own size, without Render Margin's border of the paused world around them. The margin comes back as you return to the game.
- Both eyes now always see the same moment of the game.
- In the Full modes the Primary screen is drawn together with the controllers, in step with your head, which reduces doubling on fast head turns. **Primary Screen as Panel** (Experimental) brings back the separate panel.
- New **Motion Clarity** (Experimental): each game picture shows only once, for a clearer picture when you turn your head, but it may flicker or double in demanding scenes. It works only in the Full modes, with **In Immersive Modes** on, under it.
- SteamVR's recentre and the right stick hold keep the immersive screen's size and distance. Reset Position in the menu puts everything back, the Immersive Offset too.
- Immersive mode recentres on where you face each time it resumes after the menu or the Secondary Screen, so the character stays in the middle after you change settings such as the resolution.
- Render Margin no longer cuts off Ocarina of Time 3D's world at the edge of the game's own picture, or repeats the last frame in Majora's Mask 3D.
- Kid Icarus: Uprising starts in the normal view: its aiming does not work with a head-turned camera. Its preset sets only the graphics and where the screen sits. You can still turn Immersive Mode on for it.
- Super Mario 3D Land now has proper stereoscopic 3D in immersive view.
- I replaced **Narrow Screen** with **Horizontal Scale** and added **Vertical Scale**. Each goes from -100% to +100% in 5% steps, so the immersive screen can be larger than the picture as well as smaller, and both start at +40%, as much view as the 1.4x Render Margin gave before; a value you set yourself is kept. A game where you set Narrow Screen keeps its screen: Narrow Screen 10% becomes Horizontal Scale -10%. In VR, hold the immersive screen with both grips and press D-pad right or left for wider or narrower, up or down for taller or shorter.
- **Wider View** is now called **Render Margin**, with **Render Margin: Horizontal** and **Render Margin: Vertical** rows. They apply only with **Scale Mode** on **Stretch** and show only there; their values are kept. **Field of View** is the default, so a game's old Wider View value no longer applies unless you pick **Stretch**.
- **Scale Mode** says how the picture fills the screen. **Field of View** (the default) crops a smaller screen's edges and keeps the world's shape; **Stretch** squeezes the whole picture, HUD included. Mario Kart 7's preset starts in **Stretch**.
- New **Fill View** button under Scale Mode: one press sets this game's screen size, **Horizontal Scale** and **Vertical Scale** so the screen covers your view: the width exactly, the height to the smallest 5% step that covers it. Change them from there as usual. I limited the scales so the world does not warp; a scale past the limit says **May Warp** on its row, and Fill View still fills.
- In the Full modes, hold the immersive screen with a grip and press D-pad up or down: with **Field of View** it changes **3D Depth**, one step per press (for example **3D Depth: 80%**); with **Stretch**, **Render Margin** on both axes. Both grips with the D-pad still change **Horizontal Scale** and **Vertical Scale**.
- New **World Scale** (10% to 400%, 100% by default) for how big the game world feels, changed with a grip and the stick, and **Follow Screen Size** (on by default), which makes the world grow or shrink with the screen.
- New **Dynamic Resolution** (**Settings** > **Graphics**, off by default): it lowers the resolution while the GPU cannot keep up and raises it again when it can, never below your **Internal Resolution** setting.
- The game's two eyes now follow your IPD as the headset measures it, so turning the IPD dial during play takes effect at once. Before, **Full (6DoF): Other Games** always used 65 mm. The last IPD read is kept for the next start, and only if none was ever read does a message ask you to turn the IPD dial.
- Immersive view no longer bobs up and down when the headset reports a slightly different lens view from frame to frame; I fit the screen to a steady view and only refit when it really changes.

### Smaller changes

- The 3DS microphone works through the Frame's microphone, with Azahar's microphone input on **Auto**.
- While a game waits for its shaders, a small **Preparing Shaders** badge with a progress ring shows in the top-right corner of the main screen, or at the top right of what you see in immersive view. I made it fade in and out and stay up through short gaps between stalls, so it no longer pops up in the middle of your view every few seconds when async shaders are off. In Flat it is in the status bar.
- In VR, Azahar's window has a **FLAT** button on its status bar.
- In VR and Game Mode, the game list's **Open** entries, **File** > **Open Azahar Folder** and **Open Log Folder** now open Azahar's own file dialog, where the desktop file manager never showed. It lists files with size and date, and can rename, delete and make folders.
- Messages in the headset stay for 5 seconds.
- I made long lists in the VR menu, like a game's cheats, much quicker to get through. The stick scrolls faster the further you push it and speeds up as you hold it, you can grab the scrollbar and drag it or click above or below it to jump a page, and you can drag the list itself with the trigger.
- Installing needs no Python: the Steam entry and its art, the dock icon, the controller models and Emerald 3DS's build are part of the program.
- The bundled 7-Zip is now 26.06 and LibreSSL 4.4.1. Downloads stop when they grow well past their expected size.

### Bug fixes

- Fixed a freeze when switching between VR and Flat.
- I fixed the shimmer in the 3D scenes of Fire Emblem Echoes and Fates at a raised internal resolution. Their glow came out as blotches that changed every frame and differed between the eyes. The glow is now worked out on the GPU at the full internal resolution, and the emulation no longer waits on it.
- In VR both eyes always render. **Disable Right Eye Rendering** now says **(Flat Only)**, and ticking it in one game's **Properties** no longer turns it on for the games you start after it.
- Fixed Azahar's menus no longer responding after some messages in VR.
- Fixed a crash when loading a save state after a long pause with the microphone in use.
- Fixed a crash when starting a game with a downloaded texture pack that has no options, such as 5X Redrawn UI HD in Pokémon X and Y.
- **Install Mods** no longer stays on **Loading Mods**. It asks again, and after 20 seconds shows **Couldn't Load Mods** with **Retry**.
- A game's **Properties** no longer shows a stray Turbo Speed Limit label.
- From Azahar: a freeze when a controller connects or disconnects, signature checks for encrypted content, keyboard navigation, pop-ups on screens with a different DPI, the default hotkeys and a local wireless fix.

## Download

- `azaharframe-1.2.0-linux-arm64.tar.gz`: the release, for `install.sh` and Frametanium
- `azaharframe-1.2.0-framedrop-linux-arm64.zip` and `azaharframe.framedrop.json`: the same release, for FrameDrop

## Install

Download the tarball and run `steam-frame/install.sh` from the repository with it as the argument. You can also add `simplysanatori/AzaharFrame` in Frametanium, or [install with FrameDrop](https://framedropvr.com/install?manifest=https%3A%2F%2Fgithub.com%2Fsimplysanatori%2FAzaharFrame%2Freleases%2Flatest%2Fdownload%2Fazaharframe.framedrop.json). To update, run `install.sh` from the new release, or update in Frametanium. Your settings, saves and keys stay. The [README](https://github.com/simplysanatori/AzaharFrame/blob/steam-frame/README.md) has the steps and the main controls. The [user guide](https://github.com/simplysanatori/AzaharFrame/blob/steam-frame/docs/USER_GUIDE.md) has how to add your games, every control and every setting.

## Known

- Mario Kart 7's menus show white panels in Full (6DoF): Other Games mode.
- An autosave pauses the game for about 0.2 to 0.5 seconds.

## Reporting problems

Use **Report a Problem**: in the in-game menu under **Settings** > **General** > **Help**, or under **Help** in Azahar's window. You can also open an issue with what you did, what you expected and what happened, and attach `launch.log` and `user/log/azahar_log.txt` from `~/.local/share/AzaharFrame`. They hold the paths of your games, which include your user name. Read them first.
