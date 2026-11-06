# CitraFrame

Nintendo 3DS games in VR, running natively on the Valve Steam Frame.

CitraFrame is a fork of amwatson's [CitraVR](https://github.com/amwatson/CitraVR), the 3DS emulator for the Meta Quest, which is built on [Azahar](https://github.com/azahar-emu/azahar). CitraVR runs on Android. CitraFrame builds the same emulator as a Linux Arm64 program that runs in the Frame's own SteamVR. You do not need Lepton, an APK, adb or a PC.

You need your own games. No game data is in this repository or its releases.

<!-- VERIFY: this README describes the first release; every statement that depends on the XR code is marked and gets checked against the finished build before publishing. -->

## Status

Version 1.0.0, the first release. Two floating screens in front of you: the top screen in stereoscopic 3D and the bottom screen as a touch screen you point at. A game picker in VR starts your games, and the emulator's settings are in its normal desktop window on the SteamVR dashboard. Ocarina of Time 3D and Majora's Mask 3D also have an immersive mode (below).

<!-- VERIFY: status wording, after the first full session. -->

**Compared with CitraVR on the Quest.** The Quest app is the Android build of the emulator with a VR layer on top. Here the emulator is the desktop build on Arm64 Linux, drawn with Vulkan straight onto SteamVR's device, so Azahar's own settings window, key files and controller options are all there. The Frame controllers and their D-pad are mapped to the 3DS, and games start from a picker in VR with no phone app or file picker.

<!-- VERIFY: check each claim in the paragraph above after the headset session. -->

Known issues:

- Immersive mode only exists for Ocarina of Time 3D and Majora's Mask 3D, and it is experimental.
- The 3D slider is held on while you are in VR. A game sees it at one fixed depth.

<!-- VERIFY: the three known issues -->

## What this adds over CitraVR

CitraVR's changes to the emulator are kept as they are: the stereo 3D slider forced on, the immersive mode in the vertex shader and the view matrix hook in the rasterizer. On top of that:

- A native Linux Arm64 build with the Qt window and Azahar's Vulkan renderer, one process, no Android layer.
- The 3DS's screens drawn into SteamVR's own swapchains: the top screen once for each eye, and the bottom screen.
- A VR game picker with icons and titles from your game folders.
- The Steam Frame controllers, with the real D-pad, in the layout CitraVR uses on the Quest.
- Immersive mode for Ocarina of Time 3D and Majora's Mask 3D, with the head pose fed to the game camera.
- A launch wrapper that finds SteamVR's OpenXR runtime, keeps all of the emulator's files in one folder, and an installer with a Steam library entry.

<!-- VERIFY: whole list -->

## Requirements

- A Steam Frame with SteamOS and SteamVR, and about 500 MB free, plus your games
- Your own 3DS game files, decrypted, or encrypted ones with the keys from your own 3DS (see [Games](#games))
- A terminal on the Frame: SSH, or Konsole (SteamVR dashboard, Launch a program, Desktop)
- Steam running while you install

## Install

1. Get the repository:

   ```sh
   git clone https://github.com/simplysanatori/CitraFrame.git
   cd CitraFrame
   ```

2. Download `citraframe-1.0.0-linux-arm64.tar.gz` and `SHA256SUMS` from [Releases](https://github.com/simplysanatori/CitraFrame/releases). Check and install:

   ```sh
   sha256sum -c --ignore-missing SHA256SUMS
   steam-frame/install.sh ~/Downloads/citraframe-1.0.0-linux-arm64.tar.gz
   ```

3. Launch **CitraFrame** from your Steam library.

Another installer: [Frametanium](https://github.com/Cogential/frametanium), add `simplysanatori/CitraFrame`.

## Games

Put your games in `~/Documents/Roms/3ds/` (or any folder; you pick the folders on the first run). The emulator plays `.cci`, `.3ds`, `.cxi`, `.cia` and `.3dsx` files.

- Dump your games from a 3DS you own. Decrypted dumps work as they are.
- Encrypted dumps need the keys from the same 3DS. Put `aes_keys.txt` in `~/.local/share/CitraFrame/user/sysdata/`.

## First run

1. Launch **CitraFrame** from the library. The picker is empty the first time.
2. Open the CitraFrame window on the SteamVR dashboard. It is Azahar's normal window.
3. In it, choose **File > Add New Game Directory** and select your game folder.
4. Back in VR, point at **Rescan** in the picker. Your games appear. Point at one and pull the trigger.

<!-- VERIFY: steps 2 to 4 -->

## Controls

| Steam Frame | 3DS |
|---|---|
| Left stick | Circle Pad |
| Right stick | C-Stick |
| Left D-pad | D-pad |
| **A**, **B**, **X**, **Y** | A, B, X, Y |
| Left and right grip | L and R |
| Left and right trigger | ZL and ZR |
| Trigger while aiming at the bottom screen | Touch |
| Trigger while aiming at the top screen | Drag the screen. The stick moves it nearer and farther |
| **Menu** | Start |
| **View** | Select. Hold for 1.5 seconds to go back to the picker |

All controls: [docs/CONTROLS.md](docs/CONTROLS.md).

<!-- VERIFY: table matches src/frame_xr/input/MAPPING.md -->

## Settings

Everything is in Azahar's desktop window on the SteamVR dashboard: **Emulation > Configure**. Internal resolution, the filters, the controls dialog and the audio are the ones you know from Azahar. The 3D slider is held on while you are in VR. Per-game settings work as in Azahar: right-click a game in the list and choose **Properties**.

The immersive mode switches for Ocarina of Time 3D and Majora's Mask 3D are in the same place, in each game's properties.

<!-- VERIFY: where the immersive settings are shown in the Qt dialog -->

## Immersive mode

Normally the top screen floats two metres away. In immersive mode the screen wraps the view, so the game's own world is around you. It follows your head, with a different offset for each eye. Only Ocarina of Time 3D and Majora's Mask 3D have the settings it needs. It is experimental.

<!-- VERIFY: the whole section, gate H2b -->

## Performance

The 3DS runs at 60 frames a second. The Frame shows every one of them on each of its refreshes. The first time a game draws something new, the emulator compiles its shaders and the picture can hitch. It keeps them in a disk cache in `~/.local/share/CitraFrame/user/shaders`, so it happens once per effect. Higher internal resolution sharpens the screens and costs GPU time. If a game runs short of frames, lower it in **Emulation > Configure > Graphics**.

Keep other heavy work off the Frame while you play. A compile or a download will make it stutter.

<!-- VERIFY: cache path, setting names -->

## Files

| What | Where |
|---|---|
| Program and wrapper | `~/Games/CitraFrame/app` |
| Settings, saves, keys, shader caches, logs | `~/.local/share/CitraFrame/user` |
| The wrapper's log | `~/.local/share/CitraFrame/launch.log` |
| The emulator's log | `~/.local/share/CitraFrame/user/log/citravr_log.txt` |
| One launch's test settings | `~/.local/share/CitraFrame/session.env` |

Your game files stay where you keep them.

## Update

Download the new tarball and install it. Your settings, saves and keys stay.

```sh
steam-frame/install.sh ~/Downloads/citraframe-<version>-linux-arm64.tar.gz
```

## Uninstall

Quit CitraFrame first. Add `--purge` to also delete the settings, saves, keys and shader caches. Your game files are never touched.

```sh
steam-frame/uninstall.sh
```

## Troubleshooting

- **`install.sh` says Steam's debugging port is not reachable.** Start Steam and run it again.
- **The screens do not appear in the headset.** Start SteamVR first, then CitraFrame. `launch.log` says which OpenXR runtime it found.
- **The CitraFrame window is too small on the dashboard.** Add `QT_SCALE_FACTOR=1.25` to `~/.local/share/CitraFrame/session.env`.
- **An encrypted game does not start.** The emulator needs `aes_keys.txt` in `~/.local/share/CitraFrame/user/sysdata/`.
- **Motion ghosts.** In SteamVR, check 120 Hz and motion smoothing off. The launcher sets both for its own library entry.

<!-- VERIFY: each entry -->

## Reporting a problem

Open an [issue](https://github.com/simplysanatori/CitraFrame/issues) with what you did, what you expected and what happened, and attach `launch.log` and `user/log/citravr_log.txt` from `~/.local/share/CitraFrame`. They hold file paths and the names of your games. They hold no account names or network addresses. Check them before sharing. If the same thing happens in Azahar on a PC, report it to [Azahar](https://github.com/azahar-emu/azahar/issues) instead.

## Build from source

See the [technical notes](docs/TECHNICAL_NOTES.md#building). On the Frame:

```sh
steam-frame/build.sh 1.0.0       # builds and packages steam-frame/release/citraframe-1.0.0-linux-arm64.tar.gz
```

## Support

Support upstream first: star [CitraVR](https://github.com/amwatson/CitraVR) and [Azahar](https://github.com/azahar-emu/azahar). CitraFrame's changes are not offered to them.

If you would like more Steam Frame ports, you can also [buy me a coffee](https://ko-fi.com/sana748653).

## Credits

- Built by [simplysanatori](https://github.com/simplysanatori).
- [CitraVR](https://github.com/amwatson/CitraVR) by amwatson: the VR port this is built on.
- [Azahar](https://github.com/azahar-emu/azahar) and [Citra](https://github.com/citra-emu/citra): the emulator.
- Valve, for SteamVR and the Frame.

Full list: [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md). Unofficial fan project, not affiliated with Nintendo, Valve, the Azahar team or the CitraVR author. Nintendo 3DS is a trademark of Nintendo.

## Licence

GPLv3 or any later version, as upstream. See [LICENSE.txt](LICENSE.txt). Third-party parts keep their own licences. CitraVR's own README is kept as [README.upstream.md](README.upstream.md).

## How this was made

I made this with partial assistance from Claude.
