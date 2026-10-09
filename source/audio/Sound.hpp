#pragma once

// What is heard: the map's sounds in memory, played through maxmod (by way
// of NEA where NEA has the call), in layers.
//
// A layer (HlSound.hpp) is a kind of sound: the player's own, the world's,
// the voice, the loops of a place, the menu. Each has its share of the
// fifteen voices and gives way in its turn (HlMixer.hpp has the rules), and
// each has a volume the player sets. The music is beside them: a stream of
// maxmod's, read off the card as it plays, on a DS; the DSP's, in stereo, on
// a DSi that has it switched on.
//
// Maxmod plays samples out of a "sound bank". Ours is not a file: it is the
// map's cooked sounds where they already are in memory, each given to
// maxmod as a small header that points at it -- maxmod asks for a sample by
// number, and the number is the sound's index in the level (the menu's
// sounds and the word being spoken have numbers of their own at the top).
// The hardware plays the ADPCM itself, loops and all.
//
// The game's shared code says what was heard and where (a SoundQueue); this
// turns each into a volume and a side by where the camera is, plays it, and
// goes on placing it as the camera moves. A map's sounds are read into
// memory when it loads, except the words of its spoken lines: those are
// long, said once, and read off the card as they are spoken.
//
// Sound data is played by the ARM7 straight out of main memory, so nothing
// here may be freed while a voice is still on it: unload() silences
// everything first.

#include <cstdint>
#include <cstdio>

#include "HlMixer.hpp"
#include "HlUi.hpp"

namespace hlrt
{
    class Sound
    {
    public:
        /// Once, at start-up, before any map is loaded.
        void init();
        /// Read the loaded level's sounds. Call once the game is up.
        void load();
        /// Silence everything but the music, and let the level's sounds go.
        /// The music is held where it is until load() is called again.
        void unload();
        /// Every pass of the main loop: plays what the game has queued, keeps
        /// the loops and the spoken line going, and feeds the music.
        void frame();
        /// One of the menu's own sounds (hldata::kUiSound*).
        void ui(int which);
        /// The volume settings have changed.
        void applyVolumes();
        /// The game stands still (the menu): the world, the voice and the
        /// loops are silent until it goes on. The music and the menu's own
        /// sounds are not.
        void setPaused(bool paused);

        /// On a DSi: the DSP is in use (stereo music, echo).
        bool extended() const { return extended_; }
        /// Switch the DSP's part on or off, the music going on from where it
        /// was by the other way of playing it. Returns whether it is on (it
        /// cannot be, where there is no DSP).
        bool setDsp(bool on);

        int loaded() const { return loaded_; }
        /// How many voices each layer has just now (for the readout).
        int voicesOf(hldata::SoundLayer layer) const { return mixer_.count(layer); }
        int bytes() const { return bytes_; }

        /// For maxmod: the header of sample `id`, or null.
        const void *sample(std::uint32_t id) const;
        /// ... and `count` samples of music for its stream.
        void fillMusic(std::int8_t *out, std::uint32_t count);

    private:
        /// What maxmod wants in front of a sample: its file header and its
        /// sample header (mm_mas_prefix, mm_mas_ds_sample), the data being
        /// wherever `point` says.
        struct Header
        {
            std::uint32_t size;
            std::uint8_t type, version, reserved[2];
            std::uint32_t loopStart; ///< in words
            std::uint32_t length;    ///< in words: of the loop, or of it all
            std::uint8_t format;
            std::uint8_t repeat;
            std::uint16_t frequency;
            std::uint32_t point;
        };
        struct Clip
        {
            std::uint8_t *data = nullptr; ///< the ADPCM; null if not in memory
            std::uint32_t bytes = 0;
            std::uint32_t loopWord = hldata::kSoundNoLoop;
            std::uint16_t rate = 0;
            std::uint32_t ms = 0; ///< how long it lasts
            Header header{};
        };
        /// A voice of the mixer's, as it is being played.
        struct Voice
        {
            std::uint16_t handle = 0;
            bool placed = false;      ///< it has a place: it is put there again as the camera moves
            hldata::fx pos[3] = {0, 0, 0};
            std::uint8_t volume = 0;  ///< as loud as it is at its source
            std::int16_t radius = 0;
            std::uint8_t shown = 0;   ///< what maxmod was last told: how loud, and where
            std::uint8_t pan = 64;
        };
        static constexpr int kSamples = 1024;
        static constexpr int kSpeechSample = kSamples - 1;
        static constexpr int kUiSample = kSamples - 1 - hldata::kUiSoundCount;
        static constexpr int kMaxSpoken = 24;
        static constexpr int kStreamSamples = 8192; ///< half a second of music ahead

        bool readClip(const char *name, Clip &clip);
        void describe(Clip &clip);
        void place(const hldata::fx *pos, int volume, int radius, int &outVolume, int &outPan) const;
        int start(hldata::SoundLayer layer, int sample, const Clip &clip, int volume, int pan, bool loop,
                  int tag = -1);
        void stop(int voice);
        void playOnce(const hldata::SoundEvent &event);
        void say(const hldata::SoundEvent &event);
        void speechTick(int ms);
        void loopsTick();
        void follow();
        void startMusic(int track);
        void stopMusic();
        void musicTick();

        bool up_ = false;
        bool paused_ = false;
        Clip *clips_ = nullptr;
        int count_ = 0;
        int loaded_ = 0;
        int bytes_ = 0;
        Clip ui_[hldata::kUiSoundCount];
        void roomTick();
        void holdMusic();
        void resumeMusic();
        bool extended_ = false;
        int roomDue_ = 0;
        hldata::Mixer mixer_;
        Voice voices_[hldata::Mixer::kVoices];
        std::uint32_t lastVblank_ = 0;
        int loopsDue_ = 0;

        // The line being spoken: the words still to come, and the one in
        // the air (read off the card, freed when it is over).
        std::uint16_t spoken_[kMaxSpoken];
        int spokenCount_ = 0;
        int spokenAt_ = 0;
        int spokenVolume_ = 127;
        bool spokenPlaced_ = false;
        hldata::fx spokenPos_[3] = {0, 0, 0};
        std::int16_t spokenRadius_ = 0;
        Clip word_;              ///< the word read off the card, if that is where it came from
        bool wordLoaded_ = false; ///< maxmod has it as its speech sample
        int wordVoice_ = -1;
        std::int32_t wordLeftMs_ = 0;

        // The music (a DS's way: a stream of maxmod's).
        std::FILE *music_ = nullptr;
        std::uint32_t musicLeft_ = 0;    ///< bytes of the file not yet read
        std::uint32_t musicFed_ = 0;     ///< samples handed to the stream since it started
        std::uint32_t musicPast_ = 0;    ///< ... of them, silence after the track's end
        int musicTrack_ = -1;
        std::uint32_t musicHeldMs_ = 0;  ///< how far in it was when it was put down
        std::uint32_t musicSkipped_ = 0; ///< samples of the track before where it was taken up
        bool musicHeld_ = false;
    };

    extern Sound sound;
} // namespace hlrt
