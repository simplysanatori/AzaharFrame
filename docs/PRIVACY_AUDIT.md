# Privacy audit

> **SKELETON.** The checks to run before anything is published: every tracked file, the history, the release notes and the release tarball. The "What it finds" results are filled in when the checks are run on the release candidate. <!-- VERIFY -->

## What is searched

- The owner's names and former account names, and the old commit email. Searched with a list kept outside the repository, so that the list itself does not publish them.
- Network identifiers: IPv4 addresses, MAC addresses, PC and client hostnames, Wi-Fi network names, including the Frame's own access point name pattern.
- IDs: UUIDs, Steam user IDs and `userdata/` paths, headset and controller serial numbers, OpenXR system IDs.
- Secrets: GitHub tokens (`ghp_`, `gho_`, `github_pat_`), other API key prefixes (`sk-`, `xox?-`, `AKIA`), PEM private keys and certificates, email addresses.
- Home folder paths of users other than the generic `steamos`.
- 3DS data: game files (`.cci`, `.3ds`, `.cxi`, `.cia`, `.3dsx`, `.app`), console keys (`aes_keys.txt`, `seeddb.bin`, `boot9.bin`, `sysdata/`), NAND and SD card contents, save files and cheats. None may be in the repository, its history or a release.

## What it finds

<!-- To fill in: -->
- Tracked files: <!-- result -->
- Paths under `/home/steamos` and `127.0.0.1` (Steam's local debugging port, used by `install.sh` and `steam-cef.py`) are generic and stay.
- Published history: <!-- tokens, keys, network identifiers; commit and tag identities -->
- The release tarball holds only `azahar`, `azaharframe.sh`, `install.sh`, `uninstall.sh`, `steam-cef.py`, `VERSION`, `LICENSE.txt` and `THIRD_PARTY_NOTICES.md`. `steam-frame/release/make-release.sh` packs those files by name, so settings, saves, logs and keys cannot end up in a tarball. <!-- result of the binary strings check -->
- The emulator's log (`user/log/azahar_log.txt`) and `launch.log` hold file paths and the names of the games the user loaded. They hold no account names or network addresses. The README says to check them before attaching them to a report.
- There is no log-collection script in the repository.

## Verification

Run these from the repository root. Each must print `0`. Upstream's vendored sources (`externals/`, `src/`) hold version strings and example addresses, so the address check names the folders to leave out; review what it would have found once, and keep the list of exclusions below.

```sh
# IPv4 addresses other than 127.0.0.1 and 0.0.0.0 in the files this fork owns
git grep -IPn '(?<![\d.])(?:\d{1,3}\.){3}\d{1,3}(?![\d.])' -- steam-frame docs README.md THIRD_PARTY_NOTICES.md src/frame_xr tools | grep -vE '127\.0\.0\.1|0\.0\.0\.0' | wc -l

# MAC addresses
git grep -IPni '(?<![0-9a-f:])(?:[0-9a-f]{2}:){5}[0-9a-f]{2}(?![0-9a-f:])' -- steam-frame docs README.md src/frame_xr tools | wc -l

# Email addresses other than GitHub no-reply ones
git grep -IPn '[A-Za-z0-9._%+-]+@[A-Za-z0-9-]+\.[A-Za-z]{2,}' -- steam-frame docs README.md src/frame_xr tools | grep -v 'users.noreply.github.com' | wc -l

# Tokens, API keys, private keys and certificates, in the whole tree
git grep -IPn 'ghp_[A-Za-z0-9]{20}|gho_[A-Za-z0-9]{20}|github_pat_[A-Za-z0-9_]{20}|sk-[A-Za-z0-9]{20}|xox[abp]-|AKIA[0-9A-Z]{16}|-----BEGIN [A-Z ]*(PRIVATE KEY|CERTIFICATE)' -- . ':!externals' | wc -l

# Windows PC names, Steam Frame access point names, Steam user IDs, other users' home folders
git grep -IPn 'DESKTOP-[A-Z0-9]{5,}' -- steam-frame docs README.md src/frame_xr tools | wc -l
git grep -IPni 'frame_[0-9a-f]{8}\b|userdata/[0-9]{5,}|/home/(?!steamos\b)[a-z][a-z0-9_-]+/|C:\\\\Users\\\\' -- steam-frame docs README.md src/frame_xr tools | wc -l

# 3DS files and keys tracked anywhere
git ls-files | grep -ciE '\.(cci|3ds|cxi|cia|3dsx)$|aes_keys|seeddb|boot9|sysdata/' | grep -v '^0$' | wc -l   # review: upstream's own sample files are not game data

# Secrets anywhere in the history
git log -p --all --tags | grep -cE 'ghp_[A-Za-z0-9]{20}|github_pat_[A-Za-z0-9_]{20}|-----BEGIN [A-Z ]*PRIVATE KEY'

# Commit and tag identities other than no-reply addresses, on this fork's own commits (not upstream's)
git log --first-parent --format='%ae%n%ce' citravr/master..HEAD | grep -vc 'users.noreply.github.com'

# The release tarball: only the files listed above, and nothing personal in the binary
tar -tzf steam-frame/release/azaharframe-<version>-linux-arm64.tar.gz
tar -xzOf steam-frame/release/azaharframe-<version>-linux-arm64.tar.gz azahar | strings | grep -cP '/home/(?!steamos\b)[a-z]+/|DESKTOP-|ghp_'
```

Before publishing new docs, logs or probe output, run these again. Replace anything they find with a placeholder such as `<ip-address>`, `<pc-hostname>`, `<ssid>` or `<serial>`.

The identity check runs on this fork's own commits: Azahar's and CitraVR's history carries their authors' addresses, which is public and stays.
