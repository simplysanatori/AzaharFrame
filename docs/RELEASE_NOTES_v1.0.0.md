# Azahar for Frame 1.0.0

> **DRAFT.** Checked against the finished build before it is published. <!-- VERIFY -->

Nintendo 3DS games in VR, running natively on the Valve Steam Frame. Azahar for Frame is a fork of amwatson's CitraVR, built on the Azahar emulator, with its own Vulkan output for SteamVR and the Frame controllers. You need your own games; no game data is included.

### The emulator on the Frame

- A Linux Arm64 build that runs in the Frame's SteamVR, where CitraVR is an Android app for the Quest. No Lepton, APK, adb or PC.
- Azahar 2127.0-alpha4 with CitraVR's VR changes, drawn with Vulkan straight onto SteamVR's own graphics device.
- Each screen is handed to SteamVR only once the GPU has finished drawing it.
- Azahar's own settings window on the SteamVR dashboard.
- Settings, saves, keys and shader caches in one plain folder under your home.

### The screens

- The top screen in stereoscopic 3D and the bottom screen as a touch screen, floating in front of you, as in CitraVR.
- A game picker in VR with the icons and titles from your game folders.
- Squeeze a grip on a screen to move it nearer and farther. Touch the bottom screen by pointing and pulling the trigger.
- Hold View to go back to the picker.

### Controls

- The Steam Frame controllers in CitraVR's layout, with the real D-pad where the Quest needed the thumbrest.
- Each trigger decides by itself between ZL or ZR and touch. The grips move the screens, the bumpers are L and R.
- Hide Bottom Screen, per game, for games that do not need it.
- Screen Shape, per game: Widescreen (16:9) or Ultrawide (21:9) top screen for mods that draw widescreen.
- The layout is a profile in Azahar's controls window, so you can change it.

### Immersive mode

- Ocarina of Time 3D and Majora's Mask 3D can wrap the view around you, on a flat screen that follows your head, because SteamVR has no cylinder layer.
- The head pose reaches the game as one snapshot a frame, without the race between threads that CitraVR had.

### Install

- An installer that copies the program, adds the Steam library entry with its artwork and icon, names the SteamVR dock entry, and sets SteamVR's per-app settings.
- `install.sh --dry-run` shows what it would do first.
- It installs, updates and removes through Frametanium.

## Download

- `azaharframe-1.0.0-linux-arm64.tar.gz`
- `SHA256SUMS`

## Install

Download the tarball and run `steam-frame/install.sh` from the repository with it as the argument, or add `simplysanatori/AzaharFrame` in Frametanium. It installs the program and creates the Steam library entry, and keeps your settings, saves and keys on an update. The [README](https://github.com/simplysanatori/AzaharFrame/blob/steam-frame/README.md) has the steps and the main controls. The [user guide](https://github.com/simplysanatori/AzaharFrame/blob/steam-frame/docs/USER_GUIDE.md) has how to add your games and every control.

## Known

- Immersive mode only has settings for Ocarina of Time 3D and Majora's Mask 3D, and it is experimental.
- The 3D depth is set to 50 % when it is 0.

<!-- VERIFY: known list -->

## Reporting problems

Open an issue with what you did, what you expected and what happened, and `launch.log` and `user/log/azahar_log.txt` from `~/.local/share/AzaharFrame`. They hold the paths of your games, which include your user name: read them first.
