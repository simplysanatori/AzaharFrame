# Azahar for Frame 1.1.0

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

## Download

- `azaharframe-1.1.0-linux-arm64.tar.gz`: the release, for `install.sh` and Frametanium
- `azaharframe-1.1.0-framedrop-linux-arm64.zip` and `azaharframe.framedrop.json`: the same release, for FrameDrop

## Install

Download the tarball and run `steam-frame/install.sh` from the repository with it as the argument. You can also add `simplysanatori/AzaharFrame` in Frametanium, or [install with FrameDrop](https://framedropvr.com/install?manifest=https%3A%2F%2Fgithub.com%2Fsimplysanatori%2FAzaharFrame%2Freleases%2Flatest%2Fdownload%2Fazaharframe.framedrop.json). To update, run `install.sh` from the new release, or update in Frametanium. Your settings, saves and keys stay. The [README](https://github.com/simplysanatori/AzaharFrame/blob/steam-frame/README.md) has the steps and the main controls. The [user guide](https://github.com/simplysanatori/AzaharFrame/blob/steam-frame/docs/USER_GUIDE.md) has how to add your games, every control and every setting.

## Known

- Mario Kart 7's menus show white panels in Full (6DoF): Other Games mode.

## Reporting problems

Open an issue with what you did, what you expected and what happened, and attach `launch.log` and `user/log/azahar_log.txt` from `~/.local/share/AzaharFrame`. They hold the paths of your games, which include your user name. Read them first.
