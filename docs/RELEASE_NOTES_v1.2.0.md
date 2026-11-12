# Azahar for Frame 1.2.0

A Mods tab, controls you can edit for any controller, mouse and pointer controls, screens you can detach, and a keyboard in the headset.

### Mods tab

- New **Mods** tab in the VR menu, with **Cheats**, **Mods** and **Textures**.
- **Cheats**: **Download** fetches the game's cheats from Sharkive and from my own repository.
- Cheats, patches, mods and textures known to work on Azahar for Frame (or made by me) carry a verified badge.
- Cheats made for a different version of the game show which version.
- Cheats, mods and texture packs from outside sources link to their author's page, for credit.
- **Mods**: **Install Mods** lists mods for the game and downloads them straight from their authors' GitHub pages.
- When two mods conflict, you choose which one to use.
- Texture packs have their own section in the VR menu. **Download Packs** downloads packs from their original sources, all within the app.

### Controls

- **Edit Controls** shows a New 3DS XL.
- Use several controllers at once, for example the left Frame controller and a mouse as a pointer.

### Mouse and Pointer Controls

- **Bottom Screen**: the mouse touches and drags on the bottom screen, for games that aim with the touch screen, such as Kid Icarus: Uprising.
- **Pointer**: a 3D pointer for the menu and screens, adapted from [Frametop](https://github.com/Frametop/frametop), so you can use the app outside a game.
- Hold left and right click for 3 seconds to switch modes.
- Middle click opens the VR menu.

### Screens

- **Detach Screens**: Off, Secondary or Both. Detached screens keep their place per game.
- **Touchpad Touch**: a gamepad's touchpad works as the bottom screen.

### Game keyboard

- A keyboard opens in the headset when a game asks for text. Thanks to [NicholasDavis5](https://github.com/NicholasDavis5) for reporting it ([#1](https://github.com/simplysanatori/AzaharFrame/issues/1)).
- When a game asks for a Mii, one is picked automatically. A future update may let you use your own Miis or create one.

### Immersive View upgrades

- **Match Game Camera** follows the game's camera while it runs, so cutscenes, zooms and widescreen mods stay correct. You can turn it off in the VR menu or in Azahar's settings if it causes problems.
- New presets for Super Mario 3D Land, Mario Kart 7, Kid Icarus: Uprising, Ocarina of Time 3D and Majora's Mask 3D, tuned for the Frame's wider field of view compared with Quest headsets.
- Super Mario 3D Land now has proper stereoscopic 3D in immersive view.

### Bug fixes

- From Azahar: a freeze when a controller connects or disconnects, and signature checks for encrypted content.

## Download

- `azaharframe-1.2.0-linux-arm64.tar.gz`: the release, for `install.sh` and Frametanium
- `azaharframe-1.2.0-framedrop-linux-arm64.zip` and `azaharframe.framedrop.json`: the same release, for FrameDrop

## Install

Download the tarball and run `steam-frame/install.sh` from the repository with it as the argument. You can also add `simplysanatori/AzaharFrame` in Frametanium, or [install with FrameDrop](https://framedropvr.com/install?manifest=https%3A%2F%2Fgithub.com%2Fsimplysanatori%2FAzaharFrame%2Freleases%2Flatest%2Fdownload%2Fazaharframe.framedrop.json). To update, run `install.sh` from the new release, or update in Frametanium. Your settings, saves and keys stay. The [README](https://github.com/simplysanatori/AzaharFrame/blob/steam-frame/README.md) has the steps and the main controls. The [user guide](https://github.com/simplysanatori/AzaharFrame/blob/steam-frame/docs/USER_GUIDE.md) has how to add your games, every control and every setting.

## Known

- Mario Kart 7's menus show white panels in Full (6DoF): Other Games mode.

## Reporting problems

Open an issue with what you did, what you expected and what happened, and attach `launch.log` and `user/log/azahar_log.txt` from `~/.local/share/AzaharFrame`. They hold the paths of your games, which include your user name. Read them first.
