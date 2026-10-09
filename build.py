#!/usr/bin/env python3

# The project build, expressed with architectds_nea_mod.
#
# Usage (after running ./setup-env.sh):
#   ./make.sh            # build
#   ./make.sh --help     # see all options
#
# No game content is built into the ROM. Every map, texture and sound is made
# by tools/hlds-convert from the player's own copy of Half-Life and read from
# the SD card under _nds/hl_nds/data/ at runtime.
#
# There is no NitroFS image yet: architectds errors on an empty input
# directory, and nothing in resources/ exists until the bottom-screen art does.
# When it does:
#   nitrofs = NitroFS()
#   nitrofs.add_grit(['resources/bottom/'], out_dir='bottom/')
#   nitrofs.generate_image()
# and add `nitrofs` to the NdsRom binaries.
#
# HOT CODE: a source named Foo.arm.cpp is built as ARM instead of Thumb
# (architectds), and one named Foo.itcm.arm.cpp is ALSO placed in the 32 KB
# ITCM by the BlocksDS linker script, which matches objects on `*.itcm*`. The
# order of the two suffixes matters: architectds tests for the `.arm.cpp`
# ending, so Foo.arm.itcm.cpp would be ITCM but Thumb.

from architectds import *

arm9 = Arm9Binary(
    sourcedirs=['source'],
    # 'thirdparty' puts yas on the include path as <yas/...>, the same spelling
    # the host converter uses -- both sides must agree on the serializer.
    includedirs=['source', 'shared', 'thirdparty'],
    # Add 'HLDS_PROFILE=1' here to build the frame-time readout in: where the
    # frame went, split between input, the BSP walk and the send to the
    # geometry engine. Off, every one of its entry points is an empty inline
    # (DebugLog.hpp), so the shipped build carries none of it.
    defines=['NEA_MAXMOD', 'HL_HW_MATH'],
    libs=['NEA', 'mm9', 'nds9', 'dswifi9d_noip'],
    libdirs=[
        '${BLOCKSDS}/libs/libnds',
        '${BLOCKSDS}/libs/maxmod',
        '${BLOCKSDS}/libs/dswifi',
        '${BLOCKSDSEXT}/nitro-engine-advanced',
    ],
    # -D_LITTLE_ENDIAN + force-including yas's endian config first locks yas to
    # little-endian in every TU *before* any <nds.h> can define _BIG_ENDIAN
    # (which yas would otherwise misread as "big-endian"). The converter writes
    # the yas archives on the host and the DS reads them back, so a byte-order
    # disagreement between the two would corrupt them silently.
    cxxflags='-Wall -O2 -std=gnu++26 -D_LITTLE_ENDIAN -include yas/detail/config/endian.hpp',
)

arm9.generate_elf()

nds = NdsRom(
    binaries=[arm9],
    nds_path='half-life-nds-port.nds',
    game_icon="Logo.bmp",
    game_title='HALF-LIFE DS',
    game_subtitle='Ported by Warioware64',
    game_author='github.com/Warioware64',
    # 2 = NDS + DSi hybrid: one ROM that runs on a DS and, booted in DSi mode,
    # gets the 16 MB of RAM and the 134 MHz clock (Global::Init).
    unit_code=2,
)
nds.generate_nds()

nds.run_command_line_arguments()
