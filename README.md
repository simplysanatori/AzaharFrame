# Azahar for Frame

Nintendo 3DS games in VR, running natively on the Valve Steam Frame.

> This is my personal, unofficial fork of [Azahar](https://github.com/azahar-emu/azahar). It brings over the VR modes of amwatson's [CitraVR](https://github.com/amwatson/CitraVR), which is built on [Citra](https://github.com/citra-emu/citra).
> It is not an official Azahar project. The Azahar team and the CitraVR author did not make or endorse it.
> If you're one of Azahar's maintainers and want the repo renamed or changed, open an issue or a pull request. I'll happily do it.

![Azahar for Frame](docs/images/cover.jpg)

## Features

- Stereoscopic 3D top screen and touch bottom screen, at 90 or 120 Hz
- Immersive modes, with a screen you move and resize per game
- Steam Frame controls, with Quick Save, Quick Load and Fast Forward
- Azahar's own window and settings in VR
- Flat mode in the same app, on SteamVR's big screen
- **Optimize Texture Pack for Frame**: converts HD texture packs so they load with no stutter
- Texture packs and mods

## How it works

The emulator runs on the Frame itself and draws both eyes through SteamVR. Nothing is streamed from a PC.

## Requirements

- A Steam Frame with SteamOS and SteamVR, and about 500 MB free
- SteamOS Stable, Beta or Preview
- Your own 3DS games, dumped from your own console
- A terminal on the Frame: SSH, or Konsole
- Steam running while you install

## Install

1. Get the repository:

   ```sh
   git clone -b steam-frame https://github.com/simplysanatori/AzaharFrame.git
   cd AzaharFrame
   ```

2. Download `azaharframe-1.0.0-linux-arm64.tar.gz` from [Releases](https://github.com/simplysanatori/AzaharFrame/releases) and install it:

   ```sh
   steam-frame/install.sh ~/Downloads/azaharframe-1.0.0-linux-arm64.tar.gz
   ```

3. Launch **Azahar for Frame** from your Steam library.

Other installers:

- [Frametanium](https://github.com/Cogential/frametanium): add `simplysanatori/AzaharFrame`.
- [FrameDrop](https://framedropvr.com): [Install with FrameDrop](https://framedropvr.com/install?manifest=https%3A%2F%2Fgithub.com%2Fsimplysanatori%2FAzaharFrame%2Freleases%2Flatest%2Fdownload%2Fazaharframe.framedrop.json).

Checksums and building: [user guide](docs/USER_GUIDE.md#install-update-and-uninstall).

## First run

1. Double-click the empty game list and select the folder with your games.
2. Double-click a game to start it.

Keys and texture packs: [user guide](docs/USER_GUIDE.md#games).

## Controls

![What each Frame button does in a game](docs/images/controls.png)

| Steam Frame | What it does |
|---|---|
| **A**, **B**, **X**, **Y** | 3DS B, A, Y, X (by position) |
| Hold **Menu** for 1 second | In-game menu |
| Hold **View** for 1.5 seconds | Stop the game |
| Hold **View**, then right or left bumper | Quick Save or Quick Load |
| Hold **View**, then right trigger | Fast Forward on or off |
| Both bumpers + **View** + **Menu** | Switch between VR and Flat |

The **Controls** button above Azahar's window shows this panel in VR. Full list: [docs/CONTROLS.md](docs/CONTROLS.md).

## Settings

The VR settings are in **Emulation > Configure**, on the **Graphics** tab, in the **VR** group. For one game, right-click it and choose **Properties**. [docs/SETTINGS.md](docs/SETTINGS.md) explains each one.

## Update

Quit the emulator, then install the new tarball.

```sh
steam-frame/install.sh ~/Downloads/azaharframe-<version>-linux-arm64.tar.gz
```

## Uninstall

Add `--purge` to also delete your settings, saves and keys. Your games are never touched.

```sh
steam-frame/uninstall.sh
```

## Troubleshooting

- The installer says Steam was not reachable: start Steam and run `install.sh` again.
- The SteamVR dock shows a question mark: restart Steam once.
- The screens do not appear: start SteamVR first, then Azahar for Frame.
- An encrypted game does not start: it needs `aes_keys.txt` in `~/.local/share/AzaharFrame/user/sysdata/`.

More: [docs/USER_GUIDE.md](docs/USER_GUIDE.md#troubleshooting).

## Reporting a problem

Attach `launch.log` and `user/log/azahar_log.txt` from `~/.local/share/AzaharFrame` to an [issue](https://github.com/simplysanatori/AzaharFrame/issues). They hold your user name and game names. Check them before sharing.

## More

- [User guide](docs/USER_GUIDE.md)
- [AGENTS.md](AGENTS.md): for developers and AI agents
- [Building from source](docs/TECHNICAL_NOTES.md#building)
- [Changelog](docs/CHANGELOG.md)

## Support

CitraVR's author takes donations: [buy them a beer](https://www.buymeacoffee.com/fewerwrong).
If you would like more Steam Frame ports, you can also [buy me a coffee](https://ko-fi.com/sana748653).

## Credits

- [Azahar](https://github.com/azahar-emu/azahar), which continues [Citra](https://github.com/citra-emu/citra): the emulator
- [CitraVR](https://github.com/amwatson/CitraVR) by amwatson: the VR modes
- [Qt](https://www.qt.io), [SDL](https://www.libsdl.org), [OpenXR](https://www.khronos.org/openxr/): window, gamepad, VR
- [astcenc](https://github.com/ARM-software/astc-encoder) by Arm: the texture pack encoder
- Azahar logo by angyartanddraw and PabloMK7; backgrounds by GarfieldGaming0624 on [SteamGridDB](https://www.steamgriddb.com/game/5480678)
- Valve: SteamVR and the Frame

Full list: [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md). Not affiliated with Nintendo or Valve.

## Licence

GPLv3 or any later version. See [LICENSE.txt](LICENSE.txt).

## How this was made

I started this project and used AI (Claude Opus 5.5) to speed up development. I write, review and fix code by hand all the way through, keep the AI's work checked and reined in, and aim to keep the port as lightweight as possible. I test it on my own Steam Frame.
