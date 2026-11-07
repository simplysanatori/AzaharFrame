# Azahar for Frame 1.0.0

Nintendo 3DS games in VR, running natively on the Valve Steam Frame. Azahar for Frame is the Azahar emulator with the VR modes of amwatson's CitraVR, built for SteamVR and the Frame controllers. You need your own games; no game data is included.

### Compared with CitraVR through Lepton

Until now I played 3DS games in VR on the Frame with CitraVR's Android APK, through Lepton.

- It runs natively on the Frame. There is no Android layer, APK or adb.
- It starts in under a second.
- It does far less work for each frame. In Ocarina of Time 3D at 120 Hz with a 4K texture pack, SteamVR re-projects far fewer frames, and each frame costs much less CPU and GPU time.
- Less work for the chip means better battery life.
- Texture packs load as ASTC with no stutter. CitraVR loads PNGs and stalls while it decodes them.

### The emulator on the Frame

- A native OpenXR app in the Frame's SteamVR, at 120 Hz, or 90 Hz if you prefer.
- The release brings its own Qt and libraries. It runs on SteamOS Stable, Beta and Preview.
- Azahar's own window floats in front of you: the game list, the menus and every setting. The beam is the mouse.
- A keyboard opens under the window when you click a text field.
- **Window Scale** makes the window bigger in VR: 100%, 150% or 200%. 150% is the default.
- It starts with Azahar's dark theme.
- Settings, saves, keys and shader caches live in one folder, `~/.local/share/AzaharFrame`.

### The screens

- Grip a screen to move both. The stick of that hand moves them nearer or farther.
- Tap the right stick click to swap the screens. Hold it for a second to put them back.
- Tap the left stick click to hide or show the other screen.
- **Hide Bottom Screen**, per game, for games that do not need it. Both bumpers and View bring it back.
- **Screen Shape**, per game: 5:3, 16:9, 21:9, 4:3, 3:2 or 16:10, for mods and cheats that change the game's aspect. A change shows at once.
- **Background**: black, or passthrough to see the room. The in-game menu has a button for it.

### Immersive mode

- Grip the immersive screen and move your hand to raise or lower it. The stick of that hand moves it nearer or farther, and makes it bigger or smaller.
- Each game keeps the screen where you leave it.
- The default size keeps the HUD at the sides in the sharp middle of the lenses. You can make it up to 2.5 times bigger.
- **Wider View**, per game, shows more of the game around a bigger screen.
- **Squeeze Width**, per game, makes the screen 10 or 20% narrower. The HUD at the sides moves into view.
- **Immersive Game FOV**, per game, for mods that change the game's field of view. A change shows at once.
- Mario Kart 7, Super Mario 3D Land and Asphalt 3D start in Full (6DoF) mode, set up for each game.

### Controls

- The Steam Frame controllers, with the real D-pad.
- The face buttons go by position, as on a 3DS. **Face Buttons** switches them to by label.
- A trigger aimed at the bottom screen is a touch. Anywhere else it is ZL or ZR. Each hand decides on its own.
- Hold View, then the right bumper, to Quick Save. Hold View, then the left bumper, to Quick Load.
- Hold View, then pull the right trigger, to turn Fast Forward on or off. "Fast Forward" or "Normal Speed" shows for 2 seconds.
- Hold Menu for a second to open the in-game menu: Azahar's window over the paused game. It has Quick Save, Quick Load and Passthrough buttons.
- Hold View for 1.5 seconds to stop the game.
- The right controller is the 3DS's gyroscope, for games that aim by moving the 3DS.
- A **Controls** panel lists every control, named from your own controls profile.
- A **Stats** panel shows the frame rate, the emulation speed and the frame times.
- The Frame controllers show in your hands, with soft beams that fade when you put them down.
- The layout is a controls profile in Azahar's window, so you can change it.

### VR and Flat

- Press both bumpers, View and Menu to switch between VR and a flat window. The game keeps running.
- In Flat, Azahar's window sits on SteamVR's big screen, the theater. It stays there when you close the dashboard.
- In Flat the controllers work as in VR, with the same holds and combos. A tap of View or Menu is still Select or Start.
- The next start uses the mode you used last.

### Texture packs

- **Optimize Texture Pack for Frame** in the game list converts a PNG pack to ASTC. The original stays as it is.
- **Use Original Texture Pack** switches back.
- Packs that ship DDS or KTX files keep their own mip levels.

### Install

- An installer that adds the library entry with its art, names the SteamVR dock entry, and sets SteamVR to 120 Hz with no motion smoothing. It sets them again at every launch.
- `install.sh --dry-run` shows what it would do first.
- Installing again updates the same entry. Your settings, saves and keys stay.
- It installs and updates through Frametanium and FrameDrop too.
- An uninstaller, with `--purge` for your data.

### From Azahar and CitraVR

- The emulation, the game list and the settings are Azahar 2127.0-alpha4's, and the 3D screens and immersive modes are CitraVR's. Thanks to the Azahar team and amwatson.

## Download

- `azaharframe-1.0.0-linux-arm64.tar.gz`: the release, for `install.sh` and Frametanium
- `azaharframe-1.0.0-framedrop-linux-arm64.zip` and `azaharframe.framedrop.json`: the same release, for FrameDrop

## Install

Download the tarball and run `steam-frame/install.sh` from the repository with it as the argument. You can also add `simplysanatori/AzaharFrame` in Frametanium, or [install with FrameDrop](https://framedropvr.com/install?manifest=https%3A%2F%2Fgithub.com%2Fsimplysanatori%2FAzaharFrame%2Freleases%2Flatest%2Fdownload%2Fazaharframe.framedrop.json). An update keeps your settings, saves and keys. The [README](https://github.com/simplysanatori/AzaharFrame/blob/steam-frame/README.md) has the steps and the main controls. The [user guide](https://github.com/simplysanatori/AzaharFrame/blob/steam-frame/docs/USER_GUIDE.md) has how to add your games, every control and every setting.

## Known

- The 3DS camera isn't available.
- Mario Kart 7's menus show white panels in Full (6DoF): Other Games mode.
- Pressing the headset's volume key can crash SteamOS's compositor. That is a SteamOS bug.

## Reporting problems

Open an issue with what you did, what you expected and what happened, and attach `launch.log` and `user/log/azahar_log.txt` from `~/.local/share/AzaharFrame`. They hold the paths of your games, which include your user name. Read them first.
