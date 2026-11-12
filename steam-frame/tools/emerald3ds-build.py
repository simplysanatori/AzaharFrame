#!/usr/bin/env python3
# Copyright Azahar for Frame contributors
# Licensed under GPLv2 or any later version
#
# AzaharFrame emerald: makes emerald3ds.pak from the player's own Pokemon Emerald ROM with Emerald 3DS's
# own builder (github.com/ZallaxDev/emerald-3ds, CC BY-NC-SA 4.0), which is NOT part of this program:
# it comes from the release's Emerald3DS-WebPayload.zip, downloaded by Azahar for Frame when the player
# asks, and is only imported from that unpacked archive here. It is the same package the website's web
# builder runs; it needs Python 3.11+ and its standard library (Pillow only for releases that do not
# precompile their 3D scenery).
#
#   emerald3ds-build.py --payload-zip ZIP --rom ROM --out DIR [--language LANG] [--cache DIR]
#
# Writes DIR/emerald3ds.pak, DIR/Emerald3DS.3dsx and DIR/Emerald3DS.smdh (the ROM's language variant).
# Prints "progress PERMILLE TEXT" lines, then "result {json}" (abi, release, language, entries,
# bytes); on failure "error TEXT" and exit code 1. The ROM is only read.

import argparse
import json
import os
import shutil
import signal
import sys
import zipfile
from pathlib import Path, PurePosixPath

EXIT_FAILED = 1
EXIT_STOPPED = 4  # the same code the texture pack converter uses for a stop


def say(kind, text):
    sys.stdout.write("%s %s\n" % (kind, text))
    sys.stdout.flush()


def fail(text):
    say("error", " ".join(str(text).split()))
    sys.exit(EXIT_FAILED)


def unpack(zip_path, dest):
    """The payload and the builder package; refuses names that leave dest."""
    wanted = ("python/", "payload/", "licenses/", "web-manifest.json")
    with zipfile.ZipFile(zip_path) as zf:
        for info in zf.infolist():
            name = info.filename
            parts = PurePosixPath(name).parts
            if name.startswith("/") or "\\" in name or ".." in parts or ":" in name:
                fail("The Emerald 3DS download holds an unsafe file name.")
            if not name.startswith(wanted):
                continue
            target = dest.joinpath(*parts)
            if info.is_dir():
                target.mkdir(parents=True, exist_ok=True)
                continue
            target.parent.mkdir(parents=True, exist_ok=True)
            with zf.open(info) as src, open(target, "wb") as out:
                shutil.copyfileobj(src, out)
    if not (dest / "python" / "emerald3ds_builder").is_dir() or not (dest / "payload").is_dir():
        fail("The Emerald 3DS download has no builder in it.")


def stop(*_):
    raise KeyboardInterrupt


def main():
    signal.signal(signal.SIGTERM, stop)
    ap = argparse.ArgumentParser()
    ap.add_argument("--payload-zip", required=True, type=Path)
    ap.add_argument("--rom", required=True, type=Path)
    ap.add_argument("--out", required=True, type=Path)
    ap.add_argument("--language", default="")
    ap.add_argument("--cache", type=Path, default=None)
    ap.add_argument("--no-network", action="store_true")
    args = ap.parse_args()

    if sys.version_info < (3, 11):
        fail("Emerald 3DS's builder needs Python 3.11 or newer.")

    args.out.mkdir(parents=True, exist_ok=True)
    unpacked = args.out / "payload.unpacked"
    shutil.rmtree(unpacked, ignore_errors=True)
    unpacked.mkdir(parents=True)
    try:
        say("progress", "0 Unpacking the builder")
        unpack(args.payload_zip, unpacked)
        sys.path.insert(0, str(unpacked / "python"))
        try:
            from emerald3ds_builder import followers
            from emerald3ds_builder.build import Payload, build_pack, select_payload
            from emerald3ds_builder.errors import BuilderError
            from emerald3ds_builder.rom import load_rom
        except ImportError as exc:
            fail("Emerald 3DS's builder could not start: %s" % exc)

        def progress(fraction, message):
            say("progress", "%d %s" % (int(max(0.0, min(1.0, fraction)) * 1000), message))

        language = args.language or None
        payload = Payload(unpacked / "payload")
        try:
            rom = load_rom(args.rom)
            variant = select_payload(payload, rom.sha1, need_executables=True, language=language)
            if args.no_network:
                fetcher = False
            else:
                cache = args.cache or (args.out / "followers-cache")
                fetcher = followers.Fetcher(followers.load_sources(), cache=cache)
            pak = args.out / "emerald3ds.pak"
            # In-process generators, as the web builder runs them: the web payload has no
            # command-line entry point for child processes.
            info = build_pack(args.rom, payload, pak, progress, runner="inprocess", rom=rom,
                              need_executables=True, language=language, follower_fetcher=fetcher)
            for exe in info["payload"].executables():
                shutil.copy2(exe, args.out / exe.name)
        except BuilderError as exc:
            fail(exc.message if not exc.hint else "%s %s" % (exc.message, exc.hint))
        except ImportError as exc:
            fail("This Emerald 3DS release needs a Python package that is not installed: %s" % exc.name)
        except MemoryError:
            fail("Not enough memory to build the data pack.")
        say("result", json.dumps({
            "abi": "%08x" % info["abi"],
            "release": info["release"],
            "language": info["payload"].game_language,
            "variant_matches": variant.root == info["payload"].root,
            "entries": info["entries"],
            "bytes": info["bytes"],
        }))
    except KeyboardInterrupt:
        sys.exit(EXIT_STOPPED)
    finally:
        shutil.rmtree(unpacked, ignore_errors=True)
    return 0


if __name__ == "__main__":
    sys.exit(main())
