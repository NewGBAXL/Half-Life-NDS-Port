# Half-Life NDS Port

A port of Half-Life to the Nintendo DS and DSi, written in C++ for
[BlocksDS](https://blocksds.skylyrac.net/) on top of
[Nitro Engine Advanced](https://github.com/Warioware64/nitro-engine).

**No game data is in this repository or in the ROM.** A converter turns your
own copy of Half-Life into the files the DS reads from its SD card.

## Where it is

One ROM for DS and DSi. Any of the game's maps converts and loads; the world
is drawn with its lighting, textures, water, decals and the suit's lamp; the
player moves, climbs and swims by the game's own numbers; doors, lifts,
trains, buttons, breakables, pushable boxes, triggers and scripted scenes
work; the crowbar, pistol, shotgun, MP5, .357 and grenades are in, with
headcrabs, zombies, barnacles, turrets and soldiers to use them on; there is
sound and music, a title screen, autosave, quick save and six save slots.

Not yet: the rest of the weapons and monsters, friends who talk, and a good
many smaller things. The whole list, and how each part works, is in
[docs/TECHNICAL.md](docs/TECHNICAL.md).

## Building

There are two ways. Both end with `half-life-nds-port.nds` in this folder.

### With Docker (any system)

Nothing to install but [Docker](https://docs.docker.com/get-docker/). The
image holds BlocksDS, the Nitro Engine Advanced fork, ptexconv, a compiler
and CMake.

```sh
git clone https://github.com/Warioware64/Half-Life-NDS-Port.git
cd Half-Life-NDS-Port

./docker.sh ./make.sh                    # Linux, macOS: builds the image, then the ROM
docker compose run --rm dev ./make.sh    # Windows (PowerShell), or anywhere
```

The first run builds the image, which takes a few minutes. After that,
anything can be run the same way, and `./docker.sh` alone gives a shell in
the container:

```sh
./docker.sh cmake -S tools/hlds-convert -B build/convert -DHLDS_BUILD_GUI=OFF
./docker.sh cmake --build build/convert -j4
./docker.sh ctest --test-dir build/convert
```

(If you also build the converter outside the container, give the two
different folders -- `build/convert-docker`, say: CMake remembers the paths
it was configured with.) In VS Code the same things are tasks: *Terminal >
Run Task*, the ones named "Docker: ...".

To convert the game, or to run the tests that play its maps, say where your
Half-Life `valve` folder is; it is readable inside the container as `/valve`:

```sh
HLDS_VALVE_DIR=/path/to/Half-Life/valve ./docker.sh \
    build/convert/hlds-convert cook-all /valve -o sdroot
```

(With `docker compose` directly: add
`-v "C:\path\to\Half-Life\valve:/valve:ro"` after `run --rm`.)

If building the image stops with `Could not resolve host` (seen with
Docker Desktop on Windows), it is Docker's name server and not the
download: run the command again, and if it keeps happening give Docker a
name server of your choosing -- Docker Desktop > Settings > Docker Engine,
add `"dns": ["1.1.1.1", "8.8.8.8"]`, Apply & restart.

The DSi's DSP audio is an experiment, off by default in the game, and its
toolchain is not in the image unless asked for, because that means compiling
clang (allow an hour): `WITH_TEAK=1 ./docker.sh ./make.sh`. Without it the
ROM is built with no DSP audio and is otherwise the same.

### Without Docker (Linux)

Install, under `$BLOCKSDSEXT` (`/opt/wonderful/thirdparty/blocksds/external`):

| | |
|---|---|
| [BlocksDS](https://blocksds.skylyrac.net/docs/setup/) | the SDK and its ARM compiler (a current one: the code is C++26) |
| `ptexconv` | `wf-pacman -S blocksds-ptexconv` |
| [Nitro Engine Advanced](https://github.com/Warioware64/nitro-engine) | this fork, not the one BlocksDS packages: clone it and `make -f Makefile.blocksds install NEA_MAXMOD=1` |
| [teak-llvm](https://github.com/Warioware64/teak-llvm) and [libteak](https://github.com/Warioware64/libteak) | optional, for DSP audio: `./build-teak.sh all` in the first, then `make install` in the second |

and Python 3, CMake 3.20+, a C++20 compiler; Qt 6.8+ if you want the
converter's window.

```sh
./setup-env.sh          # once: Python venv with the ROM build system
./make.sh               # builds half-life-nds-port.nds
HLDS_JOBS=4 ./make.sh   # ... with no more than four compilers at once

cmake -S tools/hlds-convert -B build/convert
cmake --build build/convert -j4
ctest --test-dir build/convert
```

## Putting the game on the card

```sh
build/convert/hlds-convert cook-all <path to>/Half-Life/valve -o <SD card root>
```

or run `build/convert/gui/hlds-convert-gui`. Everything goes to
`<card>/_nds/hl_nds/data/`; copy the ROM to the card as well. `cook-map`
converts one map; the other commands and what the files are made of are in
[docs/TECHNICAL.md](docs/TECHNICAL.md#what-the-converter-writes).

## Controls

Two layouts, chosen in the pause menu under OPTIONS > CONTROLS. The page
also says what each button does as it is set.

| | CLASSIC | MODERN |
|---|---|---|
| Stylus drag | look around | look around |
| D-pad | move | move |
| A B X Y | move, as the D-pad | X: weapon before · Y: weapon after · B: reload · A: jump |
| L | fire, or swing the crowbar | the same |
| R | the second trigger: the pistol's fast fire, both barrels of the shotgun, the MP5's grenade launcher (the .357: reload) | the same |
| R + D-pad down / up | | crouch, and stay crouched / stand |
| One short tap on the pad | | USE |
| Two quick taps | jump (hold the second to keep it held) | the same |

Whichever the layout:

| | |
|---|---|
| Touch a weapon's slot | take that weapon; the one in hand: reload it |
| Touch RELOAD, JUMP, DUCK, USE | what they say (USE: press a button, open a use-only door, charge at a wall unit) |
| Start, or touch MENU | pause menu: resume, quick save, save game, load game, new game, options, debug command line |
| Select, or touch the lamp | the suit's lamp |

**Stylus hand: left** mirrors the touch screen and changes the buttons over:
A B X Y become the pad (X up, B down, Y left, A right), the D-pad takes the
face buttons' work, and R fires with L as the second trigger.

**Pad left/right: turn** and **Pad up/down: look** give that axis of the pad
to the view; the stylus then moves on it instead, as a stick: push it away
from where it touched down.

The pause menu's COMMAND opens a debug command line on the on-screen
keyboard: `map <name>` goes to a map, `give` hands over every weapon with its
ammunition and the suit, `heal` restores health, `noclip` flies.

## Contributing

- `shared/` is compiled into both the ROM and the converter, so the tests on
  the PC run the console's own code. Run `ctest --test-dir build/convert`
  before sending a change; with `HLDS_VALVE_DIR` set it also plays the game's
  maps (about a minute).
- `build/convert/hlds-testmap <folder>` writes a small map with no game data
  in it, for trying a change on the console in seconds.
- `tools/emu/` runs the ROM in melonDS without a display and takes pictures;
  `tools/ref/` does the same with the real game, to compare.
- `HLDS_PROFILE=1 ./make.sh` builds a ROM that times itself, and
  `tools/arm9-report.sh` says what the built program is made of.
- Never commit anything made from the game: maps, textures, sounds,
  pictures of it. `sdroot/` and `build/` are ignored for that reason.

How and why things are done the way they are:
[docs/TECHNICAL.md](docs/TECHNICAL.md).

## Layout

| | |
|---|---|
| `source/` | the ARM9 program. `*.arm.cpp` is built as ARM; `*.itcm.arm.cpp` also runs from ITCM; single functions by `HL_ARM` / `HL_ITCM` (`source/Hot.hpp`) |
| `teak/` | the DSi's DSP program (music and echo), in assembly |
| `shared/` | file formats, fixed-point maths, collision and movement: compiled by both the ROM and the converter, so the host tests run the console's code |
| `tools/hlds-convert/` | the converter: `core` (no Qt), `cli`, `gui`, `tests` |
| `architectds_nea_mod/` | the ROM build system (ArchitectDS with Nitro Engine Advanced support) |
| `thirdparty/yas/` | serialization, used for the manifest |
| `thirdparty/minimp3/` | MP3 decoder (CC0), used by the converter for the music |
| `docs/` | the technical notes |
| `docker/`, `compose.yaml`, `docker.sh` | the build environment as a container |

## Third-party code

- **meshoptimizer** 1.3 (`thirdparty/meshoptimizer/`), in the converter
  only: Copyright (c) 2016-2026 Arseny Kapoulkine, MIT licence (its
  `LICENSE.md` is beside it).
- **minimp3** and **yas** (`thirdparty/`), under their own licences.

## Provenance

The runtime is a rewrite for the DS, not a port of the game's source. The
hull trace and player movement follow the algorithms and constants of the
Quake/GoldSrc engines (as seen in xash3d-fwgs and the Half-Life SDK's
`pm_shared.c`) so that the game plays like itself; they were written anew in
fixed point. No Valve code or assets are included.
