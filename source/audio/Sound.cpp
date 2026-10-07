#include "audio/Sound.hpp"

#include <cstdlib>
#include <cstring>
#include <string>

#include <nds.h>

#include "Global.hpp"
#include "Settings.hpp"
#include "audio/DspAudio.hpp"
#include "game/Player.hpp"
#include "data/LevelFile.hpp"
#include "game/Game.hpp"
#include "render/Camera.hpp"

namespace hlrt
{
    Sound sound;

    namespace
    {
        using namespace hldata;

        /// Samples of music played in one vertical blank, in 256ths: the
        /// channel's real rate (16005 Hz, the nearest its timer comes to
        /// 16000) over the screen's (59.826 Hz).
        constexpr std::uint32_t kMusicPerBlank = 68483;
    } // namespace

    void Sound::init()
    {
        soundEnable();
        // A DSi's extras: the output at 47.6 kHz instead of 32.7 (asked for
        // after the sound is switched on, which puts it back to 32.7, and
        // before anything plays), and the DSP. Neither exists on a DS, and
        // then nothing here does anything.
        // The DSP is used only if the player has asked for it (the pause
        // menu): until then a DSi sounds as a DS does, at the higher rate.
        if (isDSiMode())
        {
            soundExtSetFrequency(47);
            soundExtSetRatio(8); // all of the speakers to the ordinary channels
            extended_ = settings.dspAudio && dspAudio.start();
        }
        lastVblank_ = Global::vblanks();
    }

    bool Sound::readClip(const char *name, Clip &clip)
    {
        clip = Clip();
        const std::string path = global.dataPath("snd/") + name + ".hsn";
        std::FILE *file = std::fopen(path.c_str(), "rb");
        if (file == nullptr)
            return false;
        std::fseek(file, 0, SEEK_END);
        const long size = std::ftell(file);
        std::fseek(file, 0, SEEK_SET);
        // The header is checked against the file's real size before anything
        // in it sizes an allocation or is handed to the hardware.
        SoundHeader header;
        bool ok = size > static_cast<long>(sizeof(header)) && size < 4 * 1024 * 1024 &&
                  std::fread(&header, sizeof(header), 1, file) == 1 &&
                  soundHeaderValid(header, static_cast<std::uint32_t>(size));
        std::uint8_t *data = nullptr;
        if (ok)
        {
            data = static_cast<std::uint8_t *>(std::malloc(header.dataBytes));
            ok = data != nullptr && std::fread(data, 1, header.dataBytes, file) == header.dataBytes;
        }
        std::fclose(file);
        if (!ok)
        {
            std::free(data);
            return false;
        }
        // The ARM7 plays it out of memory itself: it must be there, not in
        // this processor's cache.
        DC_FlushRange(data, header.dataBytes);
        clip.data = data;
        clip.bytes = header.dataBytes;
        clip.loopWord = header.loopWord;
        clip.rate = header.rate;
        clip.ms = static_cast<std::uint32_t>((static_cast<std::uint64_t>(header.samples) * 1000) / header.rate);
        return true;
    }

    void Sound::load()
    {
        const LevelView &view = level.view;
        const int count = static_cast<int>(view.soundCount);
        clips_ = static_cast<Clip *>(std::calloc(count + 1, sizeof(Clip)));
        bool *kept = static_cast<bool *>(std::calloc(count + 1, sizeof(bool)));
        if (clips_ == nullptr || kept == nullptr)
        {
            std::free(clips_);
            std::free(kept);
            clips_ = nullptr;
            return;
        }
        count_ = count;
        soundsKept(view.logic, view.logicCount, view.logicSounds, view.logicSoundCount,
                   view.soundCount, kept);
        for (int i = 0; i < count; ++i)
        {
            if (!kept[i] || view.sounds[i].name[0] == '\0')
                continue;
            // Left with too little to play in, the map does without the
            // rest of its sounds rather than without its memory.
            if (global.heapHeadroom() < 300 * 1024)
                break;
            if (readClip(view.sounds[i].name, clips_[i]))
            {
                ++loaded_;
                bytes_ += static_cast<int>(clips_[i].bytes);
            }
        }
        std::free(kept);
        lastVblank_ = Global::vblanks();
        resumeMusic();
    }

    void Sound::holdMusic()
    {
        // Put down where it is, to be taken up again -- by whichever of the
        // two ways of playing it there then is, so where it is is kept in
        // milliseconds.
        if (dspAudio.musicPlaying())
        {
            musicHeld_ = true;
            musicTrack_ = dspAudio.musicTrack();
            musicHeldMs_ = static_cast<std::uint32_t>(
                (static_cast<std::uint64_t>(dspAudio.musicPair()) * 1000) / kStereoRate);
            dspAudio.stopMusic();
        }
        if (music_ != nullptr)
        {
            const std::uint32_t played =
                ((Global::vblanks() - musicStart_) * kMusicPerBlank) >> 8;
            const int track = musicTrack_;
            const std::uint32_t whole = musicEnd_;
            const std::uint32_t before = musicSkipped_;
            stopMusic();
            if (played < whole)
            {
                musicHeld_ = true;
                musicTrack_ = track;
                musicHeldMs_ = static_cast<std::uint32_t>(
                    (static_cast<std::uint64_t>(before + played) * 1000) / kMusicRate);
            }
        }
    }

    void Sound::resumeMusic()
    {
        if (!musicHeld_)
            return;
        musicHeld_ = false;
        const int track = musicTrack_;
        musicTrack_ = -1;
        // The DSP's way, taken up at the block that holds the moment...
        if (dspAudio.up() &&
            dspAudio.startMusic(track, static_cast<std::uint32_t>(
                                           (static_cast<std::uint64_t>(musicHeldMs_) * kStereoRate) / 1000)))
            return;
        // ... or the DS's.
        startMusic(track);
        const std::uint32_t at = musicHeldMs_ * (kMusicRate / 1000);
        if (music_ != nullptr && at < musicLeft_)
        {
            std::fseek(music_, static_cast<long>(sizeof(MusicHeader) + at), SEEK_SET);
            musicLeft_ -= at;
            musicEnd_ -= at;
            musicSkipped_ = at;
        }
    }

    bool Sound::setDsp(bool on)
    {
        if (on == extended_)
            return extended_;
        holdMusic();
        if (on)
        {
            extended_ = dspAudio.start();
        }
        else
        {
            dspAudio.stop();
            extended_ = false;
        }
        resumeMusic();
        return extended_;
    }

    void Sound::unload()
    {
        for (int ch = 0; ch < 16; ++ch)
        {
            // (Not the music's, which is dealt with below; nor the one the
            // capture keeps time by, which plays nothing.)
            if (ch == kMusicChannel || ch == 1)
                continue;
            soundKill(ch);
            channels_[ch] = Channel();
        }
        // The music is put down where it is, to be taken up again: a map
        // takes seconds to load, and the buffer would go round on itself.
        holdMusic();
        // (A blank, so that the ARM7 has let go of everything before it is
        // freed.)
        swiWaitForVBlank();
        for (int i = 0; i < count_; ++i)
            std::free(clips_[i].data);
        std::free(clips_);
        std::free(word_);
        word_ = nullptr;
        clips_ = nullptr;
        count_ = 0;
        loaded_ = 0;
        bytes_ = 0;
        spokenCount_ = spokenAt_ = 0;
        wordLeftMs_ = 0;
    }

    void Sound::place(const fx *pos, int volume, int radius, int &outVolume, int &outPan) const
    {
        const fx d[3] = {pos[0] - camera.pos[0], pos[1] - camera.pos[1], pos[2] - camera.pos[2]};
        const fx distance = fxLength(d);
        outVolume = soundGain(volume, fxToInt(distance), radius);
        // To one side by how far round it is: straight ahead and straight
        // behind are the middle.
        outPan = 64;
        if (distance > fxFromInt(8))
        {
            const std::int64_t side = static_cast<std::int64_t>(d[0]) * camera.right[0] +
                                      static_cast<std::int64_t>(d[1]) * camera.right[1] +
                                      static_cast<std::int64_t>(d[2]) * camera.right[2];
            outPan = 64 + static_cast<int>((side * 56) / (static_cast<std::int64_t>(distance) << kFxShift));
            outPan = outPan < 0 ? 0 : (outPan > 127 ? 127 : outPan);
        }
    }

    void Sound::playOnce(const SoundEvent &event)
    {
        if (event.sound >= count_ || clips_[event.sound].data == nullptr)
            return;
        const Clip &clip = clips_[event.sound];
        int volume = event.volume, pan = 64;
        if (!(event.flags & kSoundHere))
            place(event.pos, event.volume, event.radius, volume, pan);
        if (volume < 3)
            return;
        // A free channel, or the one nearest its end.
        int best = kFirstShot;
        for (int ch = kFirstShot; ch <= kLastShot; ++ch)
        {
            if (channels_[ch].leftMs < channels_[best].leftMs)
                best = ch;
        }
        soundPlaySampleChannel(best, clip.data, SoundFormat_ADPCM, clip.bytes, clip.rate,
                               static_cast<std::uint8_t>(volume), static_cast<std::uint8_t>(pan),
                               false, 0);
        channels_[best].leftMs = static_cast<std::int32_t>(clip.ms) + 34;
    }

    void Sound::say(int sentence)
    {
        const LevelView &view = level.view;
        if (sentence < 0 || sentence >= static_cast<int>(view.sentenceCount))
            return;
        // A line cuts in on the one before it, as on the game's own voice
        // channel.
        const SentenceRecord &record = view.sentences[sentence];
        spokenCount_ = 0;
        spokenAt_ = 0;
        for (int w = 0; w < record.wordCount && spokenCount_ < kMaxSpoken; ++w)
            spoken_[spokenCount_++] = view.sentenceWords[record.firstWord + w];
        spokenVolume_ = record.volume;
        wordLeftMs_ = 0;
        soundKill(kSpeechChannel);
    }

    void Sound::speechTick(int ms)
    {
        if (wordLeftMs_ > 0)
        {
            wordLeftMs_ -= ms;
            if (wordLeftMs_ > 0)
                return;
        }
        // The word in the air is over: its memory can go (the channel has
        // stopped by itself), and the next is read.
        if (word_ != nullptr)
        {
            soundKill(kSpeechChannel);
            std::free(word_);
            word_ = nullptr;
        }
        while (spokenAt_ < spokenCount_)
        {
            const std::uint16_t index = spoken_[spokenAt_++];
            if (index >= level.view.soundCount || level.view.sounds[index].name[0] == '\0' ||
                global.heapHeadroom() < 200 * 1024)
                continue;
            Clip clip;
            // (In memory already, if something else of the map plays it.)
            const bool borrowed = index < count_ && clips_[index].data != nullptr;
            if (borrowed)
                clip = clips_[index];
            else if (!readClip(level.view.sounds[index].name, clip))
                continue;
            soundPlaySampleChannel(kSpeechChannel, clip.data, SoundFormat_ADPCM, clip.bytes, clip.rate,
                                   static_cast<std::uint8_t>(spokenVolume_), 64, false, 0);
            word_ = borrowed ? nullptr : clip.data;
            wordLeftMs_ = static_cast<std::int32_t>(clip.ms) + 20;
            return;
        }
        spokenCount_ = spokenAt_ = 0;
    }

    void Sound::loopsTick()
    {
        const LevelView &view = level.view;
        // The four loudest of the loops that are on, where the camera is.
        int want[kLoopChannels], wantVolume[kLoopChannels], wantPan[kLoopChannels];
        int wanted = 0;
        for (std::uint32_t i = 0; i < view.logicCount; ++i)
        {
            const LogicRecord &record = view.logic[i];
            if (record.kind != static_cast<std::uint8_t>(LogicKind::Ambient) ||
                !(record.flags & kLogicToggle) || !game.logic.states[i].running ||
                game.logic.states[i].dead || record.extra[0] >= static_cast<std::uint32_t>(count_) ||
                clips_[record.extra[0]].data == nullptr)
                continue;
            int volume = static_cast<int>(record.extra[2]), pan = 64;
            if (record.extra[1] != 0)
                place(record.move, volume, static_cast<int>(record.extra[1]), volume, pan);
            if (volume < 3)
                continue;
            int at = wanted < kLoopChannels ? wanted : kLoopChannels - 1;
            if (wanted >= kLoopChannels && volume <= wantVolume[at])
                continue;
            while (at > 0 && wantVolume[at - 1] < volume)
            {
                want[at] = want[at - 1];
                wantVolume[at] = wantVolume[at - 1];
                wantPan[at] = wantPan[at - 1];
                --at;
            }
            want[at] = static_cast<int>(i);
            wantVolume[at] = volume;
            wantPan[at] = pan;
            if (wanted < kLoopChannels)
                ++wanted;
        }
        // Those already playing stay on their channels (and are only told
        // of a change worth telling); the rest stop; the new ones start
        // where there is room.
        bool placed[kLoopChannels] = {false, false, false, false};
        for (int slot = 0; slot < kLoopChannels; ++slot)
        {
            const int ch = kLoopChannel[slot];
            Channel &channel = channels_[ch];
            if (channel.record < 0)
                continue;
            int found = -1;
            for (int n = 0; n < wanted; ++n)
            {
                if (want[n] == channel.record)
                    found = n;
            }
            if (found < 0)
            {
                soundKill(ch);
                channel = Channel();
                continue;
            }
            placed[found] = true;
            const int dv = wantVolume[found] - channel.volume, dp = wantPan[found] - channel.pan;
            if (dv > 2 || dv < -2)
            {
                channel.volume = static_cast<std::uint8_t>(wantVolume[found]);
                soundSetVolume(ch, channel.volume);
            }
            if (dp > 3 || dp < -3)
            {
                channel.pan = static_cast<std::uint8_t>(wantPan[found]);
                soundSetPan(ch, channel.pan);
            }
        }
        for (int n = 0; n < wanted; ++n)
        {
            if (placed[n])
                continue;
            for (int slot = 0; slot < kLoopChannels; ++slot)
            {
                const int ch = kLoopChannel[slot];
                Channel &channel = channels_[ch];
                if (channel.record >= 0)
                    continue;
                const Clip &clip = clips_[view.logic[want[n]].extra[0]];
                const bool loops = clip.loopWord != kSoundNoLoop && clip.loopWord < 0xFFFF;
                channel.record = static_cast<std::int16_t>(want[n]);
                channel.volume = static_cast<std::uint8_t>(wantVolume[n]);
                channel.pan = static_cast<std::uint8_t>(wantPan[n]);
                soundPlaySampleChannel(ch, clip.data, SoundFormat_ADPCM, clip.bytes, clip.rate,
                                       channel.volume, channel.pan, loops,
                                       static_cast<std::uint16_t>(loops ? clip.loopWord : 0));
                break;
            }
        }
    }

    void Sound::roomTick()
    {
        // What the room sounds like: under water, that; otherwise the
        // nearest of the map's marked places that the player is within.
        int type = 0;
        if (player.state.waterLevel == 3)
        {
            type = 15;
        }
        else
        {
            fx nearest = 0;
            const LevelView &view = level.view;
            for (std::uint32_t i = 0; i < view.roomCount; ++i)
            {
                const RoomRecord &room = view.rooms[i];
                const fx d[3] = {room.origin[0] - camera.pos[0], room.origin[1] - camera.pos[1],
                                 room.origin[2] - camera.pos[2]};
                const fx distance = fxLength(d);
                if (distance > fxFromInt(room.radius) || (type != 0 && distance >= nearest))
                    continue;
                // (A place marked "normal" counts: walking into one is how a
                // map ends an echo.)
                type = room.type == 0 ? -1 : room.type;
                nearest = distance;
            }
        }
        if (type != 0)
            dspAudio.room(roomEcho(type < 0 ? 0 : type));
    }

    void Sound::startMusic(int track)
    {
        if (dspAudio.musicPlaying() && dspAudio.musicTrack() == track)
            return;
        if (track == musicTrack_ && music_ != nullptr)
            return;
        stopMusic();
        dspAudio.stopMusic();
        // A DSi's DSP plays it in stereo, if it is there and so is the file.
        if (dspAudio.startMusic(track, 0))
            return;
        char name[16];
        musicFileName(track, name, sizeof(name));
        const std::string path = global.dataPath("music/") + name;
        std::FILE *file = std::fopen(path.c_str(), "rb");
        if (file == nullptr)
            return;
        MusicHeader header;
        std::fseek(file, 0, SEEK_END);
        const long size = std::ftell(file);
        std::fseek(file, 0, SEEK_SET);
        if (std::fread(&header, sizeof(header), 1, file) != 1 ||
            std::memcmp(header.magic, kMusicMagic, 4) != 0 || header.version != kSoundVersion ||
            header.rate != kMusicRate || size < static_cast<long>(sizeof(header)) ||
            header.bytes > static_cast<std::uint32_t>(size) - sizeof(header) ||
            global.heapHeadroom() < 300 * 1024)
        {
            std::fclose(file);
            return;
        }
        ring_ = static_cast<std::uint8_t *>(std::calloc(kMusicRing, 1));
        if (ring_ == nullptr)
        {
            std::fclose(file);
            return;
        }
        // (No buffer of the C library's own: the reads are small and
        // regular, and a second copy of each is time and memory for
        // nothing.)
        std::setvbuf(file, nullptr, _IONBF, 0);
        music_ = file;
        musicSkipped_ = 0;
        musicTrack_ = track;
        musicLeft_ = header.bytes;
        musicEnd_ = header.bytes;
        musicWritten_ = 0;
        musicStart_ = Global::vblanks();
        // Half the ring before it starts; the rest is kept ahead of the
        // playing by musicTick.
        const std::uint32_t first = musicLeft_ < kMusicRing / 2 ? musicLeft_ : kMusicRing / 2;
        const std::size_t got = std::fread(ring_, 1, first, music_);
        musicLeft_ -= static_cast<std::uint32_t>(got);
        musicWritten_ = kMusicRing / 2;
        DC_FlushRange(ring_, kMusicRing);
        musicStart_ = Global::vblanks();
        soundPlaySampleChannel(kMusicChannel, ring_, SoundFormat_8Bit, kMusicRing, kMusicRate, 100, 64,
                               true, 0);
    }

    void Sound::stopMusic()
    {
        if (music_ == nullptr)
            return;
        soundKill(kMusicChannel);
        swiWaitForVBlank();
        std::fclose(music_);
        std::free(ring_);
        music_ = nullptr;
        ring_ = nullptr;
        musicTrack_ = -1;
    }

    void Sound::musicTick(std::uint32_t vblanks)
    {
        if (music_ == nullptr)
            return;
        const std::uint32_t played = ((vblanks - musicStart_) * kMusicPerBlank) >> 8;
        if (played >= musicEnd_ + kMusicRing / 4)
        {
            stopMusic(); // the track is over
            return;
        }
        // Kept half a ring ahead of what is playing, a block at a time (a
        // block is an eighth of a second: one read every seven or eight
        // frames).
        constexpr std::uint32_t kBlock = 2048;
        int blocks = 0;
        while (musicWritten_ < played + kMusicRing / 2 && blocks++ < 4)
        {
            std::uint8_t *to = ring_ + (musicWritten_ % kMusicRing);
            const std::uint32_t want = musicLeft_ < kBlock ? musicLeft_ : kBlock;
            const std::size_t got = want != 0 ? std::fread(to, 1, want, music_) : 0;
            musicLeft_ -= static_cast<std::uint32_t>(got);
            // Past the end of the track: silence, so that what is left in
            // the ring from two seconds ago is not heard again.
            if (got < kBlock)
                std::memset(to + got, 0, kBlock - got);
            DC_FlushRange(to, kBlock);
            musicWritten_ += kBlock;
        }
    }

    void Sound::frame()
    {
        const std::uint32_t now = Global::vblanks();
        const int ms = static_cast<int>(((now - lastVblank_) * 1000 + 30) / 60);
        lastVblank_ = now;
        musicTick(now);
        dspAudio.frame();
        // A DSP that has let the game down is done without from here on:
        // the music goes on the DS's way.
        if (extended_ && dspAudio.failed())
        {
            // (Said in the log, so that it is known to have happened.)
            if (std::FILE *log = std::fopen(global.dataPath("perf.txt").c_str(), "a"))
            {
                std::fprintf(log, "dsp given up: %d faults, ran short %lu times, %s\n", dspAudio.faults(),
                             static_cast<unsigned long>(dspAudio.dry()),
                             dspAudio.up() ? "answering" : "not answering");
                std::fclose(log);
            }
            setDsp(false);
        }
        if (clips_ == nullptr)
            return;
        if (dspAudio.up())
        {
            roomDue_ -= ms;
            if (roomDue_ <= 0)
            {
                roomDue_ = 500;
                roomTick();
            }
        }
        for (int ch = kFirstShot; ch <= kLastShot; ++ch)
        {
            if (channels_[ch].leftMs > 0)
                channels_[ch].leftMs -= ms;
        }
        SoundQueue &queue = game.heard;
        for (int i = 0; i < queue.count; ++i)
        {
            const SoundEvent &event = queue.events[i];
            if (event.flags & kSoundSentence)
                say(event.sound);
            else
                playOnce(event);
        }
        queue.count = 0;
        speechTick(ms);
        // The loops are looked at a few times a second: each change is a
        // message to the other processor.
        loopsDue_ -= ms;
        if (loopsDue_ <= 0)
        {
            loopsDue_ = 100;
            loopsTick();
        }
        if (game.logic.musicRequest >= 0)
        {
            startMusic(game.logic.musicRequest);
            game.logic.musicRequest = -1;
        }
    }
} // namespace hlrt
