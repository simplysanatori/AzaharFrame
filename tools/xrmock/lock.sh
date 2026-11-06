# Sourced by build.sh and ../mockrun.sh: the shared-machine guard.
# cf_wait_and_lock [--games]: waits (polling every 5 s) while the headset is in
# use, a lasrecomp run is going (or, with --games, a Steam game is running),
# then takes ~/.cache/af-bench-lock.  The lock is released by cf_unlock and, if
# the script dies first, by the EXIT trap.  Hold it for the run only.
CF_LOCK_FILE=${CF_LOCK_FILE:-$HOME/.cache/af-bench-lock}
CF_HAVE_LOCK=0

cf_unlock() {
    if [ "$CF_HAVE_LOCK" = 1 ]; then
        rm -f "$CF_LOCK_FILE"
        CF_HAVE_LOCK=0
    fi
}

cf_busy() {
    [ -e "$HOME/.cache/headset-in-use" ] && { echo "headset in use"; return 0; }
    pgrep -x lasrecomp >/dev/null 2>&1 && { echo "lasrecomp running"; return 0; }
    # [S] keeps pgrep from matching a shell whose command line holds this text.
    if [ "$1" = "--games" ] && pgrep -f '[S]teamLaunch' >/dev/null 2>&1; then
        echo "a Steam game is running"
        return 0
    fi
    return 1
}

cf_wait_and_lock() {
    mkdir -p "$(dirname "$CF_LOCK_FILE")"
    trap cf_unlock EXIT
    local why last=""
    while true; do
        if why=$(cf_busy "$1"); then
            [ "$why" != "$last" ] && echo "waiting: $why" >&2
            last=$why
        elif ( set -o noclobber; echo citraframe > "$CF_LOCK_FILE" ) 2>/dev/null; then
            CF_HAVE_LOCK=1
            return 0
        else
            [ "$last" != lock ] && echo "waiting: $CF_LOCK_FILE is held" >&2
            last=lock
        fi
        sleep 5
    done
}
