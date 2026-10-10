// The room's echo: one sample in (hlEchoIn), one out (hlEchoOut). Four
// combs side by side, each with a one-pole low pass in its loop, and two
// all-passes after them. What it works out is shared/dsp/hl_dsp.h's
// hlReverb(), bit for bit: the simulator holds the two against each other
// (tools/dspsim).
//
// Registers, for the whole of it: r4 = the damping, r5 = 16384 less the
// damping, r6 = the feedback, b0 = the combs' sum. Saturation is off (the
// start-up code's mod0), so a shift is a shift and a store is the low word.

#include <teak/asminc.h>

#include "macros.inc"

// A line's place moved on by one, and back to its start at its end.
.macro NEXT index, length
    mov     [hlLinePos+\index], a0
    addv    0x1, a0l
    cmp     \length, a0
    brr     1f, lt
    clr     a0, always
1:
    mov     a0l, [hlLinePos+\index]
.endm

.macro COMB index, base, length
    mov     [hlLinePos+\index], a0
    add     hlDelay+\base, a0
    mov     a0l, r1
    mov     [r1], a0                // what comes round
    mov     a0l, r3
    add     a0, b0
    // Dulled a little: (16384 - damp) of it and damp of what it was.
    clr     a1, always
    mov     r5, y0
    mpy     y0, r3, a1              // p = (16384 - damp) * it
    mov     [hlCombLow+\index], a0
    mov     r4, y0
    mac     y0, a0l, a1             // a1 = p; p = damp * what it was
    add     p*, a1
    SHIFTZ  a1, 14
    mov     a1l, [hlCombLow+\index]
    // ... and quieter, with what is new added, back into the line.
    mov     r6, y0
    mpy     y0, a1l, a1             // (a1 itself is left as it is)
    clr     a1, always
    add     p*, a1
    SHIFTZ  a1, 15
    mov     [hlEchoIn], a0
    add     a0, a1
    CLAMP
    mov     a1l, [r1]
    NEXT    \index, \length
.endm

.macro ALLPASS index, base, length
    mov     [hlLinePos+\index], a0
    add     hlDelay+\base, a0
    mov     a0l, r1
    mov     [r1], b0                // b0: what comes out of the line
    mov     b0, a1
    SHIFTZ  a1, 1
    mov     [hlEchoOut], a0
    add     a0, a1
    CLAMP
    mov     a1l, [r1]
    mov     [r1], a1                // (as it was stored: sixteen bits)
    SHIFTZ  a1, 1
    mov     b0, a0
    sub     a1, a0
    mov     a0, a1
    CLAMP
    mov     a1l, [hlEchoOut]
    NEXT    \index, \length
.endm

BEGIN_ASM_FUNC hlReverb

    push    y0
    push    r1
    push    r3
    push    r4
    push    r5
    push    r6
    pusha   b0

    mov     [hlDamp], a0
    mov     a0l, r4
    mov     0x4000, a1
    sub     a0, a1
    mov     a1l, r5
    mov     [hlFeedback], a0
    mov     a0l, r6
    clr     b0, always

    COMB    0, 0, 301
    COMB    1, 301, 321
    COMB    2, 622, 345
    COMB    3, 967, 366

    mov     b0, a1
    SHIFTZ  a1, 1
    CLAMP
    mov     a1l, [hlEchoOut]

    ALLPASS 4, 1333, 150
    ALLPASS 5, 1483, 119

    popa    b0
    pop     r6
    pop     r5
    pop     r4
    pop     r3
    pop     r1
    pop     y0
    ret     always
