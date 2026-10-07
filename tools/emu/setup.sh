#!/usr/bin/env bash
# Make private melonDS configurations for the test scripts: one for DS mode,
# one for DSi mode, both with this repository's sdroot/ as the SD card.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
SOURCE="${XDG_CONFIG_HOME:-$HOME/.config}/melonDS/melonDS.toml"
[ -f "$SOURCE" ] || { echo "no melonDS configuration at $SOURCE: run melonDS once first" >&2; exit 1; }
mkdir -p "$ROOT/sdroot"
for mode in ds dsi; do
    mkdir -p "$ROOT/build/emu/cfg_$mode/melonDS"
    python3 - "$SOURCE" "$ROOT" "$mode" <<'PY'
import sys
source, root, mode = sys.argv[1:4]
out = []
section = ''
for line in open(source, encoding='utf-8'):
    stripped = line.strip()
    if stripped.startswith('['):
        section = stripped.strip('[]')
    key = stripped.split('=')[0].strip() if '=' in stripped else ''
    # The card the homebrew sees: DLDI in DS mode, the SD slot in DSi mode.
    if section in ('DLDI', 'DSi.SD'):
        if key == 'FolderPath': line = 'FolderPath = "%s/sdroot"\n' % root
        elif key == 'FolderSync': line = 'FolderSync = true\n'
        elif key == 'ReadOnly': line = 'ReadOnly = false\n'
        elif key == 'ImagePath': line = 'ImagePath = "%s/build/emu/cfg_%s/%s.bin"\n' % (root, mode, section.replace('.', '_').lower())
        elif key == 'ImageSize': line = 'ImageSize = 0\n'
        elif key == 'Enable': line = 'Enable = true\n'
    if key == 'ConsoleType':
        line = 'ConsoleType = %d\n' % (1 if mode == 'dsi' else 0)
    out.append(line)
open('%s/build/emu/cfg_%s/melonDS/melonDS.toml' % (root, mode), 'w', encoding='utf-8').write(''.join(out))
PY
done
echo "private melonDS configurations written to $ROOT/build/emu/"
