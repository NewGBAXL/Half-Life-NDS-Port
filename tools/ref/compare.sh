#!/usr/bin/env bash
# usage: compare.sh <name> <map> "<x y z yaw>" "<script>"
#
# The same thing done in the real game and in the port, and the pictures of
# both side by side: build/ref/<name>.png.
#
# The script is tools/emu/xin.py's (`sleep`, `key a 0.1` -- a is attack on
# both sides -- `shot`). Write `shot` with no file name: each side numbers
# its own. The port must be on the card with <map> as its start map
# (hlds-convert cook-map ... --start <map>), and is started armed at the
# place, as tools/emu/play.sh does with EMU_START; the real game is given
# every weapon and has the crowbar in hand, so a script for the port begins
# with whatever puts its crowbar in its hand (it starts with it).
#
# The pictures are the game's: they are kept under build/, which is not in
# the repository.
set -u
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
NAME="${1:?name}"
MAP="${2:?map}"
AT="${3:?x y z yaw}"
SCRIPT="${4:?script}"
OUT="$ROOT/build/ref/$NAME"
mkdir -p "$OUT"
rm -f "$OUT"/real_*.png "$OUT"/nds_*.png

# `shot` -> `shot <dir>/<side>_NN.png`, in order.
number() {
    python3 - "$1" "$2" <<'PY'
import sys
side_dir, script = sys.argv[1], sys.argv[2]
out, n = [], 0
for part in script.split(';'):
    if part.strip() == 'shot':
        out.append(' shot %s_%02d.png' % (side_dir, n))
        n += 1
    else:
        out.append(part)
print(';'.join(out))
PY
}

"$ROOT/tools/ref/play.sh" "$MAP" "$AT" "$(number "$OUT/real" "$SCRIPT")" >/dev/null 2>"$OUT/real.err" || {
    # (Nothing to hold the port's picture against: said, and not gone on with.)
    echo "the real game did not start: $(tail -n 1 "$OUT/real.err")" >&2
    exit 3
}
EMU_START="$AT" "$ROOT/tools/emu/play.sh" "${NDS_MODE:-ds}" "sleep ${NDS_WAIT:-32};$(number "$OUT/nds" "$SCRIPT")" >/dev/null 2>&1

# The game's picture out of its screen, the port's top screen out of its
# window, each to the same height, in pairs.
ROWS=()
for real in "$OUT"/real_*.png; do
    [ -f "$real" ] || continue
    nds="$OUT/nds_${real##*real_}"
    convert "$real" -crop 640x480+80+60 +repage -resize 512x384 "$OUT/a.png"
    if [ -f "$nds" ]; then
        convert "$nds" -crop 256x192+0+55 +repage -filter point -resize 512x384 "$OUT/b.png"
    else
        convert -size 512x384 xc:black "$OUT/b.png"
    fi
    row="$OUT/row_${real##*real_}"
    convert "$OUT/a.png" "$OUT/b.png" +append "$row"
    ROWS+=("$row")
done
[ ${#ROWS[@]} -gt 0 ] && convert "${ROWS[@]}" -append "$ROOT/build/ref/$NAME.png"
rm -f "$OUT/a.png" "$OUT/b.png" "$OUT"/row_*.png
echo "$ROOT/build/ref/$NAME.png"
