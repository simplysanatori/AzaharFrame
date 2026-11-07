# Azahar for Frame

Nintendo 3DS games in VR, running natively on the Valve Steam Frame.
It is the [Azahar](https://github.com/azahar-emu/azahar) emulator with the VR mode from amwatson's [CitraVR](https://github.com/amwatson/CitraVR).

![Azahar for Frame](docs/images/cover.jpg)

## Features

- Stereoscopic 3D top screen and touch bottom screen, floating in front of you
- Steam Frame controllers, with the real D-pad
- Azahar's own window in VR: game list, menus, settings
- Flat mode in the same app, in a normal window
- Texture packs, mods and a converter for HD packs
- Immersive mode for Ocarina of Time 3D and Majora's Mask 3D (experimental)

## How it works

The emulator runs on the Frame itself. It draws both eyes through SteamVR. Nothing is streamed from a PC. You supply your own games.

## Requirements

- A Steam Frame with SteamOS and SteamVR, and about 500 MB free
- Your own 3DS games, dumped from your own console
- A terminal on the Frame: SSH, or Konsole
- Steam running while you install

## Install

The first release is not out yet. These steps are for it.

1. Get the repository:

   ```sh
   git clone -b steam-frame https://github.com/simplysanatori/AzaharFrame.git
   cd AzaharFrame
   ```

2. Download `azaharframe-1.0.0-linux-arm64.tar.gz` and `SHA256SUMS` from [Releases](https://github.com/simplysanatori/AzaharFrame/releases). Check and install:

   ```sh
   (cd ~/Downloads && sha256sum -c --ignore-missing SHA256SUMS)
   steam-frame/install.sh ~/Downloads/azaharframe-1.0.0-linux-arm64.tar.gz
   ```

3. Launch **Azahar for Frame** from your Steam library.

You can also install with [Frametanium](https://github.com/Cogential/frametanium): add `simplysanatori/AzaharFrame`.

To build it yourself (about an hour on the Frame), from the repository:

```sh
steam-frame/build.sh 1.0.0
steam-frame/install.sh
```

## First run

1. Point at the empty game list in Azahar's window and double-click it with the trigger.
2. Select your game folder.
3. Double-click a game to start it.

Games, keys for encrypted dumps and texture packs: [docs/USER_GUIDE.md](docs/USER_GUIDE.md).

## Controls

| Steam Frame | 3DS |
|---|---|
| Left stick, right stick, left D-pad | Circle Pad, C-Stick, D-pad |
| **A**, **B**, **X**, **Y** | 3DS B, A, Y, X (by position) |
| Bumpers, triggers | L and R, ZL and ZR. A trigger aimed at the bottom screen is a touch |
| Grips | Move the screens |
| **Menu** | Start. Hold 1 second: pause and open Azahar's window |
| **View** | Select. Hold 1.5 seconds: stop the game |
| Left stick click, right stick click | Home, reset the screen positions |
| Both bumpers + **View** | Show or hide the bottom screen |
| Both bumpers + **View** + **Menu** | Switch between VR and Flat |
| **Y** on Azahar's window | Open the keyboard |

Every control and setting: [docs/USER_GUIDE.md](docs/USER_GUIDE.md) and [docs/CONTROLS.md](docs/CONTROLS.md).

## Update

Quit the emulator, then install the new tarball the same way.

```sh
steam-frame/install.sh ~/Downloads/azaharframe-<version>-linux-arm64.tar.gz
```

## Uninstall

Add `--purge` to also delete your settings, saves and keys. Your games are never touched.

```sh
steam-frame/uninstall.sh
```

## Troubleshooting

- **Steam's debugging port is not reachable.** Start Steam and run `install.sh` again.
- **The dock shows a question mark.** Restart Steam once.
- **The screens do not appear.** Start SteamVR first, then Azahar for Frame.
- **An encrypted game does not start.** It needs `aes_keys.txt` from your own console.

More: [docs/USER_GUIDE.md](docs/USER_GUIDE.md#troubleshooting).

## Reporting a problem

Attach `launch.log` and `user/log/azahar_log.txt` from `~/.local/share/AzaharFrame` to an [issue](https://github.com/simplysanatori/AzaharFrame/issues). They hold your user name, game names and hardware details. Read them before sharing.

## More

- [User guide](docs/USER_GUIDE.md)
- [AGENTS.md](AGENTS.md), for developers and AI agents
- [Changelog](docs/CHANGELOG.md)

## Support

CitraVR's author takes donations: [buy them a beer](https://www.buymeacoffee.com/fewerwrong).
If you would like more Steam Frame ports, you can also [buy me a coffee](https://ko-fi.com/sana748653).

## Credits

- [CitraVR](https://github.com/amwatson/CitraVR) by amwatson: the VR port this is built on
- [Azahar](https://github.com/azahar-emu/azahar): the emulator
- Valve: SteamVR and the Frame

Unofficial fan project, not affiliated with Nintendo, Valve, the Azahar team or the CitraVR author. Full list: [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).

## Licence

GPLv3 or any later version. See [LICENSE.txt](LICENSE.txt).

## How this was made

I started this project and used AI (Claude Opus 5.5) to speed up development. I write, review and fix code by hand all the way through, keep the AI's work checked and reined in, and aim to keep the port as lightweight as possible. I test it on my own Steam Frame.
