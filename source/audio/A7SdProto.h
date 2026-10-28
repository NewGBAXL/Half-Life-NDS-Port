// ARM9 <-> ARM7 protocol for sharing SD card access, compliant with the ARM7 core.
// 
// The ARM7 core reads its data image off the SD card directly,
// without relying on the ARM9 to provide it. 
// 
// THIS FILE IS NOT TO BE MODIFIED IN ANY WAY, SHAPE, OR FORM, EXCEPT BY THE ORIGINAL AUTHOR, NEWGBAXL.
// THIS CODE IS NOT PERMITTED TO BE USED IN ANY OTHER PROJECT OTHER THAN HALF-LIFE DS PORT BY WARIOWARE64.
// 
// It is of utmost importance that this source stays compilant with the ARM7 core.
// The license will be made more explicit and open to external project use and external contributions when the ARM7 core is open-sourced.
// 
// A7SD_KIND_ARM9 is fallback for when the ARM7 cannot read DLDI directly.
// 
// Calling convention order:
// - `fatInitDefault()` (with DLDI on the ARM7: `dldiSetMode(DLDI_MODE_ARM7)` before it)
// - ONE setup message on A7SD_FIFO
// - wait for the reply (value32 on the same channel, status << 24)
// - finally give the core its other things (the audio arena...)
// The core boots when both are there; a core that never got a setup message boots from the cartridge (slot 1) as before.
//
// Sharing convention:
// ARM9's own storage requests (libfat -> FIFO_STORAGE -> libnds7 -> the same driver) and ARM7's reads must never be in the driver at once.
// ARM7 masks the FIFO interrupt around each read of at most A7SD_RUN sectors (4 KB): a request ARM9 makes meanwhile waits for it,
// at most one such read, and is served as the interrupt comes back. ARM9 does nothing for this. Between reads the core's mixer runs.

#pragma once
#include <stdint.h>
#include <nds/fifocommon.h>

#define A7SD_FIFO     FIFO_USER_05
#define A7SD_MAGIC    0x44533741u      // "A7SD"
#define A7SD_VERSION  1
#define A7SD_RUN      8                // sectors per read

enum {
	A7SD_KIND_DLDI = 1,
	A7SD_KIND_DSI_SD = 2,
	A7SD_KIND_ARM9 = 3
};

// A7SD_KIND_ARM9: the card's driver runs on the ARM9, so the ARM7 asks the ARM9 for each read.
// a data message A7SdRead on A7SD_FIFO, the sectors read into the ARM9's bounce buffer (`dldi` of the setup message: main RAM, A7SD_BOUNCE_SECTORS sectors, 32-byte aligned,
// written back out of the ARM9's cache before the reply), and a value32 back on A7SD_FIFO: status << 24 (0 = read) | tag. One request at a time. The ARM9 serves it from its main
// loop (CardShare service()), between its own uses of the card, so the two never meet in the driver; a request waits for the ARM9's turn (a frame, or a long load).
#define A7SD_BOUNCE_SECTORS 32
typedef struct {
	uint32_t sector;
	uint16_t count, tag;
} A7SdRead;

// ARM9 -> ARM7, one data message. `dldi`: for A7SD_KIND_DLDI the DISC_INTERFACE the driver lives in (dldiGetInternal() on the ARM9), a main RAM address; 0 for the DSi's SD.
// `path`: of the image on the card, from its root, '/' between the names ("hl/audio.npac").
typedef struct {
	uint32_t magic;
	uint16_t version, kind;
	uint32_t dldi;
	char path[88];
} A7SdSetup;

// ARM7 -> ARM9, a value32 on A7SD_FIFO: status << 24 | detail (the byte count of the image, in 512-byte sectors, for A7SD_OK: 0 when it is larger than 24 bits).
enum {
	A7SD_OK = 0,
	A7SD_BADMSG = 1,         // not a setup of this version
	A7SD_NOT_CAPABLE = 2,    // the DLDI driver cannot be used from the ARM7 (no FEATURE_ARM7_CAPABLE), or the DSi's SD on a DS
	A7SD_BADIO = 3,          // the driver pointer is not a driver
	A7SD_NO_VOLUME = 4,      // no FAT32 / exFAT volume on the card
	A7SD_NO_FILE = 5,        // the path is not a file there
	A7SD_BAD_IMAGE = 6       // the file is neither an NPAC archive nor a NitroFS image
};
