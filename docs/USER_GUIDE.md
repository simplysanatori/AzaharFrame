# Azahar for Frame: user guide

The short version is in the [README](../README.md). This guide has the rest: installing and removing in detail, your games, texture packs and mods, every control, the settings, flat mode, immersive mode, troubleshooting and what a bug report holds.

## Status and known issues

Version 1.2.0. The changes are listed in [CHANGELOG.md](CHANGELOG.md).

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
- Cheats, mods and texture packs download from the in-game menu's **Mods** tab.
- Gamepads and a mouse play alongside the Frame controllers, and **Edit Controls** changes the buttons of any of them.
- When a game asks for text, a keyboard opens in the headset.

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

The README has the steps. In short: get the repository, download `azaharframe-1.2.0-linux-arm64.tar.gz` from [Releases](https://github.com/simplysanatori/AzaharFrame/releases), and run `steam-frame/install.sh` with the tarball. To check the download against `SHA256SUMS` first:

```sh
(cd ~/Downloads && sha256sum -c --ignore-missing SHA256SUMS)
```

The installer adds the library entry with its artwork and icon, gives SteamVR's dock the name and icon, and sets SteamVR to 120 Hz for it. `install.sh --dry-run` lists every file it would copy and every Steam and SteamVR call, and changes nothing. It never asks a question. If Steam is not running it installs the files and says the library entry was skipped; run it again with Steam up.

Other installers:

- [Frametanium](https://github.com/Cogential/frametanium): add `simplysanatori/AzaharFrame`. It downloads the tarball and runs the same `install.sh`, so the library entry is the same one.
- [FrameDrop](https://framedropvr.com): [Install with FrameDrop](https://framedropvr.com/install?manifest=https%3A%2F%2Fgithub.com%2Fsimplysanatori%2FAzaharFrame%2Freleases%2Flatest%2Fdownload%2Fazaharframe.framedrop.json). It installs `azaharframe-1.2.0-framedrop-linux-arm64.zip` and adds its own library entry. The first start runs the installer without adding a second entry, and so does the first start after an update. Its log is `~/.local/state/framedrop-azaharframe/install.log`.

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
steam-frame/build.sh 1.2.0
steam-frame/install.sh
```

The first build takes about an hour and loads every core. It waits while the headset is in use. `build.sh` packs `steam-frame/release/azaharframe-1.2.0-linux-arm64.tar.gz` and `SHA256SUMS`, and `install.sh` installs that tarball. That build links the Qt and SDL2 SteamOS has, so a SteamOS update can break it. The release is made with `steam-frame/build-portable.sh`, which carries its own. The options are in [TECHNICAL_NOTES.md](TECHNICAL_NOTES.md#building) and [AGENTS.md](../AGENTS.md).

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

#### The Mods tab

The in-game menu's **Mods** tab does all of this from the headset. **Cheats**, **Mods** and **Textures** at its top switch between three lists for the running game. Nothing is downloaded until you press a download or install button. My own cheats and fixes carry a verified badge after their name.

**Cheats**

- **Download** fetches the game's cheats from [Sharkive](https://github.com/FlagBrew/Sharkive) and from my [3ds-game-fixes](https://github.com/simplysanatori/3ds-game-fixes) repository, and adds the new ones to the list. A line under the buttons says how many were added.
- The switch on each row turns a cheat on or off. It works at once and is kept.
- A cheat made for another version of the game shows that version in a red chip, for example **v1.0**. It may not work with yours.
- **Azahar's Window** opens Azahar's own **Cheats** window, where you add, edit and delete your own cheats. Its **Download Cheats** button fetches the same cheats.

**Mods**

- The list shows what is installed for the game. Mods installed from here have a switch and **Remove**. Files you put in the mods folder yourself have **Remove** only, which asks first.
- **Install Mods** lists the mods known for the game, with their author, their version and the date they were last updated (when the source says), and installs one with **Install**. The files come straight from the author's own page on GitHub, GameBanana, MEGA or Google Drive; nothing is re-hosted. Each row shows the author, the version, the downloads and the date on one line. The list holds my own fixes and community mods such as Single Screen Experience, Ocarina ReFramed and Project Restoration. Project Restoration has three rows, each with a chip for the game version it was made for (hover it for what it means); the row that matches your game says so and comes first.
- When two mods change the same files, installing the second asks before it replaces the first, or tells you which one to remove first.
- **Created by** and the author's name open the author's page in Steam's browser, after **Open GitHub?**.

**Textures**

- **Custom Textures** turns texture packs on or off.
- Each of the game's packs has a checkbox, and several can be on at once. Each row names the author, the version, the format and the size.
- With two or more packs on, **Enabled Textures** lists what they add, top first: a pack with a few folders (2 to 10, the parts its **Customize** page shows) as one row each, so the folders of two packs can be interleaved; any other pack as one row. When two rows change the same texture, the higher one wins (**Texture Conflicts: Higher in the List Takes Priority**), and a row says what it replaces, such as **Replaces 120 Textures From 5X Redrawn UI HD**. **Move Up** and **Move Down** set the order; the checkbox on a part's row turns that part on or off, the same as on the pack's **Customize** page. A pack goes on top when you turn it on, except one made to go under a pack that is already on. Packs made to be combined go in the right order by themselves: Modern Pokemon Sprites goes above 5X Redrawn UI in Pokémon X and Y, so its sprites replace that pack's.
- **Download Packs** lists packs for the game and downloads one with **Download**, from GitHub, GameBanana or Google Drive, with the free space checked first. They include Henriko Magnifico's 4K and 1080p packs for Ocarina of Time 3D, Majora's Mask 3D, Super Mario 3D Land and A Link Between Worlds. **Cancel** stops a download. **Delete** removes a downloaded pack.
- **Preload** loads the whole pack when the game starts, instead of while you play. It is off by default. Turning it on asks **Preload Textures?** first, with the pack's size, the memory in use and Azahar's limit. A pack too large to preload loads as you play, and a message says so.
- **Customize** on a pack opens its page. A pack with parts, such as characters or backgrounds, lists them with a checkbox each; a pack that comes in variants lets you pick one. When two parts change the same texture, the one higher in the list wins (**Texture Conflicts: Parts Higher in the List Take Priority**); **Move Up** and **Move Down** set the order. In Flat the same page is **Customize Texture Pack...** in the game list's right-click menu, and under **Emulation** for the running game; it lists the game's packs at the top, with **Enabled Textures** when two or more are on, and shows the parts of the pack you select. With two or more packs on, the game list's **Optimize** and **Use Original** entries name the pack they act on, the top one. A PNG pack has **Optimize for Frame** there: it makes the ASTC copy described under [Optimize Texture Pack for Frame](#optimize-texture-pack-for-frame), with the progress on the page and **Stop**. It keeps going if you close the menu, at the lowest priority, and never touches the files the game has open. The copy takes over at the next start, and the parts you turned off stay off. **Use Original** switches back. **Remove Original** deletes the PNG pack to free its space, once the game no longer reads it; to go back after that, download or add the pack again. A PNG pack over 1 GB asks once, **Optimize <Pack> for Frame?**, when you pick it or its download finishes.

A change to mods or textures shows **Restart the Game to Apply**. **Restart** restarts the game.

Downloaded packs go to `load/texture-packs/<TitleID>/`, and `load/textures/<TitleID>` points at the one you picked. Installed mods are kept in `load/mod-library/<TitleID>/` and copied into `load/mods/<TitleID>/`.

#### The Homebrew tab

The in-game menu's **Homebrew** tab, under **Mods**, installs homebrew built from a game you own. For now that is [Pokémon Emerald 3DS](https://github.com/ZallaxDev/emerald-3ds). **Install** continues in Azahar's window: it asks for your Pokémon Emerald ROM, the language and the game folder, then downloads the release and builds the game, with the progress on the row. Once it is installed, **Play** starts it. Once a day, when the game list loads, Azahar for Frame asks GitHub for the latest release; **Update** then shows next to **Play**. When you already have the latest, **Check for Updates** asks again at once, and the row shows the installed version and when it was checked. **Reinstall** builds the game again from the kept ROM, if something went wrong. Azahar's own **Homebrew** menu has the same entries, with **Check for Updates** at its end.

Azahar for Frame builds the game's data itself, from the release's recipe. If a newer Emerald 3DS release changes that recipe so Azahar for Frame can't follow it, it says so: **Emerald 3DS v0.6.0 Needs Its Own Builder**. **Build With Emerald 3DS's Builder** runs the build program that comes with that release, the one the project's own builder uses, in a protected sandbox. The sandbox can read only your ROM file and write only its own work folder, and has no access to the rest of your files. It reaches the internet only to download the optional follower sprites. The result is checked the same way and installed the same way. **Not Now** leaves things as they are, and the next Install or Update of that version asks again. Once you choose to build, Azahar for Frame remembers it for that version and does not ask again. It needs Python 3.11 or newer and bubblewrap (both are on SteamOS); without them it says so and offers no button. **Cancel** stops the sandbox at once.

#### Optimize Texture Pack for Frame

HD packs are PNG files, and the emulator decodes each one the first time the game uses it. A 4096×4096 texture takes over a second, so a new area shows the low-resolution textures first. I added a converter for that. In the Azahar window, right-click the game and choose **Optimize Texture Pack for Frame...**. It writes a copy of the pack in ASTC, the compressed format the Frame's GPU reads as it is, with every mip level made in advance. Those files load in a few milliseconds, with no decoding, and take a quarter of the GPU memory.

- The original pack is not changed. The copy goes next to it (`<TitleID>-astc4x4`), and the game's folder in `load/textures/` becomes a link to the copy. **Use Original Texture Pack** in the same menu switches back. To remove the copy, delete its folder. **Remove Original Texture Pack...** deletes the original to free its space; it only ever deletes the folder the copy was made from, inside the texture folders. The in-game menu does the same from the pack's page (see **Textures** above).
- The encoder (Arm's astcenc) is part of the program, so it needs nothing else.
- **Stop** is safe. Choose the menu entry again and it carries on where it left off. It runs at the lowest priority, but it is still best not to run it while you play.
- Ocarina of Time 3D with Henriko Magnifico's 4K pack: 2308 textures, 4.5 GB as PNG. As ASTC 4x4 it is 6.0 GB. It takes about 15 to 20 minutes on the Frame. Decoded, the PNGs would be 18 GB of memory; the emulator keeps an ASTC texture in memory only until the GPU has it.

Packs that already ship DDS or KTX files work too, with or without `skip_mipmap`, and their mip levels are used as stored.

#### Ocarina of Time 3D mods

[CONTROLS.md](CONTROLS.md) lists the controls for Ocarina of Time 3D with the Ocarina ReFramed single-screen mod, and the settings I use with Single Screen Experience. Short version for Single Screen Experience:

- Turn on **Hide Bottom Screen** for the game.
- Set **Screen Shape** to the mod's shape: **Widescreen (16:9)** for its 16:9 option, **Ultrawide (21:9)** for its widest one. Immersive mode reads the game's own shape and angle, so it needs no setting.
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

The top screen can grow to 3 times its starting size, enough to fill the whole view, and the bottom screen moves down to stay just under it. It is made bigger rather than brought nearer: the top screen never comes closer than 1 m. A bigger screen gets less 3D depth, so the distant scenery stays easy on the eyes. If an edge of a big screen is just out of view, keep the grip squeezed and press D-pad left or right: each press moves the screens half a degree around you, and **Moved Right** (or Left) shows for 2 seconds. With the grip still squeezed, D-pad up or down makes the main screen more or less curved (Off, then 15° to 90° in 15° steps), and **Screen Curve: 45°** (for example) shows; it is kept for every game, and **Screen Curve** in the **VR** group sets it too. While you hold the screens the D-pad does not reach the game. A small hint under the screens lists these when you take hold.

With the bottom screen in focus, it is in the main place, big, and the top screen is in the place below. Touch works on the bottom screen in either place. The screen in focus always shows; a hidden bottom screen shows while it is in focus. Immersive mode pauses while the bottom screen is in focus. Every game starts with the top screen in focus. The details are in [CONTROLS.md](CONTROLS.md#swapping-the-screens).

**Detach Screens**, on the in-game menu's **Position** tab, lets the screens move apart: **Off**, **Secondary** (the smaller screen moves on its own) or **Both** (each screen does). Grip a detached screen to take it anywhere around you; the stick of that hand sets its distance and size. Each game keeps its screen places. Details: [CONTROLS.md](CONTROLS.md#detaching-the-screens).

The right controller is also the 3DS's motion sensor, for games that aim by moving the 3DS. Hold it like a 3DS with the beam pointing into the screen.

The **Controls** button above Azahar's window opens a panel with all of this, named after your own controls profile. It opens by itself the first time VR starts. In a game, the in-game menu's **Controls** tab shows the same. The **Stats** button next to it shows the frame rate and the frame times while a game runs, in front of you or on your left wrist (**Stats Position**).

All controls, with the keyboard and the pointing rules: [CONTROLS.md](CONTROLS.md).

### Edit Controls, gamepads and a mouse

A Bluetooth or USB gamepad plays in VR alongside the Frame controllers. Pair it in SteamOS as usual. A mouse works too.

**Edit Controls**, on the in-game menu's **Controls** tab, shows a New 3DS XL with a chip on each button and stick. The chip says what drives that button on the device you picked at the bottom: **Frame Controllers**, each connected gamepad by name, and **Mouse**. Select a chip and press the button you want for it. A button that already had a job swaps with it. For the Circle Pad or the C-Stick, push the stick up, then right. **Cancel** stops listening, **Reset to Default** puts the device's layout back and **Done** goes back to the Controls tab. Each kind of gamepad keeps its own layout, in `user/controller-mappings.txt`. The Frame controllers' layout is your controls profile, as in **Emulation > Configure > Controls**.

A gamepad's touchpad is the bottom screen. The pad covers the whole screen, and a dot shows where your finger is. **Touchpad**, on the **Controls** tab, sets how it touches: **Tap for Input** (the default) touches when your finger is on the pad; **Click for Input** touches when you press the pad, and holding it down drags. Steam Input must leave the touchpad to the game: turn Steam Input off for that controller. In Edit Controls, a device with a touchpad shows the dot live on the drawing's bottom screen.

**Bottom Screen Reticle**, on the **Controls** tab, lets a controller or your head aim at the bottom screen like a stylus, for games that aim with the touch screen, such as Kid Icarus: Uprising. Its trigger touches and its grip re-centres. While it aims in a game, that controller draws no beam. **Reticle Speed**, under it, sets how far the reticle moves when you turn: at the default 2.0x your head turns about 15 degrees to cross the screen. Both can be set per game, and Kid Icarus: Uprising starts with **Headset**. **C-Stick at Edge**, under them, pushes the C-Stick toward an edge while you touch near it, which turns the camera in Kid Icarus: Uprising with the C-Stick & Gyro mod. Details: [CONTROLS.md](CONTROLS.md#bottom-screen-reticle).

The mouse has two modes, in **Mouse Mode** on its Edit Controls page. Hold the left and right buttons together for 3 seconds to switch; **Mouse: Pointer** or **Mouse: Bottom Screen** shows.

- **Bottom Screen**: the mouse moves a cursor on the bottom screen, and the left button touches and drags. With **Recentre at Edge** on, a drag that runs into an edge lets go and starts again from the middle, for games that turn the camera with the touch screen. A touchpad has the same choice.
- **Pointer**: the mouse moves a pointer around you, like the beam, for the menu, Azahar's window and the screens. It is adapted from [Frametop](https://github.com/Frametop/frametop)'s pointer.

By default the right button is A, the back and forward buttons are L and R, and the middle button opens the in-game menu. Edit Controls changes them.

### The game keyboard and the Mii Selector

When a game asks for text, such as a name, a keyboard opens in front of you over the paused game. It keeps the game's rules (length, allowed characters) and its own buttons. Point and pull the trigger (or press **A**) to type; **B** is Backspace. A keyboard connected to the Frame types into it too. Immersive mode steps aside while it is open.

When a game asks you to choose a Mii, the Mii it suggests is chosen and its name shows. In Flat both are Azahar's own dialogs.

### Playing in a window (Flat)

I wanted to play a 3DS game in a window as well, like any other flat game on the Frame, with other apps open beside it. So the same library entry starts in one of two modes: VR, as above, or Flat, which is Azahar's normal window on SteamVR's big screen. It starts in the mode you used last, VR the first time. Press both bumpers, **View** and **Menu** together to switch while it runs. [CONTROLS.md](CONTROLS.md#playing-in-a-window-flat) says how to start in Flat once.

Both modes use the same data folder, so your games, saves, mods, textures and settings are the same. In Flat, Azahar stays in its window on SteamVR's big screen (the theater), and stays there when you close the dashboard. Switching back to VR takes a few seconds while the window goes back into the dashboard. The Frame controllers come through Steam Input as a gamepad and do the same as in VR, with your VR controls profile: a tap of **View** or **Menu** is Select or Start, and the holds and combos work. Holding **Menu** for 1 second opens the Flat menu: the game pauses and the window leaves Fullscreen with its menu bar, so the pointer reaches Azahar's menus and settings. Hold **Menu** again, press **B** or select **Emulation > Continue** to go back, Fullscreen again if it was. A menu that is open when you switch modes closes and the game goes on. The mouse is the touch screen. 3D follows Azahar's own 3D setting.

In Flat the window is 16:9, so it fills the big screen. Games start in the **Large Screen** layout: one big screen and the other one small beside it. Tap the right stick click to swap the screens. Tap the left stick click to show only the main screen, and tap it again for both. Hold the right stick click for 1 second to go back to **Large Screen**. Flat keeps this layout to itself, so your layout settings stay as they are for VR. **View > Fullscreen** works in Flat only.

## Settings

Everything is in the Azahar window, under **Emulation > Configure**. Resolution, filters, controls and audio are the ones you know from Azahar. Azahar for Frame starts with the **Dark** theme, which is easier on the eyes in the headset; **Emulation > Configure > General > UI > Theme** changes it.

**Emulation > Configure** always changes the settings for every game, also when you open it from the in-game menu. To change a setting for one game only, right-click the game in the list and choose **Properties**. While a game with its own value runs, that row is greyed out in **Configure** and shows the value for all games.

In the in-game menu, the **Settings**, **Controls** and **Position** tabs have a **Global | Per-Game** switch at the top right. While a game runs the menu opens on **Per-Game**, so a change is for that game only; **Global** changes every game. Point at **Per-Game** while it is picked to see how many values on the tab this game has of its own, for example "2 Per-Game Values Set". **Use Global Values for All**, in the **⋮** menu next to the switch, removes them, so the game follows the global values again. On **Settings** it counts and clears the section you are in. In **Global**, a row the game has its own value for is greyed out.

The first time VR starts, the buttons and sticks of the current controls profile get the Frame layout (**Emulation > Configure > Controls**). After that your changes stay. In VR the Single Screen and Separate Windows layouts are changed to Default, and the 3D depth is set to 50 % if it is 0.

The VR settings are in the **VR** group at the bottom of the **Graphics** tab. [SETTINGS.md](SETTINGS.md) explains each one, with its choices and default.

### Face Buttons and Motion Controls

Global settings, not per game:

- **Face Buttons**: **By Position (Nintendo)** (default) makes the Frame's A, B, X and Y do what the 3DS button in the same place does. **By Label (Xbox)** makes A, B, X and Y the 3DS's A, B, X and Y.
- **Motion Controls**: **Off**, **Right Controller** (default), **Left Controller** or **Headset**. The controller, or your head, becomes the 3DS's gyroscope and accelerometer, for games like Ocarina of Time 3D that aim the first-person items by moving the 3DS. It can be set per game, and Kid Icarus: Uprising starts with it off.

### Window Scale

Global, not per game: **100%**, **150%** (default) or **200%**. It makes Azahar's window look larger in the headset, so the text, buttons, list rows and menus are easier to read and to hit with the beam. The window shows less at a time (about a quarter less at 150%), and the panel is wider: 30% at 150% and 50% at 200%. In VR the game list has no side-to-side scroll bar; long names are cut short. A window larger than Azahar's, such as the configuration window, makes the panel wider still, up to 50% and no taller than it is wide. The change shows as soon as you press **OK**. Flat mode always uses 100%, in a 16:9 window; there, `QT_SCALE_FACTOR` in `session.env` enlarges the window.

### Background

Global, not per game: **Black** (default) or **Passthrough**. Passthrough shows the room behind the screens, through the headset's cameras, when SteamVR offers it to apps. The in-game menu's **Settings** tab has it too, and the choice is kept. When SteamVR does not offer it, the choice is greyed out and the background stays black.

### Quick Save and Quick Load

Hold **View** and press the right bumper to Quick Save, or hold the left bumper for half a second to Quick Load. The in-game menu has **Quick Save** and **Quick Load** buttons too. Both use Azahar's Quick Save slot, the same as the Emulation menu. A short message in front of you confirms it. Details: [CONTROLS.md](CONTROLS.md#quick-save-and-quick-load).

### Save states

Save states are NOT a replacement for saving in the game. A state only loads in the version of Azahar for Frame that made it, so save in the game before you update. The first time you save or load a state, **About Save States** says so once.

A state from another version shows **From 1.1.1** (or **From Another Version**) in place of its date, dimmed. Tapping it asks first: **Load Anyway** may work, or it may break the game's progress. Before it loads, the game's save data and extdata are copied to `user/save-backups/<TitleID>/` (the last 3 copies are kept). If the game then closes unexpectedly within 10 minutes of play, the next time it starts it asks **Restore Save Data From 10:26?**, naming the time of the copy. **Restore** first copies your current save data to the same folder (named `...-before-restore`), so nothing you played since is lost; **Keep Current** leaves it as it is. If a state can't be saved (for example, the Frame is out of space), **Couldn't Save State** says so, the game keeps running and the slot keeps the state it had. If a state can't be loaded, the menu says so and the game carries on as it was; if it was only partly read, the game stops and nothing is changed. Saving over a slot makes it a normal slot again.

After you load a slot or an autosave, **Undo Load** under the slots goes back to the moment before, until the game closes.

### Autosave

Autosave makes save states of its own while you play, so you can go back a few minutes. It is off at first. Turn it on in the in-game menu (**Settings** > **General**, the **Saves** group at the end), in **Emulation > Autosave**, or in **Emulation > Configure > General**. **Every** sets the minutes of play between autosaves (1, 2, 5, 10 or 15; 5 at first), and **Keep** how many each game keeps (1 to 10; 3 at first). When there are more, the oldest go.

All three can be set for one game: in the in-game menu choose **Per-Game** (the menu opens on it while a game runs), or in the game's **Properties** choose **Set autosave** under **General > Autosave**. A game without its own values uses the ones for every game. Lowering **Keep** removes that game's extra autosaves at once; other games keep theirs until their next autosave.

An autosave is also made when the in-game menu opens and when you close the game, but at most one a minute, and none in the first minute after the game starts, while a state is saved or loaded, or while the game waits for its shaders. Autosaves are separate from the 10 slots and from Quick Save: they never replace a slot, and **Load from Newest Slot** ignores them. Each one is its own file in `user/states/autosave/`, with a small picture of the top screen beside it. The one made when the menu opens has a file of its own, replaced each time, and doesn't count toward **Keep**, so opening the menu to go back never removes your oldest autosave.

Turning Autosave on or off in the in-game menu while it shows **Per-Game** changes it for that game only, and a short message says so: "Autosave On for Ocarina of Time 3D". If an autosave fails, a short message says "Couldn't Autosave", with the reason when it is known (such as "Not Enough Space"), at most once every 3 minutes.

While you play, "Autosaved" shows for a moment just above the top screen's right corner. When that corner is out of view (you look down at the bottom screen, or the game is in immersive mode), a short message in front of you says it instead. Autosaves made when the menu opens or the game closes show nothing. In Flat the status bar says "Autosaved" for 3 seconds. If one fails, **Couldn't Autosave** says so.

To load one, open **Load State**. In VR the **Autosave** card above the slots shows the newest, with its picture and how long ago it was made; with **Keep** above 1, tap it to see the others. In Flat they are at the end of **Emulation > Load State**, as **Autosave 1** (the newest) and on. **Undo Load** takes you back if you picked the wrong one. An autosave from another version asks first, as a slot does.

An autosave pauses the game for about 0.2 to 0.5 seconds while its state is copied; the rest happens in the background.

### Microphone

The 3DS microphone works through the Frame's microphone. Azahar's microphone input, in **Emulation > Configure > Audio**, is on **Auto** by default.

### When memory runs low

The Frame's memory is shared with SteamVR, the passthrough cameras and the GPU, so the launcher gives Azahar a limit: about two thirds of the memory, or less when little is free at the start. Past it the system closes Azahar before the headset can freeze, and anything not saved in the game is lost. A large texture pack is the usual cause. I added two warnings before that point:

- At 85% of the limit, a message in front of you says how much is in use, for example **Running Low on Memory: 7.1 of 8.2 GB**. In Flat the status bar says it for 5 seconds. It shows again only after use falls under 80% and comes back, and at most once every 2 minutes.
- At 93%, the game pauses and **Azahar Is Almost Out of Memory** opens over the in-game menu (in Flat, as a dialog). **Turn Off Custom Textures**, shown while they are on, turns them off for this game until you close it, frees their memory at once and resumes the game. **Resume** carries on as you were, and **Stop Game** closes the game. Either way, save in the game soon. It asks again only after use falls under 88%, or if it keeps climbing.

There is no **Save State** in that dialog on purpose: writing a save state needs about 0.9 GB more for a moment, which could cross the limit and get Azahar closed in the middle of it. Autosaves wait while memory is that full, too.

### Fast Forward

Hold **View** and pull the right trigger to run the game at Azahar's turbo speed. Do it again to go back. "Fast Forward" shows in front of you for 2 seconds; turning it off shows "Normal Speed" for 2 seconds. It goes off by itself when the game stops, when the in-game menu or the Flat menu opens and when you switch between VR and Flat. The speed is **Turbo Speed Limit** in **Emulation > Configure > General**. The sound keeps its normal pitch; **Fast Forward Audio** (VR **Settings** tab, or **Emulation > Configure > Audio**) can also play it **Sped Up** or **Muted**. Details: [CONTROLS.md](CONTROLS.md#fast-forward).

### Show Controllers and Show Stats

Global only:

- **Show Controllers** (on by default) shows the Frame controllers in your hands, with the buttons and sticks moving. A controller you put down fades out after 30 seconds, and its beam after 4. The models are made from SteamVR's own when you install. If they are missing, the launcher makes them in the background and they show from the next start.
- **Show Stats** (off by default) shows the frame rate, the emulation speed, the frame times, the CPU load and the GPU clock while a game runs. **Stats Position** puts them in front of you (grab them with a grip to move them) or on your left wrist. The **Stats** button above Azahar's window switches it as well.

### Immersive mode

Normally the top screen floats two metres away. In immersive mode the game draws a wider view, and in the Full modes its camera follows your head. Turn it on in the game's properties, on the **Graphics** tab, in the **VR** group, under **Immersive Mode**:

- **Wide (3DoF):** a big screen fixed in the room.
- **Full (6DoF): Ocarina of Time, Majora's Mask:** the screen stays in front of your eyes and the game camera turns and moves with your head.
- **Full (6DoF): Other Games:** the same, for other games; it finds the camera while you play.

Five games start in immersive view with a setup already. Mario Kart 7 and Super Mario 3D Land start in **Full (6DoF): Other Games** with **View Matrix Register** 90, **Eye Indicator Register** `87,2` and their camera's angle; Super Mario 3D Land with **Depth Factor Multiplier** 100x, Mario Kart 7 with 10x, **Detect Game FOV** off, **Immersive Game FOV** 40 and a screen narrowed by 30% in width and 15% in height. Ocarina of Time 3D and Majora's Mask 3D start in **Full (6DoF): Ocarina of Time, Majora's Mask** with 10x, Majora's Mask 3D with **Detect Game FOV** off. Asphalt 3D starts in **Full (6DoF): Other Games** with register 8, from CitraVR's notes. That applies only when the game's own properties have no value of their own for each setting, and the global one is still the default. To play one of them without it, set **Immersive Mode** to **Off** in that game's properties. Any value you set yourself wins. Kid Icarus: Uprising starts in the normal view: it aims with a reticle on the screen, which cannot follow a camera turned by your head. You can still turn **Immersive Mode** on for it, in the VR menu or in its properties.

**Detect Game FOV** decides where the immersive screen in the Full modes gets the game camera's angle. With **Always** (the default), it reads the angle and shape from the game itself while it runs, so the world stays still when you turn your head, also through cutscenes, zooms and widescreen mods. A new angle is followed once it has held for half a second, and the view then eases into it a little at a time, so a camera cut never makes the view jump. When the game zooms in to aim, the screen keeps its size and shape and the picture zooms, as on the 3DS. With **Once**, the game's camera is measured once while you play, for about 15 seconds, leaving out cuts and cutscenes, and the result becomes that game's **Immersive Game FOV**, which then stays as it is. A message shows the value, and you can change it like any other setting. Choose **Once** again to measure again. With **Off**, **Immersive Game FOV** (60 degrees by default) is used as you set it. It is also used until the camera is found, and in a game where it is not found. Both bumpers and **View** (or a tap of the left stick click) show or hide the bottom screen. Holding the right stick click for 1 second re-centres the view.

The screen fills your view, so you see no edge around it wherever you look; the HUD at its edges sits towards the edge of the lenses. Make the screen smaller to bring the HUD in. Squeeze a grip and move your hand up or down to move it. The stick of that hand moves it farther or nearer (up or down) and makes it bigger or smaller (right or left), up to 2.5 times the starting size. The world stays where it is while you move the screen, and each game keeps it where you left it. Hold the right stick click for 1 second to put it back; that also re-centres the view. SteamVR's recenter (hold the Steam button) only re-centres the view and leaves the screen as it is.

If your eyes see one side of the immersive screen but not the other, keep a grip squeezed and press D-pad left or right. Each press shifts the screen half a degree, and **Offset Right 1.5°** (for example) shows for 2 seconds. This offset is for your lenses, so it is the same in every game and every immersive mode, and putting a game's screen back does not change it. The world stays where it is; only the screen moves. **Immersive Offset** in the **VR** group shows it, and **Reset** there puts it back to the middle.

A tap of the right stick click swaps to the bottom screen, which pauses immersive mode until you tap again. [IMMERSIVE.md](../src/frame_xr/IMMERSIVE.md) has the details and the tuning variables.

In the Full modes the screen starts 3 m ahead and shows the game at its own scale, so nothing is magnified and the world keeps its shape. Two more settings in the same group, per game, for the Full modes, both changing at once:

- **Wider View** (**Off**, **1.1x** to **1.4x**, 1.4x by default): shows more of the game around the edges. Some games leave them empty; Super Mario 3D Land can below the floor. In VR, keep a grip on the immersive screen and press D-pad up or down: the screen grows with the view, and **Wider View: 1.2x** (for example) shows. It is kept for the game.
- **Narrow Screen** (**Off** to **30%** in 5% steps, 10% by default): makes the screen narrower at the same height, so the HUD at the sides moves into view. The game looks that much thinner. Turning your head left and right turns the game camera a little more than up and down does, so the middle of the picture stays in place. In VR, squeeze both grips and press D-pad right (narrower) or left (wider). Down and up do the same for the height (**Narrow Height**). A label shows the value, and it is kept for the game.

The other settings of the group are for **Full (6DoF): Other Games**: **Depth Factor Multiplier** (how far the game camera moves with your head, and how far apart your eyes are in the game, 1x by default; raise it when the 3D is too weak, then use **3D Depth**), **View Matrix Register** and **Eye Indicator Register** (both found while you play when left on auto).

### Hide Bottom Screen

Some games do not need the bottom screen. In a game's properties, on the **Graphics** tab, in the **VR** group, tick **Hide Bottom Screen** to show only the top screen. Hold both bumpers and press **View** to bring the bottom screen back, and again to hide it. A tap of the left stick click does the same. The setting is read when the game starts. [CONTROLS.md](CONTROLS.md) lists the controls for Ocarina of Time 3D with the Ocarina ReFramed single-screen mod, and the settings I use with Single Screen Experience.

### Screen Shape

Some mods draw the top screen widescreen, squeezed into the 3DS's 5:3 picture. In a game's properties, on the **Graphics** tab, in the **VR** group, set **Screen Shape** to match: **Widescreen (16:9)** or **Ultrawide (21:9)** for a wider top screen, or **Standard (4:3)**, **3:2** or **16:10** for game cheats and mods that change the game's own aspect. The top screen keeps its height and its width follows the shape. The bottom screen stays where it was. It is also the shape of the screen in immersive mode. A change shows at once, also from the in-game menu, so you can match a mod's own aspect setting while you play. It must match what the game renders, or the picture stretches.

## Performance

The 3DS runs at 60 frames a second, and every one is shown on each refresh of the headset.
The first time a game draws a new effect, the emulator compiles its shaders and the picture can hitch. They are kept in `~/.local/share/AzaharFrame/user/shaders`, so it happens once. While a game waits for them, a small **Preparing Shaders** badge with a progress ring shows in the top-right corner of the main screen (in immersive view, at the top right of what you see; in Flat, in the status bar). It stays for a moment after the wait, so a game that stops for shaders again soon after keeps the same badge.
If a game drops frames, lower the internal resolution under **Emulation > Configure > Graphics**.
**Show Stats**, or the **Stats** button above Azahar's window, shows the game's frame rate and the headset's late frames while you play.
Keep other heavy work off the Frame while you play.

### Memory

Azahar runs under a memory limit, sized from the memory free when it starts. A texture pack that fills memory closes Azahar instead of freezing the Frame, and the next start says **Azahar Ran Out of Memory**. Before that, **Running Low on Memory** shows how many GB Azahar uses of its limit. Near the limit, the game pauses and offers **Turn Off Custom Textures**, **Resume** or **Stop Game**. The usual cause is a large PNG pack, or **Preload** on a pack that doesn't fit: turn off **Preload**, or use **Optimize for Frame** on the pack.

If a game takes too long to start, for example while a pack preloads, hold **Menu** or **View** for 3 seconds to stop it.

## Files

| What | Where |
|---|---|
| Program and launcher | `~/Games/AzaharFrame/app` |
| Steam library art and icons | `~/Games/AzaharFrame/app/art` |
| Dock icon files | `~/.local/share/applications/azaharframe.desktop`, `azaharframe.png` and `azahar.png` in `~/.local/share/icons/hicolor`, `azaharframe.vrmanifest` in the program folder |
| Icon cache of the old VR picker (`AZAHARFRAME_VR_PICKER=1`) | `~/.cache/azaharframe/icons` |
| Settings, saves, keys, shader caches | `~/.local/share/AzaharFrame/user` |
| Immersive screen places, per game | `~/.local/share/AzaharFrame/user/immersive-placement.txt` |
| Screen places, per game | `~/.local/share/AzaharFrame/user/screen-placement.txt` |
| Gamepad and mouse layouts | `~/.local/share/AzaharFrame/user/controller-mappings.txt` |
| Cheats, per game | `~/.local/share/AzaharFrame/user/cheats/<TitleID>.txt` |
| Save states, and the moment before the last load, for Undo Load (`<TitleID>.98.cst`, removed when the game closes) | `~/.local/share/AzaharFrame/user/states` |
| Save data and extdata copies made by Load Anyway and Restore, per game (last 3) | `~/.local/share/AzaharFrame/user/save-backups/<TitleID>` |
| Mods and texture packs from the Mods tab | `~/.local/share/AzaharFrame/user/load/mod-library`, `load/texture-packs` |
| Launcher log | `~/.local/share/AzaharFrame/launch.log` |
| Emulator log | `~/.local/share/AzaharFrame/user/log/azahar_log.txt` |
| Saved crash reports | `~/Documents/AzaharFrame Reports` |
| What a run that ended badly left (read and removed at the next start) | `~/.local/share/AzaharFrame/user/crash-reports` |
| Settings for one launch | `~/.local/share/AzaharFrame/session.env` |
| Mode it starts in (`vr` or `flat`) | `~/.local/share/AzaharFrame/mode` |
| Texture pack encoder, when a build has none of its own | `~/.local/share/AzaharFrame/tools` |
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
- **A gamepad's touchpad does nothing.** Turn Steam Input off for that controller.
- **Azahar Ran Out of Memory.** Turn off **Preload** for the texture pack, or use **Optimize for Frame** on it. See [Memory](#memory).
- **A downloaded mod or texture pack does not show.** Restart the game.
- **One edge of the immersive screen is cut off by the lenses.** Squeeze a grip and press the D-pad away from that edge until it shows: right when the left edge is cut off.

## Crash reports

If Azahar closed badly last time, the next start tells you why: **Ran Out of Memory**, **Graphics Driver Stopped Responding**, **The Game Hit a Fatal Error**, **Azahar Crashed**, or **Closed Unexpectedly** when I can't tell (the headset froze, or Azahar was killed). It also names the game that was running. It shows once per crash.

You can type what you were doing in **What Were You Doing?** In VR, tapping the box opens the keyboard; **Done** puts the text in the box. Then:

- **See What's Sent** shows the whole report, word for word.
- **Save Report** saves it as a zip in `~/Documents/AzaharFrame Reports`, so you can attach it to an issue.
- **Send Report** sends it to me through [Sentry](https://sentry.io), a crash reporting service. It only sends when you tap it, and only that one report.
- **Don't Send** closes it. Nothing is kept.

A report holds the reason, the game's name and title ID, Azahar for Frame's version, SteamOS's version, your graphics chip and driver, the main graphics and VR settings, the last 2,000 lines of the emulator log, what you typed, and for a crash the signal, the stack (memory addresses and the names of the program's own files) and the build IDs of those files. The log names the games you started and the folders they are in, so a report can too. Your home folder is shown as `~`, your user name as `<user>`, and IP and MAC addresses in the log as `<ip>` and `<mac>`. The report holds no account. Sentry sees the address a report comes from, like any website does; I've set it not to store IP addresses. Azahar for Frame doesn't send anything in the background.

### Report a Problem

For anything that isn't a crash, use **Report a Problem**: in VR it's in the menu under **Settings**, **General**, **Help**; in Flat it's under **Help**. Type what happened in **What Happened?** (Send Report needs it). Tick **Include a Screenshot** to add the game's top and bottom screens as they are now (never the VR view or the room). If the game is paused, it runs for one frame to take it. Then **See What's Sent**, **Save Report** or **Send Report**, as above; **Cancel** closes it. **About Azahar for Frame** is next to it in VR: the version, the GitHub and Ko-fi links, and the credits.

## Reporting a problem

**Report a Problem** (above) is the quickest way. You can also attach `launch.log` and `user/log/azahar_log.txt` from `~/.local/share/AzaharFrame` to an [issue](https://github.com/simplysanatori/AzaharFrame/issues).
They hold the paths of your games, which include your user name, the names of the games you started, your computer's processor, graphics card and system version, your headset's name, and the settings you use. They hold no network addresses. Read them before sharing and replace anything you want to keep private.
If the same thing happens in Azahar on a PC, report it to [Azahar](https://github.com/azahar-emu/azahar/issues) instead.

## Support upstream

Support upstream first: star [CitraVR](https://github.com/amwatson/CitraVR) and [Azahar](https://github.com/azahar-emu/azahar). Azahar for Frame's changes are not offered to them.
This is my personal, unofficial fork; the Azahar team and the CitraVR author did not make or endorse it. If you're one of Azahar's maintainers and want the repo renamed or changed, open an issue or a pull request. I'll happily do it.
If you find a bug that also happens in Azahar on a PC, report it to [Azahar](https://github.com/azahar-emu/azahar/issues).

## Credits and notices

- [CitraVR](https://github.com/amwatson/CitraVR) by amwatson: the VR port this is built on
- [Azahar](https://github.com/azahar-emu/azahar) and [Citra](https://github.com/citra-emu/citra): the emulator
- The Azahar logo is by angyartanddraw and PabloMK7 (CC BY 4.0). The artwork backgrounds are SteamGridDB uploads by GarfieldGaming0624
- [astcenc](https://github.com/ARM-software/astc-encoder) by Arm: the texture pack encoder, part of the program
- [Frametop](https://github.com/Frametop/frametop) by DeeJanuz (MIT): the 3D mouse pointer is adapted from its pointer helper
- [Sharkive](https://github.com/FlagBrew/Sharkive) by FlagBrew (GPL-3.0), with the 3DS codes kept by JourneyOver and its contributors, and my [3ds-game-fixes](https://github.com/simplysanatori/3ds-game-fixes) (MIT): the cheats
- Mods and texture packs come from their authors' own pages, under their own licences: Ocarina ReFramed by OTPR26, Project Restoration by leoetlino, Single Screen Experience by M-1, packs by Henriko Magnifico, DeathWrench, Gray-Rice, Ullr8 and Volya, and others the lists name
- Valve: SteamVR and the Frame

Full list: [THIRD_PARTY_NOTICES.md](../THIRD_PARTY_NOTICES.md). Artwork sources: [steam-frame/art/CREDITS.md](../steam-frame/art/CREDITS.md). CitraVR's README: [README.upstream.md](../README.upstream.md). Unofficial fan project, not affiliated with Nintendo, Valve, the Azahar team or the CitraVR author. Nintendo 3DS is a trademark of Nintendo.
