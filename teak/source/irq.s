// The speakers' side: the interrupt that asks for a pair of samples 47,605
// times a second, and what hands one over. The interrupt only hands over
// what the main loop has worked out (tick.s) -- and as much of it as the
// port will take, not one pair: the port then has a little in hand, and is
// not empty the moment anything is late.

#include <teak/asminc.h>

#include "hl_dsp_proto.h"
#include "macros.inc"

#define PORT_STAT 0x82c2 // the speakers' port: how full it is
#define PORT_DATA 0x82c6 // ... and where a sample is put
#define PORT_FULL 0x8
#define IRQ_ACK 0x8202
#define IRQ_PORT 0x800

// One pair to the speakers' port, if it has room for one: the next the
// main loop worked out; or, if it has not kept up, the last again, a
// quarter quieter each time -- so that falling behind is a dip and not a
// bang.
BEGIN_ASM_FUNC hlSendPair

    push    r0
    mov     PORT_STAT, r0
    tst0    PORT_FULL, [r0]
    br      .Lsend_done, neq

    mov     [hlOutRead], a0
    mov     [hlOutWrite], a1
    sub     a0, a1
    br      .Lsend_starved, eq
    add     hlOutRing, a0
    mov     a0l, r0
    mov     [r0++], a1
    mov     a1l, [hlPortL]
    mov     [r0], a1
    mov     a1l, [hlPortR]
    mov     [hlOutRead], a0
    addv    0x2, a0l
    cmp     HL_DSP_OUT_WORDS, a0
    br      .Lsend_next, lt
    clr     a0, always
.Lsend_next:
    mov     a0l, [hlOutRead]
    br      .Lsend_out, always

.Lsend_starved:
    mov     [hlStarved], a0
    cmp     0x7fff, a0
    br      .Lsend_fade, ge
    addv    0x1, a0l
    mov     a0l, [hlStarved]
.Lsend_fade:
    mov     [hlPortL], a1
    SHIFTZ  a1, 2
    mov     [hlPortL], a0
    sub     a1, a0
    mov     a0l, [hlPortL]
    mov     [hlPortR], a1
    SHIFTZ  a1, 2
    mov     [hlPortR], a0
    sub     a1, a0
    mov     a0l, [hlPortR]

.Lsend_out:
    mov     [hlPortL], a0
    mov     a0l, [PORT_DATA]
    mov     [hlPortR], a0
    mov     a0l, [PORT_DATA]
.Lsend_done:
    pop     r0
    ret     always

// (Called by the library's start-up code, which has put away the
// accumulators, the product and r0.) The Teak has two copies of its mode
// register and swaps them on an interrupt: saturation is switched off in
// this one too -- the two low bits of mod0 SET are "off" -- since every
// shift and store here counts on it.
BEGIN_ASM_FUNC irqHandlerInt1

    set     0x3, mod0
.Lirq_more:
    call    hlSendPair, always
    mov     [hlOutRead], a0
    mov     [hlOutWrite], a1
    sub     a0, a1
    br      .Lirq_done, eq
    mov     PORT_STAT, r0
    tst0    PORT_FULL, [r0]
    br      .Lirq_more, eq
.Lirq_done:
    mov     IRQ_PORT, a0
    mov     a0l, [IRQ_ACK]
    ret     always
