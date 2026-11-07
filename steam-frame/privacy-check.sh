#!/bin/bash
# Looks for personal data and game files before anything is published.  Adapted from AwakeningFrame's and
# GalaxyFrame's privacy checkers.
#   steam-frame/privacy-check.sh                 the tree (steam-frame/, files this work added, new files) and our commits:
#                                                added lines, identities and messages
#   steam-frame/privacy-check.sh --tree          only the tree
#   steam-frame/privacy-check.sh --history       only our commits (those not reachable from the azahar and citravr remotes
#                                                or from a tag), plus uncommitted changes; AZAHARFRAME_PRIVACY_REVS="--branches"
#                                                scans from those tips instead of HEAD
#   steam-frame/privacy-check.sh DIR|FILE...     every file under each DIR, inside each .tar.gz / .zip, or the plain FILE (the unpacked
#                                                release archives, the FrameDrop zip); add --tree / --history to scan
#                                                those as well
# Upstream's own files and commits carry their authors' names and e-mail addresses, so only what this work added is scanned.
# Prints each finding (the line, except for the personal patterns, whose matches are never printed) and exits 1 when
# there is one.
#
# Built-in checks: e-mail addresses other than the maintainer's GitHub no-reply address; LAN addresses (192.168.); home
# folders (/home/steamos and any other); 3DS game file names (a name followed by .3ds / .cci / .cia, or a region tag such as
# (USA), (Europe) or (En,); a bare ".3ds" in a list of file types is fine, AZAHARFRAME_PRIVACY_STRICT_ROM=1 flags those
# too); access tokens and private keys; commit identities other than the no-reply address; and Co-Authored-By trailers.
#
# Personal values (the maintainer's real name, the home network's SSID, other host and user names) must not be written
# into the repository, not even as search patterns.  Put them, one case-insensitive extended regular expression per line,
# in a file outside it:
#   AZAHARFRAME_PRIVACY_PATTERNS=FILE    (default ~/.config/moonlight-xr-privacy-patterns.txt, the list the other projects read)
# Without that list the script stops (exit 2), because a clean result would mean little; AZAHARFRAME_PRIVACY_NO_LIST=1
# runs the built-in checks alone.
# Known harmless hits go in steam-frame/privacy-allow.txt: a path regex, a tab, and a regex for the text to ignore on lines
# of the files whose path matches.
set -uo pipefail
HERE=$(cd "$(dirname "$0")" && pwd)
cd "$HERE/.." || exit 2
PAT=${AZAHARFRAME_PRIVACY_PATTERNS:-$HOME/.config/moonlight-xr-privacy-patterns.txt}
if [ ! -f "$PAT" ] && [ -z "${AZAHARFRAME_PRIVACY_NO_LIST:-}" ]; then
    echo "privacy-check: no personal pattern list at $PAT (AZAHARFRAME_PRIVACY_PATTERNS=FILE names one; AZAHARFRAME_PRIVACY_NO_LIST=1 runs the built-in checks alone)" >&2
    exit 2
fi
python3 -I - "$PAT" "$HERE/privacy-allow.txt" "$@" << 'PY'
import io, os, re, subprocess, sys, tarfile, tempfile, zipfile

patp, allowp, *args = sys.argv[1:]
do_tree = "--tree" in args
do_hist = "--history" in args
paths = [a for a in args if not a.startswith("--")]
if not do_tree and not do_hist and not paths:
    do_tree = do_hist = True
OK_EMAIL = "149214925+simplysanatori@users.noreply.github.com"
SELF = ("steam-frame/privacy-check.sh", "steam-frame/privacy-allow.txt")
strict_rom = bool(os.environ.get("AZAHARFRAME_PRIVACY_STRICT_ROM"))

def rx(p, f=0): return re.compile(p, f)
text_checks = [
    ("email", rx(r"[A-Za-z0-9._%+-]+@[A-Za-z0-9-]+(?:\.[A-Za-z0-9-]+)*\.[A-Za-z]{2,}")),
    ("LAN address", rx(r"192\.168\.")),
    ("home folder", rx(r"/home/[A-Za-z_][A-Za-z0-9_-]*")),
    ("3DS game file name", rx(r"\.(?:3ds|cci|cia)\b" if strict_rom else r"[\w)\]]\.(?:3ds|cci|cia)\b", re.I)),
    ("3DS game region tag", rx(r"\((?:USA|Europe)\)|\(En,")),
    ("token", rx(r"\b(?:ghp|gho|ghu|ghs|ghr)_[A-Za-z0-9]{20,}|\bgithub_pat_[A-Za-z0-9_]{20,}|\bsk-[A-Za-z0-9_-]{20,}|\bAKIA[0-9A-Z]{16}\b|\bxox[abps]-[A-Za-z0-9-]{10,}")),
    ("private key", rx(r"-----BEGIN (?:RSA |EC |OPENSSH |ENCRYPTED )?PRIVATE KEY-----")),
]
# in binaries: the checks that make sense on bytes
bin_names = {"home folder", "LAN address", "token", "private key", "3DS game region tag"}
personal = []
if os.path.isfile(patp):
    for l in open(patp, encoding="utf-8"):
        l = l.strip()
        if l and not l.startswith("#"): personal.append(re.compile(l, re.I))
print("privacy-check: %d personal patterns%s" % (len(personal), "" if personal else " (none: built-in checks only)"))

allow = []
if os.path.isfile(allowp):
    for l in open(allowp, encoding="utf-8"):
        if l.startswith("#") or "\t" not in l: continue
        a, b = l.rstrip("\n").split("\t", 1)
        allow.append((re.compile(a), re.compile(b, re.I)))
found = 0
def scan_line(line, path, names=None):
    """Hits (category, excerpt-or-None) in one line."""
    for pr, tr in allow:
        if pr.search(path): line = tr.sub(" ", line)
    hits = []
    for name, r in text_checks:
        if names is not None and name not in names: continue
        for m in r.finditer(line):
            if name == "email" and m.group(0).lower() == OK_EMAIL: continue
            hits.append((name, line.strip()[:140]))
            break
    for i, r in enumerate(personal):
        if r.search(line): hits.append(("personal pattern %d" % (i + 1), None))
    return hits
def report(where, hits):
    global found
    for cat, ex in hits:
        found += 1
        print("  %s: %s%s" % (where, cat, ": " + ex if ex else ""))

def git(*a):
    return subprocess.run(("git",) + a, capture_output=True, text=True, errors="replace").stdout

def scan_name(rel):
    return scan_line(rel, "", {"3DS game file name", "3DS game region tag", "home folder", "email"})

def scan_data(rel, data):
    n = 0
    if b"\0" in data[:4096]:
        lines = data.decode("latin-1").splitlines(); names = bin_names
    else:
        lines = data.decode("utf-8", "replace").splitlines(); names = None
    for no, line in enumerate(lines, 1):
        report("%s:%d" % (rel, no), scan_line(line, rel, names))
        n += 1
    return n

# ---- the tree: steam-frame/, files our commits added, new files
if do_tree or do_hist:
    OURS = os.environ.get("AZAHARFRAME_PRIVACY_REVS", "HEAD").split() + ["--not", "--remotes=azahar", "--remotes=citravr", "--tags"]   # a range: ours, not upstream's
    revs = git("rev-list", *OURS).split()
    if not revs:
        print("privacy-check: no commits of ours found (are the azahar and citravr remotes fetched?)", file=sys.stderr)
        sys.exit(2)
if do_tree:
    files = set(git("ls-files", "steam-frame").splitlines())
    files |= set(git("ls-files", "-o", "--exclude-standard").splitlines())
    tracked = set(git("ls-files").splitlines())
    for l in git("log", "--diff-filter=A", "--name-only", "--format=", *OURS).splitlines():
        if l in tracked: files.add(l)
    files -= set(SELF)
    n = nfiles = 0
    for rel in sorted(files):
        if rel.startswith("third_party/") or rel.startswith("externals/") or not os.path.isfile(rel): continue
        report(rel + " (name)", scan_name(rel))
        n += scan_data(rel, open(rel, "rb").read()); nfiles += 1
    print("privacy-check: tree: %d files, %d lines" % (nfiles, n))

# ---- our commits: identities, messages, added lines, uncommitted changes
if do_hist:
    meta = git("log", "--format=@@@%h%n%an <%ae>%n%cn <%ce>%n%B%n@@@END", *OURS)
    ncommits = 0
    for block in meta.split("@@@")[1:]:
        lines = block.split("\n")
        if lines[0] == "END": continue
        ncommits += 1; h = lines[0]
        for who in lines[1:3]:
            m = re.search(r"<([^>]*)>", who)
            if not m or m.group(1).lower() != OK_EMAIL:
                report("commit %s" % h, [("identity is not the GitHub no-reply address", None)])
        for no, line in enumerate(lines[3:], 1):
            if re.search(r"co-authored-by|generated with|claude|anthropic", line, re.I):
                report("commit %s message line %d" % (h, no), [("AI trailer or mention", line.strip()[:140])]); continue
            hits = scan_line(line, "", {"email", "LAN address", "home folder", "3DS game region tag", "token"})
            report("commit %s message line %d" % (h, no), hits)
        report("commit %s message" % h, [h2 for h2 in scan_line("\n".join(lines[3:]).replace("\n", " "), "", {"3DS game file name"})])
    diff = git("log", "-p", "-U0", "--no-color", "--no-ext-diff", "--format=@@@COMMIT %h", *OURS)
    diff += "@@@COMMIT worktree\n" + git("diff", "-U0", "--no-color", "--no-ext-diff", "HEAD")
    commit = path = None; skip = False; ln = 0; added = 0
    for line in diff.split("\n"):
        if line.startswith("@@@COMMIT "): commit = line[10:]; continue
        if line.startswith("+++ "):
            path = line[6:] if line.startswith("+++ b/") else line[4:]
            skip = path in SELF or path == "/dev/null" or path.startswith(("third_party/", "externals/")); continue
        if line.startswith("@@ "):
            m = re.match(r"@@ -\S+ \+(\d+)", line); ln = int(m.group(1)) if m else 0; continue
        if skip or not line.startswith("+") or line.startswith("+++"): continue
        added += 1
        report("commit %s %s:%d" % (commit, path, ln), scan_line(line[1:], path))
        ln += 1
    print("privacy-check: history: %d commits, %d added lines (plus uncommitted changes)" % (ncommits, added))

# ---- unpacked release archives, FrameDrop zip, plain folders
for p in paths:
    tmp = None
    base = p
    if os.path.isfile(p):
        tmp = tempfile.mkdtemp(prefix="privacy-check-"); base = tmp
        if not p.endswith((".zip", ".tar", ".tar.gz", ".tgz", ".tar.xz")):    # a single plain file, such as the json
            import shutil; shutil.copy(p, tmp)
        elif p.endswith(".zip"):
            with zipfile.ZipFile(p) as z: z.extractall(tmp)
        else:
            with tarfile.open(p) as t: t.extractall(tmp, filter="data")
    elif not os.path.isdir(p):
        print("privacy-check: no such file or folder: %s" % p, file=sys.stderr); sys.exit(2)
    label = os.path.basename(p.rstrip("/"))
    n = nfiles = 0
    for d, dn, fn in os.walk(base):
        if ".git" in dn: dn.remove(".git")
        for f in fn:
            full = os.path.join(d, f); rel = os.path.relpath(full, base)
            if os.path.islink(full): continue
            report("%s/%s (name)" % (label, rel), scan_name(rel))
            n += scan_data("%s/%s" % (label, rel), open(full, "rb").read()); nfiles += 1
    print("privacy-check: %s: %d files, %d lines" % (label, nfiles, n))
    if tmp:
        import shutil; shutil.rmtree(tmp, ignore_errors=True)

print("privacy-check: %d finding(s)" % found)
sys.exit(1 if found else 0)
PY
