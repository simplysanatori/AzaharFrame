# Azahar for Frame: user guide

The short version is in the [README](../README.md). This guide has the rest: installing and removing in detail, your games, texture packs and mods, every control, the settings, flat mode, immersive mode, troubleshooting and what a bug report holds.

## Status and known issues

Version 1.1.1. The changes are listed in [CHANGELOG.md](CHANGELOG.md).

Known issues:

- The 3DS camera isn't available. Games that ask for it see no camera.
- Mario Kart 7's menus show white panels in **Full (6DoF): Other Games**.

## What this adds

Compared with CitraVR on the Quest, played on the Frame through Lepton:

- It runs as a Linux program on the Frame. There is no Lepton, Android layer, APK, adb or PC.
- It draws with Vulkan into SteamVR through OpenXR, at 90 or 120 Hz.
- It does much less work per frame: less load on the chip and better battery life.
- It starts in under a second.
- HD texture packs load as ASTC with no stutter.
- It uses the Steam Frame controllers, with the real D-pad where CitraVR needed a trick.
- Azahar's own window is in VR, with the game list, menus and settings, and the beam is the mouse.
- Holding **Menu** pauses the game and opens that window over it.
- The immersive screen can be moved and resized, and each game keeps its place. A grip and the D-pad shift it to fit your lenses.
- The normal screens can be made big enough to fill the view.
- The same app also plays in a flat window.

Compared with Azahar on a PC:

- There is a stereoscopic 3D top screen and a bottom touch screen, floating in front of you.
- You aim at the screens with a controller.
- The 3D depth starts at 50 %, where Azahar's own default is 0.

## Requirements

- A Steam Frame with SteamOS and SteamVR, and about 500 MB free, plus your games. Stable, Beta and Preview all work: the release carries its own Qt and libraries
- Your own 3DS games, dumped from your own console. Decrypted dumps need nothing else. Encrypted ones need the keys from the same console (see [Games](#games))
- A terminal on the Frame: SSH, or Konsole (SteamVR dashboard, Launch a program, Desktop)
- Steam running while you install

## Install, update and uninstall

### Install

The README has the steps. In short: get the repository, download `azaharframe-1.1.1-linux-arm64.tar.gz` from [Releases](https://github.com/simplysanatori/AzaharFrame/releases), and run `steam-frame/install.sh` with the tarball. To check the download against `SHA256SUMS` first:

```sh
(cd ~/Downloads && sha256sum -c --ignore-missing SHA256SUMS)
```

The installer adds the library entry with its artwork and icon, gives SteamVR's dock the name and icon, and sets SteamVR to 120 Hz for it. `install.sh --dry-run` lists every file it would copy and every Steam and SteamVR call, and changes nothing. It never asks a question. If Steam is not running it installs the files and says the library entry was skipped; run it again with Steam up.

Other installers:

- [Frametanium](https://github.com/Cogential/frametanium): add `simplysanatori/AzaharFrame`. It downloads the tarball and runs the same `install.sh`, so the library entry is the same one.
- [FrameDrop](https://framedropvr.com): [Install with FrameDrop](https://framedropvr.com/install?manifest=https%3A%2F%2Fgithub.com%2Fsimplysanatori%2FAzaharFrame%2Freleases%2Flatest%2Fdownload%2Fazaharframe.framedrop.json). It installs `azaharframe-1.1.1-framedrop-linux-arm64.zip` and adds its own library entry. The first start runs the installer without adding a second entry, and so does the first start after an update. Its log is `~/.local/state/framedrop-azaharframe/install.log`.

### Update

Download the new tarball and install it, or update from Frametanium or FrameDrop. Your settings, saves and keys stay, and the same library entry is updated. Quit the emulator first.

```sh
steam-frame/install.sh ~/Downloads/azaharframe-<version>-linux-arm64.tar.gz
```

### Uninstall

Quit the emulator first. This removes the library entry, the program folder and the dock icon files. Add `--purge` to also delete your settings, saves, keys and shader caches. Your games are never touched. Removing it from Frametanium does the same without `--purge`.

```sh
steam-frame/uninstall.sh
```

### Build from source

On the Frame, with a C++ compiler, CMake 3.24 or newer, Ninja, the Qt 6 and SDL2 development files and Python 3:

```sh
steam-frame/build.sh 1.1.1
steam-frame/install.sh
```

The first build takes about an hour and loads every core. It waits while the headset is in use. `build.sh` packs `steam-frame/release/azaharframe-1.1.1-linux-arm64.tar.gz` and `SHA256SUMS`, and `install.sh` installs that tarball. That build links the Qt and SDL2 SteamOS has, so a SteamOS update can break it. The release is made with `steam-frame/build-portable.sh`, which carries its own. The options are in [TECHNICAL_NOTES.md](TECHNICAL_NOTES.md#building) and [AGENTS.md](../AGENTS.md).

## First run

The game list is empty the first time.

1. Azahar's window floats in front of you. Point at the empty game list and double-click it with the trigger (or **A**).
2. Select your game folder.
3. Double-click a game to start it.

### Games

Put your games in `~/Documents/Roms/3ds/`, or any folder you like. It plays `.3ds`, `.cci`, `.cxi`, `.app` and `.3dsx` files, and the compressed `.zcci`, `.zcxi` and `.z3dsx` ones. Install a `.cia` first, from **File > Install CIA...** in the Azahar window. It then appears in the game list with your other games.

- Decrypted dumps work as they are.
- Encrypted dumps need `aes_keys.txt` from your own console in `~/.local/share/AzaharFrame/user/sysdata/`.

### Texture packs and mods

Custom textures and game mods go in the data folder, in a folder named for the game's title ID (the 16 characters shown in the emulator's log, for example `0004000000033500`):

- `~/.local/share/AzaharFrame/user/load/textures/<TitleID>/` for texture packs (the pack's `pack.json` goes directly in that folder).
- `~/.local/share/AzaharFrame/user/load/mods/<TitleID>/` for mods (a `romfs/` folder, `exheader.bin` or `code.ips`).

Custom textures are on from the first launch, and they load in the background while you play. If you already have your own settings file, turn on **Emulation > Configure > Graphics > Advanced > Use custom textures** yourself.

#### Optimize Texture Pack for Frame

HD packs are PNG files, and the emulator decodes each one the first time the game uses it. A 4096×4096 texture takes over a second, so a new area shows the low-resolution textures first. I added a converter for that. In the Azahar window, right-click the game and choose **Optimize Texture Pack for Frame...**. It writes a copy of the pack in ASTC, the compressed format the Frame's GPU reads as it is, with every mip level made in advance. Those files load in a few milliseconds, with no decoding, and take a quarter of the GPU memory.

- The original pack is not changed. The copy goes next to it (`<TitleID>-astc4x4`), and the game's folder in `load/textures/` becomes a link to the copy. **Use Original Texture Pack** in the same menu switches back. To remove the copy, delete its folder.
- The first time, it builds its encoder (ARM's astcenc). That takes under a minute and needs git, CMake and a C++ compiler.
- **Stop** is safe. Choose the menu entry again and it carries on where it left off. It runs at the lowest priority, but it is still best not to run it while you play.
- Ocarina of Time 3D with Henriko Magnifico's 4K pack: 2308 textures, 4.5 GB as PNG. As ASTC 4x4 it is 6.0 GB, and about 2.7 GB with `--block 6x6`. It takes about 15 to 20 minutes on the Frame, or about 6 with `--quality fast`, which looks nearly the same. Decoded, the PNGs would be 18 GB of memory; the emulator keeps an ASTC texture in memory only until the GPU has it.

From a terminal, with more options (`--block 6x6` for a pack half the size, `--quality thorough`, `--jobs`):

```sh
~/Games/AzaharFrame/app/tools/convert-texture-pack.sh --activate ~/.local/share/AzaharFrame/user/load/textures/<TitleID>
~/Games/AzaharFrame/app/tools/convert-texture-pack.sh --restore ~/.local/share/AzaharFrame/user/load/textures/<TitleID>
```

Packs that already ship DDS or KTX files work too, with or without `skip_mipmap`, and their mip levels are used as stored.

#### Ocarina of Time 3D mods

[CONTROLS.md](CONTROLS.md) lists the controls for Ocarina of Time 3D with the Ocarina ReFramed single-screen mod, and the settings I use with Single Screen Experience. Short version for Single Screen Experience:

- Turn on **Hide Bottom Screen** for the game.
- If you turn on the mod's 16:9 option, set **Screen Shape** to **Widescreen (16:9)**.
- Leave the mod's FOV at its default. If you change it, set **Immersive Game FOV** to the same value.
- If it stutters, turn off the mod's 60 FPS option first.

## Controls

| Steam Frame | 3DS |
|---|---|
| Left stick | Circle Pad |
| Right stick | C-Stick |
| Left D-pad | D-pad |
| **A** (bottom), **B** (right), **X** (left), **Y** (top) | B, A, Y, X, by position as on the 3DS |
| Left and right bumper | L and R |
| Left and right trigger | ZL and ZR |
| Trigger aimed at the bottom screen | Touch. Hold and move to drag |
| Grip aimed at a screen | Move the screens. The stick of that hand moves them nearer and farther (up, down) and makes them bigger or smaller (right, left). D-pad left or right nudges them a little, up or down curves them (**Screen Curve**: Off, then 15° to 90° in 15° steps) |
| **Menu** (right controller) | Start. Hold for 1 second to pause and open the in-game menu |
| **View** (left controller) | Select. Hold for 1.5 seconds to stop the game; **Stopping...** shows half way |
| Right stick click | Swap Screens: the other screen becomes the main one. Hold for 1 second: Reset Screens and View |
| Left stick click | Hide or Show Other Screen. Hold for 1 second: the next screen layout (**Stacked** or **Side by Side**; in Flat **Large Screen**, **Side by Side**, **Default**, also a grip + D-pad right or left) |
| Both bumpers, then **View** | Show or hide the bottom screen, in games that hide it |
| Both bumpers + **View** + **Menu** | Switch between VR and Flat |
| Hold **View**, then the right bumper | Quick Save. Hold the left bumper for half a second instead: Quick Load |
| Hold **View**, then the right trigger | Fast Forward on or off |
| Grip, in immersive mode | Move the immersive screen. The stick of that hand: farther or nearer, bigger or smaller. The D-pad shifts it to fit your lenses |

On Azahar's window and in the in-game menu:

| Frame | Window |
|---|---|
| Trigger or **A** | Left click |
| Right stick click | Right click. Hold for 1 second to bring the window back in front of you |
| **B** | Back. In the in-game menu with nothing open, back to the game |
| Grip | Move the window |
| Stick up and down | Scroll |
| **Y** | Show or hide the keyboard |

The top screen can grow to 3 times its starting size, enough to fill the whole view, and the bottom screen moves down to stay just under it. It is made bigger rather than brought nearer: the top screen never comes closer than 1 m. A bigger screen gets less 3D depth, so the distant scenery stays easy on the eyes. If an edge of a big screen is just out of view, keep the grip squeezed and press D-pad left or right: each press moves the screens half a degree around you, and **Moved Right** (or Left) shows for 2 seconds. With the grip still squeezed, D-pad up or down makes the main screen more or less curved (Off, Gentle, Medium, Strong, Max), and **Screen Curve: Medium** (for example) shows; it is kept for every game, and **Screen Curve** in the **VR** group sets it too. While you hold the screens the D-pad does not reach the game. A small hint under the screens lists these when you take hold.

With the bottom screen in focus, it is in the main place, big, and the top screen is in the place below. Touch works on the bottom screen in either place. The screen in focus always shows; a hidden bottom screen shows while it is in focus. Immersive mode pauses while the bottom screen is in focus. Every game starts with the top screen in focus. The details are in [CONTROLS.md](CONTROLS.md#swapping-the-screens).

The right controller is also the 3DS's motion sensor, for games that aim by moving the 3DS. Hold it like a 3DS with the beam pointing into the screen.

The **Controls** button above Azahar's window opens a panel with all of this, named after your own controls profile. It opens by itself the first time VR starts. In a game, the in-game menu's **Controls** tab shows the same. The **Stats** button next to it shows the frame rate and the frame times while a game runs, in front of you or on your left wrist (**Stats Position**).

All controls, with the keyboard and the pointing rules: [CONTROLS.md](CONTROLS.md).

### Playing in a window (Flat)

I wanted to play a 3DS game in a window as well, like any other flat game on the Frame, with other apps open beside it. So the same library entry starts in one of two modes: VR, as above, or Flat, which is Azahar's normal window on SteamVR's big screen. It starts in the mode you used last, VR the first time. Press both bumpers, **View** and **Menu** together to switch while it runs. [CONTROLS.md](CONTROLS.md#playing-in-a-window-flat) says how to start in Flat once.

Both modes use the same data folder, so your games, saves, mods, textures and settings are the same. In Flat, Azahar stays in its window on SteamVR's big screen (the theater), and stays there when you close the dashboard. Switching back to VR takes a few seconds while the window goes back into the dashboard. The Frame controllers come through Steam Input as a gamepad and do the same as in VR, with your VR controls profile: a tap of **View** or **Menu** is Select or Start, and the holds and combos work. Holding **Menu** for 1 second opens the Flat menu: the game pauses and the window leaves Fullscreen with its menu bar, so the pointer reaches Azahar's menus and settings. Hold **Menu** again, press **B** or select **Emulation > Continue** to go back, Fullscreen again if it was. A menu that is open when you switch modes closes and the game goes on. The mouse is the touch screen. 3D follows Azahar's own 3D setting.

In Flat the window is 16:9, so it fills the big screen. Games start in the **Large Screen** layout: one big screen and the other one small beside it. Tap the right stick click to swap the screens. Tap the left stick click to show only the main screen, and tap it again for both. Hold the right stick click for 1 second to go back to **Large Screen**. Flat keeps this layout to itself, so your layout settings stay as they are for VR. **View > Fullscreen** works in Flat only.

## Settings

Everything is in the Azahar window, under **Emulation > Configure**. Resolution, filters, controls and audio are the ones you know from Azahar. Azahar for Frame starts with the **Dark** theme, which is easier on the eyes in the headset; **Emulation > Configure > General > UI > Theme** changes it.

**Emulation > Configure** always changes the settings for every game, also when you open it from the in-game menu. To change a setting for one game only, right-click the game in the list and choose **Properties**. While a game with its own value runs, that row is greyed out in **Configure** and shows the value for all games.

The first time VR starts, the buttons and sticks of the current controls profile get the Frame layout (**Emulation > Configure > Controls**). After that your changes stay. In VR the Single Screen and Separate Windows layouts are changed to Default, and the 3D depth is set to 50 % if it is 0.

The VR settings are in the **VR** group at the bottom of the **Graphics** tab. [SETTINGS.md](SETTINGS.md) explains each one, with its choices and default.

### Face Buttons and Motion Controls

Global settings, not per game:

- **Face Buttons**: **By Position (Nintendo)** (default) makes the Frame's A, B, X and Y do what the 3DS button in the same place does. **By Label (Xbox)** makes A, B, X and Y the 3DS's A, B, X and Y.
- **Motion Controls**: **Off**, **Right Controller** (default) or **Left Controller**. The controller becomes the 3DS's gyroscope and accelerometer, for games like Ocarina of Time 3D that aim the first-person items by moving the 3DS.

### Window Scale

Global, not per game: **100%**, **150%** (default) or **200%**. It makes Azahar's window look larger in the headset, so the text, buttons, list rows and menus are easier to read and to hit with the beam. The window shows less at a time (about a quarter less at 150%), and the panel is wider: 30% at 150% and 50% at 200%. In VR the game list has no side-to-side scroll bar; long names are cut short. A window larger than Azahar's, such as the configuration window, makes the panel wider still, up to 50% and no taller than it is wide. The change shows as soon as you press **OK**. Flat mode always uses 100%, in a 16:9 window; there, `QT_SCALE_FACTOR` in `session.env` enlarges the window.

### Background

Global, not per game: **Black** (default) or **Passthrough**. Passthrough shows the room behind the screens, through the headset's cameras, when SteamVR offers it to apps. The in-game menu's **Settings** tab has it too, and the choice is kept. When SteamVR does not offer it, the choice is greyed out and the background stays black.

### Quick Save and Quick Load

Hold **View** and press the right bumper to Quick Save, or hold the left bumper for half a second to Quick Load. The in-game menu has **Quick Save** and **Quick Load** buttons too. Both use Azahar's Quick Save slot, the same as the Emulation menu. A short message in front of you confirms it. Details: [CONTROLS.md](CONTROLS.md#quick-save-and-quick-load).

### Fast Forward

Hold **View** and pull the right trigger to run the game at Azahar's turbo speed. Do it again to go back. "Fast Forward" shows in front of you for 2 seconds; turning it off shows "Normal Speed" for 2 seconds. It goes off by itself when the game stops, when the in-game menu or the Flat menu opens and when you switch between VR and Flat. The speed is **Turbo Speed Limit** in **Emulation > Configure > General**. Details: [CONTROLS.md](CONTROLS.md#fast-forward).

### Show Controllers and Show Stats

Global only:

- **Show Controllers** (on by default) shows the Frame controllers in your hands, with the buttons and sticks moving. A controller you put down fades out after 30 seconds, and its beam after 4. The models are made from SteamVR's own when you install. If they are missing, the launcher makes them in the background and they show from the next start.
- **Show Stats** (off by default) shows the frame rate, the emulation speed, the frame times, the CPU load and the GPU clock while a game runs. **Stats Position** puts them in front of you (grab them with a grip to move them) or on your left wrist. The **Stats** button above Azahar's window switches it as well.

### Immersive mode

Normally the top screen floats two metres away. In immersive mode the game draws a wider view, and in the Full modes its camera follows your head. Turn it on in the game's properties, on the **Graphics** tab, in the **VR** group, under **Immersive Mode**:

- **Wide (3DoF):** a big screen fixed in the room.
- **Full (6DoF): Ocarina of Time, Majora's Mask:** the screen stays in front of your eyes and the game camera turns and moves with your head.
- **Full (6DoF): Other Games:** the same, for other games; it finds the camera while you play.

Three games start with a setup already, from CitraVR's notes: Mario Kart 7, Super Mario 3D Land and Asphalt 3D. They start in **Full (6DoF): Other Games**, with **View Matrix Register** 90 for the first two and 8 for Asphalt 3D. That applies only when the game's own properties have no **Immersive Mode** or register of their own, and the global one is still **Off** or **Auto**. To play one of them without it, set **Immersive Mode** to **Off** in that game's properties. Any value you set yourself wins.

**Immersive Game FOV** is the game camera's vertical angle in degrees (60 by default). Change it only if a mod changes the game's FOV. A change shows at once. Both bumpers and **View** (or a tap of the left stick click) show or hide the bottom screen. Holding the right stick click for 1 second re-centres the view.

The screen starts a little below eye level. The whole picture is in view, and its sides, where most games keep the HUD, sit inside the sharp middle of the lenses. Squeeze a grip and move your hand up or down to move it. The stick of that hand moves it farther or nearer (up or down) and makes it bigger or smaller (right or left), up to 2.5 times the starting size. The world stays where it is while you move the screen, and each game keeps it where you left it. Hold the right stick click for 1 second to put it back; that also re-centres the view.

If your eyes see one side of the immersive screen but not the other, keep a grip squeezed and press D-pad left or right. Each press shifts the screen half a degree, and **Offset Right 1.5°** (for example) shows for 2 seconds. This offset is for your lenses, so it is the same in every game and every immersive mode, and putting a game's screen back does not change it. The world stays where it is; only the screen moves. **Immersive Offset** in the **VR** group shows it, and **Reset** there puts it back to the middle.

A tap of the right stick click swaps to the bottom screen, which pauses immersive mode until you tap again. [IMMERSIVE.md](../src/frame_xr/IMMERSIVE.md) has the details and the tuning variables.

Two more settings in the same group, per game, for the Full modes. **Wider View** changes at once; **Narrow Screen** is read when the game starts:

- **Wider View** (**Off**, **1.1x** to **1.4x**): shows more of the game around the edges. Some games leave them empty; Super Mario 3D Land does. In VR, keep a grip on the immersive screen and press D-pad up or down: the screen grows with the view, and **Wider View: 1.2x** (for example) shows. It is kept for the game.
- **Narrow Screen** (**Off**, **10%**, **20%**, **30%**): makes the screen narrower at the same height, so the HUD at the sides moves into view. The game looks that much thinner. Turning your head left and right turns the game camera a little more than up and down does, so the middle of the picture stays in place.

The other settings of the group are for **Full (6DoF): Other Games**: **Depth Factor Multiplier** (how far the game camera moves with your head, 1x by default), **View Matrix Register** and **Eye Indicator Register** (both found while you play when left on auto).

### Hide Bottom Screen

Some games do not need the bottom screen. In a game's properties, on the **Graphics** tab, in the **VR** group, tick **Hide Bottom Screen** to show only the top screen. Hold both bumpers and press **View** to bring the bottom screen back, and again to hide it. A tap of the left stick click does the same. The setting is read when the game starts. [CONTROLS.md](CONTROLS.md) lists the controls for Ocarina of Time 3D with the Ocarina ReFramed single-screen mod, and the settings I use with Single Screen Experience.

### Screen Shape

Some mods draw the top screen widescreen, squeezed into the 3DS's 5:3 picture. In a game's properties, on the **Graphics** tab, in the **VR** group, set **Screen Shape** to match: **Widescreen (16:9)** or **Ultrawide (21:9)** for a wider top screen, or **Standard (4:3)**, **3:2** or **16:10** for game cheats and mods that change the game's own aspect. The top screen keeps its height and its width follows the shape. The bottom screen stays where it was. It is also the shape of the screen in immersive mode. A change shows at once, also from the in-game menu, so you can match a mod's own aspect setting while you play. It must match what the game renders, or the picture stretches.

## Performance

The 3DS runs at 60 frames a second, and every one is shown on each refresh of the headset.
The first time a game draws a new effect, the emulator compiles its shaders and the picture can hitch. They are kept in `~/.local/share/AzaharFrame/user/shaders`, so it happens once.
If a game drops frames, lower the internal resolution under **Emulation > Configure > Graphics**.
**Show Stats**, or the **Stats** button above Azahar's window, shows the game's frame rate and the headset's late frames while you play.
Keep other heavy work off the Frame while you play.

## Files

| What | Where |
|---|---|
| Program and launcher | `~/Games/AzaharFrame/app` |
| Steam library art and icons | `~/Games/AzaharFrame/app/art` |
| Dock icon files | `~/.local/share/applications/azaharframe.desktop`, `azaharframe.png` and `azahar.png` in `~/.local/share/icons/hicolor`, `azaharframe.vrmanifest` in the program folder |
| Icon cache of the old VR picker (`AZAHARFRAME_VR_PICKER=1`) | `~/.cache/azaharframe/icons` |
| Settings, saves, keys, shader caches | `~/.local/share/AzaharFrame/user` |
| Immersive screen places, per game | `~/.local/share/AzaharFrame/user/immersive-placement.txt` |
| Launcher log | `~/.local/share/AzaharFrame/launch.log` |
| Emulator log | `~/.local/share/AzaharFrame/user/log/azahar_log.txt` |
| Settings for one launch | `~/.local/share/AzaharFrame/session.env` |
| Mode it starts in (`vr` or `flat`) | `~/.local/share/AzaharFrame/mode` |
| Texture pack encoder (built on first use) | `~/.local/share/AzaharFrame/tools` |
| Controller models for VR | `~/.local/share/AzaharFrame/user/frame-models` (log: `~/.local/share/AzaharFrame/frame-models.log`) |
| FrameDrop install log | `~/.local/state/framedrop-azaharframe/install.log` |

How the port works: [TECHNICAL.md](TECHNICAL.md).

## Troubleshooting

- **The installer says Steam was not reachable.** Start Steam and run `install.sh` again.
- **SteamVR's dock shows a question mark instead of the flower.** Steam reads the icon when it starts. Restart Steam once.
- **The screens do not appear in the headset.** Start SteamVR first, then Azahar for Frame. `launch.log` says which OpenXR runtime it found, and the emulator log says `running flat` when it could not start VR.
- **A game in a subfolder is not in the list.** Right-click the game folder in the list (right stick click) and tick **Scan Subfolders**.
- **Azahar's window is too small in VR.** Set **Window Scale** in the **VR** group of the Graphics tab to 200%. In flat mode, add `QT_SCALE_FACTOR=1.25` to `session.env`.
- **An encrypted game does not start.** It needs `aes_keys.txt` in `user/sysdata/`.
- **Motion ghosts.** In SteamVR, check 120 Hz and motion smoothing off.
- **No controllers in VR, only the beams.** The models are made from SteamVR's. Delete `user/frame-models` and start once more, then read `frame-models.log`.
- **The immersive screen is in an odd place.** Squeeze a grip and click the right stick to put it back.
- **One edge of the immersive screen is cut off by the lenses.** Squeeze a grip and press the D-pad away from that edge until it shows: right when the left edge is cut off.

## Reporting a problem

Attach `launch.log` and `user/log/azahar_log.txt` from `~/.local/share/AzaharFrame` to an [issue](https://github.com/simplysanatori/AzaharFrame/issues).
They hold the paths of your games, which include your user name, the names of the games you started, your computer's processor, graphics card and system version, your headset's name, and the settings you use. They hold no network addresses. Read them before sharing and replace anything you want to keep private.
If the same thing happens in Azahar on a PC, report it to [Azahar](https://github.com/azahar-emu/azahar/issues) instead.

## Support upstream

Support upstream first: star [CitraVR](https://github.com/amwatson/CitraVR) and [Azahar](https://github.com/azahar-emu/azahar). Azahar for Frame's changes are not offered to them.
If you find a bug that also happens in Azahar on a PC, report it to [Azahar](https://github.com/azahar-emu/azahar/issues).

## Credits and notices

- [CitraVR](https://github.com/amwatson/CitraVR) by amwatson: the VR port this is built on
- [Azahar](https://github.com/azahar-emu/azahar) and [Citra](https://github.com/citra-emu/citra): the emulator
- The Azahar logo is by angyartanddraw and PabloMK7 (CC BY 4.0). The artwork backgrounds are SteamGridDB uploads by GarfieldGaming0624
- [astcenc](https://github.com/ARM-software/astc-encoder) by Arm: the texture pack encoder, built on first use
- Valve: SteamVR and the Frame

Full list: [THIRD_PARTY_NOTICES.md](../THIRD_PARTY_NOTICES.md). Artwork sources: [steam-frame/art/CREDITS.md](../steam-frame/art/CREDITS.md). CitraVR's README: [README.upstream.md](../README.upstream.md). Unofficial fan project, not affiliated with Nintendo, Valve, the Azahar team or the CitraVR author. Nintendo 3DS is a trademark of Nintendo.
