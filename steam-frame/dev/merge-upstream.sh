#!/bin/bash
# Merge an upstream Azahar ref into the current branch and keep the removed upstream
# paths (steam-frame/dev/removed-upstream-paths.txt, the Android app) deleted.
# Usage: steam-frame/dev/merge-upstream.sh [--no-commit] <ref>   e.g. azahar/master, 2127.0-rc3
# Fetches the azahar remote, merges <ref>, resolves every modify/delete or add of a
# removed path by deleting it, then lists the remaining real conflicts and stops.
# With no conflicts left it commits (unless --no-commit).
set -u
COMMIT=1
[ "${1:-}" = "--no-commit" ] && { COMMIT=0; shift; }
REF=${1:?usage: merge-upstream.sh [--no-commit] <ref>}
cd "$(git rev-parse --show-toplevel)"
LIST=steam-frame/dev/removed-upstream-paths.txt
[ -z "$(git status --porcelain --untracked-files=no | grep -v '^ m')" ] || { echo "working tree not clean"; exit 1; }
git fetch --tags azahar || exit 1
git rev-parse -q --verify "$REF^{commit}" >/dev/null || { echo "unknown ref $REF"; exit 1; }
git merge --no-commit --no-ff "$REF" 2>&1 | tail -n 15
git rev-parse -q --verify MERGE_HEAD >/dev/null || { echo "no merge in progress (already up to date?)"; exit 0; }

# Drop the removed submodules' sections if a merge brought them back.
while IFS= read -r p; do
    [[ -z "$p" || "$p" == \#* ]] && continue
    # a gitlink or file/dir that the merge added or left unmerged
    if [ -n "$(git ls-files -- "$p")" ]; then
        n=$(git ls-files -- "$p" | wc -l)
        git rm -rqf --ignore-unmatch -- "$p" && echo "kept deleted: $p ($n files)"
    fi
    rm -rf -- "$p" 2>/dev/null
    name=$(git config -f .gitmodules --get-regexp '^submodule\..*\.path$' 2>/dev/null | awk -v p="$p" '$2==p{print $1}' | sed 's/^submodule\.//;s/\.path$//')
    if [ -n "$name" ]; then
        git config -f .gitmodules --remove-section "submodule.$name" && git add .gitmodules
        echo "dropped .gitmodules entry: $name"
    fi
done < "$LIST"

REAL=$(git diff --name-only --diff-filter=U)
if [ -n "$REAL" ]; then
    echo; echo "REAL CONFLICTS (resolve by hand, then git commit):"; echo "$REAL" | sed 's/^/  /'
    exit 2
fi
if [ $COMMIT = 1 ]; then
    git commit -q -m "Merge $REF (upstream Azahar); removed Android paths kept deleted" && echo "committed $(git rev-parse --short HEAD)"
else
    echo "merge staged, no conflicts; review and git commit"
fi
