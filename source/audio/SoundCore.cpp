// ARM7 core handles all of the sound; reading audio off SD card itself, mixing them,
// placing relative to the listener, speaking the sentences word by word, and music streaming.
// Effectively Sound.arm.cpp's job with maxmod removed and more features added.
// Shared code defines what was heard and where, sending each as a command.
//
// Each sound of the level is named by the key its file name was cooked to (SoundCook.cpp's soundCardName).
// `audio.keys`, written by the audio bake with `audio.nds`, turns that into the its number in the ARM7's bank,
// once for every sound of a map when it is loaded.

#ifdef HL_ARM7_PREBUILT

#include "audio/Sound.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <malloc.h>

#include <nds.h>
#include <nds/arm9/dldi.h>
#include <nds/disc_io.h>

#include "Global.hpp"
#include "Settings.hpp"
#include "audio/A7Mix.h"
#include "audio/CardShare.hpp"
#include "data/LevelFile.hpp"
#include "game/Game.hpp"
#include "render/Camera.hpp"

namespace hlrt
{
    Sound sound;

    namespace
    {
        using namespace hldata;

        constexpr std::uint16_t kNoId = 0xFFFF;
        constexpr int kDsArena = 512 * 1024;
        constexpr int kArenaLeast = 512 * 1024;
        constexpr int kLoopSlots = 24;      //keeps 32 origins
        constexpr int kSpeakers = 4;        //whoever is speaking 
        constexpr int kSpeakerEnt = 100;
        constexpr int kOneShotEnt = 256;    //entities from here round; one a sound, so no sound cuts another off
        constexpr int kChanVoice = 2;
        constexpr int kTrackFirst = 2;      //game's first track of music is number 2

        struct KeyRow
        {
            std::uint32_t hash;
            std::uint16_t id, pad;
        };

        struct Loop
        {
            std::uint16_t tag = 0;
            bool used = false;
        };

        struct Speaker
        {
            fx pos[3] = {0, 0, 0};
            bool placed = false;
            std::uint32_t born = 0;
        };

        struct Core
        {
            bool started = false;
            volatile bool ready = false;
            KeyRow *keys = nullptr;
            std::uint32_t keyCount = 0;
            std::uint16_t *ids = nullptr;   //the level's sounds -> the core's bank
            std::uint32_t idCount = 0;
            Loop loops[kLoopSlots];
            Speaker speakers[kSpeakers];
            std::uint32_t speakerClock = 0;
            int entCounter = 0;
            // the latest status (FIFO interrupt)
            volatile std::uint32_t posMs = 0;
            volatile bool musicOn = false;
            volatile int voices = 0;
            volatile bool logged = false;
            char log[A7M_NAME] = {};
            int track = -1;
            std::uint32_t startMs = 0;
            std::uint32_t heldMs = 0;
            bool held = false;
            bool wasOn = false;
            std::uint32_t asked = 0;        //last vblank asked at; status follows is of new track
            int effects = 10, voicesVol = 10, ambience = 8;
        } core;

        void onMessage(int bytes, void *)
        {
            std::uint8_t buffer[128] __attribute__((aligned(4)));
            if (bytes > static_cast<int>(sizeof(buffer)))
                bytes = sizeof(buffer);
            fifoGetDatamsg(A7M_FIFO, bytes, buffer);
            if (buffer[0] == A7M_LOG)
            {
                const A7MMsg *line = reinterpret_cast<const A7MMsg *>(buffer);
                std::memcpy(core.log, line->name, sizeof(core.log) - 1);
                core.logged = true;
                return;
            }
            if (buffer[0] != A7M_STATUS)
                return;
            const A7MStatus *status = reinterpret_cast<const A7MStatus *>(buffer);
            core.ready = true;
            core.musicOn = status->musicOn != 0;
            core.posMs = status->musicPosMs;
            core.voices = status->voices;
        }

        void send(const A7MMsg &message)
        {
            if (!core.ready)
                return; // not booted; what it would be told would wait in a queue that is not read until it is
            for (int tries = 0; tries < 2000 && !fifoSendDatamsg(A7M_FIFO, sizeof(message), const_cast<std::uint8_t *>(
                                                                    reinterpret_cast<const std::uint8_t *>(&message)));
                 ++tries)
            {
            }
        }

        A7MMsg blank(int op)
        {
            A7MMsg message;
            std::memset(&message, 0, sizeof(message));
            message.op = static_cast<std::uint8_t>(op);
            return message;
        }

        /// A distance in the core's unit, a sixteenth of a unit.
        std::int32_t sixteenths(fx value)
        {
            return value >> (kFxShift - 4);
        }

        /// ... and the camera's yaw (65536 to the turn) in sixteenths of a degree.
        std::int32_t yawSixteenths(std::uint16_t turns)
        {
            return static_cast<std::int32_t>((static_cast<std::uint32_t>(turns) * 45u) >> 9);
        }

        /// 0..127 and a layer's volume of 0..10, as the core's 0..1000.
        int loudness(int volume, int layerVolume)
        {
            return (volume * layerVolume * 1000) / (127 * 10);
        }

        /// How fast a sound dies away with distance (GoldSrc's attenuation x 1000): a sound carrying
        /// `radius` units is the game's attenuation 1000 / radius. 0: heard as loud everywhere.
        int attenuation(int radius, bool here)
        {
            if (here || radius <= 0)
                return 0;
            const int value = 1000000 / radius;
            return value < 1 ? 1 : value;
        }

        int layerVolume(SoundLayer layer)
        {
            switch (layer)
            {
            case SoundLayer::Voice:
                return core.voicesVol;
            case SoundLayer::Ambience:
                return core.ambience;
            default:
                return core.effects;
            }
        }

        int nextEntity()
        {
            core.entCounter = (core.entCounter + 1) & 0x3FFF;
            return kOneShotEnt + core.entCounter;
        }

        /// Where the listener is (an ear is the camera): the "here" of a sound that has no place.
        void listenerQ(std::int32_t out[3])
        {
            out[0] = sixteenths(camera.pos[0]);
            out[1] = sixteenths(camera.pos[1]);
            out[2] = sixteenths(camera.pos[2]);
        }

        /// The key a cooked sound name stands for: the name without its ".11k" / ".22l" tail.
        std::uint32_t keyOf(const char *name)
        {
            char key[sizeof(SoundRef::name)];
            std::size_t length = 0;
            for (; name[length] != '\0' && length + 1 < sizeof(key); ++length)
                key[length] = name[length];
            key[length] = '\0';
            for (std::size_t i = length; i > 0; --i)
            {
                if (key[i - 1] == '.')
                {
                    key[i - 1] = '\0';
                    break;
                }
            }
            return nameHash(key);
        }

        std::uint16_t idOfKey(std::uint32_t hash)
        {
            std::uint32_t low = 0, high = core.keyCount;
            while (low < high)
            {
                const std::uint32_t mid = (low + high) / 2;
                if (core.keys[mid].hash < hash)
                    low = mid + 1;
                else
                    high = mid;
            }
            return low < core.keyCount && core.keys[low].hash == hash ? core.keys[low].id : kNoId;
        }

        std::uint16_t idOf(int sound)
        {
            return core.ids != nullptr && sound >= 0 && static_cast<std::uint32_t>(sound) < core.idCount
                       ? core.ids[sound]
                       : kNoId;
        }

        void musicName(int track, char *out, std::size_t size)
        {
            std::snprintf(out, size, "#%d", track - kTrackFirst);
        }

        void musicStart(std::uint32_t fromMs)
        {
            A7MMsg message = blank(A7M_MUSIC_PLAY);
            musicName(core.track, message.name, sizeof(message.name));
            message.flags = 0; // (a track plays through once)
            message.a[0] = static_cast<std::int32_t>(fromMs);
            send(message);
            core.startMs = fromMs;
            core.posMs = 0;
            core.wasOn = false;
            core.asked = Global::vblanks();
            A7MMsg volume = blank(A7M_MUSIC_VOL);
            volume.flags = (settings.volume[kVolumeMusic] * 127 * 4) / (5 * 10);
            send(volume);
        }

        void musicStop()
        {
            send(blank(A7M_MUSIC_STOP));
        }
    } // namespace

    namespace
    {
        /// One line of perf.txt about how the core is getting on, for a start that is silent.
        void diagnose(const char *what)
        {
            if (!global.hasFat)
                return;
            if (std::FILE *log = std::fopen(global.dataPath("perf.txt").c_str(), "a"))
            {
                const DISC_INTERFACE *io = dldiGetInternal();
                std::fprintf(log, "sound %s: cardshare=%d dldiMode=%d features=%lx keys=%lu headroom=%lu started=%d ready=%d\n", what,
                             global.sdAudio, static_cast<int>(dldiGetMode()),
                             io != nullptr ? static_cast<unsigned long>(io->features) : 0ul,
                             static_cast<unsigned long>(core.keyCount), static_cast<unsigned long>(global.heapHeadroom()),
                             core.started ? 1 : 0, core.ready ? 1 : 0);
                std::fclose(log);
            }
        }
    } // namespace

    void Sound::init()
    {
        core.effects = settings.volume[kVolumeEffects];
        core.voicesVol = settings.volume[kVolumeVoices];
        core.ambience = settings.volume[kVolumeAmbience];
        fifoSetDatamsgHandler(A7M_FIFO, onMessage, nullptr);
        diagnose("init");

        // audio.keys: what turns a level's sound names into the core's numbers.
        if (global.hasFat)
        {
            if (std::FILE *file = std::fopen(global.dataPath("audio.keys").c_str(), "rb"))
            {
                char head[8];
                if (std::fread(head, 1, 8, file) == 8 && std::memcmp(head, "AKY1", 4) == 0)
                {
                    std::uint32_t count;
                    std::memcpy(&count, head + 4, 4);
                    if (count > 0 && count < 20000)
                    {
                        core.keys = static_cast<KeyRow *>(std::malloc(count * sizeof(KeyRow)));
                        if (core.keys != nullptr && std::fread(core.keys, sizeof(KeyRow), count, file) == count)
                            core.keyCount = count;
                        else
                        {
                            std::free(core.keys);
                            core.keys = nullptr;
                        }
                    }
                }
                std::fclose(file);
            }
        }

        // The arena the core works in (its sample cache is what is left of it), as much as a console
        // with this much memory can spare. Handed over for good; the ARM9 never touches it again.
        const int headroom = static_cast<int>(global.heapHeadroom());
        int bytes = global.isDSi ? A7M_ARENA_BYTES : kDsArena;
        if (bytes > headroom - 512 * 1024)
            bytes = (headroom - 512 * 1024) & ~0xFFFF;
        if (bytes < kArenaLeast || core.keyCount == 0)
            return; // no room, or no keys (no audio.keys on the card): no sound
        std::uint8_t *arena = static_cast<std::uint8_t *>(memalign(32, static_cast<std::size_t>(bytes)));
        if (arena == nullptr)
            return;
        DC_FlushRange(arena, static_cast<std::size_t>(bytes)); // (written back and let go: the ARM7's data is never overwritten by a stale line)
        A7MMsg size = blank(A7M_ARENA_SIZE);
        size.a[0] = bytes;
        for (int tries = 0; tries < 2000 && !fifoSendDatamsg(A7M_FIFO, sizeof(size), reinterpret_cast<std::uint8_t *>(&size));
             ++tries)
        {
        }
        fifoSendAddress(A7M_FIFO, arena);
        core.started = true;
        up_ = true;
        diagnose("arena sent");
    }

    void Sound::applyVolumes()
    {
        core.effects = settings.volume[kVolumeEffects];
        core.voicesVol = settings.volume[kVolumeVoices];
        core.ambience = settings.volume[kVolumeAmbience];
        if (core.track >= 0 && !core.held)
        {
            A7MMsg volume = blank(A7M_MUSIC_VOL);
            volume.flags = (settings.volume[kVolumeMusic] * 127 * 4) / (5 * 10);
            send(volume);
        }
    }

    void Sound::load()
    {
        const LevelView &view = level.view;
        std::free(core.ids);
        core.ids = nullptr;
        core.idCount = 0;
        loaded_ = 0;
        bytes_ = 0;
        count_ = static_cast<int>(view.soundCount);
        if (view.soundCount != 0)
        {
            core.ids = static_cast<std::uint16_t *>(std::malloc(view.soundCount * sizeof(std::uint16_t)));
            if (core.ids != nullptr)
            {
                core.idCount = view.soundCount;
                for (std::uint32_t i = 0; i < view.soundCount; ++i)
                {
                    core.ids[i] = view.sounds[i].name[0] != '\0' ? idOfKey(keyOf(view.sounds[i].name)) : kNoId;
                    loaded_ += core.ids[i] != kNoId ? 1 : 0;
                }
            }
        }
        // The music is taken up where it was put down.
        if (core.held && !paused_)
        {
            core.held = false;
            musicStart(core.heldMs);
        }
    }

    void Sound::unload()
    {
        send(blank(A7M_STOPALL));
        for (Loop &loop : core.loops)
            loop = Loop();
        for (Speaker &speaker : core.speakers)
            speaker = Speaker();
        // The music is put down where it is, to be taken up again: the card is busy with the map.
        if (core.track >= 0 && !core.held)
        {
            core.heldMs = musicMs();
            core.held = true;
            musicStop();
        }
        std::free(core.ids);
        core.ids = nullptr;
        core.idCount = 0;
        count_ = 0;
        loaded_ = 0;
        bytes_ = 0;
    }

    const void *Sound::sample(std::uint32_t) const
    {
        return nullptr; // (no maxmod)
    }

    void Sound::fillMusic(std::int8_t *out, std::uint32_t count)
    {
        std::memset(out, 0, count);
    }

    bool Sound::setDsp(bool)
    {
        return false; // (the DSi's DSP is not the core's)
    }

    void Sound::ui(int which)
    {
        if (which < 0 || which >= kUiSoundCount)
            return;
        A7MMsg message = blank(A7M_EMIT);
        std::snprintf(message.name, sizeof(message.name), "%s", kUiSoundSources[which]);
        message.ent = A7M_UI_ENT;
        message.fixed = 1;
        listenerQ(message.a);
        message.vol1000 = loudness(110, core.effects);
        message.attn1000 = 0;
        message.pitch = 100;
        send(message);
    }

    void Sound::setPaused(bool paused)
    {
        if (paused == paused_)
            return;
        paused_ = paused;
        A7MMsg message = blank(A7M_PAUSE);
        message.flags = paused ? 1 : 0;
        send(message);
        if (paused)
        {
            if (core.track >= 0 && !core.held)
            {
                core.heldMs = musicMs();
                core.held = true;
                musicStop();
            }
        }
        else if (core.held && core.idCount != 0)
        {
            core.held = false;
            musicStart(core.heldMs);
        }
    }

    int Sound::musicTrack() const
    {
        return core.track;
    }

    std::uint32_t Sound::musicMs() const
    {
        if (core.held)
            return core.heldMs;
        if (core.track < 0)
            return 0;
        return core.startMs + core.posMs;
    }

    void Sound::setMusic(int track, std::uint32_t ms)
    {
        if (core.track >= 0 && !core.held)
            musicStop();
        core.track = track;
        core.held = false;
        if (track < 0)
            return;
        core.heldMs = ms;
        core.held = true; // put down at that moment, and taken up from it when the game next goes on
        if (!paused_ && level.view.soundCount != 0)
        {
            core.held = false;
            musicStart(ms);
        }
    }

    void Sound::frame()
    {
        if (core.logged)
        {
            core.logged = false;
            if (std::FILE *log = std::fopen(global.dataPath("perf.txt").c_str(), "a"))
            {
                std::fprintf(log, "a7: %s\n", core.log);
                std::fclose(log);
            }
        }
        cardshare::service();
        SoundQueue &queue = game.heard;
        static std::uint32_t lastNote = 0;
        if (core.started && Global::vblanks() - lastNote > 600)
        {
            lastNote = Global::vblanks();
            if (!core.ready || core.ids == nullptr)
                diagnose(core.ready ? "no level ids" : "core not ready");
        }
        if (!core.ready || core.ids == nullptr)
        {
            queue.count = 0;
            return;
        }
        // The track is over once the core says it is no longer playing what was started
        if (core.track >= 0 && !core.held)
        {
            if (core.musicOn)
                core.wasOn = true;
            else if (core.wasOn || Global::vblanks() - core.asked > 180)
            {
                core.track = -1;
                core.wasOn = false;
            }
        }
        // The listener, every frame: where the ear is and which way it faces
        {
            A7MMsg message = blank(A7M_LISTENER);
            message.a[0] = sixteenths(camera.pos[0]);
            message.a[1] = sixteenths(camera.pos[1]);
            message.a[2] = yawSixteenths(camera.yaw);
            send(message);
        }
        if (paused_)
        {
            queue.count = 0;
            return;
        }
        for (int i = 0; i < queue.count; ++i)
        {
            const SoundEvent &event = queue.events[i];
            const bool here = (event.flags & kSoundHere) != 0;
            if (event.flags & kSoundSentence)
            {
                // The words of the line are the core's to speak, one after another without a gap.
                const LevelView &view = level.view;
                if (event.sound >= view.sentenceCount)
                    continue;
                const SentenceRecord &record = view.sentences[event.sound];
                // Whose voice: the speaker already at that place (a line cuts in on that speaker's last), else the oldest.
                int slot = -1;
                for (int s = 0; s < kSpeakers && slot < 0; ++s)
                {
                    const Speaker &speaker = core.speakers[s];
                    if (speaker.born == 0 || speaker.placed != !here)
                        continue;
                    const fx d[3] = {speaker.pos[0] - event.pos[0], speaker.pos[1] - event.pos[1],
                                     speaker.pos[2] - event.pos[2]};
                    if (here || fxLength(d) < fxFromInt(96))
                        slot = s;
                }
                for (int s = 0; s < kSpeakers && slot < 0; ++s)
                    slot = core.speakers[s].born == 0 ? s : -1;
                if (slot < 0)
                {
                    slot = 0;
                    for (int s = 1; s < kSpeakers; ++s)
                        slot = core.speakers[s].born < core.speakers[slot].born ? s : slot;
                }
                Speaker &speaker = core.speakers[slot];
                speaker.placed = !here;
                speaker.born = ++core.speakerClock;
                for (int k = 0; k < 3; ++k)
                    speaker.pos[k] = event.pos[k];

                std::uint8_t words[32 * 5];
                int count = 0;
                for (int w = 0; w < record.wordCount && count < 32; ++w)
                {
                    const SentenceWord &word = view.sentenceWords[record.firstWord + w];
                    const std::uint16_t id = idOf(word.sound);
                    if (id == kNoId)
                        continue;
                    std::uint8_t *at = words + count * 5;
                    at[0] = static_cast<std::uint8_t>(id);
                    at[1] = static_cast<std::uint8_t>(id >> 8);
                    at[2] = word.pitch;
                    at[3] = word.volume;
                    at[4] = word.end;
                    ++count;
                }
                for (int first = 0; first < count; first += A7M_WORDS_PER_MSG)
                {
                    const int n = count - first < A7M_WORDS_PER_MSG ? count - first : A7M_WORDS_PER_MSG;
                    A7MMsg message = blank(A7M_WORDS);
                    message.ent = static_cast<std::int16_t>(kSpeakerEnt + slot);
                    message.chan = kChanVoice;
                    message.vol1000 = loudness((record.volume * event.volume) / 127, layerVolume(SoundLayer::Voice));
                    message.attn1000 = attenuation(event.radius, here);
                    message.pitch = event.pitch;
                    message.flags = count | (first << 8) | (n << 16);
                    std::memcpy(message.name, words + first * 5, static_cast<std::size_t>(n) * 5);
                    send(message);
                }
                continue;
            }
            if (event.flags & kSoundLoopOff)
            {
                for (int s = 0; s < kLoopSlots; ++s)
                {
                    if (!core.loops[s].used || core.loops[s].tag != event.tag)
                        continue;
                    A7MMsg message = blank(A7M_STOPENT);
                    message.ent = static_cast<std::int16_t>(1 + s);
                    message.chan = 255;
                    message.ambient = 1;
                    send(message);
                    core.loops[s] = Loop();
                }
                continue;
            }
            const std::uint16_t id = idOf(event.sound);
            if (id == kNoId)
                continue;
            if (event.flags & kSoundLoopOn)
            {
                // Round and round from where it is, until it is told to stop; heard again, it is moved to where it is now.
                int slot = -1, free = -1;
                for (int s = 0; s < kLoopSlots; ++s)
                {
                    if (core.loops[s].used && core.loops[s].tag == event.tag)
                        slot = s;
                    else if (!core.loops[s].used && free < 0)
                        free = s;
                }
                if (slot >= 0 || free >= 0)
                {
                    const bool again = slot >= 0;
                    if (!again)
                    {
                        slot = free;
                        core.loops[slot].used = true;
                        core.loops[slot].tag = event.tag;
                    }
                    A7MMsg where = blank(A7M_ENTPOS);
                    where.ent = static_cast<std::int16_t>(1 + slot);
                    for (int k = 0; k < 3; ++k)
                        where.a[k] = here ? 0 : sixteenths(event.pos[k]);
                    if (here)
                        listenerQ(where.a);
                    send(where);
                    if (again)
                        continue;
                    A7MMsg message = blank(A7M_EMIT_ID);
                    message.name[0] = static_cast<char>(id & 0xFF);
                    message.name[1] = static_cast<char>(id >> 8);
                    message.ent = static_cast<std::int16_t>(1 + slot);
                    message.ambient = 1;
                    message.vol1000 = loudness(event.volume, layerVolume(event.layer));
                    message.attn1000 = attenuation(event.radius, here);
                    message.pitch = event.pitch;
                    send(message);
                    continue;
                }
                // no slots free; played once, as a sound that does not go round is
            }
            A7MMsg message = blank(A7M_EMIT_ID);
            message.name[0] = static_cast<char>(id & 0xFF);
            message.name[1] = static_cast<char>(id >> 8);
            message.ent = static_cast<std::int16_t>(nextEntity());
            message.fixed = 1;
            if (here)
                listenerQ(message.a);
            else
                for (int k = 0; k < 3; ++k)
                    message.a[k] = sixteenths(event.pos[k]);
            message.vol1000 = loudness(event.volume, layerVolume(event.layer));
            message.attn1000 = attenuation(event.radius, here);
            message.pitch = event.pitch;
            send(message);
        }
        queue.count = 0;
        if (game.logic.musicRequest >= 0)
        {
            const int track = game.logic.musicRequest;
            game.logic.musicRequest = -1;
            if (track != core.track || core.held)
            {
                core.track = track;
                core.held = false;
                musicStart(0);
            }
        }
    }
} // namespace hlrt

#endif // HL_ARM7_PREBUILT
