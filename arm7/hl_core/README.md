# Custom ARM7 core for DS ports

2026 NewGBAXL

`arm7.bin` is only built against when `HL_ARM7_CORE=1` is set via `build.py`.
Its source code is not provided here, but will be in the future.

Use with any other program and external modifications to it are not permitted.
When the program is open-sourced, contributions will be accepted on a strict NO-AI policy and AI forks to it
are explicitly not permitted under the license.

The core provided to this game supports DLDI and SD Card but the public code will not, rather it uses other "tricks" to work
with flashcarts seamlessly without ever being aware that it is running on a flashcart if so.

Maxmod is not used in any capacity. New lightweight audio helper code is used instead, alongside a reimplementation
of the GoldSrc audio engine. Every IPC FIFO audio implementation is game-specific. The shared codebase also supports
other audio engines, such as libultra's libaudio.

This build uses `https://github.com/Warioware64/dswifi`. Future ports will use a fully-custom multiplayer library.

The ARM7 core also supports giving the ARM7 "jobs" to complete for the ARM9. It is not used for this Half-Life port,
yet is made good use of for the GTAIII DS port. As jobs take up ARM7 frame time, it is recommended they only be used for
lightweight audio engines and/or games that do not use multiplayer.

## Technical overview, in short

Includes:
- GoldSrc audio engine reimplementation (mixer, channels, VOX, room DSP, music stream)
- Warioware64's dswifi fork for local multiplayer and Download Play
- libnds's storage layer for DLDI & SD Card
- libnds for input, touch, and system code

Load at `0x02380000`, entry at `0x02380000` (`-r7`, `-e7`)
Filesize 60,240 bytes; about 83.4 KB of ARM7 WRAM used of 98,304; about 14.8 KB free before the stacks
Uses shared WRAM and Main RAM for the wifi code and stacks

## Audio stored on the card

* `sd/audio.nds`: a NitroFS image of the game's sounds, `sentences.txt`, `materials.txt` and music. Place at `_nds/hl_nds/data/audio.nds`.
* `sd/audio.keys`: what turns a level's sound names into the core's sound numbers. Place at `_nds/hl_nds/data/audio.keys`.

All sounds are converted to ADPCM.
Every sound and track retain the original individual sample rate from the game.
The channel count is also retained from the original (mono/stereo), except for sound effects,
which are mixed to mono, as the engine only plays effects mono.

The audio in the game as stored is roughly 122MB.

## Brief calling conventions

Init:
- `fatInitDefault()`, with DLDI on the ARM7 (a driver with `FEATURE_ARM7_CAPABLE`) if possible
- `cardshare::attach()` or similar, using the protocol defined by `A7SdProto.h`
- other ARM7 core messages

Every step:
- `source/audio/SoundCore.cpp` or similar code, as an implementation of the commands defined in `source/audio/A7Mix.h`
  The game's `SoundQueue` becomes `A7M_EMIT_ID` (a level sound by its bank number, from `audio.keys`), `A7M_WORDS`
  (a sentence cut into its words), loops by an entity each, `A7M_LISTENER` every frame, `A7M_PAUSE`, and the music by
  track (`A7M_MUSIC_PLAY` "#N", N = the game's track - 2, a track resumed from the checkpoint before where it was put down).
  This is a non-exhaustive list and may be outdated, and is provided as an short example reference.

## Card sharing

The ARM9's libfat requests and the ARM7's reads both end in the same DLDI driver, on the ARM7. The core masks the FIFO
interrupt around each read of 8 sectors (4 KB) at most and runs its mixer between reads, so a libfat request waits for
one read, never for a file. The core finds the image itself (`a7fat.c`: MBR, FAT32 or exFAT, long names, fragmented files;
tested on the PC against generated images by `tools/a7fat_test.c`). A request of the ARM9's runs to its end inside the
FIFO interrupt, though: a large read (a map loading) stops the core's mixer for as long as it takes.

## Final note

If an updated version of the dswifi library or other libraries are desired, message `NewGBAXL` to rebuild the ARM7 core
with the latest versions of the relevant libraries. `A7SdProto.h` and `A7Mix.h` follow the ARM7 core, so if the custom engine
code is updated these will be updated too.
