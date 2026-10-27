// ARM9 <-> ARM7 protocol for the ARM7 audio mixer, compliant with the ARM7 core.
//
// The ARM7 core reads its audio off the SD card and NitroFS directly, and runs the
// entire audio engine autonomously, albeit from simple public APIs. The entire set
// of DSP effects are emulated by the ARM7, and the audio core resides in the ARM7.
//
// Generic features such as the DSP emulator are defined in `/arm7/source/backend/` and `/backend_mix/`,
// while a reimplementation of the GoldSrc audio engine is written directly above it,
// and is what largely defines the IPC FIFO protocol used here.
//
// THIS FILE IS NOT TO BE MODIFIED IN ANY WAY, SHAPE, OR FORM, EXCEPT BY THE ORIGINAL AUTHOR, NEWGBAXL.
// THIS CODE IS NOT PERMITTED TO BE USED IN ANY OTHER PROJECT OTHER THAN HALF-LIFE DS PORT BY WARIOWARE64.
//
// It is of utmost importance that this source stays compilant with the ARM7 core.
// The license will be made more explicit and open to external project use and external contributions when the ARM7 core is open-sourced.

#pragma once
#include <nds/ndstypes.h>
#include <nds/fifocommon.h>
#define A7_AUDIO_FIFO	FIFO_USER_02
#define A7M_FIFO        A7_AUDIO_FIFO		// uses custom protocol for Half-Life
#define A7M_NAME        48
#define A7M_ARENA_BYTES (2 * 1024 * 1024)   // default; A7M_ARENA_SIZE says less
#define A7M_ARENA_LEAST (512 * 1024)

enum A7MOp {
	// ARM9 -> ARM7
	A7M_EMIT = 1,       // EMIT_SOUND_DYN2 / EMIT_AMBIENT_SOUND (ambient + fixed origin a[]/16): ent, chan, name, vol1000, attn1000, flags (SND_*), pitch
	A7M_SENTENCE,       // "!N" or a sentence name: same fields; name without the '!'
	A7M_STOPALL,
	A7M_STOPENT,        // ent, chan (255 = any), ambient: stop what that entity plays
	A7M_ENTPOS,         // ent, a[0..2] = origin x 16
	A7M_LISTENER,       // a[0..2] = origin x 16, a[2] re-used as yaw degrees x 16 (z is 0)
	A7M_ROOM,           // flags = room type 0..28 (SVC_ROOMTYPE)
	A7M_UNDERWATER,     // flags = 0 / 1 (the DSP uses waterroom_type while the player's head is under water)
	A7M_FADE,           // a[0] = percent, a[1] = fade out s, a[2] = hold s, flags = fade in s
	A7M_MUSIC_PLAY,     // name = track (without extension), flags = loop, a[0] = -1 random point / 0 start
	A7M_MUSIC_STOP,
	A7M_MUSIC_VOL,      // flags = 0..127

	// ARM7 -> ARM9
	A7M_STATUS,         // A7MStatus
	A7M_LOG,            // name = a line for the log
	
	// More ARM9 -> ARM7
	A7M_EMIT_ID,        // as A7M_EMIT, but the sound is the bank's number, not its name: name[0..1] = id (little endian). Half-Life's level data names a sound by a key the ARM9 turns into this
	A7M_WORDS,          // a sentence already cut into words (gsEmitWords), 9 words to a message: ent, chan, vol1000, attn1000, pitch as A7M_EMIT; flags = total | first << 8 | n << 16;
	                    // name = n x { u16 id, u8 pitch, u8 vol, u8 end } (percent; 0 = as recorded / whole). The sentence starts when the last piece (first + n == total) has come
	A7M_ARENA_SIZE,     // a[0] = the bytes of the arena the address message that follows hands over (default A7M_ARENA_BYTES); taken in the FIFO interrupt, before the boot
	A7M_PAUSE,          // flags = 1 pause / 0 go on: the sounds of the game stand where they are (the music is the ARM9's to stop, A7M_MUSIC_STOP); a sound with ent GS_UI_ENT (0x7FFF) is not stopped
};

typedef struct {
	u8 op, ambient, fixed, chan;
	s16 ent, pitch;
	s32 vol1000, attn1000, flags;
	s32 a[3];
	char name[A7M_NAME];
} A7MMsg;

#define A7M_WORDS_PER_MSG 9
#define A7M_UI_ENT     0x7FFF
#define A7M_CHF_USED   1
#define A7M_CHF_LOOP   2
#define A7M_CHF_AMBIENT 4
#define A7M_CHF_VOX    8
typedef struct { s16 ent; u8 chan, flags; u8 vol, pan; s16 pitch; u8 word, words; char name[14]; } A7MChan;      // 24 bytes

typedef struct {      // up to 4 chan per msg: a snapshot is as many messages as it needs, the first one has first = 0
	u8 op, n, first, musicOn;
	s16 fade, room;
	u16 cpuAvg, cpuMax;      // percent of a quarter's time, last second
	u16 underruns, voices;
	u32 musicPosMs;
	A7MChan ch[4];
} A7MStatus;
