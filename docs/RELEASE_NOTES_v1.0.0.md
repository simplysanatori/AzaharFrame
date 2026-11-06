# Azahar for Frame 1.0.0

> **DRAFT.** Checked against the finished build before it is published. <!-- VERIFY -->

Nintendo 3DS games in VR, running natively on the Valve Steam Frame. Azahar for Frame is a fork of amwatson's CitraVR, built on Azahar, with its own SteamVR presentation and Frame controls. You need your own games; no game data is included.

### The emulator on the Frame

- A Linux Arm64 build that runs in the Frame's SteamVR. No Lepton, APK, adb or PC.
- Azahar 2127.0-alpha4 with CitraVR's VR changes, drawn with Vulkan straight onto SteamVR's device.
- Azahar's own settings window on the SteamVR dashboard.
- Settings, saves, keys and shader caches in plain folders under your home.

### The screens

- The top screen in stereoscopic 3D and the bottom screen as a touch screen, floating in front of you, as in CitraVR.
- A game picker in VR with the icons and titles from your game folders.
- Drag the top screen and move it nearer and farther. Touch the bottom screen by pointing and pulling the trigger.

### Controls

- The Steam Frame controllers in CitraVR's layout, with the real D-pad.
- The layout is a default profile in Azahar's controls window, so you can change it.

### Immersive mode

- Ocarina of Time 3D and Majora's Mask 3D can wrap the view around you.

### Install

- An installer that copies the program, adds the Steam library entry and sets SteamVR's per-app settings.
- It installs and updates through Frametanium.

## Download

- `azaharframe-1.0.0-linux-arm64.tar.gz`
- `SHA256SUMS`

## Install

Run `steam-frame/install.sh` from the repository with the tarball as its argument. It installs the program and creates the Steam library entry. The [README](https://github.com/simplysanatori/AzaharFrame/blob/steam-frame/README.md) has the steps, how to add your games and the controls.

## Known

- Immersive mode only exists for Ocarina of Time 3D and Majora's Mask 3D, and it is experimental.
- The 3D slider is held on while you are in VR.

<!-- VERIFY: known list -->

## Reporting problems

Open an issue with what you did, what you expected and what happened, and `launch.log` and `user/log/azahar_log.txt` from `~/.local/share/AzaharFrame`.
