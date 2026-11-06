# Azahar for Frame

Nintendo 3DS games in VR, running natively on the Valve Steam Frame.
It is the [Azahar](https://github.com/azahar-emu/azahar) emulator with the VR mode from amwatson's [CitraVR](https://github.com/amwatson/CitraVR).

![Azahar for Frame](docs/images/cover.jpg)

The emulator runs on the Frame itself, inside SteamVR. Azahar draws the 3DS screens with Vulkan straight into SteamVR, so nothing is streamed from a PC. You supply your own games.

## Status

I build it from source on the Frame. The first release is not out yet, and the install steps below are written for it.
The parts that depend on the VR code are checked against the finished build before it is published.

<!-- VERIFY: everything below that depends on the XR code is checked against the finished build before publishing. -->

Known issues:

- Immersive mode only has settings for Ocarina of Time 3D and Majora's Mask 3D. It is experimental.
- The 3D slider is held on while you are in VR.

<!-- VERIFY: both items -->

The changes are listed in [docs/CHANGELOG.md](docs/CHANGELOG.md).

## What this adds

Compared with CitraVR on the Quest:

- It runs as a Linux program on the Frame. There is no Lepton, APK, adb or PC.
- It draws with Vulkan into SteamVR through OpenXR.
- It uses the Steam Frame controllers, with the real D-pad where CitraVR needed a trick.
- It has a game picker in VR, with the icons and titles from your folders.
- Azahar's own settings window is on the SteamVR dashboard.
- Immersive mode runs on SteamVR.

Compared with Azahar on a PC:

- There is a stereoscopic 3D top screen and a bottom touch screen, floating in front of you.
- You aim at the screens with a controller.
- The 3D slider is held on.

## Requirements

- A Steam Frame with SteamOS and SteamVR, and about 500 MB free, plus your games
- Your own 3DS games, dumped from your own console. Decrypted dumps need nothing else. Encrypted ones need the keys from the same console (see [Games](#games))
- A terminal on the Frame: SSH, or Konsole (SteamVR dashboard, Launch a program, Desktop)
- Steam running while you install

## Install

1. Get the repository:

   ```sh
   git clone -b steam-frame https://github.com/simplysanatori/AzaharFrame.git
   cd AzaharFrame
   ```

2. Download `azaharframe-1.0.0-linux-arm64.tar.gz` and `SHA256SUMS` from [Releases](https://github.com/simplysanatori/AzaharFrame/releases). Check and install:

   ```sh
   sha256sum -c --ignore-missing SHA256SUMS
   steam-frame/install.sh ~/Downloads/azaharframe-1.0.0-linux-arm64.tar.gz
   ```

3. Launch **Azahar for Frame** from your Steam library.

The installer adds the library entry with its artwork and icon, gives SteamVR's dock the name and icon, and sets SteamVR to 120 Hz for it. `install.sh --dry-run` lists everything it would do without doing it. Another installer: [Frametanium](https://github.com/Cogential/frametanium), add `simplysanatori/AzaharFrame`.

### Update

Download the new tarball and install it. Your settings, saves and keys stay.

```sh
steam-frame/install.sh ~/Downloads/azaharframe-<version>-linux-arm64.tar.gz
```

### Uninstall

Quit the emulator first. Add `--purge` to also delete your settings, saves, keys and shader caches. Your games are never touched.

```sh
steam-frame/uninstall.sh
```

## First run

The picker is empty the first time.

1. Open the **Azahar for Frame** window on the SteamVR dashboard.
2. Choose **File > Add New Game Directory** and select your game folder.
3. Back in VR, point at **Rescan**. Your games appear.
4. Point at one and pull the trigger.

<!-- VERIFY: steps 1 to 4 -->

### Games

Put your games in `~/Documents/Roms/3ds/`, or any folder you like. It plays `.cci`, `.3ds`, `.cxi`, `.cia` and `.3dsx` files.

- Decrypted dumps work as they are.
- Encrypted dumps need `aes_keys.txt` from your own console in `~/.local/share/AzaharFrame/user/sysdata/`.

## Controls

| Steam Frame | 3DS |
|---|---|
| Left stick | Circle Pad |
| Right stick | C-Stick |
| Left D-pad | D-pad |
| **A**, **B**, **X**, **Y** | A, B, X, Y |
| Left and right grip | L and R |
| Left and right trigger | ZL and ZR |
| Trigger aimed at the bottom screen | Touch |
| Trigger aimed at the top screen | Drag the screen. The stick moves it nearer and farther |
| **Menu** | Start |
| **View** | Select. Hold for 1.5 seconds to go back to the picker |
| Left stick click | Home |
| Right stick click | Reset the screen positions |

All controls: [docs/CONTROLS.md](docs/CONTROLS.md).

<!-- VERIFY: table against src/frame_xr/input/MAPPING.md -->

## Settings

Everything is in the Azahar window on the SteamVR dashboard, under **Emulation > Configure**. Resolution, filters, controls and audio are the ones you know from Azahar.
Per-game settings work as in Azahar: right-click a game in the list and choose **Properties**.

The 3D slider is held on while you are in VR.

### Immersive mode

Normally the top screen floats two metres away. In immersive mode the game's world wraps around you and follows your head.
Turn it on in the game's properties, on the **Graphics** tab, in the **VR** group. Only Ocarina of Time 3D and Majora's Mask 3D have the settings it needs.
The left bumper shows or hides the bottom screen.

<!-- VERIFY: the whole section -->

## Performance

The 3DS runs at 60 frames a second, and every one is shown on each refresh of the headset.
The first time a game draws a new effect, the emulator compiles its shaders and the picture can hitch. They are kept in `~/.local/share/AzaharFrame/user/shaders`, so it happens once.
If a game drops frames, lower the internal resolution under **Emulation > Configure > Graphics**.
Keep other heavy work off the Frame while you play.

## Files

| What | Where |
|---|---|
| Program and launcher | `~/Games/AzaharFrame/app` |
| Steam library art and icons | `~/Games/AzaharFrame/app/art` |
| Dock icon files | `~/.local/share/applications/azaharframe.desktop`, `azaharframe.png` and `azahar.png` in `~/.local/share/icons/hicolor` |
| Settings, saves, keys, shader caches | `~/.local/share/AzaharFrame/user` |
| Launcher log | `~/.local/share/AzaharFrame/launch.log` |
| Emulator log | `~/.local/share/AzaharFrame/user/log/azahar_log.txt` |
| Settings for one launch | `~/.local/share/AzaharFrame/session.env` |

How the port works: [docs/TECHNICAL.md](docs/TECHNICAL.md).

## Troubleshooting

- **Steam's debugging port is not reachable.** Start Steam and run `install.sh` again.
- **SteamVR's dock shows a question mark instead of the flower.** Steam reads the icon when it starts. Restart Steam once.
- **The screens do not appear in the headset.** Start SteamVR first, then Azahar for Frame. `launch.log` says which OpenXR runtime it found.
- **The Azahar window is too small on the dashboard.** Add `QT_SCALE_FACTOR=1.25` to `session.env`.
- **An encrypted game does not start.** It needs `aes_keys.txt` in `user/sysdata/`.
- **Motion ghosts.** In SteamVR, check 120 Hz and motion smoothing off.

<!-- VERIFY: each entry -->

## Reporting a problem

Attach `launch.log` and `user/log/azahar_log.txt` from `~/.local/share/AzaharFrame` to an [issue](https://github.com/simplysanatori/AzaharFrame/issues).
They hold file paths and the names of your games. They hold no account names or network addresses. Check them before sharing.
If the same thing happens in Azahar on a PC, report it to [Azahar](https://github.com/azahar-emu/azahar/issues) instead.

## Build from source

On the Frame, with a C++ compiler, CMake 3.24 or newer, Ninja, the Qt 6 and SDL2 development files and Python 3:

```sh
steam-frame/build.sh 1.0.0
steam-frame/install.sh
```

The first build takes about an hour and loads every core. It waits while the headset is in use. `build.sh` packs `steam-frame/release/azaharframe-1.0.0-linux-arm64.tar.gz` and `SHA256SUMS`, and `install.sh` installs that tarball. The options are in [docs/TECHNICAL_NOTES.md](docs/TECHNICAL_NOTES.md#building).

## Support

Support upstream first: star [CitraVR](https://github.com/amwatson/CitraVR) and [Azahar](https://github.com/azahar-emu/azahar). Azahar for Frame's changes are not offered to them.
If you find a bug that also happens in Azahar on a PC, report it to [Azahar](https://github.com/azahar-emu/azahar/issues).
If you would like more Steam Frame ports, you can also [buy me a coffee](https://ko-fi.com/sana748653).

## Credits

- [CitraVR](https://github.com/amwatson/CitraVR) by amwatson: the VR port this is built on
- [Azahar](https://github.com/azahar-emu/azahar) and [Citra](https://github.com/citra-emu/citra): the emulator
- The Azahar logo is by angyartanddraw and PabloMK7 (CC BY 4.0). The artwork backgrounds are SteamGridDB uploads by GarfieldGaming0624
- Valve: SteamVR and the Frame

Full list: [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md). Artwork sources: [steam-frame/art/CREDITS.md](steam-frame/art/CREDITS.md). CitraVR's README: [README.upstream.md](README.upstream.md). Unofficial fan project, not affiliated with Nintendo, Valve, the Azahar team or the CitraVR author. Nintendo 3DS is a trademark of Nintendo.

## Licence

GPLv3 or any later version, same as upstream. See [LICENSE.txt](LICENSE.txt). Third-party parts keep their own licences.

## How this was made

I made this with partial assistance from Claude.
