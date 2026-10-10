/* What the DSi's DSP program and everything that talks to it agree on: the
 * commands, the sizes of the rings, the marks. Nothing but numbers: the
 * program's assembly includes this as well as the C that models it
 * (hl_dsp.h) and the ARM9 that feeds it. */
#ifndef HL_DSP_PROTO_H
#define HL_DSP_PROTO_H

#define HL_DSP_MAGIC 0x484C /* "HL": the answer to a ping */

#define HL_DSP_MUSIC_WORDS 4096 /* a third of a second of music */
#define HL_DSP_SEND_WORDS 2048  /* a sixth of a second of what is to echo */

/* Commands: the number in register 0, its arguments in registers 1 and 2
 * (written first). Every command is answered in register 0, with the number
 * of the tick it took effect before in register 1 (which is how the tests
 * line the model up with the program). */
#define HL_DSP_CMD_PING 1        /* -> HL_DSP_MAGIC */
#define HL_DSP_CMD_INFO 2        /* arg1: HL_DSP_INFO_* -> a DSP data address */
#define HL_DSP_CMD_MUSIC_START 3 /* arg1: the ring's write position (what is in it already) */
#define HL_DSP_CMD_MUSIC_STOP 4
#define HL_DSP_CMD_MUSIC_FED 5   /* arg1: the ring's new write position -> its read position */
#define HL_DSP_CMD_SEND_FED 6    /* the same for the echo's ring */
#define HL_DSP_CMD_ROOM 7        /* arg1: feedback (Q15), arg2: damping (Q14, high 8 bits) | level (Q7, low 8) */
#define HL_DSP_CMD_STATUS 8      /* -> times the music ran dry since last asked (low 15 bits),
                                  * and HL_DSP_FAULT if the music has been stopped as wrong */
#define HL_DSP_CMD_HEARD 9       /* -> the loudest sample sent to echo since last asked */

#define HL_DSP_INFO_MUSIC 0
#define HL_DSP_INFO_SEND 1

#define HL_DSP_FAULT 0x8000

/* The music arrives in blocks: a mark, the four words of both decoders'
 * states at the block's start (left sample, left step, right sample, right
 * step), then HL_DSP_BLOCK_WORDS words of samples. The decoders take their
 * states afresh from every block, so nothing that goes wrong outlasts one;
 * and a block that does not begin with the mark is the stream out of step,
 * at which the music is stopped (HL_DSP_FAULT) rather than played as noise. */
#define HL_DSP_BLOCK_MARK 0xB10C
#define HL_DSP_BLOCK_WORDS 1024
#define HL_DSP_BLOCK_WHOLE (HL_DSP_BLOCK_WORDS + 5)

/* What has been worked out and not yet sent to the speakers: pairs, in a
 * ring between the program's main loop (which fills it) and the interrupt
 * that asks for a pair 47,605 times a second (which only empties it). */
#define HL_DSP_OUT_WORDS 128

/* The echo's six delay lines, in one array. */
#define HL_DSP_DELAY_WORDS 1602

#endif
