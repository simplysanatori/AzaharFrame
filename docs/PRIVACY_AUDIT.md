# Privacy audit

Run on 2026-11-06 over the tracked tree of the release branch and the whole history reachable from `steam-frame` (1,502 commits at `96f929e03`, of which 24 are this fork's own; the rest is Azahar's and CitraVR's, with their authors' public identities). Run it again on the release candidate: the tracked tree, the history that is pushed, and the tarball.

## What is searched

- The owner's names and former account names, and the old commit email. Searched with a list kept outside the repository, so that the list itself does not publish them.
- Network identifiers: IPv4 addresses (`192.168.*`, `10.*`), MAC addresses, PC and client hostnames, Wi-Fi network names, including the Frame's own access point name pattern.
- IDs: UUIDs, Steam user IDs and `userdata/` paths, headset and controller serial numbers, OpenXR system IDs.
- Secrets: GitHub tokens (`ghp_`, `gho_`, `github_pat_`), other API key prefixes (`sk-`, `xox?-`, `AKIA`), PEM private keys and certificates, email addresses.
- Home folder paths: `/home/<name>` in shipped files other than upstream's own, and other users' folders.
- Personal game files and names: the file names and product codes (`CTR-P-XXXX`) of the owner's own dumps.
- 3DS data: game files (`.cci`, `.3ds`, `.cxi`, `.cia`, `.3dsx`, `.app`), console keys (`aes_keys.txt`, `seeddb.bin`, `boot9.bin`, `sysdata/`), NAND and SD card contents, save files and cheats. None may be in the repository, its history or a release.
- Image metadata (PNG text chunks, EXIF) of the art and the cover.
- Commit and tag identities and commit messages.

## What it finds

| Check | Result |
|---|---|
| Owner names, former account names, old emails, Wi-Fi name, local IP ranges, PC login: tracked tree | None. The only hits are upstream's: authors' addresses in four old Citra source headers, `192.168.0.1` as the default router address in the Android netplay code, and the word "Author" in translation files |
| Same patterns: whole history | The one hit below (the ROM file name) |
| IPv4, MAC, email, tokens and keys, Windows PC names, access point names, Steam user IDs, other users' home folders in the files this fork owns | 0 each |
| Tokens, API keys, private keys and certificates in the whole tracked tree outside `externals/` | 0 |
| Tokens and private keys in any commit | 0 |
| 3DS files and keys tracked | 0 |
| Image metadata of `steam-frame/art/*.png`, `docs/images/*` | No text chunks, no EXIF. The SVG sources in `steam-frame/art/source/` carry only Inkscape view settings and the document name `name_logo_embedded.svg` |
| Personal game file names and product codes | One hit: `src/frame_xr/IMMERSIVE.md` named a dump of the owner's by its file name. Removed from the tree in this pass; it is still in the pushed history (note below) |
| Absolute `/home/steamos` paths in shipped files | None. `~` is used throughout. The only `/home/` strings are this document's own check commands and upstream's `tools/*-kotlin-formatting.sh` (`/home/linuxbrew`) |
| Commit identities of this fork's own commits | All `simplysanatori <149214925+simplysanatori@users.noreply.github.com>`, as author and committer. No tags of this fork |
| Commit messages | Every one of this fork's commits ends with a `Co-Authored-By` trailer naming the assistant. Nine messages name internal branches (`Merge lane/...`) or say a commit is unbuilt or WIP: reword or squash them before the repository is made public. They hold no names or addresses |
| The release tarball | Holds only `azahar`, `azaharframe.sh`, `install.sh`, `uninstall.sh`, `steam-cef.py`, `dock-icon.py`, `VERSION`, `LICENSE.txt`, `THIRD_PARTY_NOTICES.md` and `art/` (eight PNG files). `make-release.sh` packs those by name, so settings, saves, logs and keys cannot end up in it. A test tarball made from a stand-in program was listed to confirm this. The binary string check below runs on the real program at release time |

Note on the history: `steam-frame` is pushed to the private repository up to `004790172`, which includes `e0d831665`, the commit that added the ROM file name to `IMMERSIVE.md` (the name of a retail game's standard dump, with no path or product code). The repository is not public. Before it is made public, rewrite the history of this fork's own commits (24 of them, and nothing of Azahar's or CitraVR's) to drop that name and to reword the nine messages above, and push the rewritten branch; a rewrite limited to commits not reachable from the Azahar tag and CitraVR's master leaves upstream's commit ids alone.

## The outbound connections

The emulator makes no network connection of its own (web service, telemetry, update check and Discord are all compiled out). The exception is the Cheats window's "Download Cheats" button and the Cheats tab of the VR menu (Download, Install), in `src/core/cheats/online.cpp`. Only when the player clicks them, the program makes HTTPS requests to these hosts and no others:

| Host | Request | Why |
|---|---|---|
| `api.github.com` | `GET /repos/simplysanatori/3ds-game-fixes/git/trees/main?recursive=1` (once per download) | Finds the game's Developer cheat file and code patch in the file list |
| `raw.githubusercontent.com` | `GET /simplysanatori/3ds-game-fixes/main/games/<slug>/cheats/<title id>.txt` and `.../patch/<title id>/code.ips` | The Developer cheats and fix |
| `raw.githubusercontent.com` | `GET /FlagBrew/Sharkive/master/3ds/<title id>.txt` | The Sharkive cheats |

| `raw.githubusercontent.com` | `GET /simplysanatori/3ds-game-fixes/main/catalog/mods.json` (once per run, when a fix is installed or the Mods tab refreshes its list) | The mod catalog: which mods exist, for which title IDs, and where each is downloaded from |
| `api.github.com` | `GET /repos/<owner>/<repo>/releases/latest` (or `/releases/tags/<tag>`), `GET /repos/<owner>/<repo>/git/trees/<ref>?recursive=1` | Only when the player installs a mod from the Mods tab: the author's own repository named in the catalog entry |
| `github.com`, `raw.githubusercontent.com`, `objects.githubusercontent.com`, `release-assets.githubusercontent.com`, `codeload.github.com` | `GET` of the release asset or repository file named by the catalog entry (a redirect from `github.com` to GitHub's asset hosts is followed; any other host or plain `http` is refused) | The mod's files, straight from its author's GitHub repository; nothing is re-hosted |

Texture packs (VR menu, Textures segment, "Download Packs", `src/core/custom_tex/download.*`). Only when the player taps Download on a pack, never in the background; the catalog list itself is read with the one catalog request above (`catalog/textures.json`, same host) or from the copy compiled in:

| Host | Request | Why |
|---|---|---|
| `api.github.com` | `GET /repos/<owner>/<repo>/releases/tags/<tag>` (or `latest`) | Finds the release asset of a pack the catalog names |
| `github.com`, `objects.githubusercontent.com`, `release-assets.githubusercontent.com`, `codeload.github.com` | `GET` of the release asset (redirects between these followed; Range request to resume) | The pack archive, straight from its author's repository |
| `drive.usercontent.google.com`, `drive.google.com` | `GET /download?id=<file id>&export=download&confirm=t`, and the address Drive's "can't scan for viruses" page names (same two hosts only) | A pack its author shares as a Google Drive file. Folder links are not supported |
| any one exact host a catalog entry names (`https` source, or its `hosts` list) | `GET` of the pack archive | A pack its author hosts on a plain https link. No wildcards, no plain `http`; the catalog is the maintainer's to edit |

Nothing is re-hosted. Redirects go only to the hosts above (`HostAllowed` in `src/core/custom_tex/download.cpp`); anything else is refused. The requests carry no title ID of the game, no account and no cookie (Google sees the IP address and the file id, as for any shared file). Files are saved to `load/texture-downloads/<pack id>.part`, unpacked (7z and rar need the system's `bsdtar`) and moved into `load/texture-packs/<title id>/<folder>/`; the archive is deleted afterwards. The release check grows to: `git grep -n "drive.google.com\|drive.usercontent.google.com" src` must find `src/core/custom_tex/download.*` only.

The requests carry the open game's 16-digit title ID in the path and the usual HTTP headers (host, `User-Agent: cpp-httplib/<version>`), no account, no cookie and no other identifier; GitHub sees the player's IP address as for any download. The certificate is checked against the system's store (`/etc/ssl/certs/ca-certificates.crt` and the usual alternatives); with none, nothing is sent. Redirects are followed only by the mod downloader and only to the GitHub hosts above, over HTTPS (`src/core/mods/library.cpp`, `HostAllowed`); the cheat requests follow none. Nothing is stored except the cheats merged into `cheats/<title id>.txt` (disabled, each with a `*source:` line) and, when a mod is installed, its files in `load/mod-library/<title id>/<mod id>/` (with `manifest.json`) and copied into `load/mods/<title id>/`. Mods are downloaded only when the player installs one; the catalog entry decides the address, never the title ID or any other player data. The release check: `git grep -n "githubusercontent.com\|api.github.com" src` must find `src/core/cheats/online.cpp`, `src/core/cheats/sharkive.*` and `src/core/mods/*` only.

## What the emulator writes to logs

Two files are what a user attaches to an issue: `~/.local/share/AzaharFrame/launch.log` and `~/.local/share/AzaharFrame/user/log/azahar_log.txt`.

- `launch.log` holds the date, the Steam app ID of the library entry, the lines of `session.env`, the OpenXR runtime file, SteamVR settings that could not be set, and the exit status. No account name, no network address.
- `azahar_log.txt` holds Azahar's usual start-up lines (version, processor name, system version, memory, graphics device and driver, every setting including audio and camera device names), the full path of each game it loads (these include the Linux user name, for example under `~/Documents`), the picker's launch line (`xr: picker launches <path>`), the OpenXR runtime and the headset's name, and the per-second performance line. A custom storage folder, if set, is logged as a path too. It holds no Steam or GitHub account, no network address and no serial number.

The README tells users this in its "Reporting a problem" section, and the user guide in full, and both ask them to read the logs before sharing. There is no log-collection script in the repository.

## Verification

Run these from the repository root. Each must print `0`, except where it says to review. Upstream's vendored sources (`externals/`, `src/` outside `frame_xr`) hold version strings, example addresses and their authors' addresses, so most checks name the folders this fork owns.

```sh
OWN="steam-frame docs README.md AGENTS.md THIRD_PARTY_NOTICES.md src/frame_xr tools/xrmock tools/mockrun.sh src/tests/frame_xr"

# IPv4 addresses other than 127.0.0.1 and 0.0.0.0 in the files this fork owns
git grep -IPn '(?<![\d.])(?:\d{1,3}\.){3}\d{1,3}(?![\d.])' -- $OWN ':!docs/PRIVACY_AUDIT.md' | grep -vE '127\.0\.0\.1|0\.0\.0\.0' | wc -l

# MAC addresses
git grep -IPni '(?<![0-9a-f:])(?:[0-9a-f]{2}:){5}[0-9a-f]{2}(?![0-9a-f:])' -- $OWN | wc -l

# Email addresses other than GitHub no-reply ones
git grep -IPn '[A-Za-z0-9._%+-]+@[A-Za-z0-9-]+\.[A-Za-z]{2,}' -- $OWN | grep -v 'users.noreply.github.com' | wc -l

# Tokens, API keys, private keys and certificates, in the whole tree
git grep -IPn 'ghp_[A-Za-z0-9]{20}|gho_[A-Za-z0-9]{20}|github_pat_[A-Za-z0-9_]{20}|sk-[A-Za-z0-9]{20}|xox[abp]-|AKIA[0-9A-Z]{16}|-----BEGIN [A-Z ]*(PRIVATE KEY|CERTIFICATE)' -- . ':!externals' | wc -l

# Windows PC names, Steam Frame access point names, Steam user IDs, other users' home folders, any /home/steamos path
git grep -IPn 'DESKTOP-[A-Z0-9]{5,}|frame_[0-9a-f]{8}\b|userdata/[0-9]{5,}|/home/[a-z][a-z0-9_-]+/|C:\\\\Users\\\\' -- $OWN ':!docs/PRIVACY_AUDIT.md' | wc -l

# Personal strings (keep the list of names outside the repository: one pattern per line in $LIST)
git grep -IP -f "$LIST" -- . ':!externals' ':!src/android' ':!dist/languages' ':!docs/PRIVACY_AUDIT.md' | wc -l      # review: upstream's own authors' addresses are public

# Product codes and file names of game dumps
git grep -IPn 'CTR-[PN]-[A-Z0-9]{4}|\((USA|Europe|Japan)\)\.(3ds|cci|cia)' -- $OWN ':!docs/PRIVACY_AUDIT.md' | wc -l

# 3DS files and keys tracked anywhere
git ls-files | grep -ciE '\.(cci|3ds|cxi|cia|3dsx)$|aes_keys|seeddb|boot9|sysdata/'

# Secrets and personal strings in the history (commit messages and every diff)
git log refs/heads/steam-frame -p -m | grep -acP 'ghp_[A-Za-z0-9]{20}|github_pat_[A-Za-z0-9_]{20}|gho_[A-Za-z0-9]{20}|-----BEGIN [A-Z ]*PRIVATE KEY'
git log refs/heads/steam-frame -p -m | grep -acP -f "$LIST"

# Commit identities of this fork's own commits (not upstream's): must print 0
git log --format='%ae%n%ce' refs/heads/steam-frame --not citravr/master azahar/master | grep -vc 'users.noreply.github.com'

# Commit messages of this fork's own commits that name internal branches or work states (review)
git log --format='%h %s' refs/heads/steam-frame --not citravr/master azahar/master | grep -iE 'lane/|unbuilt|WIP|untested'

# Image metadata: any text chunk or EXIF in the art
python3 -I -c 'import sys; [print(p, b"tEXt" in d or b"iTXt" in d or b"Exif" in d) for p in sys.argv[1:] for d in [open(p,"rb").read()]]' steam-frame/art/*.png docs/images/*

# The release tarball: only the files listed above, and nothing personal in the binary
tar -tzf steam-frame/release/azaharframe-<version>-linux-arm64.tar.gz
tar -xzOf steam-frame/release/azaharframe-<version>-linux-arm64.tar.gz azahar | strings | grep -cP '/home/(?!steamos\b)[a-z]+/|DESKTOP-|ghp_'
tar -xzOf steam-frame/release/azaharframe-<version>-linux-arm64.tar.gz azahar | strings | grep -c '/home/steamos'
```

The last command shows build paths the compiler left in the binary (assertion text and the like); it is expected to be non-zero for a program built on the Frame. Review what it finds: a path with the build user's name is not a secret, but the release can be built with `-ffile-prefix-map=$HOME=.` to leave it out.

Before publishing new docs, logs or probe output, run these again. Replace anything they find with a placeholder such as `<ip-address>`, `<pc-hostname>`, `<ssid>` or `<serial>`.

The identity check runs on this fork's own commits: Azahar's and CitraVR's history carries their authors' addresses, which is public and stays.
