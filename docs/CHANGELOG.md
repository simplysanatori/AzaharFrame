# Changelog

> **DRAFT.** Written before the first headset session; every line is checked against the finished build before it is published. <!-- VERIFY -->

## 1.0.0 (November 2026)

The first release of CitraFrame, Nintendo 3DS games in VR running natively on the Valve Steam Frame. It is a fork of amwatson's CitraVR, built on Azahar.

### The emulator on the Frame

- A Linux Arm64 build that runs in the Frame's SteamVR. No Lepton, APK, adb or PC.
- Azahar 2127.0-alpha4, with CitraVR's VR changes to the renderer. Azahar's Vulkan renderer draws straight onto SteamVR's device.
- Azahar's settings window on the SteamVR dashboard, for every setting the emulator has.
- Settings, saves, keys and shader caches in one folder, `~/.local/share/CitraFrame`.

### The screens

- The top screen in stereoscopic 3D, one image for each eye, and the bottom screen as a touch screen, both floating in front of you.
- Drag the top screen with the trigger and move it nearer and farther with the stick. Touch the bottom screen with the trigger.
- A game picker in VR, with the icons and titles from your game folders.

### Controls

- The Steam Frame controllers, with the real D-pad, in CitraVR's layout. Grips are L and R, triggers are ZL and ZR, the left stick is the Circle Pad and the right stick is the C-Stick.
- The layout is a default profile in Azahar's controls window, so you can change it.

### Immersive mode

- Ocarina of Time 3D and Majora's Mask 3D can wrap the view around you, with the head pose fed to the game's camera.

### Install

- An installer that copies the program, adds a CitraFrame entry to your Steam library and sets SteamVR's per-app settings (120 Hz, no throttling, no motion smoothing).
- It installs and updates through Frametanium too. Your settings, saves and keys stay on an update.
