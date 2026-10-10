/* What the DSi's DSP does for the game's sound: plays the music, and adds
 * the room's echo.
 *
 * This file is the DSP program's MODEL. The program itself is assembly
 * (teak/source/*.s), written to give the same samples as this, bit for bit:
 * the host tests run this, and the simulator's harness (tools/dspsim) runs
 * both -- the real program in an emulated DSP -- and compares every pair
 * that comes out. Anything the program is to do is written here first.
 * (What the two share by name -- the numbers of the commands, the sizes of
 * the rings -- is in hl_dsp_proto.h, which both include.) On the DSP the
 * pairs are worked out by the main loop into a small ring (hlDspFill) and
 * only handed to the speakers by the interrupt (hlDspPull): a pair that
 * takes long to work out is then a moment's delay, not a missed sample. It
 * is plain C in the DSP's own terms: 16-bit words, 32-bit sums.
 *
 * MUSIC is two channels of IMA-ADPCM at half the output rate, four bits a
 * sample, packed two pairs to a word (left, right, left, right, low nibble
 * first), in a ring the ARM9 keeps filled. Every other tick a new pair is
 * unpacked; the tick between is half way from the last pair to it.
 *
 * ECHO is a reverb fed with what the DS's own sixteen channels are playing,
 * captured by the ARM7 and sent on by the ARM9: one channel, a quarter of
 * the output rate, in a second ring. Four combs side by side and two
 * all-passes after them (Schroeder's arrangement, the lengths Freeverb's
 * scaled to this rate). Only the echo is put out: the sound itself is
 * already coming out of the ordinary channels.
 */
#ifndef HL_DSP_H
#define HL_DSP_H

#include <stdint.h>

#include "hl_dsp_proto.h"

static uint16_t hlMusicRing[HL_DSP_MUSIC_WORDS];
static int16_t hlSendRing[HL_DSP_SEND_WORDS];
static int16_t hlOutRing[HL_DSP_OUT_WORDS];
static volatile uint16_t hlOutRead, hlOutWrite;
static int16_t hlPortL, hlPortR;
static uint16_t hlStarved;

static uint16_t hlMusicRead, hlMusicWrite, hlMusicOn, hlMusicHalf;
static uint16_t hlBlockLeft, hlFault, hlPinned;
static uint16_t hlSendRead, hlSendWrite;
static uint16_t hlTick, hlDry, hlHeard;
static int16_t hlValue[2], hlIndex[2], hlLast[2];
static int16_t hlOutL, hlOutR;

/* The echo: six delay lines in one array. */
static int16_t hlDelay[HL_DSP_DELAY_WORDS];
static const uint16_t hlLineBase[6] = {0, 301, 622, 967, 1333, 1483};
static const uint16_t hlLineLength[6] = {301, 321, 345, 366, 150, 119};
static uint16_t hlLinePos[6];
static int16_t hlCombLow[4];
static int16_t hlFeedback, hlDamp, hlLevel;
static int16_t hlWet, hlWetStep;

static const int16_t hlStepTable[89] = {
    7,     8,     9,     10,    11,    12,    13,    14,    16,    17,    19,    21,    23,
    25,    28,    31,    34,    37,    41,    45,    50,    55,    60,    66,    73,    80,
    88,    97,    107,   118,   130,   143,   157,   173,   190,   209,   230,   253,   279,
    307,   337,   371,   408,   449,   494,   544,   598,   658,   724,   796,   876,   963,
    1060,  1166,  1282,  1411,  1552,  1707,  1878,  2066,  2272,  2499,  2749,  3024,  3327,
    3660,  4026,  4428,  4871,  5358,  5894,  6484,  7132,  7845,  8630,  9493,  10442, 11487,
    12635, 13899, 15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794, 32767};
static const int16_t hlIndexStep[8] = {-1, -1, -1, -1, 2, 4, 6, 8};

__attribute__((unused)) static void hlDspInit(void)
{
    uint16_t i;
    for (i = 0; i < HL_DSP_DELAY_WORDS; i++)
        hlDelay[i] = 0;
    for (i = 0; i < 6; i++)
        hlLinePos[i] = 0;
    for (i = 0; i < 4; i++)
        hlCombLow[i] = 0;
    hlMusicRead = hlMusicWrite = hlMusicOn = hlMusicHalf = 0;
    hlBlockLeft = hlFault = hlPinned = 0;
    hlOutRead = 0;
    hlOutWrite = 0;
    hlPortL = hlPortR = 0;
    hlStarved = 0;
    hlSendRead = hlSendWrite = 0;
    hlTick = hlDry = hlHeard = 0;
    hlValue[0] = hlValue[1] = hlLast[0] = hlLast[1] = 0;
    hlIndex[0] = hlIndex[1] = 0;
    hlOutL = hlOutR = 0;
    hlFeedback = 0;
    hlDamp = 0;
    hlLevel = 0;
    hlWet = hlWetStep = 0;
}

#define HL16(wide) ((int16_t)(wide))

static int16_t hlSaturate(int32_t value)
{
    if (value > 32767)
        value = 32767;
    if (value < -32767)
        value = -32767;
    return HL16(value);
}

/* `value` over two to the `bits`, towards nothing: a negative number is
 * shifted as its size is, so that the echo dies away to silence and not to
 * minus one. (`value` is worked out more than once: always a plain variable
 * here.) */
#define hlShift(value, bits) ((value) < 0 ? -((-(value)) >> (bits)) : ((value) >> (bits)))

#define hlMul(a, b) ((int32_t)(a) * (b))

/* One sample of one channel out of its four bits. */
static void hlDecode(uint16_t channel, uint16_t code)
{
    int16_t index = hlIndex[channel];
    int16_t step = hlStepTable[index];
    /* (2c + 1) / 8 of the step, for the low three bits c: one multiply. */
    int32_t diff = ((int32_t)step * (int16_t)(((code & 7) << 1) + 1)) >> 3;
    int32_t value = hlValue[channel];
    if (code & 8)
        value -= diff;
    else
        value += diff;
    hlValue[channel] = hlSaturate(value);
    index += hlIndexStep[code & 7];
    if (index < 0)
        index = 0;
    if (index > 88)
        index = 88;
    hlIndex[channel] = index;
}

/* The music starts (or starts again somewhere else): the ring is read from
 * its beginning, where a block begins. */
static void hlMusicStart(void)
{
    hlMusicOn = 0;
    hlValue[0] = hlLast[0] = 0;
    hlValue[1] = hlLast[1] = 0;
    hlIndex[0] = hlIndex[1] = 0;
    hlMusicRead = 0;
    hlMusicHalf = 0;
    hlBlockLeft = 0;
    hlFault = 0;
    hlPinned = 0;
    hlMusicOn = 1;
}

/* The music is wrong, and is stopped: silence is better than what a decoder
 * makes of the wrong words. */
static void hlMusicFault(void)
{
    hlMusicOn = 0;
    hlFault = 1;
    hlValue[0] = hlValue[1] = hlLast[0] = hlLast[1] = 0;
}

static uint16_t hlMusicTake(void)
{
    uint16_t word = hlMusicRing[hlMusicRead];
    hlMusicRead++;
    if (hlMusicRead >= HL_DSP_MUSIC_WORDS)
        hlMusicRead = 0;
    return word;
}

/* The next pair of the music, if there is one to be had: 1 if hlValue[] has
 * moved on. A new block is begun here when the last is done. */
static uint16_t hlMusicPair(void)
{
    uint16_t have = hlMusicWrite - hlMusicRead;
    uint16_t word;
    if (hlMusicWrite < hlMusicRead)
        have = have + HL_DSP_MUSIC_WORDS;
    if (hlBlockLeft == 0)
    {
        /* (A block is told of only when all of it is in the ring, so its
         * five words of heading are either all there or none.) */
        if (have < 5)
            return 0;
        if (hlMusicTake() != HL_DSP_BLOCK_MARK)
        {
            hlMusicFault();
            return 0;
        }
        hlValue[0] = (int16_t)hlMusicTake();
        hlIndex[0] = (int16_t)hlMusicTake();
        hlValue[1] = (int16_t)hlMusicTake();
        hlIndex[1] = (int16_t)hlMusicTake();
        if (hlIndex[0] < 0 || hlIndex[0] > 88 || hlIndex[1] < 0 || hlIndex[1] > 88)
        {
            hlMusicFault();
            return 0;
        }
        hlBlockLeft = HL_DSP_BLOCK_WORDS;
        hlMusicHalf = 0;
        have = have - 5;
    }
    if (have == 0)
        return 0;
    word = hlMusicRing[hlMusicRead];
    if (hlMusicHalf)
        word >>= 8;
    hlDecode(0, word & 15);
    hlDecode(1, (word >> 4) & 15);
    if (hlMusicHalf)
    {
        hlMusicTake();
        hlBlockLeft--;
    }
    hlMusicHalf ^= 1;
    /* Both channels against the stops for a long run is not music. */
    if (hlValue[0] >= 32767 || hlValue[0] <= -32767 || hlValue[1] >= 32767 || hlValue[1] <= -32767)
    {
        hlPinned++;
        if (hlPinned > 64)
            hlMusicFault();
    }
    else
    {
        hlPinned = 0;
    }
    return 1;
}

/* One sample of the echo, from one sample of what is to echo: hlEchoIn in,
 * hlEchoOut out. */
static int16_t hlEchoIn, hlEchoOut, hlEchoY;
static int32_t hlEchoSum, hlEchoWide, hlEchoIn32, hlEchoY32, hlEchoOut32;
static uint16_t hlEchoAt, hlEchoLine;

static void hlReverb(void)
{
    hlEchoSum = 0;
    hlEchoIn32 = hlEchoIn;
    for (hlEchoLine = 0; hlEchoLine < 4; hlEchoLine++)
    {
        hlEchoAt = hlLineBase[hlEchoLine] + hlLinePos[hlEchoLine];
        hlEchoY = hlDelay[hlEchoAt];
        /* What comes round is dulled a little each time (a one-pole low
         * pass in the loop), and comes round quieter. */
        hlEchoWide = hlMul(hlEchoY, HL16(16384 - (int32_t)hlDamp));
        hlEchoWide += hlMul(hlCombLow[hlEchoLine], hlDamp);
        hlEchoWide = hlShift(hlEchoWide, 14);
        hlCombLow[hlEchoLine] = HL16(hlEchoWide);
        hlEchoWide = hlMul(hlCombLow[hlEchoLine], hlFeedback);
        hlEchoWide = hlShift(hlEchoWide, 15);
        hlEchoWide = hlEchoWide + hlEchoIn32;
        hlDelay[hlEchoAt] = hlSaturate(hlEchoWide);
        hlEchoY32 = hlEchoY;
        hlEchoSum = hlEchoSum + hlEchoY32;
        hlLinePos[hlEchoLine]++;
        if (hlLinePos[hlEchoLine] >= hlLineLength[hlEchoLine])
            hlLinePos[hlEchoLine] = 0;
    }
    hlEchoSum = hlShift(hlEchoSum, 1);
    hlEchoOut = hlSaturate(hlEchoSum);
    for (hlEchoLine = 4; hlEchoLine < 6; hlEchoLine++)
    {
        hlEchoAt = hlLineBase[hlEchoLine] + hlLinePos[hlEchoLine];
        hlEchoY = hlDelay[hlEchoAt];
        hlEchoY32 = hlEchoY;
        hlEchoOut32 = hlEchoOut;
        hlEchoWide = hlShift(hlEchoY32, 1);
        hlEchoWide = hlEchoWide + hlEchoOut32;
        hlDelay[hlEchoAt] = hlSaturate(hlEchoWide);
        hlEchoWide = hlDelay[hlEchoAt];
        hlEchoWide = hlShift(hlEchoWide, 1);
        hlEchoWide = hlEchoY32 - hlEchoWide;
        hlEchoOut = hlSaturate(hlEchoWide);
        hlLinePos[hlEchoLine]++;
        if (hlLinePos[hlEchoLine] >= hlLineLength[hlEchoLine])
            hlLinePos[hlEchoLine] = 0;
    }
}

/* One pair of samples out: hlOutL and hlOutR. */
static void hlDspTick(void)
{
    int16_t left = 0, right = 0;
    int32_t wide;

    if (hlMusicOn)
    {
        if ((hlTick & 1) == 0)
        {
            /* A new pair, if the ring has one; the tick between two pairs
             * is half way from one to the other. */
            hlLast[0] = hlValue[0];
            hlLast[1] = hlValue[1];
            if (!hlMusicPair() && hlDry < 0x7FFF)
                hlDry++;
            wide = (int32_t)hlLast[0] + hlValue[0];
            wide = hlShift(wide, 1);
            left = HL16(wide);
            wide = (int32_t)hlLast[1] + hlValue[1];
            wide = hlShift(wide, 1);
            right = HL16(wide);
        }
        else
        {
            left = hlValue[0];
            right = hlValue[1];
        }
    }

    if ((hlTick & 3) == 0)
    {
        /* A new sample of echo every fourth tick, and a straight line to it
         * over the four. */
        int16_t wet = 0;
        hlEchoIn = 0;
        if (hlSendRead != hlSendWrite)
        {
            wide = hlSendRing[hlSendRead];
            if (wide > (int32_t)hlHeard)
                hlHeard = (uint16_t)wide;
            wide = hlShift(wide, 2);
            hlEchoIn = HL16(wide);
            hlSendRead++;
            if (hlSendRead >= HL_DSP_SEND_WORDS)
                hlSendRead = 0;
        }
        if (hlLevel != 0)
        {
            hlReverb();
            wide = hlMul(hlEchoOut, hlLevel);
            wide = hlShift(wide, 6);
            wet = hlSaturate(wide);
        }
        wide = (int32_t)wet - hlWet;
        wide = hlShift(wide, 2);
        hlWetStep = HL16(wide);
    }
    hlWet += hlWetStep;
    hlTick++;

    hlOutL = hlSaturate((int32_t)left + hlWet);
    hlOutR = hlSaturate((int32_t)right + hlWet);
}

/* The main loop's part: work out pairs until the ring between it and the
 * interrupt is full. */
__attribute__((unused)) static void hlDspFill(void)
{
    while (1)
    {
        uint16_t next = hlOutWrite + 2;
        if (next >= HL_DSP_OUT_WORDS)
            next = 0;
        if (next == hlOutRead)
            return;
        hlDspTick();
        hlOutRing[hlOutWrite] = hlOutL;
        hlOutRing[hlOutWrite + 1] = hlOutR;
        hlOutWrite = next;
    }
}

/* The interrupt's part: the next pair for the speakers, in hlPortL and
 * hlPortR. If the main loop has not kept up there is none; then the last
 * pair is sent again, a quarter quieter each time, so that falling behind is
 * a dip and not a bang. */
__attribute__((unused)) static void hlDspPull(void)
{
    int32_t wide;
    if (hlOutRead != hlOutWrite)
    {
        uint16_t next = hlOutRead + 2;
        hlPortL = hlOutRing[hlOutRead];
        hlPortR = hlOutRing[hlOutRead + 1];
        if (next >= HL_DSP_OUT_WORDS)
            next = 0;
        hlOutRead = next;
        return;
    }
    if (hlStarved < 0x7FFF)
        hlStarved++;
    wide = hlPortL;
    wide = hlShift(wide, 2);
    hlPortL = HL16((int32_t)hlPortL - wide);
    wide = hlPortR;
    wide = hlShift(wide, 2);
    hlPortR = HL16((int32_t)hlPortR - wide);
}

/* What a command does, and its answer. */
__attribute__((unused)) static uint16_t hlDspCommand(uint16_t command, uint16_t arg1, uint16_t arg2)
{
    uint16_t answer = 0;
    if (command == HL_DSP_CMD_PING)
    {
        answer = HL_DSP_MAGIC;
    }
    else if (command == HL_DSP_CMD_MUSIC_START)
    {
        hlMusicOn = 0;
        hlMusicWrite = arg1 < HL_DSP_MUSIC_WORDS ? arg1 : 0;
        hlMusicStart();
    }
    else if (command == HL_DSP_CMD_MUSIC_STOP)
    {
        hlMusicOn = 0;
    }
    else if (command == HL_DSP_CMD_MUSIC_FED)
    {
        if (arg1 < HL_DSP_MUSIC_WORDS)
            hlMusicWrite = arg1;
        answer = hlMusicRead;
    }
    else if (command == HL_DSP_CMD_SEND_FED)
    {
        if (arg1 < HL_DSP_SEND_WORDS)
            hlSendWrite = arg1;
        answer = hlSendRead;
    }
    else if (command == HL_DSP_CMD_ROOM)
    {
        hlFeedback = HL16(arg1 & 0x7FFF);
        hlDamp = HL16((arg2 >> 8) << 6);
        hlLevel = HL16(arg2 & 0xFF);
    }
    else if (command == HL_DSP_CMD_STATUS)
    {
        answer = hlDry + hlStarved;
        if (answer > 0x7FFF)
            answer = 0x7FFF;
        if (hlFault)
            answer |= HL_DSP_FAULT;
        hlDry = 0;
        hlStarved = 0;
    }
    else if (command == HL_DSP_CMD_HEARD)
    {
        answer = hlHeard;
        hlHeard = 0;
    }
    return answer;
}

#endif
