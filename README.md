# Half-Life NDS Port

A port of Half-Life to the Nintendo DS and DSi, written in C++ for
[BlocksDS](https://blocksds.skylyrac.net/) on top of Nitro Engine Advanced.

**No game data is in this repository or in the ROM.** A converter turns your
own copy of Half-Life into the files the DS reads from its SD card.

## Status: milestone 7, "more monsters", with DSi audio

Working:

- Any of the game's maps converts to a cooked level: BSP tree, PVS, clip hulls,
  and the map's faces as ready-made runs of geometry-engine commands with their
  lightmaps baked into vertex colours. Neighbouring faces share vertices in
  quad strips, and faces are merged only where that provably changes nothing
  on screen (about 12% fewer vertices over the campaign).
- World textures in the DS's 4x4 compressed format; textures with holes
  (grates, fences) in A3I5, with soft edges. Both are sized per map to fit the
  texture and palette banks.
- The DS renders the world with PVS, frustum and back-face culling against a
  polygon and vertex budget. Single-pass rendering (60 fps) is the default; two-pass (30 fps, twice the
  capacity), in two methods, and a per-map automatic choice are in the pause
  menu. "Two-pass" joins the halves in video memory; "Two-pass DMA" builds
  each picture in main memory and shows it only when it is whole (a third
  texture bank is free, 190 KB of RAM is not, and it costs more CPU). Back faces are rejected on the CPU, surface by surface; the
  hardware's own test is off, because it goes by a polygon's first three
  vertices and misjudges very thin ones; the converter orders every polygon
  so that those three make a real corner. In two-pass mode each displayed frame is one game state: the left
  pass captures the whole composited screen, so the two halves never come
  from different moments. (What two-pass cannot keep is the 3D picture's
  18-bit colour: the half that is not being drawn comes from display
  capture, which is 15-bit. "Two-pass DMA" shows both halves from capture,
  so at least they match; plain "Two-pass" shows one half live.)
- On a DSi the world's surfaces go to the geometry engine as one display
  list, fed by the DSi's own DMA (NDMA) while the processor works out where
  the models' bones are. (Not while the DSP option is on: a DMA to the
  geometry engine and the DSP's traffic do not get on.) NEA's own display
  lists use NDMA there too (`NEA_DisplayListEnableNDMA`).
- A view with too much in it loses what is least seen first. Pressure (0 to
  3; `pN` on the overlay) rises while passes run out of budget and leaves
  out, more with each step, world surfaces and brush entities only a few
  pixels across and all but the nearest models. Only what is still too much
  is cut off at a distance, and the hardware's fog is brought in just short
  of that distance, so the far end fades out instead of ending in a hole.
  A view that fits has neither.
- Brush entities are drawn and solid: opaque, cut-out, see-through (glass),
  flat-colour, and invisible-but-solid.
- Doors (sliding and hinged), buttons, lifts and trains on path corners,
  ladders, walls that switch on and off, and the wiring between them:
  triggers, relays, multi_managers, multisources, delays and killtargets.
  Moving brushes carry and push the player, and turn back when blocked.
- Maps chain: level changes with landmarks carry the player (and the tram
  they are riding) into the next map. The opening tram ride runs from c0a0 to
  the station in c0a0e by itself, platforms that lower the tram included.
- Fans and doors that turn about any axis; boxes that break (with USE, until
  there is a crowbar) and boxes that are pushed; volumes that hurt, push and
  teleport; health, and dying.
- Water: swimming, sinking, jumping out onto a ledge, drowning, slime and
  lava; liquid surfaces sway. The sky, as a box of six pictures behind the
  world.
- Saving: at every level change, at the map's own save points, and from the
  pause menu. The game continues from the save when the console is switched
  on again.
- Models: the game's studio models, cooked to a DS format (one skin texture a
  model, animation as keys) and skinned by the hardware's matrix stack. The
  converter thins each to about 350 triangles (people are 600-850 in the
  game), so that several fit in a frame with the world.
  Scientists, guards and props stand and animate in the maps; scripted
  sequences play (an actor goes to its mark as soon as a script takes it,
  waits there, acts when told, and what waits on it happens when it is
  done); actors cross level changes with the player. The crowbar is in hand.
- Monsters: headcrabs run at the player and leap, zombies walk up and swing,
  barnacles hang with their tongues down and pull up what walks into them.
  They move by the game's own rule for things that walk (a step is made if,
  lifted a stair's height, nothing is in the way and there is floor where it
  ends), straight at the player where that can be walked and otherwise from
  path node to path node: the converter joins the map's nodes up and works
  out the way from each to each, so the console only looks it up.
  Monstermakers make them.
- Weapons: the crowbar, the pistol and the shotgun, found where the game
  leaves them, with its numbers at the middle difficulty (a zombie shrugs off
  most of a bullet, as it does there). Ammunition, health kits, batteries and
  the wall chargers; armour. What is killed or picked up stays so when a map
  is come back to, and in the save.
- Sound: the game's own, converted to the 4-bit ADPCM the DS's sound chip
  plays by itself. Weapons, monsters, doors, buttons, breakage, footsteps and
  pick-ups; the maps' ambient sounds, louder and to one side as the player
  nears them; the announcer and the scripted lines of scientists and guards
  (from the game's sentences.txt), read off the card as they are spoken; and
  the music, decoded from the game's MP3s and streamed from the card. A map's
  sounds have an allowance of memory (350 KB); the longest are cooked at half
  the rate where a map has more than that.
- On a DSi the sound has more to it. The output runs at 47.6 kHz. With
  "DSi SOUND" switched on in the pause menu (it is **off** unless it is: the
  DSP has not been proved on a console, and off is exactly what a DS does),
  the DSP
  plays the music in stereo (cooked as a second file for each track), and
  adds the echo of the room: the ARM7 captures what the sixteen channels are
  playing, the ARM9 passes it to the DSP, and the DSP puts out a reverb of it
  set by the map's own room markers (env_sound). The DSP program is in
  `teak/`; its arithmetic is one C header (`shared/dsp/hl_dsp.h`) that the
  host tests run too, and `tools/dspsim` checks that the two give the same
  samples, bit for bit, in an emulated DSP. A DS is unchanged.
  The DSP is built not to make a noise when something goes wrong: its
  interrupt only hands finished samples to the speakers (the working out is
  in its main loop, and a port left short gets the last sample, fading); the
  music goes to it in blocks, each behind a mark and with the decoders' own
  state, so that a slip is over within 86 ms; a block out of step is silence
  and is reported, the track is started again from where it was, and after
  three such faults (or a DSP that stops answering) the game goes back to
  the DS's way for the session and says so in `perf.txt`.
  melonDS (1.1) is no judge of it: it lets the DSP run only when the game
  touches one of its registers, so music from the DSP comes out there in
  bursts with silence between.
- More to fight: alien slaves zap from a distance and claw up close,
  bullsquids spit, bite and whip, houndeyes run up and let off a blast. Hand
  grenades bounce, and burst after three seconds. Bolts, spit and blasts are
  drawn as plain shapes.
- The HEV suit is taken where the game has it (it is what opens the way on),
  and armour is for someone wearing it.
- The seated and the dead are let down onto what is under them, as the game
  lets them down (a scientist at a desk in the low box he is given, onto the
  floor his chair stands on; a body, and furniture, as a point); a scientist
  at a desk goes between his five ways of sitting.
- People sent somewhere by a script walk there as the monsters walk: round
  what is in the way, by the map's path nodes. Only those the landmark can
  see cross a level change, and only if there is room for them on the other
  side.
- The renderer listens to the hardware: it reads how many polygons and
  vertices a frame really used, whether any were dropped, and how close the
  rasteriser came to not finishing its lines, and moves its budget by that --
  up, where the hardware throws away what is off the screen (often a fifth of
  what was counted), and down when a frame was too much. The same numbers are
  on the debug overlay and, for each map played, in `perf.txt` on the card.
- Player movement with Half-Life's own numbers: walking, jumping, crouching,
  steps, climbing, sliding along walls, in fixed point throughout (the DS has
  no FPU).
- One ROM for DS and DSi; DSi mode uses the 134 MHz clock and 16 MB of RAM.

Not yet: turrets; friends who follow and talk; the game's own sprites and
beams; room echo, the suit's voice, the
chatter of scientists and guards, the sounds of trains and lifts. Scientists and guards do not
think: they stand where the map puts them and go, in a straight line, where a
script sends them. A script whose actor this port does not have is taken as
played after a few seconds, so that what waits on it still happens. Monsters
do not open doors or jump gaps. A map that is come back to starts as it
started, except that what was broken, killed or picked up stays so.
The converter counts what it does not handle rather than refusing a map.

## Controls

| | |
|---|---|
| Stylus drag | look around |
| D-pad, or A B X Y | move |
| L | fire, or swing the crowbar |
| Touch WEAPON | the next weapon |
| Tap twice | jump (hold the second tap to keep it held) |
| Touch DUCK | crouch |
| Touch USE | press a button, open a use-only door, charge at a wall unit |
| Start | pause menu: rendering mode, DSi sound (on a DSi), save, new game, debug command line |
| Select | fly through walls (debug) |

The pause menu's COMMAND opens a debug command line on the on-screen
keyboard: `map <name>` goes to a map, `give` hands over every weapon with its
ammunition and the suit, `heal` restores health, `noclip` flies.

## Building

Requirements: BlocksDS with `nitro-engine-advanced` and `ptexconv` installed
under `$BLOCKSDSEXT`, Python 3, CMake 3.20+, a C++20 compiler, and optionally
Qt 6.8+ for the converter's GUI.
NEA has to be one with `NEA_MaterialGetTexParam` and `NEA_PaletteGetBase`
(the fork's two accessors for building display lists that change texture).

```sh
./setup-env.sh          # once: Python venv with the ROM build system
./make.sh               # builds half-life-nds-port.nds

cmake -S tools/hlds-convert -B build/convert
cmake --build build/convert -j
ctest --test-dir build/convert
```

Set `HLDS_VALVE_DIR` to a Half-Life `valve` folder to make the tests also
check collision and movement on a real map, run the whole tram ride through
its six maps, play the resonance cascade through to the map after it, fight
headcrabs and zombies in theirs, check that nobody in twenty-three maps
stands or walks inside the world, and pack every common sound (under a
minute, nothing is drawn or heard).

The DSP program needs the Teak LLVM toolchain and libteak in `$BLOCKSDSEXT`
(`llvm-teak/`, `libteak/`); `./make.sh` builds it and links it into the ROM,
and without them builds a ROM with no DSP audio. `tools/dspsim/run.sh` runs it
against its model (it needs teakra, built: see the script).

`tools/emu/` has scripts that run the ROM in melonDS without a display, for
looking at a change on the console's own renderer -- and, with `EMU_AUDIO`,
for writing what it would have played to a file.

## Converting the game

```sh
build/convert/hlds-convert cook-all <path to>/Half-Life/valve -o <SD card root>
```

or run `build/convert/gui/hlds-convert-gui`. Everything goes to
`<card>/_nds/hl_nds/data/`. Other commands: `cook-map` (one map), `info`
(what a map will cost the DS to draw), `verify` (check a cooked level with the
DS's own reader; `--brushes` lists its brush entities).

`build/convert/hlds-testmap <folder>` writes a small synthetic map -- one room
with a door, a button, a lift and one brush of each drawing class -- that can
be cooked and booted in seconds and contains no game data. It is the quickest
way to look at a rendering or logic change on the console.

## Layout

| | |
|---|---|
| `source/` | the ARM9 program. `*.arm.cpp` is built as ARM; `*.itcm.arm.cpp` also runs from ITCM |
| `teak/` | the DSi's DSP program (music and echo) |
| `shared/` | file formats, fixed-point maths, collision and movement: compiled by both the ROM and the converter, so the host tests run the console's code |
| `tools/hlds-convert/` | the converter: `core` (no Qt), `cli`, `gui`, `tests` |
| `architectds_nea_mod/` | the ROM build system (ArchitectDS with Nitro Engine Advanced support) |
| `thirdparty/yas/` | serialization, used for the manifest |
| `thirdparty/minimp3/` | MP3 decoder (CC0), used by the converter for the music |

## Provenance

The runtime is a rewrite for the DS, not a port of the game's source. The
hull trace and player movement follow the algorithms and constants of the
Quake/GoldSrc engines (as seen in xash3d-fwgs and the Half-Life SDK's
`pm_shared.c`) so that the game plays like itself; they were written anew in
fixed point. No Valve code or assets are included.
