#pragma once

// The DSi's DSP, as the game uses it: the music in stereo, and the echo of
// the room (the program is teak/, assembly; its model shared/dsp/hl_dsp.h).
//
// Nothing here exists on a DS, or on a DSi running the game without DSi
// rights: start() says no, and everything else does nothing. Sound then plays
// as it does on a DS.
//
// The DSP cannot hear the sixteen ordinary channels. What echoes is a copy
// of their mix that the ARM7 captures into `capture_`; each frame the new
// part of it is passed on to the DSP, which puts out the echo alone.

#include <cstdint>
#include <cstdio>

#include "HlSound.hpp"

namespace hlrt
{
    class DspAudio
    {
    public:
        /// Whether there is a DSP to be had at all: a DSi, run with DSi
        /// rights, and a ROM built with the DSP program in it.
        static bool available();
        /// Load the program and see that it answers. After the sound hardware
        /// is on.
        bool start();
        /// Silence it and switch it off: the speakers go back to the
        /// ordinary channels alone.
        void stop();
        bool up() const { return up_; }
        /// It has gone wrong too often (or stopped answering) to be used
        /// again this session: the caller goes back to the DS's way.
        bool failed() const { return failed_ || (started_ && !up_); }
        /// How often it has said its music had gone wrong, and the block of
        /// the track it was last given (for the overlay and the log).
        int faults() const { return faults_; }
        std::uint32_t block() const { return block_; }

        /// What the room sounds like from now on.
        void room(const hldata::RoomEcho &echo);

        /// Start a track (its .hsd file) `fromPair` sample pairs in. False if
        /// there is no DSP, or no such file: the caller plays it the DS way.
        bool startMusic(int track, std::uint32_t fromPair);
        void stopMusic();
        bool musicPlaying() const { return music_ != nullptr; }
        int musicTrack() const { return track_; }
        /// How far into its track the music is, in sample pairs.
        std::uint32_t musicPair() const;

        /// Every pass of the main loop: feeds the music and the echo.
        void frame();

        /// For the overlay and the log: times a ring ran dry, in all.
        std::uint32_t dry() const { return dry_; }
        /// ... and the loudest sample the DSP was lately sent to echo (0 to
        /// 32767): that the sound is getting to it at all.
        int heard() const { return heard_; }

    private:
        bool command(std::uint16_t cmd, std::uint16_t a1, std::uint16_t a2, std::uint16_t &answer);
        void write(const std::uint16_t *words, std::uint16_t ring, std::uint16_t at, int count, int size);
        void feedMusic();
        bool sendBlock();
        void feedEcho();

        bool up_ = false;
        bool failed_ = false;
        bool started_ = false;
        int faults_ = 0;
        std::uint16_t musicAt_ = 0, sendAt_ = 0;
        std::uint32_t dry_ = 0;
        int heard_ = 0;
        int statusDue_ = 0;

        // Music.
        std::FILE *music_ = nullptr;
        int track_ = -1;
        std::uint32_t blocks_ = 0, pairs_ = 0;
        std::uint32_t block_ = 0;     ///< the next block to send
        std::uint16_t musicWrite_ = 0;
        std::uint32_t startPair_ = 0; ///< where in the track it was started
        std::uint32_t startBlank_ = 0;

        // The echo's send.
        std::int16_t *capture_ = nullptr;
        std::uint32_t captureBlank_ = 0;
        std::uint32_t captureRead_ = 0; ///< samples taken out since it started
        std::uint16_t sendWrite_ = 0;
        hldata::RoomEcho echo_{0, 0, 0};
    };

    extern DspAudio dspAudio;
} // namespace hlrt
