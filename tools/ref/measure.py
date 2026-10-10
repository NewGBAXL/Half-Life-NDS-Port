#!/usr/bin/env python3
"""measure.py <name> <map> "<x y z yaw>" ["<x y z yaw>" ...]

How bright the real game and the port each draw the same views, as
numbers: both are photographed at each place (tools/ref/compare.sh), each
picture is cut into a grid of patches, and the mean colour of every patch
on both sides goes into build/ref/measure_<name>.csv -- with, at the end, a
summary: how much brighter or darker the port is, dark patches and light
ones apart.

The port must be on the card with <map> as its start map. The bottom of the
picture (the HUD) and its right-hand third (the weapon) are left out.
"""
import os
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
COLS, ROWS = 4, 3


def patches(path, crop, size):
    """Mean R, G, B of each patch of the picture's upper left two thirds."""
    w, h = size
    raw = subprocess.run(['convert', path, '-crop', crop, '+repage', '-resize', '%dx%d!' % (w, h), '-depth', '8',
                          'rgb:-'], capture_output=True, check=True).stdout
    used_w, used_h = w * 2 // 3, h * 3 // 4
    out = []
    for row in range(ROWS):
        for col in range(COLS):
            x0, x1 = used_w * col // COLS, used_w * (col + 1) // COLS
            y0, y1 = used_h * row // ROWS, used_h * (row + 1) // ROWS
            total, n = [0, 0, 0], 0
            for y in range(y0, y1):
                base = (y * w + x0) * 3
                line = raw[base:base + (x1 - x0) * 3]
                for c in range(3):
                    total[c] += sum(line[c::3])
                n += x1 - x0
            out.append([t / n for t in total])
    return out


def main():
    name, game_map, spots = sys.argv[1], sys.argv[2], sys.argv[3:]
    rows = []
    for index, spot in enumerate(spots):
        tag = '%s_%02d' % (name, index)
        done = subprocess.run([os.path.join(ROOT, 'tools/ref/compare.sh'), tag, game_map, spot, 'sleep 0.5; shot'],
                              check=False, stdout=subprocess.DEVNULL)
        if done.returncode == 3:
            sys.exit('the real game did not start: nothing measured (is Steam running?)')
        real = os.path.join(ROOT, 'build/ref', tag, 'real_00.png')
        nds = os.path.join(ROOT, 'build/ref', tag, 'nds_00.png')
        if not (os.path.exists(real) and os.path.exists(nds)):
            print('no pictures for', spot, file=sys.stderr)
            continue
        a = patches(real, '640x480+80+60', (256, 192))
        b = patches(nds, '256x192+0+55', (256, 192))
        for patch, (p, q) in enumerate(zip(a, b)):
            rows.append((spot, patch, p, q))
    out = os.path.join(ROOT, 'build/ref', 'measure_%s.csv' % name)
    with open(out, 'w') as f:
        f.write('spot,patch,real_r,real_g,real_b,nds_r,nds_g,nds_b,real_luma,nds_luma\n')
        for spot, patch, p, q in rows:
            luma = lambda c: 0.299 * c[0] + 0.587 * c[1] + 0.114 * c[2]
            f.write('"%s",%d,%.1f,%.1f,%.1f,%.1f,%.1f,%.1f,%.1f,%.1f\n' % (spot, patch, *p, *q, luma(p), luma(q)))
    # The summary: by how bright the real game's patch is.
    luma = lambda c: 0.299 * c[0] + 0.587 * c[1] + 0.114 * c[2]
    bands = [(0, 24), (24, 48), (48, 80), (80, 128), (128, 256)]
    print('%s: %d patches at %d places' % (name, len(rows), len(spots)))
    for low, high in bands:
        pairs = [(luma(p), luma(q)) for _, _, p, q in rows if low <= luma(p) < high]
        if not pairs:
            continue
        real = sum(p for p, _ in pairs) / len(pairs)
        port = sum(q for _, q in pairs) / len(pairs)
        print('  real %3d..%3d: %3d patches, real %5.1f, port %5.1f (port/real %.2f)' %
              (low, high, len(pairs), real, port, port / real if real else 0))
    print(out)


if __name__ == '__main__':
    main()
