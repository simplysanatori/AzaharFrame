#!/usr/bin/env python3
"""Refresh "downloads" and "updated" in the mod and texture catalogs.

For every entry whose source (or first part) is a GameBanana mod, the mod's lifetime download count
and the month it was last updated are read from GameBanana's public API; for a github_release source
the download count of the matching release asset(s) and the month the release was published. Nothing
else in the catalogs changes. Dry run by default: prints what would change; --apply writes the files.

    catalog-refresh.py [--catalog-dir DIR] [--apply] [--delay SECONDS] [--only ID]

Standard library only. Sends a generic user agent and nothing else about the person running it.
Set GITHUB_TOKEN in the environment to avoid GitHub's anonymous rate limit (sent only to api.github.com).
"""
import argparse
import fnmatch
import json
import os
import sys
import time
import urllib.error
import urllib.request
from datetime import datetime, timezone

UA = "catalog-refresh/1.0"
FILES = (("mods.json", "mods"), ("textures.json", "packs"))


def get_json(url, headers=None):
    req = urllib.request.Request(url, headers={"User-Agent": UA, "Accept": "application/json", **(headers or {})})
    with urllib.request.urlopen(req, timeout=30) as r:
        return json.load(r)


def month_of_unix(ts):
    return datetime.fromtimestamp(int(ts), tz=timezone.utc).strftime("%Y-%m")


def gamebanana(mod_id):
    j = get_json(f"https://gamebanana.com/apiv11/Mod/{mod_id}/ProfilePage")
    if j.get("_bIsTrashed") or j.get("_bIsWithheld") or j.get("_bIsPrivate"):
        raise RuntimeError("mod not available")
    stamp = j.get("_tsDateUpdated") or j.get("_tsDateModified") or j.get("_tsDateAdded")
    return {
        "downloads": int(j["_nDownloadCount"]) if j.get("_nDownloadCount") is not None else None,
        "updated": month_of_unix(stamp) if stamp else None,
    }


def github_release(source):
    tag = source.get("tag") or "latest"
    path = "latest" if tag == "latest" else f"tags/{tag}"
    headers = {}
    if os.environ.get("GITHUB_TOKEN"):
        headers["Authorization"] = "Bearer " + os.environ["GITHUB_TOKEN"]
    j = get_json(f"https://api.github.com/repos/{source['repo']}/releases/{path}", headers)
    pattern = source.get("asset_pattern", "*")
    counts = [a.get("download_count", 0) for a in j.get("assets", []) if fnmatch.fnmatchcase(a.get("name", ""), pattern)]
    stamp = j.get("published_at") or j.get("created_at") or ""
    return {
        "downloads": sum(counts) if counts else None,
        "updated": stamp[:7] if len(stamp) >= 7 else None,
    }


def lookup(entry):
    source = entry.get("source")
    if not isinstance(source, dict):
        parts = entry.get("parts") or []
        source = parts[0] if parts and isinstance(parts[0], dict) else {}
    kind = source.get("type")
    if kind == "gamebanana":
        return gamebanana(int(source["mod"]))
    if kind == "github_release":
        return github_release(source)
    return None


def main():
    here = os.path.dirname(os.path.abspath(__file__))
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--catalog-dir", default=os.path.join(os.getcwd(), "catalog"),
                    help="folder holding mods.json and textures.json (default: ./catalog)")
    ap.add_argument("--apply", action="store_true", help="write the files (default: dry run)")
    ap.add_argument("--delay", type=float, default=0.5, help="seconds between requests (default 0.5)")
    ap.add_argument("--only", help="only the entry with this id")
    args = ap.parse_args()
    del here

    changed_total = 0
    failures = 0
    for fname, key in FILES:
        path = os.path.join(args.catalog_dir, fname)
        if not os.path.exists(path):
            print(f"{fname}: not found in {args.catalog_dir}", file=sys.stderr)
            continue
        raw = open(path, encoding="utf-8").read()
        data = json.loads(raw)
        changed = 0
        for index, entry in enumerate(data[key]):
            if args.only and entry.get("id") != args.only:
                continue
            try:
                found = lookup(entry)
            except (urllib.error.URLError, RuntimeError, KeyError, ValueError, TimeoutError) as e:
                print(f"{fname}: {entry.get('id')}: {e}", file=sys.stderr)
                failures += 1
                found = None
            if found is None:
                continue
            time.sleep(args.delay)
            new = {k: v for k, v in found.items() if v is not None}
            old = {k: entry.get(k) for k in new}
            if new == old:
                continue
            changed += 1
            print(f"{fname}: {entry['id']}: " + ", ".join(f"{k} {old[k]} -> {new[k]}" for k in new if new[k] != old[k]))
            # "updated" and "downloads" go in front of "source" (or "parts"), whatever else is there
            rebuilt = {}
            placed = False
            for k, v in entry.items():
                if k in new:
                    continue
                if not placed and k in ("source", "parts"):
                    rebuilt.update(new)
                    placed = True
                rebuilt[k] = v
            if not placed:
                rebuilt.update(new)
            data[key][index] = rebuilt
        changed_total += changed
        if args.apply and changed:
            out = json.dumps(data, indent=2, ensure_ascii=False) + "\n"
            if raw.endswith("\n\n"):
                out += "\n"
            with open(path, "w", encoding="utf-8") as f:
                f.write(out)
            print(f"{fname}: wrote {changed} entries")
    print(("applied" if args.apply else "dry run") + f": {changed_total} entries differ, {failures} lookups failed")
    return 0 if failures == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
