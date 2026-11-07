# Changelog

> **DRAFT.** Checked against the finished build before it is published. <!-- VERIFY -->

## 1.0.0 (November 2026)

The first release of Azahar for Frame, Nintendo 3DS games in VR running natively on the Valve Steam Frame. It is a fork of amwatson's CitraVR, built on Azahar.

### The emulator on the Frame

- A Linux Arm64 build that runs in the Frame's SteamVR, where CitraVR is an Android app for the Quest. No Lepton, APK, adb or PC.
- Azahar 2127.0-alpha4 with CitraVR's VR changes to the renderer.
- Azahar's Vulkan renderer draws straight onto SteamVR's own graphics device, one device for the whole run, so changing games does not rebuild the VR session.
- Each screen is handed to SteamVR only once the GPU has finished drawing it.
- Azahar's own settings window on the SteamVR dashboard, with every setting the emulator has. CitraVR has a small menu of its own on the Quest.
- Settings, saves, keys and shader caches in one plain folder, `~/.local/share/AzaharFrame`.
- A clean exit when Steam stops the app.

### The screens

- The top screen in stereoscopic 3D, one image for each eye, and the bottom screen as a touch screen, both floating in front of you as in CitraVR. Internal resolution up to 8x.
- Aim a controller at the bottom screen and pull the trigger to touch it. Squeeze a grip on either screen to move it, and use the stick of that hand to move it nearer and farther. The other screen follows. Hold the right stick click for 1 second to put both back, and tap it to swap which screen is the main one.
- A dot shows where each controller points.
- A game picker in VR with the icons and titles from your game folders, the installed titles, pages and a Rescan button. A folder you add in the Azahar window appears in it at once.
- Hold View for 1.5 seconds to stop the game and go back to the picker.
- The 3D depth starts at 50 %, and is set to 50 % when it is 0, so the top screen is always stereoscopic.

### Controls

- The Steam Frame controllers, in CitraVR's layout, with the real D-pad instead of the Quest's thumbrest trick. Bumpers are L and R, triggers are ZL and ZR (a touch on the bottom screen), grips move the screens, the left stick is the Circle Pad and the right stick is the C-Stick, Menu is Start, View is Select and the left stick click, held for 1 second, is Home. A tap of the left stick click hides or shows the screen that is not in focus.
- Each hand decides for itself whether its trigger is ZL or ZR, or a touch, when you pull it. In CitraVR one shared flag blocked both triggers, and a trigger aimed at the top screen dragged it instead of being ZL or ZR.
- A per-game **Hide Bottom Screen** setting (properties, **Graphics**, **VR**) shows only the top screen, for games that do not need the bottom one. Both bumpers and View bring it back.
- A per-game **Screen Shape** setting (**VR** group) widens the top screen to Widescreen (16:9) or Ultrawide (21:9) for mods that draw widescreen, in the normal view and in immersive mode. It is read when the game starts.
- The face buttons go by position, as on the 3DS: the Frame's A (bottom) is the 3DS's B, B (right) is A, X (left) is Y and Y (top) is X. The **Face Buttons** setting (**VR** group) switches to by label.
- Motion controls: the right controller (or the left, or none, in the **Motion Controls** setting) is the 3DS's gyroscope and accelerometer, for games that aim by moving the 3DS, such as the first-person items in Ocarina of Time 3D.
- Fast Forward: hold View, then pull the right trigger, to run the game at Azahar's turbo speed, and do it again to go back. "Fast Forward" shows in front of you while it is on.
- The layout is a controls profile in Azahar's controls window, with the Frame's controls named there, so you can change it.
- A **Controls** panel in VR lists what every control does in a game, on Azahar's window and in the in-game menu, named from your own controls profile. It opens the first time VR starts, and from the **Controls** button above Azahar's window after that.
- The Frame controllers show in your hands, with the buttons and sticks moving, and a soft beam that fades when you put a controller down (**Show Controllers**, on by default). The models are made from SteamVR's own on your Frame.
- A **Stats** panel under the top screen shows the frame rate, the emulation speed, the frame times, the headset's late frames, the CPU load and the GPU clock (**Show Stats**, or the **Stats** button).
- A **Window Scale** setting (**VR** group, 100%, 150% or 200%, 200% by default) makes Azahar's window look larger in VR: text, buttons and list rows are easier to read and to hit with the beam. Flat mode stays at 100%.

### Immersive mode

- Ocarina of Time 3D and Majora's Mask 3D can wrap the view around you, as in CitraVR: Wide, Full for those two games, and Full for other games.
- SteamVR has no cylinder layer, so the screen is a flat quad that stays in front of your eyes, sized to the game's camera, or fixed in the room in Wide mode.
- The head pose reaches the game once a frame as one snapshot, where CitraVR let the emulation and VR threads race over it. The eye distance comes from your headset.
- A per-game **Immersive Game FOV** setting replaces the fixed 60 degree game camera that sizes the screen, for mods that change the game's FOV.
- Both bumpers and View show the bottom screen (the left bumper is L here), and holding the right stick click for 1 second re-centres the view.
- The screen is fitted into the part of the view both eyes see, a little below eye level, so the whole picture and the HUD are in view. Squeeze a grip to move it up or down, and use the stick of that hand to move it nearer or farther and make it bigger or smaller, with the world staying put. Each game remembers where you left it.
- Fixed from CitraVR: the immersive factor had no default outside the Quest app, which divided the picture by zero.

### Install

- An installer that copies the program, adds an Azahar for Frame entry to your Steam library with its artwork and icon, gives SteamVR's dock the name and icon, and sets SteamVR's per-app settings (120 Hz, no throttling, no motion smoothing) and sets them again at every launch.
- `install.sh --dry-run` lists everything it would do without doing it.
- Installing again updates the same entry. Your settings, saves and keys stay.
- An uninstaller that removes the entry, the program and the dock icon files, and `--purge` for your data.
- It installs, updates and removes through Frametanium too.
