# Azahar for Frame 1.1.1

Sharper screens at high resolution, the game list on the controller, 3D depth per game, better stats placement and head motion controls.

- At 5x resolution and above, the screens are sharp edge to edge. Valve's foveation layer lowered the detail away from where you looked; Azahar for Frame now turns it off.
- The game list works with the controller, in VR and in Flat. The D-pad or the left stick moves through the games, and **A** pressed twice quickly starts the one you're on.
- Double clicks with the beam are reliable.
- **3D Depth** is on the in-game menu's **Settings** tab, from 0% to 100%, for every game or just this one. It changes the 3D at once.
- The stats appear in front of you and stay there, never behind a screen. Grip them to move them; the stick sets their distance and size.
- **Stats Position** can put the stats on your left wrist instead. Turn your wrist toward your face to see them.
- **Motion Controls** can use the headset: turn and tilt your head to aim.

## Download

- `azaharframe-1.1.1-linux-arm64.tar.gz`: the release, for `install.sh` and Frametanium
- `azaharframe-1.1.1-framedrop-linux-arm64.zip` and `azaharframe.framedrop.json`: the same release, for FrameDrop

## Install

Download the tarball and run `steam-frame/install.sh` from the repository with it as the argument. You can also add `simplysanatori/AzaharFrame` in Frametanium, or [install with FrameDrop](https://framedropvr.com/install?manifest=https%3A%2F%2Fgithub.com%2Fsimplysanatori%2FAzaharFrame%2Freleases%2Flatest%2Fdownload%2Fazaharframe.framedrop.json). To update, run `install.sh` from the new release, or update in Frametanium. Your settings, saves and keys stay. The [README](https://github.com/simplysanatori/AzaharFrame/blob/steam-frame/README.md) has the steps and the main controls. The [user guide](https://github.com/simplysanatori/AzaharFrame/blob/steam-frame/docs/USER_GUIDE.md) has how to add your games, every control and every setting.

## Known

- Mario Kart 7's menus show white panels in Full (6DoF): Other Games mode.

## Reporting problems

Open an issue with what you did, what you expected and what happened, and attach `launch.log` and `user/log/azahar_log.txt` from `~/.local/share/AzaharFrame`. They hold the paths of your games, which include your user name. Read them first.
