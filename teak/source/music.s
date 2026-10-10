// The music: IMA-ADPCM, four bits a sample. What it works out is
// shared/dsp/hl_dsp.h's, bit for bit (tools/dspsim holds the two against
// each other).

#include <teak/asminc.h>

#include "macros.inc"

// void hlDecode(uint16_t channel, uint16_t code): one sample of one channel
// out of its four bits. hlValue[channel] and hlIndex[channel] move on.
//
// r0: the channel's index (into the table of steps), r1: its value, r2: the
// four bits.

BEGIN_ASM_FUNC hlDecode

    push    y0
    push    r0
    push    r1
    push    r2
    push    r3

    mov     a1l, r2
    mov     a0l, r1
    add     hlIndex, a0
    mov     a0l, r0
    mov     r1, a0
    add     hlValue, a0
    mov     a0l, r1

    // The step, and (2c + 1) / 8 of it for the low three bits c.
    mov     [r0], a0
    add     hlStepTable, a0
    mov     a0l, r3
    mov     [r3], y0
    mov     r2, a0
    and     0x7, a0
    shfi    a0, a0, 1
    addv    0x1, a0l
    mpy     y0, a0l, a1
    clr     a1, always
    add     p*, a1
    shfi    a1, a1, -3

    // Down for the fourth bit, up without it; held within sixteen bits.
    mov     r2, a0
    and     0x8, a0
    brr     1f, eq
    neg     a1, always
1:
    mov     [r1], a0
    add     a0, a1
    CLAMP
    mov     a1l, [r1]

    // The index moves by what the three bits say, between 0 and 88.
    mov     r2, a0
    and     0x7, a0
    add     hlIndexStep, a0
    mov     a0l, r3
    mov     [r3], a1
    mov     [r0], a0
    add     a1, a0
    cmp     0x0u8, a0
    brr     2f, ge
    clr     a0, always
2:
    cmp     0x58, a0
    brr     3f, le
    mov     0x58, a0
3:
    mov     a0l, [r0]

    pop     r3
    pop     r2
    pop     r1
    pop     r0
    pop     y0
    ret     always
