#pragma once

// System for the SD card to be shared with the ARM7 directly.
// The ARM7 core plays the game's audio from a data image it reads off the card itself,
// through the same driver the ARM9's libfat reaches the card by.
// The messages it sends and receives are defined in A7SdProto.h.

namespace hlrt::cardshare
{
    enum class Result
    {
        Ok,             // image open on ARM7
        DriverOnArm9,   // DLDI is ARM9-only and ARM7 cannot reach the card directly
        NoReply,        // ARM7 core did not answer in time (missing SD channel)
        Refused         // request refused, call `detail()` for explanation
    };

    // On init, after the FAT is loaded (Global.cpp), before ARM7 is given anything else.
    // `device`: libfat's default drive ("sd:", "fat:");
    // `image`: data image's path from the card's root ("hl/audio.nds", no drive).
    Result attach(const char *device, const char *image);

    // Once per frame, pass of the main loop.
    // Provide the read ARM7 has asked for, if any, when the card's driver is on ARM9.
    void service();

    // A7SD_* status of the last answer (A7SD_OK if none).
    int detail();
}
