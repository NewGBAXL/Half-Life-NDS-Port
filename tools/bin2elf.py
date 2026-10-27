#!/usr/bin/env python3
"""Wraps a flat ARM7 core (and, for a DSi, its ARM7i part) in a bare ELF for ndstool.

ndstool only makes a DSi header (unit code, the ARM7i block) from ELF inputs, not from raw binaries. The prebuilt core is kept in the repository as flat
binaries (arm7/hl_core/arm7.bin, arm7i.bin: no symbols, no debug info), and the build wraps them here, in build/, in an ELF that has nothing but
program headers: no sections, no symbols.

    bin2elf.py OUT.elf arm7.bin [arm7i.bin]

arm7.bin is loaded and entered at 0x02380000. arm7i.bin is the DSi-only part, which the DSi loads to 0x02E80000 and the core's startup code copies to
0x03000000 (the same load and run addresses the core's own link gives it)."""
import struct
import sys

NUL = bytes(1)
ARM7_LOAD = 0x02380000
ARM7_RUN = 0x037F8000
CRT0_BYTES = 0x220
ARM7I_LOAD = 0x02E80000
ARM7I_RUN = 0x03000000


def build(arm7, arm7i):
    # (as the core's own link lays it out, and ndstool reads it: the startup code at the load address, which copies the rest to 0x037F8000 and runs it there; then the DSi part)
    parts = [(arm7[:CRT0_BYTES], ARM7_LOAD, ARM7_LOAD, b'.crt0', 6),
             (arm7[CRT0_BYTES:], ARM7_RUN, ARM7_LOAD + CRT0_BYTES, b'.text', 6)]
    if arm7i:
        parts.append((arm7i, ARM7I_RUN, ARM7I_LOAD, b'.twl', 6))      # (virtual, physical: the physical is where it is loaded)
    ehsize, phentsize, shentsize = 52, 32, 40
    off = ehsize + phentsize * len(parts)
    heads, body, secs = [], b'', []
    for data, virt, phys, name, flags in parts:
        pad = (-off) % 4096                                # (page-aligned file offsets, as a linker leaves them)
        off += pad; body += bytes(pad)
        pflags = 7 | (0x100000 if name == b'.twl' else 0)      # (RWX; the DSi's part carries a flag of its own, which is how ndstool finds it)
        heads.append(struct.pack('<8I', 1, off, virt, phys, len(data), len(data), pflags, 4096))      # PT_LOAD
        secs.append((name, virt, off, len(data), flags))
        body += data; off += len(data)
    # ndstool tells the DSi's part by the name of its section (.twl), and reads it first, as the core's own link lists them: a section table, names and addresses, no symbols
    secs.sort(key=lambda sec: sec[0] != b'.twl')
    names = NUL + b''.join(n + NUL for n, *_ in secs) + b'.shstrtab' + NUL
    strtab_off = off; body += names; off += len(names)
    pad = (-off) % 4; body += bytes(pad); off += pad
    shoff = off
    sh = bytes(shentsize)
    at = 1
    for name, virt, o, size, flags in secs:
        sh += struct.pack('<10I', at, 1, flags, virt, o, size, 0, 0, 16 if name == b'.crt0' else 8, 0)      # SHT_PROGBITS, AX
        at += len(name) + 1
    sh += struct.pack('<10I', at, 3, 0, 0, strtab_off, len(names), 0, 0, 1, 0)      # SHT_STRTAB
    ident = bytes([0x7F]) + b'ELF' + bytes([1, 1, 1, 0]) + bytes(8)
    header = ident + struct.pack('<HHIIIIIHHHHHH', 2, 40, 1, ARM7_LOAD, ehsize, shoff, 0x05000200, ehsize, phentsize,
                                 len(parts), shentsize, len(secs) + 2, len(secs) + 1)
    return header + b''.join(heads) + body + sh


def main():
    if len(sys.argv) not in (3, 4):
        sys.exit(__doc__)
    arm7 = open(sys.argv[2], 'rb').read()
    arm7i = open(sys.argv[3], 'rb').read() if len(sys.argv) == 4 else None
    open(sys.argv[1], 'wb').write(build(arm7, arm7i))


if __name__ == '__main__':
    main()
