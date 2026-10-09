#include "audio/Sound.hpp"

#include <cstdlib>
#include <cstring>
#include <string>

#include <nds.h>

#include <NEAMain.h>
#include <maxmod9.h>
#include <mm_mas.h>

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

        /// Maxmod's table of what is loaded: a word a sample.
        mm_word bankMemory[1024];

        /// Maxmod asks for a sample by number, and says when it has done
        /// with one. The samples are ours and stay where they are: asked,
        /// it is told where; told, nothing is done.
        mm_word bankHandler(mm_word message, mm_word parameter)
        {
            if (message == MMCB_SAMPREQUEST)
                return reinterpret_cast<mm_word>(sound.sample(parameter));
            return 0;
        }

        mm_word musicHandler(mm_word length, mm_addr dest, mm_stream_formats format)
        {
            (void)format;
            sound.fillMusic(static_cast<std::int8_t *>(dest), length);
            return length;
        }
    } // namespace

    void Sound::init()
    {
        soundEnable();
        // A DSi's extras: the output at 47.6 kHz instead of 32.7 (asked of
        // NEA, after the sound is switched on -- which puts it back to 32.7
        // -- and before anything plays), and the DSP. Neither exists on a
        // DS, and then nothing here does anything.
        // The DSP is used only if the player has asked for it (the pause
        // menu): until then a DSi sounds as a DS does, at the higher rate.
        const bool dsi = NEA_SoundDSiOutputAvailable();
        if (dsi)
        {
            // (All of the speakers to the ordinary channels, until the DSP
            // has something for them: DspAudio.)
            NEA_SoundSetDSiMixRatio(8);
            NEA_SoundEnableDSiOutput(NEA_DSI_SOUND_FREQ_47KHZ);
        }
        // Maxmod, with a bank that is ours (Sound.hpp): NEA's own start-up
        // wants a bank file, so it is told only to make its sound ready,
        // and maxmod is started here.
        static_assert(sizeof(Header) == 24);
        static_assert(sizeof(bankMemory) / sizeof(bankMemory[0]) == kSamples);
        NEA_SoundSystemResetPool(1);
        mm_ds_system system{};
        system.mod_count = 0;
        system.samp_count = kSamples;
        system.mem_bank = bankMemory;
        system.fifo_channel = FIFO_MAXMOD;
        mmInit(&system);
        mmSetCustomSoundBankHandler(bankHandler);
        // (Channel 1 is not maxmod's: its clock is what a DSi's sound
        // capture keeps time by -- DspAudio.)
        mmLockChannels(1u << 1);
        up_ = true;
        applyVolumes();
        // The menu's own sounds, for as long as the game runs.
        for (int n = 0; n < kUiSoundCount; ++n)
        {
            if (global.hasFat && readClip(kUiSoundFiles[n], ui_[n]))
            {
                describe(ui_[n]);
                mmLoadEffect(kUiSample + n);
            }
        }
        if (dsi)
            extended_ = settings.dspAudio && dspAudio.start();
        lastVblank_ = Global::vblanks();
    }

    const void *Sound::sample(std::uint32_t id) const
    {
        if (id == static_cast<std::uint32_t>(kSpeechSample))
            return word_.data != nullptr ? &word_.header : nullptr;
        if (id >= static_cast<std::uint32_t>(kUiSample) && id < static_cast<std::uint32_t>(kUiSample + kUiSoundCount))
            return ui_[id - kUiSample].data != nullptr ? &ui_[id - kUiSample].header : nullptr;
        if (id < static_cast<std::uint32_t>(count_) && clips_ != nullptr && clips_[id].data != nullptr)
            return &clips_[id].header;
        return nullptr;
    }

    void Sound::applyVolumes()
    {
        const auto effects = settings.volume[kVolumeEffects];
        mixer_.volume[static_cast<int>(SoundLayer::Player)] = effects;
        mixer_.volume[static_cast<int>(SoundLayer::World)] = effects;
        mixer_.volume[static_cast<int>(SoundLayer::Ui)] = effects;
        mixer_.volume[static_cast<int>(SoundLayer::Voice)] = settings.volume[kVolumeVoices];
        mixer_.volume[static_cast<int>(SoundLayer::Ambience)] = settings.volume[kVolumeAmbience];
        // (The music's is taken as it is fed: fillMusic. What is playing in
        // the other layers is told when it is next placed.)
        loopsDue_ = 0;
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

    /// The header maxmod reads a clip by. The hardware plays the ADPCM as
    /// it is: all of it once, or round and round from where it loops.
    void Sound::describe(Clip &clip)
    {
        const std::uint32_t words = clip.bytes / 4;
        const bool loops = clip.loopWord != kSoundNoLoop && clip.loopWord < words && clip.loopWord < 0xFFFF;
        Header &h = clip.header;
        h = Header{};
        h.size = sizeof(Header) - 8;
        h.type = 2; // (a DS sample)
        h.loopStart = loops ? clip.loopWord : 0;
        h.length = loops ? words - clip.loopWord : words;
        h.format = MM_SFORMAT_ADPCM;
        h.repeat = loops ? MM_SREPEAT_FORWARD : MM_SREPEAT_OFF;
        // (Maxmod's unit: 1024ths of 32768 Hz.)
        h.frequency = static_cast<std::uint16_t>((static_cast<std::uint32_t>(clip.rate) * 1024 + 16384) / 32768);
        h.point = reinterpret_cast<std::uint32_t>(clip.data);
        DC_FlushRange(&h, sizeof(h));
    }

    void Sound::load()
    {
        const LevelView &view = level.view;
        // (The numbers at the top are the menu's and the spoken word's.)
        const int count = static_cast<int>(view.soundCount) < kUiSample ? static_cast<int>(view.soundCount) : kUiSample;
        clips_ = static_cast<Clip *>(std::calloc(count + 1, sizeof(Clip)));
        bool *kept = static_cast<bool *>(std::calloc(view.soundCount + 1, sizeof(bool)));
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
            // (A DSi has the memory for the better copy, where there is one.)
            char twin[sizeof(SoundRef)];
            if ((global.isDSi && soundTwinName(view.sounds[i], twin, sizeof(twin)) && readClip(twin, clips_[i])) ||
                readClip(view.sounds[i].name, clips_[i]))
            {
                describe(clips_[i]);
                if (up_)
                    mmLoadEffect(i);
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
            // (What has been handed over, less what the stream still has
            // in hand, less any silence after the end.)
            const std::uint32_t fed = musicFed_ - musicPast_;
            const std::uint32_t played = fed > kStreamSamples / 2 ? fed - kStreamSamples / 2 : 0;
            const int track = musicTrack_;
            const bool over = musicLeft_ == 0;
            const std::uint32_t before = musicSkipped_;
            stopMusic();
            if (!over)
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
        musicSkipped_ = musicHeldMs_ * (kMusicRate / 1000);
        startMusic(track);
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
        if (up_)
            mmEffectCancelAll();
        for (int v = 0; v < Mixer::kVoices; ++v)
        {
            mixer_.stop(v);
            voices_[v] = Voice();
        }
        // The music is put down where it is, to be taken up again: a map
        // takes seconds to load, and nothing would feed it.
        holdMusic();
        // (Two blanks, so that the ARM7 has let go of everything before it
        // is freed.)
        swiWaitForVBlank();
        swiWaitForVBlank();
        for (int i = 0; i < count_; ++i)
        {
            if (clips_[i].data != nullptr && up_)
                mmUnloadEffect(i);
            std::free(clips_[i].data);
        }
        if (wordLoaded_)
            mmUnloadEffect(kSpeechSample);
        wordLoaded_ = false;
        std::free(word_.data);
        word_ = Clip();
        wordVoice_ = -1;
        std::free(clips_);
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

    /// Play sample `sample` (which is `clip`) in `layer`, `volume` loud
    /// (0..127, before the layer's own volume) and `pan` to one side
    /// (0..127). The voice it got, or -1 if it is not to be heard.
    int Sound::start(SoundLayer layer, int sample, const Clip &clip, int volume, int pan, bool loop, int tag)
    {
        if (!up_ || (paused_ && layer != SoundLayer::Ui))
            return -1;
        const int gain = mixer_.scaled(layer, volume);
        if (gain < 3)
            return -1;
        const int v = mixer_.take(layer, gain);
        if (v < 0)
            return -1;
        if (mixer_.voices[v].active)
            stop(v);
        const mm_sfxhand handle = NEA_SfxPlayEx(static_cast<mm_word>(sample), static_cast<mm_byte>(gain * 2),
                                                static_cast<mm_byte>(pan * 2), 1024);
        if (handle == 0)
            return -1;
        voices_[v] = Voice();
        voices_[v].handle = handle;
        voices_[v].shown = static_cast<std::uint8_t>(gain);
        voices_[v].pan = static_cast<std::uint8_t>(pan);
        mixer_.start(v, layer, gain, loop ? -1 : static_cast<std::int32_t>(clip.ms) + 34, tag);
        return v;
    }

    void Sound::stop(int v)
    {
        if (voices_[v].handle != 0)
            NEA_SfxStop(voices_[v].handle);
        voices_[v] = Voice();
        mixer_.stop(v);
    }

    void Sound::playOnce(const SoundEvent &event)
    {
        if (event.sound >= count_ || clips_[event.sound].data == nullptr)
            return;
        const Clip &clip = clips_[event.sound];
        int volume = event.volume, pan = 64;
        const bool placed = !(event.flags & kSoundHere);
        if (placed)
            place(event.pos, event.volume, event.radius, volume, pan);
        const int v = start(event.layer, event.sound, clip, volume, pan, false);
        if (v < 0 || !placed)
            return;
        Voice &voice = voices_[v];
        voice.placed = true;
        voice.volume = event.volume;
        voice.radius = event.radius;
        for (int k = 0; k < 3; ++k)
            voice.pos[k] = event.pos[k];
    }

    /// What has a place is put there again as the camera moves and turns:
    /// louder and quieter, and from one side to the other.
    void Sound::follow()
    {
        for (int v = 0; v < Mixer::kVoices; ++v)
        {
            Voice &voice = voices_[v];
            MixVoice &mix = mixer_.voices[v];
            if (!mix.active || !voice.placed || mix.layer == SoundLayer::Ambience)
                continue;
            int volume, pan;
            place(voice.pos, voice.volume, voice.radius, volume, pan);
            const int gain = mixer_.scaled(mix.layer, volume);
            const int dv = gain - voice.shown, dp = pan - voice.pan;
            if (dv > 2 || dv < -2)
            {
                voice.shown = static_cast<std::uint8_t>(gain);
                mix.gain = voice.shown;
                NEA_SfxSetVolume(voice.handle, static_cast<mm_word>(gain * 2));
            }
            if (dp > 3 || dp < -3)
            {
                voice.pan = static_cast<std::uint8_t>(pan);
                NEA_SfxSetPanning(voice.handle, static_cast<mm_byte>(pan * 2));
            }
        }
    }

    void Sound::say(const SoundEvent &event)
    {
        const LevelView &view = level.view;
        const int sentence = event.sound;
        if (sentence < 0 || sentence >= static_cast<int>(view.sentenceCount))
            return;
        // A line cuts in on the one before it, as on the game's own voice
        // channel.
        const SentenceRecord &record = view.sentences[sentence];
        spokenCount_ = 0;
        spokenAt_ = 0;
        for (int w = 0; w < record.wordCount && spokenCount_ < kMaxSpoken; ++w)
            spoken_[spokenCount_++] = view.sentenceWords[record.firstWord + w];
        // (As loud as the line is said and as whoever says it is far off:
        // a line from somewhere comes from there.)
        spokenVolume_ = (record.volume * event.volume) / 127;
        spokenPlaced_ = !(event.flags & kSoundHere);
        spokenRadius_ = event.radius;
        for (int k = 0; k < 3; ++k)
            spokenPos_[k] = event.pos[k];
        wordLeftMs_ = 1; // (the word in the air is cut short at the next tick)
    }

    void Sound::speechTick(int ms)
    {
        if (wordLeftMs_ > 0)
        {
            wordLeftMs_ -= ms;
            if (wordLeftMs_ > 0)
                return;
        }
        // The word in the air is over: its voice and its memory can go,
        // and the next is read.
        if (wordVoice_ >= 0 && mixer_.voices[wordVoice_].active && mixer_.voices[wordVoice_].layer == SoundLayer::Voice)
            stop(wordVoice_);
        wordVoice_ = -1;
        if (wordLoaded_)
        {
            mmUnloadEffect(kSpeechSample);
            wordLoaded_ = false;
        }
        std::free(word_.data);
        word_ = Clip();
        while (spokenAt_ < spokenCount_)
        {
            const std::uint16_t index = spoken_[spokenAt_++];
            if (index >= level.view.soundCount || level.view.sounds[index].name[0] == '\0' ||
                global.heapHeadroom() < 200 * 1024)
                continue;
            int volume = spokenVolume_, pan = 64;
            if (spokenPlaced_)
                place(spokenPos_, spokenVolume_, spokenRadius_, volume, pan);
            // (In memory already, if something else of the map plays it.)
            const bool borrowed = index < count_ && clips_[index].data != nullptr;
            int v = -1;
            std::uint32_t lasts = 0;
            if (borrowed)
            {
                lasts = clips_[index].ms;
                v = start(SoundLayer::Voice, index, clips_[index], volume, pan, false);
            }
            else if (readClip(level.view.sounds[index].name, word_))
            {
                describe(word_);
                lasts = word_.ms;
                wordLoaded_ = up_ && mmLoadEffect(kSpeechSample) == 0;
                if (wordLoaded_)
                    v = start(SoundLayer::Voice, kSpeechSample, word_, volume, pan, false);
            }
            else
            {
                continue;
            }
            if (v >= 0 && spokenPlaced_)
            {
                Voice &voice = voices_[v];
                voice.placed = true;
                voice.volume = static_cast<std::uint8_t>(spokenVolume_);
                voice.radius = spokenRadius_;
                for (int k = 0; k < 3; ++k)
                    voice.pos[k] = spokenPos_[k];
            }
            // (Too far off to be heard, it still takes its time.)
            wordVoice_ = v;
            wordLeftMs_ = static_cast<std::int32_t>(lasts) + 20;
            return;
        }
        spokenCount_ = spokenAt_ = 0;
    }

    void Sound::loopsTick()
    {
        const LevelView &view = level.view;
        constexpr int kLoops = 4;
        // The four loudest of the loops that are on, where the camera is.
        int want[kLoops], wantVolume[kLoops], wantPan[kLoops];
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
            if (mixer_.scaled(SoundLayer::Ambience, volume) < 3)
                continue;
            int at = wanted < kLoops ? wanted : kLoops - 1;
            if (wanted >= kLoops && volume <= wantVolume[at])
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
            if (wanted < kLoops)
                ++wanted;
        }
        // Those already playing stay on their voices (and are only told of
        // a change worth telling); the rest stop; the new ones start.
        bool placed[kLoops] = {false, false, false, false};
        for (int v = 0; v < Mixer::kVoices; ++v)
        {
            MixVoice &mix = mixer_.voices[v];
            if (!mix.active || mix.layer != SoundLayer::Ambience)
                continue;
            int found = -1;
            for (int n = 0; n < wanted; ++n)
            {
                if (want[n] == mix.tag)
                    found = n;
            }
            if (found < 0)
            {
                stop(v);
                continue;
            }
            placed[found] = true;
            Voice &voice = voices_[v];
            const int gain = mixer_.scaled(SoundLayer::Ambience, wantVolume[found]);
            const int dv = gain - voice.shown, dp = wantPan[found] - voice.pan;
            if (dv > 2 || dv < -2)
            {
                voice.shown = static_cast<std::uint8_t>(gain);
                mix.gain = voice.shown;
                NEA_SfxSetVolume(voice.handle, static_cast<mm_word>(gain * 2));
            }
            if (dp > 3 || dp < -3)
            {
                voice.pan = static_cast<std::uint8_t>(wantPan[found]);
                NEA_SfxSetPanning(voice.handle, static_cast<mm_byte>(voice.pan * 2));
            }
        }
        for (int n = 0; n < wanted; ++n)
        {
            if (placed[n])
                continue;
            const int sample = static_cast<int>(view.logic[want[n]].extra[0]);
            const Clip &clip = clips_[sample];
            // (Something that does not loop is played through, and again
            // when it is over, for as long as it is on.)
            start(SoundLayer::Ambience, sample, clip, wantVolume[n], wantPan[n], clip.header.repeat == MM_SREPEAT_FORWARD,
                  want[n]);
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

    void Sound::ui(int which)
    {
        if (which < 0 || which >= kUiSoundCount || ui_[which].data == nullptr)
            return;
        start(SoundLayer::Ui, kUiSample + which, ui_[which], 110, 64, false);
    }

    void Sound::setPaused(bool paused)
    {
        if (paused == paused_)
            return;
        paused_ = paused;
        lastVblank_ = Global::vblanks();
        if (!paused)
        {
            loopsDue_ = 0;
            return;
        }
        // Everything of the game's own stops where it is. (The loops start
        // again when it goes on; a line goes on from its next word.)
        for (int v = 0; v < Mixer::kVoices; ++v)
        {
            if (mixer_.voices[v].active && mixer_.voices[v].layer != SoundLayer::Ui)
                stop(v);
        }
        wordVoice_ = -1;
    }

    void Sound::startMusic(int track)
    {
        if (dspAudio.musicPlaying() && dspAudio.musicTrack() == track)
            return;
        if (track == musicTrack_ && music_ != nullptr)
            return;
        const std::uint32_t skip = musicSkipped_;
        musicSkipped_ = 0;
        stopMusic();
        dspAudio.stopMusic();
        // A DSi's DSP plays it in stereo, if it is there and so is the file.
        if (dspAudio.startMusic(track, 0))
            return;
        if (!up_)
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
        // (No buffer of the C library's own: the reads are small and
        // regular, and a second copy of each is time and memory for
        // nothing.)
        std::setvbuf(file, nullptr, _IONBF, 0);
        music_ = file;
        musicTrack_ = track;
        musicLeft_ = header.bytes;
        musicFed_ = 0;
        musicPast_ = 0;
        // Taken up part of the way in, if it was put down there.
        if (skip != 0 && skip < musicLeft_)
        {
            std::fseek(music_, static_cast<long>(sizeof(MusicHeader) + skip), SEEK_SET);
            musicLeft_ -= skip;
            musicSkipped_ = skip;
        }
        // A stream of maxmod's, which asks for more when it is told to
        // look (musicTick): eight bits, one channel, as the file is.
        mm_stream stream{};
        stream.sampling_rate = kMusicRate;
        stream.buffer_length = kStreamSamples;
        stream.callback = musicHandler;
        stream.format = MM_STREAM_8BIT_MONO;
        stream.timer = MM_TIMER3;
        stream.manual = true;
        mmStreamOpen(&stream);
    }

    void Sound::fillMusic(std::int8_t *out, std::uint32_t count)
    {
        const std::uint32_t want = music_ == nullptr ? 0 : (musicLeft_ < count ? musicLeft_ : count);
        const std::size_t got = want != 0 ? std::fread(out, 1, want, music_) : 0;
        musicLeft_ -= static_cast<std::uint32_t>(got);
        // As loud as the player has the music (the track itself is played
        // at about four fifths of full).
        const int level = settings.volume[kVolumeMusic] * 205; // of 2560
        for (std::size_t i = 0; i < got; ++i)
            out[i] = static_cast<std::int8_t>((out[i] * level) / 2560);
        // Past the end of the track: silence.
        if (got < count)
            std::memset(out + got, 0, count - got);
        musicFed_ += count;
        musicPast_ += count - static_cast<std::uint32_t>(got);
    }

    void Sound::stopMusic()
    {
        if (music_ == nullptr)
            return;
        mmStreamClose();
        std::fclose(music_);
        music_ = nullptr;
        musicTrack_ = -1;
    }

    void Sound::musicTick()
    {
        if (music_ == nullptr)
            return;
        mmStreamUpdate();
        // The track is over once all of it, and what the stream had in
        // hand, has been played.
        if (musicLeft_ == 0 && musicPast_ > kStreamSamples)
            stopMusic();
    }

    void Sound::frame()
    {
        const std::uint32_t now = Global::vblanks();
        const int ms = static_cast<int>(((now - lastVblank_) * 1000 + 30) / 60);
        lastVblank_ = now;
        musicTick();
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
        // What has run its time is over: its voice is maxmod's again.
        const std::uint32_t ended = mixer_.tick(ms);
        for (int v = 0; v < Mixer::kVoices; ++v)
        {
            if (!((ended >> v) & 1))
                continue;
            if (voices_[v].handle != 0)
                NEA_SfxStop(voices_[v].handle);
            voices_[v] = Voice();
        }
        if (clips_ == nullptr)
            return;
        SoundQueue &queue = game.heard;
        if (paused_)
        {
            queue.count = 0;
            return;
        }
        if (dspAudio.up())
        {
            roomDue_ -= ms;
            if (roomDue_ <= 0)
            {
                roomDue_ = 500;
                roomTick();
            }
        }
        for (int i = 0; i < queue.count; ++i)
        {
            const SoundEvent &event = queue.events[i];
            if (event.flags & kSoundSentence)
                say(event);
            else
                playOnce(event);
        }
        queue.count = 0;
        speechTick(ms);
        // The loops, and whatever has a place, are looked at a few times a
        // second: each change is a message to the other processor.
        loopsDue_ -= ms;
        if (loopsDue_ <= 0)
        {
            loopsDue_ = 100;
            loopsTick();
            follow();
        }
        if (game.logic.musicRequest >= 0)
        {
            startMusic(game.logic.musicRequest);
            game.logic.musicRequest = -1;
        }
    }
} // namespace hlrt
