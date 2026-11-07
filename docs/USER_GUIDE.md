# Azahar for Frame: user guide

The short version is in the [README](../README.md). This guide has the rest: installing and removing in detail, your games, texture packs and mods, every control, the settings, flat mode, immersive mode, troubleshooting and what a bug report holds.

<!-- VERIFY: everything below that depends on the XR code is checked against the finished build before publishing. -->

## Status and known issues

I build it from source on the Frame. The first release is not out yet, and the install steps in the README are written for it.
The parts that depend on the VR code are checked against the finished build before it is published.

Known issues:

- Immersive mode only has settings for Ocarina of Time 3D and Majora's Mask 3D, and presets for Mario Kart 7, Super Mario 3D Land and Asphalt 3D. It is experimental.
- The 3D depth is set to 50 % when it is 0, so the top screen is always stereoscopic in VR.

<!-- VERIFY: both items -->

The changes are listed in [CHANGELOG.md](CHANGELOG.md).

## What this adds

Compared with CitraVR on the Quest:

- It runs as a Linux program on the Frame. There is no Lepton, APK, adb or PC.
- It draws with Vulkan into SteamVR through OpenXR.
- It uses the Steam Frame controllers, with the real D-pad where CitraVR needed a trick.
- Azahar's own window is in VR, with the game list, menus and settings, and the laser is the mouse.
- Holding **Menu** pauses the game and opens that window over it.
- Immersive mode runs on SteamVR.

Compared with Azahar on a PC:

- There is a stereoscopic 3D top screen and a bottom touch screen, floating in front of you.
- You aim at the screens with a controller.
- The 3D depth starts at 50 %, where Azahar's own default is 0.

## Requirements

- A Steam Frame with SteamOS and SteamVR, and about 500 MB free, plus your games
- Your own 3DS games, dumped from your own console. Decrypted dumps need nothing else. Encrypted ones need the keys from the same console (see [Games](#games))
- A terminal on the Frame: SSH, or Konsole (SteamVR dashboard, Launch a program, Desktop)
- Steam running while you install

## Install, update and uninstall

### Install

The README has the steps. In short: get the repository, download `azaharframe-1.0.0-linux-arm64.tar.gz` and `SHA256SUMS` from [Releases](https://github.com/simplysanatori/AzaharFrame/releases), check them, and run `steam-frame/install.sh` with the tarball.

The installer adds the library entry with its artwork and icon, gives SteamVR's dock the name and icon, and sets SteamVR to 120 Hz for it. `install.sh --dry-run` lists every file it would copy and every Steam and SteamVR call, and changes nothing. It never asks a question, and if Steam is not running it installs the files and says the library entry was skipped; run it again with Steam up.

You can also install with [Frametanium](https://github.com/Cogential/frametanium): add `simplysanatori/AzaharFrame` and it runs the same `install.sh`.

### Update

Download the new tarball and install it, or update from Frametanium. Your settings, saves and keys stay, and the same library entry is updated. Quit the emulator first.

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
steam-frame/build.sh 1.0.0
steam-frame/install.sh
```

The first build takes about an hour and loads every core. It waits while the headset is in use. `build.sh` packs `steam-frame/release/azaharframe-1.0.0-linux-arm64.tar.gz` and `SHA256SUMS`, and `install.sh` installs that tarball. The options are in [TECHNICAL_NOTES.md](TECHNICAL_NOTES.md#building) and [AGENTS.md](../AGENTS.md).

## First run

The game list is empty the first time.

1. Azahar's window floats in front of you. Point at the empty game list and double-click it with the trigger (or **A**).
2. Select your game folder.
3. Double-click a game to start it.

<!-- VERIFY: steps 1 to 3 -->

### Games

Put your games in `~/Documents/Roms/3ds/`, or any folder you like. It plays `.3ds`, `.cci`, `.cxi`, `.app` and `.3dsx` files, and the compressed `.zcci`, `.zcxi` and `.z3dsx` ones. A `.cia` is installed first, from **File > Install CIA...** in the Azahar window; it then appears in the game list with your other games.

- Decrypted dumps work as they are.
- Encrypted dumps need `aes_keys.txt` from your own console in `~/.local/share/AzaharFrame/user/sysdata/`.

### Texture packs and mods

Custom textures and game mods go in the data folder, in a folder named for the game's title ID (the 16 characters shown in the emulator's log, for example `0004000000033500`):

- `~/.local/share/AzaharFrame/user/load/textures/<TitleID>/` for texture packs (the pack's `pack.json` goes directly in that folder).
- `~/.local/share/AzaharFrame/user/load/mods/<TitleID>/` for mods (a `romfs/` folder, `exheader.bin` or `code.ips`).

Custom textures are on from the first launch, and they load in the background while you play, so a new area can show the original textures for a moment. If you already have your own settings file, turn on **Emulation > Configure > Graphics > Advanced > Use custom textures** yourself.

#### Optimize Texture Pack for Frame

HD packs are PNG files, and the emulator decodes each one the first time the game uses it. A 4096×4096 texture takes over a second, which is why a new area shows the low-resolution textures first. So I added a converter: in the Azahar window, right-click the game and choose **Optimize Texture Pack for Frame...**. It writes a copy of the pack in ASTC, the compressed format the Frame's GPU reads as it is, with every mip level made in advance. Those files are read in a few milliseconds instead of being decoded, and they take a quarter of the GPU memory.

- The original pack is not changed. The copy goes next to it (`<TitleID>-astc4x4`), and the game's folder in `load/textures/` becomes a link to the copy. **Use Original Texture Pack** in the same menu switches back. To remove the copy, delete its folder.
- The first time, it builds its encoder (ARM's astcenc), which takes under a minute and needs git, CMake and a C++ compiler.
- **Stop** is safe. Choose the menu entry again and it carries on where it left off. It runs at the lowest priority, but it is still best not to run it while you play.
- Ocarina of Time 3D with Henriko Magnifico's 4K pack: 2308 textures, 4.5 GB as PNG. As ASTC 4x4 it is 6.0 GB, and about 2.7 GB with `--block 6x6`. It takes about 15 to 20 minutes on the Frame, or about 6 with `--quality fast`, which looks nearly the same. Decoded, the PNGs would be 18 GB of memory; the emulator now keeps an ASTC texture in memory only until the GPU has it.

From a terminal, with more options (`--block 6x6` for a pack half the size, `--quality thorough`, `--jobs`):

```sh
~/Games/AzaharFrame/app/tools/convert-texture-pack.sh --activate ~/.local/share/AzaharFrame/user/load/textures/<TitleID>
~/Games/AzaharFrame/app/tools/convert-texture-pack.sh --restore ~/.local/share/AzaharFrame/user/load/textures/<TitleID>
```

Packs that already ship DDS or KTX files work too, with or without `skip_mipmap`, and their mip levels are used as stored.

<!-- VERIFY: the ASTC optimizer in the headset session; it is built and mock-tested only -->

#### Ocarina of Time 3D mods

[CONTROLS.md](CONTROLS.md) lists the controls for Ocarina of Time 3D with the Ocarina ReFramed single-screen mod, and the settings I use with Single Screen Experience (Hide Bottom Screen, Screen Shape, Immersive Game FOV). Short version for Single Screen Experience:

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
| Grip aimed at a screen | Move the screens. The stick of that hand moves them nearer and farther |
| **Menu** (right controller) | Start. Hold for 1 second to pause and open the in-game menu |
| **View** (left controller) | Select. Hold for 1.5 seconds to stop the game |
| Right stick click | Swap the screens: the other screen becomes the main one. Tap again to swap back. Hold for 1 second to reset the screen positions |
| Left stick click | Hide or show the screen that is not in focus |
| Both bumpers + **View** | Show or hide the bottom screen, in games that hide it |
| Both bumpers + **View** + **Menu** | Switch between VR and Flat (Azahar's window on the Frame's screen) |
| **View**, then the right bumper | Quick Save. Then the left bumper instead: Quick Load |
| **View**, then the right trigger | Fast Forward on or off |

On Azahar's window and in the in-game menu:

| Frame | Window |
|---|---|
| Trigger or **A** | Left click |
| Right stick click | Right click. Hold for 1 second to bring the window back in front of you |
| **B** | Back. In the in-game menu with nothing open, back to the game |
| Grip | Move the window |
| Stick up and down | Scroll |
| **Y** | Show or hide the keyboard |

The bottom screen in focus is the one in the main place, big, and the top screen is in the place below. Touch works on the bottom screen in either place. The screen in focus is always shown; a hidden bottom screen shows while it is in focus. Immersive mode pauses while the bottom screen is in focus. Every game starts with the top screen in focus. The details are in [CONTROLS.md](CONTROLS.md#swapping-the-screens).

The right controller is also the 3DS's motion sensor, for games that aim by moving the 3DS. Hold it like a 3DS with the beam pointing into the screen.

The **Controls** button above Azahar's window, on the home window and in the in-game menu, opens a panel with all of this, named after your own controls profile. It opens by itself the first time VR starts, and each time the in-game menu opens (it closes with the menu), unless you closed it yourself. The **Stats** button next to it shows the frame rate and the frame times under the top screen while a game runs.

All controls, with the keyboard and the pointing rules: [CONTROLS.md](CONTROLS.md).

<!-- VERIFY: the table in the headset. It matches src/frame_xr/input/MAPPING.md. -->
<!-- VERIFY: the keyboard on the window (Y, typing) is built and mock-tested only -->

### Playing in a window (Flat)

I wanted to be able to play a 3DS game in a window as well, like any other flat game on the Frame, with other apps open beside it, instead of loading the app and having black all around me. So the same library entry starts in one of two modes: VR, as above, or Flat, which is Azahar's normal window on the headset's virtual screen. It starts in the mode you used last, and VR the first time. Press both bumpers, **View** and **Menu** together to switch between them while it runs; [CONTROLS.md](CONTROLS.md#playing-in-a-window-flat) says how to start in Flat once.

Both modes use the same data folder, so your games, saves, mods, textures and settings are the same. In Flat, Azahar is not forced to full screen, so it stays in its window, on SteamVR's big screen (the theater), and stays there when you close the dashboard. Switching back to VR takes a few seconds while the window goes back into the dashboard. The Frame controllers come through Steam Input as a gamepad and are mapped by position, like the 3DS, in their own controls profile, **Flat Gamepad**, so your VR controls are left alone. The mouse is the touch screen. 3D follows Azahar's own 3D setting.

<!-- VERIFY: the VR/flat switch in the headset; it is built and mock-tested only -->

## Settings

Everything is in the Azahar window on the SteamVR dashboard, under **Emulation > Configure**. Resolution, filters, controls and audio are the ones you know from Azahar. Azahar for Frame starts with the **Dark** theme, which is easier on the eyes in the headset; **Emulation > Configure > General > UI > Theme** changes it.

**Emulation > Configure** always changes the settings for every game, also when you open it from the in-game menu. To change a setting for one game only, use that game's **Properties**. While a game with its own value runs, that row is greyed out in **Configure** and shows the value for all games.
Per-game settings work as in Azahar: right-click a game in the list and choose **Properties**.

The first time VR starts, the buttons and sticks of the current controls profile are replaced with the Frame layout (**Emulation > Configure > Controls**). After that your changes stay. In VR the Single Screen and Separate Windows layouts are changed to Default, and the 3D depth is set to 50 % if it is 0.

### Face Buttons and Motion Controls

In the **VR** group of the Graphics tab (global settings, not per game):

- **Face Buttons**: **By Position (Nintendo)** (default) makes the Frame's A, B, X and Y do what the 3DS button in the same place does. **By Label (Xbox)** makes A, B, X and Y the 3DS's A, B, X and Y.
- **Motion Controls**: **Off**, **Right Controller** (default) or **Left Controller**. The controller becomes the 3DS's gyroscope and accelerometer, for games like Ocarina of Time 3D that aim the first-person items by moving the 3DS.

<!-- VERIFY: motion axes and speed in the headset -->

### Window Scale

In the **VR** group of the Graphics tab (global, not per game): **Window Scale** is **100%**, **150%** (default) or **200%**. It makes Azahar's window look larger in the headset, so that the text, buttons, list rows and menus are easier to read and to hit with the beam. The window shows less at a time (at 150% about 40% of the area of 100% is left out, so the game list is shorter) and the panel is wider, 30% at 150% and 50% at 200%. The game list has no side-to-side scroll bar in VR: long names are cut short. Windows that are larger than Azahar's window, such as the configuration window, make the panel wider still (up to 50%, and no taller than it is wide), so they look larger than at 100%, but not twice as large. The change shows as soon as you press **OK**. Flat mode always uses 100%: the window is on the screen there, and `QT_SCALE_FACTOR` in `session.env` is how to enlarge it.

<!-- VERIFY: Window Scale in the headset (size of the text, the panel's width) -->

### Background

In the **VR** group of the Graphics tab (global, not per game): **Background** is **Black** (default) or **Passthrough**. Passthrough shows the room behind the screens, through the headset's cameras, if SteamVR offers it to apps; the log says at start whether it does ("environment blend modes"). The in-game menu has a **Passthrough** button for it too, and the choice is kept.

<!-- VERIFY: Passthrough in the headset. It needs SteamVR to offer the alpha blend mode; if it does not, the button is greyed out and the background stays black. -->

### Quick Save and Quick Load

Hold **View** and press the right bumper to Quick Save, or the left bumper to Quick Load. The in-game menu has **Quick Save** and **Quick Load** buttons too. Both use Azahar's Quick Save slot, the same as the Emulation menu. A short message in front of you confirms it. Details: [CONTROLS.md](CONTROLS.md#quick-save-and-quick-load).

### Fast Forward

Hold **View** and pull the right trigger to run the game at Azahar's turbo speed; do it again to go back to normal speed. "Fast Forward" shows in front of you for 2 seconds, then fades; turning it off shows "Normal Speed" for 2 seconds. It goes off by itself when the game stops, when the in-game menu opens and when you switch between VR and Flat. The speed is **Turbo Speed Limit** in **Emulation > Configure > General**. Details: [CONTROLS.md](CONTROLS.md#fast-forward).

### Show Controllers and Show Stats

Also in the **VR** group, global only:

- **Show Controllers** (on by default) shows the Frame controllers in your hands, with the buttons and sticks moving. A controller you put down fades out after 30 seconds, and its beam after 4. The models are made from SteamVR's own when you install; if they are missing, the launcher makes them in the background and they show from the next start.
- **Show Stats** (off by default) shows the frame rate, the emulation speed, the frame times, the CPU load and the GPU clock under the top screen while a game runs. The **Stats** button above Azahar's window switches it as well.

<!-- VERIFY: the controller models, the beam fade and the stats panel in the headset -->

### Immersive mode

Normally the top screen floats two metres away. In immersive mode the game draws a wider view, and in the Full modes its camera follows your head.
Turn it on in the game's properties, on the **Graphics** tab, in the **VR** group, under **Immersive Mode**:

- **Wide (3DoF):** a big screen fixed in the room.
- **Full (6DoF): Ocarina of Time, Majora's Mask:** the screen stays in front of your eyes and the game camera turns and moves with your head.
- **Full (6DoF): Other Games:** the same, for games the other mode does not fit; it finds the camera while you play.

Three games start with a setup already, from CitraVR's playtest notes: Mario Kart 7, Super Mario 3D Land and Asphalt 3D (every region the compatibility list knows) start in **Full (6DoF): Other Games**, with **View Matrix Register** 90 for the first two and 8 for Asphalt 3D. It only applies when the game's own properties have no **Immersive Mode** (or register) of their own and the global one is still **Off** (or **Auto**). If you would rather play one of them without it, set **Immersive Mode** to **Off** in that game's properties; any value you set yourself wins.

<!-- VERIFY: the three presets in the headset. They come from CitraVR's Quest notes, which may not hold on SteamVR. -->

The **Immersive Game FOV** setting next to it is the game camera's vertical angle in degrees (60 by default). Change it only if a mod changes the game's FOV. Only Ocarina of Time 3D and Majora's Mask 3D have the settings it needs. Both bumpers and **View** (or a tap of the left stick click) show or hide the bottom screen, and holding the right stick click for 1 second re-centres the view.

The screen starts a little below eye level, with the whole picture in view. Squeeze a grip and move your hand up or down to move it; the stick of that hand moves it farther or nearer (up or down) and makes it bigger or smaller (right or left). The world stays where it is while you move the screen, and each game remembers where you left it. Click the right stick while you hold the screen to put it back. A tap of the right stick click swaps to the bottom screen, which pauses immersive mode until you tap again. [IMMERSIVE.md](../src/frame_xr/IMMERSIVE.md) has the details and the tuning variables.

<!-- VERIFY: the whole section -->

### Hide Bottom Screen

Some games do not need the bottom screen. In a game's properties, on the **Graphics** tab, in the **VR** group, tick **Hide Bottom Screen** to show only the top screen. Hold both bumpers and press **View** to bring the bottom screen back, and again to hide it. The setting is read when the game starts. [CONTROLS.md](CONTROLS.md) lists the controls for Ocarina of Time 3D with the Ocarina ReFramed single-screen mod, and the settings I use with Single Screen Experience.

### Screen Shape

Some mods draw the top screen widescreen, squeezed into the 3DS's 5:3 picture. In a game's properties, on the **Graphics** tab, in the **VR** group, set **Screen Shape** to **Widescreen (16:9)** or **Ultrawide (21:9)** to stretch the top screen in VR to match. It keeps its height and gets wider, and the bottom screen stays where it was. It is also the shape of the screen in immersive mode. The setting is read when the game starts, and it has to agree with the mod's own aspect option.

## Performance

The 3DS runs at 60 frames a second, and every one is shown on each refresh of the headset.
The first time a game draws a new effect, the emulator compiles its shaders and the picture can hitch. They are kept in `~/.local/share/AzaharFrame/user/shaders`, so it happens once.
If a game drops frames, lower the internal resolution under **Emulation > Configure > Graphics**.
**Show Stats** in the **VR** group, or the **Stats** button above Azahar's window, shows the game's frame rate and the headset's late frames while you play.
Keep other heavy work off the Frame while you play.

## Files

| What | Where |
|---|---|
| Program and launcher | `~/Games/AzaharFrame/app` |
| Steam library art and icons | `~/Games/AzaharFrame/app/art` |
| Dock icon files | `~/.local/share/applications/azaharframe.desktop`, `azaharframe.png` and `azahar.png` in `~/.local/share/icons/hicolor`, `azaharframe.vrmanifest` in the program folder |
| Icon cache of the old VR picker (`AZAHARFRAME_VR_PICKER=1`) | `~/.cache/azaharframe/icons` |
| Settings, saves, keys, shader caches | `~/.local/share/AzaharFrame/user` |
| Launcher log | `~/.local/share/AzaharFrame/launch.log` |
| Emulator log | `~/.local/share/AzaharFrame/user/log/azahar_log.txt` |
| Settings for one launch | `~/.local/share/AzaharFrame/session.env` |
| Mode it starts in (`vr` or `flat`) | `~/.local/share/AzaharFrame/mode` |
| Texture pack encoder (built on first use) | `~/.local/share/AzaharFrame/tools` |
| Controller models for VR | `~/.local/share/AzaharFrame/user/frame-models` (log: `~/.local/share/AzaharFrame/frame-models.log`) |

How the port works: [TECHNICAL.md](TECHNICAL.md).

## Troubleshooting

- **The installer says Steam was not reachable.** Start Steam and run `install.sh` again.
- **SteamVR's dock shows a question mark instead of the flower.** Steam reads the icon when it starts. Restart Steam once.
- **The screens do not appear in the headset.** Start SteamVR first, then Azahar for Frame. `launch.log` says which OpenXR runtime it found, and the emulator log says `running flat` when it could not start VR.
- **A game in a subfolder is not in the list.** Right-click the game folder in the list (right stick click) and tick **Scan Subfolders**.
- **Azahar's window is too small in VR.** Set **Window Scale** in the **VR** group of the Graphics tab to 150% or 200%. In flat mode, add `QT_SCALE_FACTOR=1.25` to `session.env`.
- **An encrypted game does not start.** It needs `aes_keys.txt` in `user/sysdata/`.
- **Motion ghosts.** In SteamVR, check 120 Hz and motion smoothing off.
- **No controllers in VR, only the beams.** The models are made from SteamVR's. Delete `user/frame-models` and start once more, then read `frame-models.log`.

<!-- VERIFY: each entry -->

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
- Valve: SteamVR and the Frame

Full list: [THIRD_PARTY_NOTICES.md](../THIRD_PARTY_NOTICES.md). Artwork sources: [steam-frame/art/CREDITS.md](../steam-frame/art/CREDITS.md). CitraVR's README: [README.upstream.md](../README.upstream.md). Unofficial fan project, not affiliated with Nintendo, Valve, the Azahar team or the CitraVR author. Nintendo 3DS is a trademark of Nintendo.
