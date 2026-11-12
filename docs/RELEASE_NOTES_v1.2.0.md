# Azahar for Frame 1.2.0

A Mods tab for cheats, mods and texture packs, controls you can edit for any controller, a mouse in VR, screens you can detach, and the game keyboard in the headset.

### Mods tab

The in-game menu has a new **Mods** tab, with **Cheats**, **Mods** and **Textures** at the top.

- **Cheats**: **Download** fetches the game's cheats from Sharkive and from my own fixes repository. Each cheat has a switch. It works at once and is kept.
- My own cheats, fixes and mods carry a verified badge.
- A cheat made for another version of your game shows that version in a chip, such as **v1.0**.
- Azahar's **Cheats** window has the same download, as **Download Cheats**.
- **Mods**: **Install Mods** lists mods for the game and installs them straight from their authors' GitHub pages. **Remove** takes one out.
- When two mods change the same files, it asks before replacing one.
- **Created by** opens the mod author's page in Steam's browser.
- **Textures**: pick the game's texture pack, or **Off**. **Custom Textures** and **Preload** are here too.
- **Download Packs** downloads texture packs from GitHub and Google Drive. Henriko Magnifico's 4K and 1080p packs for Ocarina of Time 3D, Majora's Mask 3D, Super Mario 3D Land and A Link Between Worlds are in the list.
- After a change, **Restart the Game to Apply** shows with a **Restart** button.

### Edit Controls

- **Edit Controls** on the **Controls** tab shows a New 3DS XL with a chip on every button. Pick a chip, then press the button you want. A button already in use swaps.
- It edits any connected device: the Frame controllers, a Bluetooth or USB gamepad, or a mouse.
- Gamepads play in VR next to the Frame controllers. Each kind of gamepad keeps its own layout.
- To set a stick, push it up, then right.
- **Reset to Default** puts a device back.

### Mouse

- A mouse works in VR, in two modes.
- **Bottom Screen**: the mouse moves a cursor on the bottom screen, and the left button touches. With **Recentre at Edge**, a drag that hits the edge starts again from the middle, for games that turn the camera with the touch screen.
- **Pointer**: the mouse moves a 3D pointer around you, like the beam, for the menu, Azahar's window and the screens. I adapted it from Frametop's pointer.
- Hold the left and right buttons for 3 seconds to switch modes.
- The middle button opens the in-game menu. Edit Controls changes every button.

### Detach Screens

- **Detach Screens** on the **Position** tab: **Off**, **Secondary** or **Both**. **Secondary** lets the smaller screen move on its own. **Both** frees each screen.
- Grip a detached screen to put it anywhere around you. The stick sets its distance and size.
- Each game keeps its own screen places.

### Touchpad Touch

- **Touchpad Touch** on the **Controls** tab makes a gamepad's touchpad the bottom screen, with a dot where your finger is.
- **Touch**: a finger on the pad touches. **Click**: pressing the pad touches, and your finger only moves the dot.

### Game keyboard and Mii Selector

- When a game asks for text, a keyboard opens in the headset. Before, Azahar's dialog opened outside the headset and the game waited forever. Thanks to [NicholasDavis5](https://github.com/NicholasDavis5) for reporting it ([#1](https://github.com/simplysanatori/AzaharFrame/issues/1)).
- The keyboard keeps the game's own rules and buttons. A keyboard connected to the Frame types into it too.
- When a game asks you to choose a Mii, the one the game suggests is chosen and its name shows. That dialog was out of view as well.

### Immersive mode

- **Match Game Camera** reads the game camera's angle and shape while the game runs. The world stays still through cutscenes, zooms and widescreen mods. It is on by default.
- Super Mario 3D Land, Mario Kart 7, Kid Icarus: Uprising, Ocarina of Time 3D and Majora's Mask 3D start with a setup made for each: the right Full mode and world scale.
- Super Mario 3D Land has real 3D in **Full (6DoF): Other Games**. Both eyes used to see the same picture.

### Fixes

- Two fixes from Azahar: a controller connecting or disconnecting could freeze the emulator, and encrypted content now gets its signature checks.

## Download

- `azaharframe-1.2.0-linux-arm64.tar.gz`: the release, for `install.sh` and Frametanium
- `azaharframe-1.2.0-framedrop-linux-arm64.zip` and `azaharframe.framedrop.json`: the same release, for FrameDrop

## Install

Download the tarball and run `steam-frame/install.sh` from the repository with it as the argument. You can also add `simplysanatori/AzaharFrame` in Frametanium, or [install with FrameDrop](https://framedropvr.com/install?manifest=https%3A%2F%2Fgithub.com%2Fsimplysanatori%2FAzaharFrame%2Freleases%2Flatest%2Fdownload%2Fazaharframe.framedrop.json). To update, run `install.sh` from the new release, or update in Frametanium. Your settings, saves and keys stay. The [README](https://github.com/simplysanatori/AzaharFrame/blob/steam-frame/README.md) has the steps and the main controls. The [user guide](https://github.com/simplysanatori/AzaharFrame/blob/steam-frame/docs/USER_GUIDE.md) has how to add your games, every control and every setting.

## Known

- Mario Kart 7's menus show white panels in Full (6DoF): Other Games mode.

## Reporting problems

Open an issue with what you did, what you expected and what happened, and attach `launch.log` and `user/log/azahar_log.txt` from `~/.local/share/AzaharFrame`. They hold the paths of your games, which include your user name. Read them first.
