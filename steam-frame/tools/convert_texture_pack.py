# Copyright 2026 AzaharFrame contributors
# Licensed under GPLv3 or any later version
"""Converts an Azahar texture pack to ASTC in KTX files with full mip chains.

Run through convert-texture-pack.sh (python3 -I, standard library only). Every PNG of the
pack becomes a .ktx of the same name, made by af-ktxenc (build-astcenc.sh). Every other file
is linked or copied. pack.json is copied, with .png names in its "textures" map changed to
.ktx. The new pack is a sibling folder of the original, named <pack>-astc<block>; the
original is never changed. A second run only redoes PNGs newer than their .ktx.

--activate: when PACK is the folder Azahar reads (load/textures/<TitleID>), point it at the
new pack when done. A symlink is repointed; a real folder is renamed <TitleID>-original and
a symlink takes its place. --restore PACK undoes that.
"""

import argparse
import concurrent.futures
import json
import os
import shutil
import signal
import struct
import subprocess
import sys
import threading
import time

MARKER = ".azaharframe-ktx.json"
BLOCKS = {"4x4": (4, 4), "6x6": (6, 6), "8x6": (8, 6)}
EXIT_FAILED, EXIT_USAGE, EXIT_NO_ENCODER, EXIT_STOPPED, EXIT_NO_SPACE = 1, 2, 3, 4, 5


def say(*args, **kwargs):
    print(*args, flush=True, **kwargs)


def human(size):
    if size >= 1e9:
        return f"{size / 1e9:.1f} GB"
    return f"{size / 1e6:.0f} MB"


def encoder_path():
    env = os.environ.get("AF_KTXENC")
    if env:
        return env
    data = os.environ.get("AZAHARFRAME_HOME") or os.path.expanduser("~/.local/share/AzaharFrame")
    return os.path.join(data, "tools", "af-ktxenc")


def png_size(path):
    """Width and height from the PNG header, or None."""
    try:
        with open(path, "rb") as f:
            head = f.read(24)
    except OSError:
        return None
    if len(head) < 24 or head[:8] != b"\x89PNG\r\n\x1a\n" or head[12:16] != b"IHDR":
        return None
    return struct.unpack(">II", head[16:24])


def ktx_size(width, height, block):
    """Bytes of a KTX with the full mip chain, as af-ktxenc writes it."""
    bx, by = BLOCKS[block]
    total = 64
    while True:
        total += 4 + -(-width // bx) * -(-height // by) * 16
        if width == 1 and height == 1:
            return total
        width, height = max(width // 2, 1), max(height // 2, 1)


def read_json(path):
    with open(path, encoding="utf-8-sig") as f:
        return json.load(f)


def write_json(path, value):
    tmp = path + ".part"
    with open(tmp, "w", encoding="utf-8") as f:
        json.dump(value, f, indent=4, ensure_ascii=False)
        f.write("\n")
    os.replace(tmp, path)


def is_up_to_date(src, out):
    try:
        return os.path.getsize(out) > 0 and os.path.getmtime(out) >= os.path.getmtime(src)
    except OSError:
        return False


def default_output(src_real, block):
    base = src_real[: -len("-original")] if src_real.endswith("-original") else src_real
    return f"{base}-astc{block}"


def scan(src_real):
    """Relative paths of every file in the pack (symlinked folders followed once)."""
    files = []
    seen = set()
    for root, dirs, names in os.walk(src_real, followlinks=True):
        real = os.path.realpath(root)
        if real in seen:
            dirs[:] = []
            continue
        seen.add(real)
        dirs.sort()
        for name in sorted(names):
            if name == MARKER or name.endswith(".part"):
                continue
            files.append(os.path.relpath(os.path.join(root, name), src_real))
    return files


def link_or_copy(src, out):
    tmp = out + ".part"
    if os.path.lexists(tmp):
        os.remove(tmp)
    try:
        os.link(src, tmp)
    except OSError:
        shutil.copy2(src, tmp)
    os.replace(tmp, out)


def swap_link(link, target):
    """Points the symlink `link` at `target` (relative) in one step."""
    tmp = link + ".af-new"
    if os.path.lexists(tmp):
        os.remove(tmp)
    os.symlink(os.path.relpath(target, os.path.dirname(link)), tmp)
    os.replace(tmp, link)


def activate(link, out_dir, marker):
    if os.path.islink(link):
        source = os.path.realpath(link)
        swap_link(link, out_dir)
    else:
        source = link + "-original"
        if os.path.lexists(source):
            say(f"error: {source} exists; move it away and run this again", file=sys.stderr)
            return False
        os.rename(link, source)
        os.symlink(os.path.relpath(out_dir, os.path.dirname(link)), link)
    marker["source"] = os.path.relpath(source, out_dir)
    write_json(os.path.join(out_dir, MARKER), marker)
    say(f"{link} now uses {out_dir}")
    say(f"the original pack is in {source}")
    return True


def restore(link):
    link = os.path.abspath(link)
    real = os.path.realpath(link)
    marker_path = os.path.join(real, MARKER)
    if not os.path.isfile(marker_path):
        say(f"{link} is not an optimized pack; nothing to do")
        return 0
    source = os.path.normpath(os.path.join(real, read_json(marker_path).get("source", "")))
    if not os.path.isdir(source) or source == real:
        say(f"error: the original pack ({source}) is missing", file=sys.stderr)
        return EXIT_FAILED
    if not os.path.islink(link):
        say(f"error: {link} is not a symlink; nothing was changed", file=sys.stderr)
        return EXIT_FAILED
    if source == link + "-original":
        os.remove(link)
        os.rename(source, link)
    else:
        swap_link(link, source)
    say(f"{link} uses the original pack again ({source})")
    say(f"the optimized pack stays in {real}; delete that folder to free its space")
    return 0


class Converter:
    def __init__(self, args, src_real, out_dir):
        self.args = args
        self.src = src_real
        self.out = out_dir
        self.encoder = encoder_path()
        self.stop = threading.Event()
        self.running = {}
        self.lock = threading.Lock()
        self.failed = []

    def encode(self, rel, flip):
        src = os.path.join(self.src, rel)
        out = os.path.join(self.out, rel[:-4] + ".ktx")
        cmd = [self.encoder, "--block", self.args.block, "--quality", self.args.quality]
        if flip:
            cmd.append("--flip")
        cmd += [src, out]
        with self.lock:
            if self.stop.is_set():
                return None
            proc = subprocess.Popen(cmd, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)
            self.running[rel] = proc
        _, err = proc.communicate()
        with self.lock:
            del self.running[rel]
        if self.stop.is_set():
            return None
        if proc.returncode != 0:
            return err.decode(errors="replace").strip() or f"exit {proc.returncode}"
        # A PNG kept by an earlier run would be a second file for the same texture
        kept = os.path.join(self.out, rel)
        if os.path.lexists(kept):
            os.remove(kept)
        return ""

    def halt(self):
        with self.lock:
            self.stop.set()
            for proc in self.running.values():
                proc.terminate()

    def run(self):
        args = self.args
        config_path = os.path.join(self.src, "pack.json")
        config = read_json(config_path) if os.path.isfile(config_path) else None
        # Azahar's defaults for a pack without pack.json flip PNGs too
        flip = bool(((config or {}).get("options") or {}).get("flip_png_files", True))

        files = scan(self.src)
        pngs = [f for f in files if f.endswith(".png")]
        others = [f for f in files if not f.endswith(".png") and f != "pack.json"]

        # Work is counted in texels, so the progress and the estimate follow the big textures
        work = {}
        total = 0
        need = 0
        for rel in pngs:
            src = os.path.join(self.src, rel)
            dims = png_size(src)
            work[rel] = dims[0] * dims[1] if dims else os.path.getsize(src)
            size = ktx_size(*dims, args.block) if dims else os.path.getsize(src)
            total += size
            if not is_up_to_date(src, os.path.join(self.out, rel[:-4] + ".ktx")):
                need += size
        if args.estimate:
            say(f"estimate {len(pngs)} {total} {need}")
            return 0

        os.makedirs(self.out, exist_ok=True)
        free = shutil.disk_usage(self.out).free
        if need > free:
            say(f"error: this needs about {human(need)} free on the drive of {self.out}; "
                f"{human(free)} is free", file=sys.stderr)
            return EXIT_NO_SPACE

        for rel in others:
            out = os.path.join(self.out, rel)
            os.makedirs(os.path.dirname(out), exist_ok=True)
            if not is_up_to_date(os.path.join(self.src, rel), out):
                link_or_copy(os.path.join(self.src, rel), out)

        todo = []
        done_work = 0
        done = 0
        for rel in pngs:
            out = os.path.join(self.out, rel[:-4] + ".ktx")
            os.makedirs(os.path.dirname(out), exist_ok=True)
            if is_up_to_date(os.path.join(self.src, rel), out):
                done += 1
                done_work += work[rel]
            else:
                todo.append(rel)
        todo.sort(key=lambda r: -work[r])
        total_work = max(sum(work.values()), 1)
        start = time.monotonic()
        start_work = done_work
        say(f"{len(pngs)} PNG textures, {done} already converted, {len(todo)} to go, "
            f"ASTC {args.block}, {args.jobs} at a time")

        def report():
            permille = done_work * 1000 // total_work
            if args.machine:
                say(f"progress {done} {len(pngs)} {permille}")
                return
            rate = (done_work - start_work) / max(time.monotonic() - start, 0.001)
            left = (total_work - done_work) / rate if rate > 0 else 0
            eta = f", about {int(left // 60)} min {int(left % 60)} s left" if rate > 0 else ""
            sys.stdout.write(f"\r{done}/{len(pngs)} ({permille / 10:.1f}%){eta}   ")
            sys.stdout.flush()

        report()
        with concurrent.futures.ThreadPoolExecutor(max_workers=args.jobs) as pool:
            futures = {pool.submit(self.encode, rel, flip): rel for rel in todo}
            for future in concurrent.futures.as_completed(futures):
                rel = futures[future]
                error = future.result()
                if error is None:
                    continue
                if error:
                    # The PNG goes in as it is, so the pack still shows this texture
                    self.failed.append((rel, error))
                    link_or_copy(os.path.join(self.src, rel), os.path.join(self.out, rel))
                done += 1
                done_work += work[rel]
                report()
        if not args.machine:
            sys.stdout.write("\n")

        if self.stop.is_set():
            for rel in todo:
                part = os.path.join(self.out, rel[:-4] + ".ktx.part")
                if os.path.exists(part):
                    os.remove(part)
            say("stopped; run this again to carry on where it left off")
            return EXIT_STOPPED

        failed = {rel for rel, _ in self.failed}
        for rel, error in self.failed:
            say(f"kept as PNG: {rel}: {error}")
        if config is not None:
            textures = config.get("textures")
            if isinstance(textures, dict):
                def rename(name):
                    if not isinstance(name, str) or not name.endswith(".png"):
                        return name
                    if any(os.path.basename(f) == os.path.basename(name) for f in failed):
                        return name
                    return name[:-4] + ".ktx"
                for key, value in textures.items():
                    textures[key] = [rename(v) for v in value] if isinstance(value, list) \
                        else rename(value)
            write_json(os.path.join(self.out, "pack.json"), config)

        marker = {
            "format": f"ASTC {args.block}",
            "quality": args.quality,
            "source": os.path.relpath(self.src, self.out),
            "textures": len(pngs),
            "kept_as_png": len(failed),
        }
        write_json(os.path.join(self.out, MARKER), marker)
        size = sum(os.path.getsize(os.path.join(r, n)) for r, _, ns in os.walk(self.out)
                   for n in ns)
        say(f"done: {self.out} ({human(size)})")
        self.marker = marker
        return 0


def main():
    parser = argparse.ArgumentParser(
        prog="convert-texture-pack.sh",
        description="Converts an Azahar texture pack to ASTC (KTX files with mip chains).")
    parser.add_argument("pack", nargs="?", help="the pack folder (pack.json at its root)")
    parser.add_argument("output", nargs="?", help="where the new pack goes "
                        "(default: next to the pack, named <pack>-astc<block>)")
    parser.add_argument("--block", choices=sorted(BLOCKS), default="4x4",
                        help="ASTC block size: 4x4 looks best (8 bits a texel), 6x6 is less "
                        "than half the size (3.6 bits a texel), 8x6 smaller still")
    parser.add_argument("--quality", default="medium",
                        help="astcenc effort: fast, medium (default), thorough, or 0-100")
    parser.add_argument("--jobs", type=int, default=0, help="textures at a time "
                        "(default: one less than the CPU count)")
    parser.add_argument("--activate", action="store_true",
                        help="when done, make the folder Azahar reads use the new pack")
    parser.add_argument("--restore", action="store_true",
                        help="make the folder Azahar reads use the original pack again")
    parser.add_argument("--estimate", action="store_true",
                        help="print 'estimate FILES TOTAL_BYTES BYTES_STILL_TO_WRITE' and exit")
    parser.add_argument("--check", action="store_true",
                        help="exit 0 if the encoder is built, 3 if not")
    parser.add_argument("--machine", action="store_true",
                        help="progress as 'progress DONE TOTAL PERMILLE' lines")
    args = parser.parse_args()

    encoder = encoder_path()
    have_encoder = os.path.isfile(encoder) and os.access(encoder, os.X_OK)
    if args.check:
        return 0 if have_encoder else EXIT_NO_ENCODER
    if not args.pack:
        parser.print_usage(sys.stderr)
        return EXIT_USAGE
    link = os.path.abspath(args.pack.rstrip("/") or "/")
    if args.restore:
        return restore(link)

    if not os.path.isdir(link):
        say(f"error: no such folder: {link}", file=sys.stderr)
        return EXIT_USAGE
    src_real = os.path.realpath(link)
    if os.path.isfile(os.path.join(src_real, MARKER)):
        say(f"{link} is already an optimized pack")
        return 0
    if args.output and args.activate:
        say("error: --activate uses the default output folder", file=sys.stderr)
        return EXIT_USAGE
    out_dir = os.path.abspath(args.output) if args.output else default_output(src_real, args.block)
    if os.path.realpath(out_dir) == src_real or \
            os.path.realpath(out_dir).startswith(src_real + os.sep):
        say("error: the output folder must be outside the pack", file=sys.stderr)
        return EXIT_USAGE
    if not args.estimate and not have_encoder:
        say(f"error: the encoder is not built ({encoder}); run build-astcenc.sh first",
            file=sys.stderr)
        return EXIT_NO_ENCODER
    if args.jobs <= 0:
        args.jobs = max(1, len(os.sched_getaffinity(0)) - 1)

    converter = Converter(args, src_real, out_dir)
    for sig in (signal.SIGTERM, signal.SIGINT, signal.SIGHUP):
        signal.signal(sig, lambda *_: converter.halt())
    result = converter.run()
    if result == 0 and args.activate and not args.estimate:
        if not activate(link, out_dir, converter.marker):
            return EXIT_FAILED
    return result


if __name__ == "__main__":
    sys.exit(main())
