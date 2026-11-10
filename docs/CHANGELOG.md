# Changelog

## 1.1.0 (November 2026)

A new in-game menu, curved screens, a side-by-side layout and live controls for immersive mode.

### The in-game menu

- Holding **Menu** opens a new menu with five tabs: Main, Position, Stats, Controls and Settings. It follows CitraVR's layout, in the style of Android XR.
- **Main**: Continue, Quick Save, Quick Load, Save State and Load State (10 slots, with the time each was saved), Screenshot, Restart, Switch to Flat and Close Game.
- **Position**: Reset Position, Swap Screens, Hide Bottom Screen, Screen Shape, Screen Layout, Screen Curve and Immersive Offset.
- **Stats**: the frame rates and frame times from just before the pause.
- **Controls**: the controller diagram, in game and for Azahar's window.
- **Settings**: the VR settings, for every game or just this one. Rest the beam on a setting to see what it does.
- **Azahar's Window** shows Azahar's own window over the game, for everything else. **B** goes back to the menu.

### The screens

- **Screen Curve**: the main screen can bend toward you like a curved monitor, from Off to 90° in 15° steps. The smaller screen stays flat, and touch still lands where the beam points.
- Hold the screens with a grip and press D-pad up or down to change the curve. D-pad left or right still nudges them. To move them up or down, move your hand.
- **Screen Layout**: **Side by Side** puts the smaller screen to the right of the main one, facing you. **Stacked** keeps it below. Hold the left stick click for 1 second to switch.
- In Flat, the same hold cycles **Large Screen**, **Side by Side** and **Default**. So does a grip held with D-pad left or right.

### Immersive mode

- **Wider View** now has steps per game: Off, 1.1x, 1.2x, 1.3x and 1.4x. It shows more of the game around the edges.
- Hold the immersive screen with a grip and press D-pad up or down to change it as you play. The screen grows with the view, so the picture keeps its scale.
- D-pad left or right still shifts the screen for your lenses. To raise or lower it, move your hand.
- **Squeeze Width** is now **Narrow Screen**, with a new 30% step.

### Controls

- Holding the right stick click for 1 second is the one reset. It puts the screens back and, in immersive mode, re-centres the view.
- Quick Load needs the left bumper held for half a second, so a brush of the bumper never loads.
- Grabbing the screens shows a short hint of what the D-pad and stick do.

### Fixed

- In Flat, dialogs such as Configure opened squeezed, with OK and Cancel off the bottom of the screen. They now fill the big screen and scroll.

## 1.0.0 (November 2026)

The first release of Azahar for Frame: Nintendo 3DS games in VR, running natively on the Valve Steam Frame. It is the Azahar emulator with the VR modes of amwatson's CitraVR.

### Compared with CitraVR through Lepton and similar forks

- It runs natively on the Frame. There is no Android layer, APK or adb.
- It starts in under a second.
- Far less work per frame. In Ocarina of Time 3D at 120 Hz with a 4K texture pack: 0.6 ms of CPU and 1.5 ms of GPU time, against 4.8 ms and 2.7 ms.
- Less work for the chip means better battery life.
- Texture packs load as ASTC with no stutter. CitraVR loads PNGs and stalls while it decodes them.

### The emulator on the Frame

- A native OpenXR app in the Frame's SteamVR, at 120 Hz, or 90 Hz if you prefer.
- The release brings its own Qt and libraries. It runs on SteamOS Stable, Beta and Preview.
- Play in VR on 3D screens, in immersive VR with head tracking, or flat on SteamVR's big screen.
- Settings, saves, keys and shader caches are in one folder, `~/.local/share/AzaharFrame`.

### The screens

- Grip a screen to move both. The stick of that hand moves them nearer or farther, and makes them bigger or smaller, up to 3 times: big enough to fill the whole view.
- While you hold them, the D-pad nudges them half a degree at a time, for a big screen with an edge just out of view.
- Tap the right stick click to swap the screens. Hold it for a second to put them back.
- Tap the left stick click to hide or show the other screen.
- **Hide Bottom Screen**, per game, for games that do not need it. Both bumpers and View bring it back.
- **Screen Shape**, per game: 5:3, 16:9, 21:9, 4:3, 3:2 or 16:10, for mods and cheats that change the game's aspect. A change shows at once.
- **Background**: black, or passthrough to see the room. The in-game menu has a button for it.

### Immersive mode

- Grip the immersive screen and move your hand to raise or lower it. The stick of that hand moves it nearer or farther, and makes it bigger or smaller.
- The screen position is remembered for each game.
- SteamVR's recenter (hold the Steam button) also re-centres the immersive view.
- The default size keeps the HUD within the clear centre of the lenses. You can make the screen up to 2.5 times bigger.
- While you hold it, the D-pad shifts it half a degree at a time. **Immersive Offset** is global, since it corrects for small differences in each user's IPD.
- **Wider View**, per game, shows more of the game around a bigger screen.
- **Squeeze Width**, per game, makes the screen 10 or 20% narrower, to bring HUD elements at the sides into view.
  - In future updates I may extend this, or add game-specific cheats or mods that move the HUD.
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
- A **Controls** panel shows both controllers, with a line from each button to what it does. It has a page for the game and one for Azahar's window.
- A **Stats** panel shows the frame rate, the emulation speed and the frame times.
- The Frame controllers show in your hands, with soft beams that fade when you put them down.
- The layout is a controls profile in Azahar's window, so you can change it.

### VR and Flat

- Press both bumpers, View and Menu to switch between VR and a flat window. The game keeps running.
- In Flat, Azahar's window sits on SteamVR's big screen, the theater. It stays there when you close the dashboard.
- In Flat the controllers work as in VR, with the same holds and combos. A tap of View or Menu is still Select or Start.
- In Flat the window is 16:9, so it fills the big screen with no bars at the sides.
- In Flat, hold Menu for a second to open the Flat menu: the game pauses and the window leaves Fullscreen with its menu bar, so the pointer reaches Azahar's menus and settings. Hold Menu again, press B or select Emulation > Continue to go back.
- In Flat the stick clicks change the window's layout. Tap the right one to swap the screens and the left one to show only one screen. Hold the right one to go back to Large Screen, one big screen and one small, which is where Flat starts. VR keeps your own layout.
- The next start uses the mode you used last.

### Texture packs

- **Optimize Texture Pack for Frame** in the game list converts a PNG pack to ASTC, so it loads with no stutter.
- **Use Original Texture Pack** switches it back to how it was before.
- Packs that ship DDS or KTX files keep their own mip levels.

### Install

- An installer that adds the library entry with its art, names the SteamVR dock entry, and sets SteamVR to 120 Hz with no motion smoothing. It sets them again at every launch.
- `install.sh --dry-run` shows what it would do first.
- Installing again updates the same entry. Your settings, saves and keys stay.
- It installs and updates through Frametanium and FrameDrop too.
- An uninstaller, with `--purge` for your data.

### From Azahar and CitraVR

- The emulation, the game list and the settings are Azahar 2127.0-alpha4's, and the 3D screens and immersive modes are CitraVR's. Thanks to the Azahar team and amwatson.
