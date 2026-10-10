#!/usr/bin/env bash
# Make the side folder the real game is run from when it is used as a
# reference: <Half-Life>/hlds_ref, a "mod" with nothing of its own, which
# falls back to the game's own folder (valve) for everything. Whatever the
# game writes when it is run by these tools -- its config, its screenshots,
# its saves -- goes there, and the player's own valve folder is not touched.
# tools/ref/remove.sh takes it away again.
set -euo pipefail
HL="${HL_DIR:-$HOME/.steam/debian-installation/steamapps/common/Half-Life}"
[ -f "$HL/hl_linux" ] || { echo "no Half-Life at $HL (set HL_DIR)" >&2; exit 1; }
REF="$HL/hlds_ref"
mkdir -p "$REF/maps"
cat > "$REF/liblist.gam" <<'GAM'
// Made by the Half-Life NDS port's tools/ref/setup.sh: the real game, run
// as a reference, with a folder of its own to write in. Safe to delete.
game "HLDS reference"
gamedll "..\valve\dlls\hl.dll"
gamedll_linux "../valve/dlls/hl.so"
type "singleplayer_only"
fallback_dir "valve"
GAM
echo "$REF"
