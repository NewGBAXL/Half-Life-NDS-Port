# Emulator test helpers

Scripts that run the ROM in melonDS without a display, press buttons, and take
screenshots. They are for development; nothing here is needed to build or play.

Requirements: melonDS, `xvfb-run`, ImageMagick (`import`), Python 3 with the
X11 and XTest libraries present (`libX11.so.6`, `libXtst.so.6`).

```sh
export MELONDS=/path/to/melonDS      # if it is not on PATH
tools/emu/setup.sh                   # once: private emulator config in build/emu/
tools/emu/play.sh ds "sleep 9;shot /tmp/a.png"
```

`setup.sh` copies your melonDS configuration to `build/emu/` and points the
copies' SD card at this repository's `sdroot/`, with card images of their own.
Your own configuration and card images are never written to.

`play.sh <ds|dsi> "<script>"` runs the ROM and feeds it a script of commands
separated by `;`:

| command | |
|---|---|
| `sleep <s>` | wait |
| `ready [s]` | wait until the game is on screen (the emulator can take half a minute over the card first; at most `s` seconds, 90 if not given) |
| `key <keysym> <s>` | hold a key (your melonDS key mapping applies) |
| `drag <x0> <y0> <x1> <y1>` | stylus drag, in window pixels |
| `tap <x> <y>` | a quick touch (two close together are a jump) |
| `sweep <s> <px per step>` | hold the stylus and sweep left-right for a time |
| `shot <file>` | screenshot of both screens |
| `burst <dir> <n>` | n screenshots as fast as they come |
| `sweepburst <dir> <n> <px>` | n screenshots while the view sweeps |

`EMU_AUDIO=<file>` before `play.sh` writes the emulator's sound to that file
instead of playing it: raw 16-bit stereo samples at 48 kHz (SDL's "disk" audio
driver), which can be measured, or listened to with
`ffplay -f s16le -ar 48000 -ac 2 <file>`.

`EMU_START="x y z yaw [track [mode]]"` starts the player at a place, armed
and wearing the suit, with the debug readout on: `track` starts that track of
the music (-1 for none) and `mode` says how to draw (1 single-pass, 2
two-pass, 3 two-pass DMA).

The private card images are a gigabyte each (`setup.sh` says so to melonDS):
a whole cooked campaign is a quarter of that, and an image that is full
silently leaves new files out. If something on `sdroot/` is "not on the
card" in the emulator, delete `build/emu/cfg_*/*.bin*` and run `setup.sh`
again.

The window is 256 wide; the top screen starts at about y = 55 and the touch
screen at about y = 247.
