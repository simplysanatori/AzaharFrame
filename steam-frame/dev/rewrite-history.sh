#!/bin/bash
# Rewrites our history for publishing, on a MIRROR CLONE: never in the working repository, and it never pushes.
#   steam-frame/dev/rewrite-history.sh [--source REPO] [--mirror DIR]
# What it does to the commits that are ours (reachable from a branch or from origin, but not from the azahar and
# citravr remotes or a tag), and nothing to upstream's:
#   - strips every "Co-Authored-By:" trailer from the messages;
#   - removes the sentence in src/frame_xr/IMMERSIVE.md that names the test game's file (it was added by one commit
#     and removed by a later one; branches that were cut in between still carry it) from every commit that has it;
#   - rewords the merge subjects that name a lane ("Merge branch 'lane/x' into lane/y", "Merge lane/x (...)") into a plain
#     description of what the merge brought in (the table in MERGE_SUBJECTS below; a merge that is not in the table gets
#     "Merge: <the subject of the commit it merged>" and a warning, so add it to the table);
# and then checks, on the mirror: upstream's refs and commits are exactly as before, no Co-Authored-By is left, our
# trees are unchanged except for that one file, and steam-frame/privacy-check.sh finds nothing over all our commits
# (messages, identities, every line of every diff).  Prints the old -> new subjects.
# --source defaults to the repository this script is in; --mirror to ~/Games/AzaharFrame/release-history (deleted
# and cloned afresh each run, so it must be a folder this script made, or not exist).
# The result is for review.  Publishing it (git push --force-with-lease of the wanted branches from the mirror)
# is a separate, deliberate step that this script does not do.
set -euo pipefail
HERE=$(cd "$(dirname "$0")" && pwd)
SF=$(cd "$HERE/.." && pwd)
SOURCE=$(cd "$SF/.." && pwd)
MIRROR=$HOME/Games/AzaharFrame/release-history
while [ $# -gt 0 ]; do
    case "$1" in
        --source) SOURCE=$(realpath "$2"); shift 2 ;;
        --mirror) MIRROR=$(realpath -m "$2"); shift 2 ;;
        *) echo "rewrite-history: unknown argument $1" >&2; exit 2 ;;
    esac
done
case "$MIRROR/" in "$SOURCE"/*|"$SOURCE/") echo "rewrite-history: the mirror must not be inside the source" >&2; exit 2 ;; esac
if [ -e "$MIRROR" ] && [ ! -f "$MIRROR/.rewrite-history-mirror" ]; then
    echo "rewrite-history: $MIRROR exists and is not a mirror this script made; remove it or name another" >&2; exit 2
fi
TMP=$(mktemp -d); trap 'rm -rf "$TMP"' EXIT

# ---- the mirror ---------------------------------------------------------------------------------------------------
rm -rf "$MIRROR"
git clone --quiet --mirror --no-hardlinks "$SOURCE" "$MIRROR"
touch "$MIRROR/.rewrite-history-mirror"
export GIT_DIR=$MIRROR
# nothing in here can push anywhere (not "git remote remove": with a mirror's refspec that deletes every ref)
git config remote.origin.pushurl "no-push-from-the-dry-run"
git config --unset-all remote.origin.fetch
UP=(--remotes=azahar --remotes=citravr --tags)
TIPS=(--branches --remotes=origin)                    # origin's refs are only there if the source has them
git for-each-ref --format='%(refname) %(objectname)' > "$TMP/refs.before"
git for-each-ref --format='%(refname) %(tree)' refs/heads refs/remotes/origin > "$TMP/trees.before"
git rev-list "${UP[@]}" | sort > "$TMP/upstream.before"
git rev-list "${TIPS[@]}" --not "${UP[@]}" -- > "$TMP/ours.before"
git log --topo-order --format='%H%x09%s' "${TIPS[@]}" --not "${UP[@]}" -- > "$TMP/subjects.before"
echo "rewrite-history: $(wc -l < "$TMP/ours.before") commits of ours, $(wc -l < "$TMP/upstream.before") of upstream's"

# ---- the filters --------------------------------------------------------------------------------------------------
cat > "$TMP/msg.py" << 'PY'
import os, re, sys
MAP = os.environ["RH_MAP"]
MERGE_SUBJECTS = {
    # first characters of the old commit id: the new subject
    "ecbd95741": "Merge the portable build: the program with its own Qt, the bundle check and the host-library allowlist",
    "8aa4e8221": "Merge the release tooling: FrameDrop zip, privacy check and installer test",
    "d8d2df46f": "Merge the VR focus fix: keep every other top-level window off the gamescope screen",
    "c64018f6f": "Merge the immersive follow-up: a HUD-safe default and bigger sizes without widening the game's view",
    "d2c6e89a8": "Merge controls help, controller models, beams and stats panel",
    "6fc20a3ee": "Merge Quick Save/Load, Passthrough background, VR toolbar and immersive presets",
    "83b994459": "Merge the Steam Frame branch's screen settings, grip and bumper remap and seeded textures into the mirror work",
    "aba46e365": "Merge per-game Screen Shape and Immersive Game FOV",
    "946049169": "Merge seeded custom textures and the texture pack docs",
    "d910dc6ed": "Merge the grips, bumpers and triggers remap and the per-game Hide Bottom Screen setting",
    "b143771b7": "Merge docs, privacy audit, installer tweaks and the changelog refresh",
    "33c652298": "Merge the picker refresh when a game folder is added",
    "76ddfafa5": "Merge Frame icons, the picker logo and the desktop file name",
    "b1ed87775": "Merge the XR mock runtime and headless test tools into the Steam Frame branch",
    "c75001811": "Merge packaging and docs into the Steam Frame branch",
    "095799f8f": "Merge the in-VR application picker into the OpenXR core work",
    "c2baeb8dd": "Merge the controller input engine into the OpenXR core work",
    "85afd2abc": "Merge the 1.0.0 docs: README for players, controls checked against the code",
    "a033d7906": "Merge the XR screens handoff: the emulator thread never waits for the handoff",
    "afeb6d39e": "Merge the XR screens handoff fix: return a held image before its swapchain is destroyed",
    "2329efc29": "Merge Quick Save, Quick Load and Fast Forward on release, and flat rendering after leaving VR",
    "d494f0e11": "Merge the Controls panel diagram: the Frame controllers with a line to each button",
    "7f3439ec6": "Merge the README note on the fork, the settings page and the controls picture",
    "48bc1314b": "Merge the screen nudge (grip + D-pad), the immersive lens offset and bigger normal screens",
}
msg = sys.stdin.read()
lines = [l for l in msg.split("\n") if not re.match(r"(?i)\s*co-authored-by:", l)]
while lines and not lines[-1].strip(): lines.pop()
subject = lines[0] if lines else ""
new = subject
old = os.environ.get("GIT_COMMIT", "")
if re.match(r"(?i)^merge\b", subject) and "lane/" in subject:
    for k, v in MERGE_SUBJECTS.items():
        if old.startswith(k): new = v; break
    else:
        # not in the table: describe it by the commit it merged
        import subprocess
        parents = subprocess.run(["git", "rev-list", "--parents", "-n", "1", old], capture_output=True, text=True).stdout.split()[1:]
        what = subprocess.run(["git", "log", "-1", "--format=%s", parents[-1]], capture_output=True, text=True).stdout.strip() if len(parents) > 1 else ""
        new = "Merge: " + (what or "work from a side branch")
        print("rewrite-history: WARNING: merge %s is not in the table, now \"%s\"" % (old[:9], new), file=sys.stderr)
    lines[0] = new
if new != subject or lines != msg.rstrip("\n").split("\n"):
    with open(MAP, "a") as f: f.write("%s\t%s\t%s\n" % (old, subject, new))
sys.stdout.write("\n".join(lines) + "\n")
PY
cat > "$TMP/idx.py" << 'PY'
# the sentence that names the test game's file, in the immersive mode notes: from "<game> is at `<path>`." to the
# end of that sentence, wherever the file wraps the name
import re, subprocess, sys
PATH = "src/frame_xr/IMMERSIVE.md"
SENTENCE = re.compile(r"Ocarina of Time 3D is at `[^`]*`\.[ \t]*\n?")
ls = subprocess.run(["git", "ls-files", "-s", "--", PATH], capture_output=True, text=True).stdout.split()
if len(ls) >= 4:
    mode, sha = ls[0], ls[1]
    data = subprocess.run(["git", "cat-file", "blob", sha], capture_output=True).stdout.decode("utf-8", "surrogateescape")
    new = SENTENCE.sub("", data)
    if new != data:
        out = subprocess.run(["git", "hash-object", "-w", "--stdin"], input=new.encode("utf-8", "surrogateescape"), capture_output=True).stdout.decode().strip()
        subprocess.run(["git", "update-index", "--cacheinfo", "%s,%s,%s" % (mode, out, PATH)], check=True)
PY
export RH_MAP=$TMP/map
: > "$RH_MAP"
FILTER_BRANCH_SQUELCH_WARNING=1 git filter-branch -f \
    --msg-filter "python3 -I $TMP/msg.py" \
    --index-filter "python3 -I $TMP/idx.py" \
    -- "${TIPS[@]}" --not "${UP[@]}" > "$TMP/filter.log" 2>&1 || { tail -n 30 "$TMP/filter.log" >&2; echo "rewrite-history: filter-branch failed" >&2; exit 1; }
grep -E "WARNING|rewrite-history" "$TMP/filter.log" | head || true
git for-each-ref --format='%(refname)' refs/original | xargs -r -n1 git update-ref -d
git reflog expire --expire=now --all
git gc --quiet --prune=now

# ---- the checks ---------------------------------------------------------------------------------------------------
fail=0
bad() { echo "CHECK FAILED: $*"; fail=1; }
git rev-list "${UP[@]}" | sort > "$TMP/upstream.after"
cmp -s "$TMP/upstream.before" "$TMP/upstream.after" || bad "upstream's commits changed"
git for-each-ref --format='%(refname) %(objectname)' refs/remotes/azahar refs/remotes/citravr refs/tags | sort > "$TMP/up-refs.after"
grep -E '^refs/(remotes/(azahar|citravr)|tags)/' "$TMP/refs.before" | sort | cmp -s - "$TMP/up-refs.after" || bad "an upstream ref or a tag moved"
echo "check: upstream: $(wc -l < "$TMP/upstream.after") commits and $(wc -l < "$TMP/up-refs.after") refs unchanged"
git rev-list "${TIPS[@]}" --not "${UP[@]}" -- > "$TMP/ours.after"
[ "$(wc -l < "$TMP/ours.before")" = "$(wc -l < "$TMP/ours.after")" ] || bad "the number of our commits changed"
echo "check: our commits: $(wc -l < "$TMP/ours.after") (before $(wc -l < "$TMP/ours.before")), none dropped or added"
left=$(git log "${TIPS[@]}" --not "${UP[@]}" --format=%B -- | grep -ciE '^[[:space:]]*co-authored-by:' || true)
[ "$left" = 0 ] || bad "$left Co-Authored-By lines left"
echo "check: Co-Authored-By lines left: $left"
lane=$(git log "${TIPS[@]}" --not "${UP[@]}" --format=%s -- | grep -ci "^merge.*lane/" || true)
[ "$lane" = 0 ] || bad "$lane merge subjects still name a lane"
echo "check: merge subjects that still name a lane: $lane"
# every branch tip: the same tree as before, except where the removed sentence was
while read -r ref ot; do
    nt=$(git rev-parse -q --verify "$ref^{tree}" || true)
    [ -n "$nt" ] || { bad "$ref is gone"; continue; }
    [ "$ot" = "$nt" ] || echo "check: note: the tree of $ref differs from before (the removed sentence)"
done < "$TMP/trees.before"
# the privacy check over every commit of ours on every branch
PC_OUT=$(AZAHARFRAME_PRIVACY_REVS="--branches --remotes=origin" "$SF/privacy-check.sh" --history 2>&1) && pc=0 || pc=$?
echo "$PC_OUT" | grep -vE "^  " | sed 's/^/privacy-check: /'
if [ "$pc" != 0 ]; then echo "$PC_OUT" | grep -E '^  ' | cut -c1-200; bad "the privacy check found something"; fi

# ---- the report -----------------------------------------------------------------------------------------------------
echo
echo "reworded subjects (old -> new):"
awk -F'\t' '$2 != $3 { printf "  %.9s  %s  ->  %s\n", $1, $2, $3 }' "$RH_MAP" | sort -u
echo
echo "messages that lost Co-Authored-By lines: $(awk -F'\t' '$2 == $3' "$RH_MAP" | wc -l) (plus the reworded ones above that also had one)"
if [ "$fail" = 0 ]; then echo "rewrite-history: DRY RUN OK, in $MIRROR (nothing was pushed)"; else echo "rewrite-history: CHECKS FAILED, in $MIRROR (nothing was pushed)"; exit 1; fi
