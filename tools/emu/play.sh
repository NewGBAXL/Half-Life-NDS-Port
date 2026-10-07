#!/usr/bin/env bash
# usage: play.sh <ds|dsi> "<script>"     (see README.md)
# EMU_START="x y z yaw" starts the player there (whole units, degrees), armed;
# a fifth number starts that track of the music.
# EMU_AUDIO=<file> captures the sound (see below).
set -u
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
MODE="${1:?ds or dsi}"
CFG="$ROOT/build/emu/cfg_$MODE"
[ -d "$CFG" ] || "$ROOT/tools/emu/setup.sh" >/dev/null
cd "$ROOT"
# A save left by an earlier run would be gone back to instead of the start
# map: EMU_KEEP_SAVE=1 keeps it (to test exactly that).
[ -n "${EMU_KEEP_SAVE:-}" ] || rm -f "${ROOT:?}/sdroot/_nds/hl_nds/data/saves/slot0.sav"
START_FILE="$ROOT/sdroot/_nds/hl_nds/data/debug_start.txt"
[ -n "${EMU_START:-}" ] && echo "$EMU_START" > "$START_FILE"
trap 'rm -f "$START_FILE"' EXIT
# EMU_AUDIO=<file> writes what the emulator would have played to a file
# instead of to the sound card (raw samples: SDL's "disk" driver), so that
# sound can be looked at without being heard.
if [ -n "${EMU_AUDIO:-}" ]; then
    export SDL_AUDIODRIVER=disk SDL_DISKAUDIOFILE="$EMU_AUDIO"
    rm -f "$EMU_AUDIO"
fi
env -u WAYLAND_DISPLAY QT_QPA_PLATFORM=xcb XDG_CONFIG_HOME="$CFG" EMU_SCRIPT="${2:-sleep 5}" \
    xvfb-run -a -s "-screen 0 300x450x24" bash -c \
    '"${MELONDS:-melonDS}" half-life-nds-port.nds >"'"$CFG"'/melonDS.log" 2>&1 &
     python3 tools/emu/xin.py "$EMU_SCRIPT"; kill %1' 2>/dev/null || true
