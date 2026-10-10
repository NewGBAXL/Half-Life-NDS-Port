// The game's DSP program (DSi): the music in stereo, and the room's echo.
//
// All of it is assembly: this file is the start and the loop that takes the
// ARM9's commands; tick.s works a pair of samples out, music.s and reverb.s
// are what it is made of, irq.s hands the pairs to the speakers, data.s is
// its memory. What it must work out is written down in C in
// shared/dsp/hl_dsp.h, its model, and tools/dspsim holds the two against
// each other, sample for sample.
//
// (The library's own start-up code and its few set-up routines are called,
// once, at the start; nothing of theirs runs for a sample.)

#include <teak/asminc.h>

#include "hl_dsp_proto.h"

#define PORT_STAT 0x82c2
#define PORT_DATA 0x82c6
#define PORT_NOT_EMPTY 0x10
#define APBP_STAT 0x80d6
#define APBP_CMD0_NEW 0x100

// (data.s: from hlState to its end.)
#define HL_STATE_WORDS (HL_DSP_OUT_WORDS + 5 + 7 + 5 + 8 + HL_DSP_DELAY_WORDS + 6 + 4 + 7)

    .section .bss.hl_main, "aw", %nobits
    .align 2
mn_command: .space 2
mn_arg1:    .space 2
mn_arg2:    .space 2
mn_answer:  .space 2

BEGIN_ASM_FUNC main

    call    teakInit, always

    // Nothing is anything yet. (Not the rings the ARM9 fills: what is in
    // them is not looked at until it says there is something.)
    mov     hlState, r0
    mov     HL_STATE_WORDS, a0
    clr     a1, always
.Lmain_clear:
    mov     a1l, [r0++]
    sub     0x1, a0
    br      .Lmain_clear, neq

    // The speakers' port, asking for samples through CPU interrupt 1. One
    // pair of silence to start with: the interrupt is "the port has just
    // run out", and a port that has never had anything never runs out.
    clr     a0, always
    mov     0x1, a1
    call    btdmpSetupOutputSpeakers, always
    clr     a0, always
    mov     a0l, [PORT_DATA]
    mov     a0l, [PORT_DATA]
    call    cpuEnableInt1, always
    call    cpuEnableIrqs, always

.Lmain_loop:
    // Working out what is to be heard comes first, always; a command is
    // taken between two fillings of the ring.
    call    hlDspFill, always
    // The port found empty: it ran out before the interrupt got to it, and
    // it asks only when it runs out. It is started again from here.
    mov     PORT_STAT, r0
    tst0    PORT_NOT_EMPTY, [r0]
    br      .Lmain_fed, neq
    call    cpuDisableIrqs, always
    call    hlSendPair, always
    call    cpuEnableIrqs, always
.Lmain_fed:
    mov     APBP_STAT, r0
    tst0    APBP_CMD0_NEW, [r0]
    br      .Lmain_loop, eq

    // The arguments are written before the command, so they are there.
    clr     a0, always
    call    apbpReceiveData, always
    mov     a0l, [mn_command]
    mov     0x1, a0
    call    apbpReceiveData, always
    mov     a0l, [mn_arg1]
    mov     0x2, a0
    call    apbpReceiveData, always
    mov     a0l, [mn_arg2]
    clr     a0, always
    mov     a0l, [mn_answer]

    mov     [mn_command], a0
    cmp     HL_DSP_CMD_PING, a0
    br      .Lcmd_ping, eq
    cmp     HL_DSP_CMD_INFO, a0
    br      .Lcmd_info, eq
    cmp     HL_DSP_CMD_MUSIC_START, a0
    br      .Lcmd_start, eq
    cmp     HL_DSP_CMD_MUSIC_STOP, a0
    br      .Lcmd_stop, eq
    cmp     HL_DSP_CMD_MUSIC_FED, a0
    br      .Lcmd_music, eq
    cmp     HL_DSP_CMD_SEND_FED, a0
    br      .Lcmd_send, eq
    cmp     HL_DSP_CMD_ROOM, a0
    br      .Lcmd_room, eq
    cmp     HL_DSP_CMD_STATUS, a0
    br      .Lcmd_status, eq
    cmp     HL_DSP_CMD_HEARD, a0
    br      .Lcmd_heard, eq
    br      .Lcmd_done, always

.Lcmd_ping:
    mov     HL_DSP_MAGIC, a0
    mov     a0l, [mn_answer]
    br      .Lcmd_done, always

// Where the ARM9 is to write: addresses in the DSP's data memory.
.Lcmd_info:
    mov     hlMusicRing, a1
    mov     [mn_arg1], a0
    cmp     HL_DSP_INFO_MUSIC, a0
    br      .Lcmd_info_is, eq
    mov     hlSendRing, a1
.Lcmd_info_is:
    mov     a1l, [mn_answer]
    br      .Lcmd_done, always

// The music starts (or starts again somewhere else): the ring is read from
// its beginning, where a block begins.
.Lcmd_start:
    clr     a0, always
    mov     a0l, [hlMusicOn]
    mov     a0l, [hlMusicWrite]
    mov     [mn_arg1], a1
    cmp     0x0u8, a1
    br      .Lcmd_start_at, lt
    cmp     HL_DSP_MUSIC_WORDS, a1
    br      .Lcmd_start_at, ge
    mov     a1l, [hlMusicWrite]
.Lcmd_start_at:
    mov     a0l, [hlValue]
    mov     a0l, [hlValue+1]
    mov     a0l, [hlLast]
    mov     a0l, [hlLast+1]
    mov     a0l, [hlIndex]
    mov     a0l, [hlIndex+1]
    mov     a0l, [hlMusicRead]
    mov     a0l, [hlMusicHalf]
    mov     a0l, [hlBlockLeft]
    mov     a0l, [hlFault]
    mov     a0l, [hlPinned]
    mov     0x1, a0
    mov     a0l, [hlMusicOn]
    br      .Lcmd_done, always

.Lcmd_stop:
    clr     a0, always
    mov     a0l, [hlMusicOn]
    br      .Lcmd_done, always

// More has been written into a ring: where to; the answer is where it has
// been read to.
.Lcmd_music:
    mov     [mn_arg1], a1
    cmp     0x0u8, a1
    br      .Lcmd_music_read, lt
    cmp     HL_DSP_MUSIC_WORDS, a1
    br      .Lcmd_music_read, ge
    mov     a1l, [hlMusicWrite]
.Lcmd_music_read:
    mov     [hlMusicRead], a0
    mov     a0l, [mn_answer]
    br      .Lcmd_done, always

.Lcmd_send:
    mov     [mn_arg1], a1
    cmp     0x0u8, a1
    br      .Lcmd_send_read, lt
    cmp     HL_DSP_SEND_WORDS, a1
    br      .Lcmd_send_read, ge
    mov     a1l, [hlSendWrite]
.Lcmd_send_read:
    mov     [hlSendRead], a0
    mov     a0l, [mn_answer]
    br      .Lcmd_done, always

// The room: how much comes round (Q15), how dulled (Q14, the second
// argument's high byte times 64), how loud (its low byte).
.Lcmd_room:
    mov     [mn_arg1], a0
    and     0x7fff, a0
    mov     a0l, [hlFeedback]
    mov     [mn_arg2], a0
    shfi    a0, a0, -8
    and     0xff, a0
    shfi    a0, a0, 6
    mov     a0l, [hlDamp]
    mov     [mn_arg2], a0
    and     0xff, a0
    mov     a0l, [hlLevel]
    br      .Lcmd_done, always

// How often the music ran dry or the speakers went short since last asked,
// and whether the music has been stopped as wrong.
.Lcmd_status:
    mov     [hlDry], a0
    mov     [hlStarved], a1
    add     a1, a0
    cmp     0x7fff, a0
    br      .Lcmd_status_fits, le
    mov     0x7fff, a0
.Lcmd_status_fits:
    mov     [hlFault], a1
    cmp     0x0u8, a1
    br      .Lcmd_status_well, eq
    or      HL_DSP_FAULT, a0
.Lcmd_status_well:
    mov     a0l, [mn_answer]
    clr     a0, always
    mov     a0l, [hlDry]
    mov     a0l, [hlStarved]
    br      .Lcmd_done, always

.Lcmd_heard:
    mov     [hlHeard], a0
    mov     a0l, [mn_answer]
    clr     a0, always
    mov     a0l, [hlHeard]

// Answered: in register 1 the tick it took effect before (how the tests
// line the model up with the program), then the answer itself.
.Lcmd_done:
    mov     0x1, a0
    mov     [hlTick], a1
    call    apbpSendData, always
    clr     a0, always
    mov     [mn_answer], a1
    call    apbpSendData, always
    br      .Lmain_loop, always
