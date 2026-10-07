#pragma once

// What is heard: the map's sounds in memory, and the DS's sixteen sound
// channels shared out between them.
//
//   channels 0, 2, 3, 15   ambient loops: of all the map's loops that are
//                          switched on, the four loudest where the player
//                          stands
//   channel  1             nothing: its clock is the sound capture's (on a
//                          DSi, what echoes is captured: audio/DspAudio)
//   channels 4..12         everything that is heard once
//   channel  13            spoken lines, a word after a word
//   channel  14            music, on a DS (a DSi's DSP plays it, in stereo)
//
// The game's shared code says what was heard and where (a SoundQueue); this
// turns each into a volume and a side by where the camera is, and plays it.
// A map's sounds are read into memory when it loads, except the words of
// its spoken lines: those are long, said once, and read off the card as they
// are spoken. Music is read off the card as it plays, through a buffer the
// hardware goes round and round.
//
// Sound data is played by the ARM7 straight out of main memory, so nothing
// here may be freed while a channel is still on it: unload() silences
// everything first.

#include <cstdint>
#include <cstdio>

#include "HlSound.hpp"

namespace hlrt
{
    class Sound
    {
    public:
        /// Once, at start-up.
        void init();
        /// Read the loaded level's sounds. Call once the game is up.
        void load();
        /// Silence everything but the music, and let the level's sounds go.
        /// The music is held where it is until load() is called again.
        void unload();
        /// Every pass of the main loop: plays what the game has queued, keeps
        /// the loops and the spoken line going, and feeds the music.
        void frame();

        /// On a DSi: the DSP is in use (stereo music, echo).
        bool extended() const { return extended_; }
        /// Switch the DSP's part on or off, the music going on from where it
        /// was by the other way of playing it. Returns whether it is on (it
        /// cannot be, where there is no DSP).
        bool setDsp(bool on);

        int loaded() const { return loaded_; }
        int bytes() const { return bytes_; }

    private:
        struct Clip
        {
            std::uint8_t *data = nullptr; ///< the ADPCM; null if not in memory
            std::uint32_t bytes = 0;
            std::uint32_t loopWord = hldata::kSoundNoLoop;
            std::uint16_t rate = 0;
            std::uint32_t ms = 0; ///< how long it lasts
        };
        struct Channel
        {
            std::int32_t leftMs = 0; ///< until it is free again (one-shots)
            std::int16_t record = -1; ///< the Ambient record looping on it; -1 = none
            std::uint8_t volume = 0;
            std::uint8_t pan = 64;
        };
        static constexpr int kLoopChannels = 4;
        /// The hardware channel each loop slot plays on.
        static constexpr int kLoopChannel[kLoopChannels] = {0, 2, 3, 15};
        static constexpr int kFirstShot = 4;
        static constexpr int kLastShot = 12;
        static constexpr int kSpeechChannel = 13;
        static constexpr int kMusicChannel = 14;
        static constexpr int kMaxSpoken = 24;
        static constexpr int kMusicRing = 32768; ///< two seconds of it

        bool readClip(const char *name, Clip &clip);
        void place(const hldata::fx *pos, int volume, int radius, int &outVolume, int &outPan) const;
        void playOnce(const hldata::SoundEvent &event);
        void say(int sentence);
        void speechTick(int ms);
        void loopsTick();
        void startMusic(int track);
        void stopMusic();
        void musicTick(std::uint32_t vblanks);

        Clip *clips_ = nullptr;
        int count_ = 0;
        int loaded_ = 0;
        int bytes_ = 0;
        void roomTick();
        void holdMusic();
        void resumeMusic();
        bool extended_ = false;
        int roomDue_ = 0;
        Channel channels_[16];
        std::uint32_t lastVblank_ = 0;
        int loopsDue_ = 0;

        // The line being spoken: the words still to come, and the one in
        // the air (read off the card, freed when it is over).
        std::uint16_t spoken_[kMaxSpoken];
        int spokenCount_ = 0;
        int spokenAt_ = 0;
        int spokenVolume_ = 127;
        std::uint8_t *word_ = nullptr;
        std::int32_t wordLeftMs_ = 0;

        // The music.
        std::FILE *music_ = nullptr;
        std::uint8_t *ring_ = nullptr;
        std::uint32_t musicLeft_ = 0;    ///< bytes of the file not yet read
        std::uint32_t musicWritten_ = 0; ///< samples put into the ring since it started
        std::uint32_t musicStart_ = 0;   ///< the blank it started on
        std::uint32_t musicEnd_ = 0;     ///< samples in all: when to stop
        int musicTrack_ = -1;
        std::uint32_t musicHeldMs_ = 0;  ///< how far in it was when it was put down
        std::uint32_t musicSkipped_ = 0; ///< samples of the track before where it was taken up
        bool musicHeld_ = false;
    };

    extern Sound sound;
} // namespace hlrt
