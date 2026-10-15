#include "data/TextureStream.hpp"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include <NEAMain.h>

#include "Global.hpp"
#include "HlTexture.hpp"
#include "HlTextureStream.hpp"
#include "data/LevelFile.hpp"
#include "data/ModelCache.hpp"
#include "data/TextureCache.hpp"

namespace hlrt
{
    TextureStream textureStream;
    std::uint32_t *g_textureSeenAt = nullptr;
    std::uint32_t g_textureFrame = 0;

    namespace
    {
        using namespace hldata;

        /// What is read at the end of a frame waits here for the vertical
        /// blank. (The biggest streamed texture is 128 x 128, about 7 KB.)
        constexpr std::uint32_t kStageBytes = 32 * 1024;
        constexpr int kMaxStaged = 16;
        constexpr int kMaxEvicted = 16;
        /// A texture drawn this recently is wanted above everything; one
        /// drawn within kRememberFrames is remembered a little.
        constexpr std::uint32_t kSeenFrames = 6;
        constexpr std::uint32_t kRememberFrames = 120;
        constexpr std::uint32_t kSeenScore = 1u << 20;
        /// What a map keeps back from NEA's idea of its free memory: the
        /// memory is not all one piece, and 4x4 textures cannot go
        /// everywhere in it.
        constexpr int kMemoryPercent = 85;
        constexpr std::uint32_t kMemoryMargin = 8 * 1024;
        /// The textures every view wants, put there for good: at most this
        /// share of the memory (in hundredths), of those seen from at least
        /// this share of the map's leaves.
        constexpr int kPinnedPercent = 35;
        constexpr int kPinnedShare = 60;
        /// What a cycle that is drawn is kept ahead by: the pictures of the
        /// next two changes of picture (they come ten times a second).
        constexpr std::uint32_t kPictureTicks = 6;
        /// What a picture of a cycle that is not the one shown is wanted
        /// with (if there is room): above what is only remembered.
        constexpr std::uint32_t kSiblingScore = 1500;
        /// Of the heap's headroom, as much as a third may be the copies of
        /// textures kept in main memory (the cycles' pictures and the tiled
        /// floors' variants), and this much at the very most.
        constexpr std::size_t kCacheMost = 1024 * 1024;

        TextureStreamer planner;
        StreamInfo infos[kStreamMaxTextures];
        std::uint32_t seenAt[kStreamMaxTextures];
        alignas(32) std::uint8_t stage[kStageBytes];

        struct Staged
        {
            int id;
            TextureHeader header;
            std::uint32_t at;          ///< where its body starts in `stage`
            const std::uint8_t *body; ///< or where it is in main memory (then `at` is not used)
        };
        Staged staged[kMaxStaged];
        int stagedCount = 0;
        std::uint32_t stageUsed = 0;

        /// For a picture of a cycle (the game's '+' textures, and the other
        /// face's): the cycle's first picture and how many pictures it has.
        /// 1 for anything else.
        std::int16_t groupFirst[kStreamMaxTextures];
        std::uint8_t groupCount[kStreamMaxTextures];
        /// A copy of the texture file's header and body in main memory, for
        /// the ones that are swapped most (cycles' pictures, tiled floors'
        /// variants): nothing is read off the card to bring them back.
        std::uint8_t *ram[kStreamMaxTextures];
        int ramCount = 0;
        std::size_t ramBytes = 0;

        int streamedTotal = 0;
        int pinnedTotal = 0;
        int refusals = 0;
        int flatNow = 0;
        std::uint32_t frameNo = 0;

        /// What a streamed texture costs NEA, and what the plan counts. The
        /// palette is a guess from above (the cooker's own rule for how many
        /// colours a texture of that many texels gets: the file has the true
        /// number, and is never bigger).
        StreamInfo infoFor(const TextureRef &ref)
        {
            StreamInfo info;
            const std::uint32_t texels = static_cast<std::uint32_t>(ref.width) * ref.height;
            const std::uint32_t colours = std::min<std::uint32_t>(512, std::max<std::uint32_t>(16, texels / 16)) & ~1u;
            info.texelBytes = texels / 4 + texels / 8; // 4x4: the texels and the index words
            info.paletteBytes = colours * 2;
            info.stageBytes = info.texelBytes + info.paletteBytes;
            info.streamed = true;
            return info;
        }

        bool fileFor(int id, TextureHeader &header, std::uint8_t *body, std::size_t room)
        {
            const TextureRef &ref = level.view.textures[id];
            char name[40];
            hldata::textureFileName(ref.name, ref.width, ref.height, false, name);
            const std::string path = global.texDir() + name;
            return TextureCache::readFile(path.c_str(), ref.width, ref.height, 0, header, body, room);
        }
    } // namespace

    bool TextureStream::loadNow(int id)
    {
        TextureHeader header;
        const std::uint8_t *body = stage;
        if (ram[id] != nullptr)
        {
            std::memcpy(&header, ram[id], sizeof(header));
            body = ram[id] + sizeof(header);
        }
        else if (!fileFor(id, header, stage, kStageBytes))
        {
            planner.failed(id);
            return false;
        }
        NEA_Material *material = nullptr;
        NEA_Palette *palette = nullptr;
        if (!TextureCache::create(header, body, false, material, palette))
        {
            planner.failed(id);
            planner.shrinkCapacity();
            ++refusals;
            return false;
        }
        textureCache.attachStreamed(static_cast<unsigned>(id), material, palette);
        planner.loaded(id);
        return true;
    }

    void TextureStream::begin()
    {
        end();
        const int count = std::min<int>(level.view.textureCount, kStreamMaxTextures);
        streamedTotal = 0;
        for (int i = 0; i < count; ++i)
        {
            infos[i] = StreamInfo();
            seenAt[i] = 0xFFFFFFFFu; // never
            if (textureCache.streamed(static_cast<unsigned>(i)))
            {
                infos[i] = infoFor(level.view.textures[i]);
                ++streamedTotal;
            }
        }
        if (streamedTotal == 0)
            return;

        // The cycles: each picture knows its cycle's first and how many there
        // are. (The first picture of a texture says how many it has; the
        // other face's cycle where it starts and how many.)
        for (int i = 0; i < count; ++i)
        {
            groupFirst[i] = static_cast<std::int16_t>(i);
            groupCount[i] = 1;
            ram[i] = nullptr;
        }
        auto markCycle = [&](int first, int pictures)
        {
            if (pictures <= 1 || first < 0 || first + pictures > count)
                return;
            for (int k = 0; k < pictures; ++k)
            {
                groupFirst[first + k] = static_cast<std::int16_t>(first);
                groupCount[first + k] = static_cast<std::uint8_t>(pictures);
            }
        };
        for (int i = 0; i < count; ++i)
        {
            const TextureRef &ref = level.view.textures[i];
            if (ref.frames > 1)
                markCycle(i, ref.frames);
            if (ref.other != kNoTexture && ref.otherFrames > 1)
                markCycle(ref.other, ref.otherFrames);
        }
        // Main-memory copies of the pictures of the cycles and of the variants
        // a floor is tiled with (the game's '-' textures), as far as the heap
        // can spare: a cycle changes picture ten times a second, and a tiled
        // floor wears several, and when video memory is short the ones not
        // wanted at the moment are let go and come back from here, not from the
        // card.
        {
            ramCount = 0;
            ramBytes = 0;
            const std::size_t room = std::min<std::size_t>(global.heapHeadroom() / 3, kCacheMost);
            for (int i = 0; i < count; ++i)
            {
                const TextureRef &ref = level.view.textures[i];
                const bool tiled = ref.name[0] == '-' && ref.name[1] >= '0' && ref.name[1] <= '9';
                if (!infos[i].streamed || (groupCount[i] <= 1 && !tiled))
                    continue;
                TextureHeader header;
                if (!fileFor(i, header, stage, kStageBytes))
                    continue;
                const std::size_t body = header.texelBytes + header.indexBytes + header.paletteBytes;
                if (ramBytes + sizeof(header) + body > room)
                    continue;
                auto *copy = static_cast<std::uint8_t *>(std::malloc(sizeof(header) + body));
                if (copy == nullptr)
                    continue;
                std::memcpy(copy, &header, sizeof(header));
                std::memcpy(copy + sizeof(header), stage, body);
                ram[i] = copy;
                infos[i].stageBytes = 0; // (nothing to read: it is there)
                ramBytes += sizeof(header) + body;
                ++ramCount;
            }
        }

        // What there is room for: what NEA has left, less what it will want
        // back for what is not the planner's (the sky of a map that comes
        // later, a weapon's skin).
        const int texelFree = NEA_TextureFreeMem() - modelCache.reservedBytes();
        const int paletteFree = NEA_PaletteFreeMem();
        const std::uint32_t texelRoom =
            texelFree > static_cast<int>(kMemoryMargin) ? (texelFree - kMemoryMargin) / 100 * kMemoryPercent : 0;
        const std::uint32_t paletteRoom = paletteFree > 512 ? static_cast<std::uint32_t>(paletteFree - 512) : 0;
        planner.init(infos, count, texelRoom, paletteRoom);
        pinnedTotal = 0;
        refusals = 0;
        flatNow = 0;
        frameNo = 1;
        g_textureFrame = frameNo;

        if (planner.allFit())
        {
            // Nothing to stream: every texture is there for good.
            for (int i = 0; i < count; ++i)
            {
                std::uint32_t t, p;
                if (infos[i].streamed && planner.pin(i, t, p))
                {
                    ++pinnedTotal;
                    loadNow(i);
                }
            }
        }
        else
        {
            // What most views want is there for good: widest first.
            std::uint32_t pinRoom = texelRoom / 100 * kPinnedPercent;
            for (int share = 100; share >= kPinnedShare; --share)
            {
                for (int i = 0; i < count; ++i)
                {
                    if (!infos[i].streamed || groupCount[i] > 1 || level.view.textures[i].share != share ||
                        infos[i].texelBytes > pinRoom)
                        continue;
                    std::uint32_t t, p;
                    if (!planner.pin(i, t, p))
                        continue;
                    pinRoom -= infos[i].texelBytes;
                    ++pinnedTotal;
                    loadNow(i);
                }
            }
            // The rest by how widely they are wanted, as far as there is room.
            planner.clearScores();
            for (int i = 0; i < count; ++i)
            {
                if (!infos[i].streamed || planner.pinned(i))
                    continue;
                planner.addScore(i, 1u + level.view.textures[i].share);
            }
            planner.rebuild();
            for (int round = 0; round < 64; ++round)
            {
                StreamLoad loads[kMaxStaged];
                int evicted[kMaxEvicted], evictedCount = 0;
                const int made = planner.plan(0xFFFFFFFFu, kStageBytes, loads, kMaxStaged, evicted, kMaxEvicted, evictedCount);
                if (made == 0)
                    break;
                for (int k = 0; k < made; ++k)
                    loadNow(loads[k].id);
                planner.rebuild(); // (what did not fit is let alone: Failed)
            }
        }
        // Those that did not fit are tried again once something is seen.
        planner.retryFailed();
        stagedCount = 0;
        stageUsed = 0;
        g_textureSeenAt = seenAt;
        active_ = true;
    }

    void TextureStream::end()
    {
        for (int i = 0; i < kStreamMaxTextures; ++i)
        {
            std::free(ram[i]);
            ram[i] = nullptr;
        }
        ramCount = 0;
        ramBytes = 0;
        g_textureSeenAt = nullptr;
        active_ = false;
        stagedCount = 0;
        stageUsed = 0;
        streamedTotal = 0;
        pinnedTotal = 0;
        // (The materials go with TextureCache.)
    }

    void TextureStream::demand()
    {
        planner.clearScores();
        flatNow = 0;
        for (int id = 0; id < planner.count(); ++id)
        {
            if (!infos[id].streamed || planner.pinned(id))
                continue;
            const std::uint32_t at = seenAt[id];
            if (at == 0xFFFFFFFFu)
                continue;
            const std::uint32_t age = frameNo - at;
            std::uint32_t score = 0;
            if (age <= kSeenFrames)
                score = kSeenScore;
            else if (age <= kRememberFrames)
                score = 1000 - age;
            if (score != 0)
                planner.addScore(id, score);
            if (age <= kSeenFrames && !planner.resident(id))
                ++flatNow;
        }
        // A cycle that is being drawn: the pictures it will show in the next
        // changes are wanted as much as the one shown; its other pictures a
        // little, so that they stay if there is room and go first if not.
        const std::uint32_t blanks = static_cast<std::uint32_t>(Global::vblanks());
        for (int first = 0; first < planner.count(); ++first)
        {
            const int pictures = groupCount[first];
            if (pictures <= 1 || groupFirst[first] != first)
                continue;
            bool drawn = false;
            for (int k = 0; k < pictures && !drawn; ++k)
            {
                const std::uint32_t at = seenAt[first + k];
                drawn = at != 0xFFFFFFFFu && frameNo - at <= kSeenFrames;
            }
            if (!drawn)
                continue;
            for (int k = 0; k < pictures; ++k)
            {
                const int id = first + k;
                if (infos[id].streamed && !planner.pinned(id))
                    planner.addScore(id, kSiblingScore);
            }
            // (The picture of the next frame, and of the two changes after it: a
            // picture comes in at the vertical blank before the frame that
            // shows it.)
            static const std::uint32_t kAhead[3] = {1, 4, 7};
            for (const std::uint32_t ahead : kAhead)
            {
                const int id = first + static_cast<int>(((blanks + ahead) / kPictureTicks) % static_cast<std::uint32_t>(pictures));
                if (infos[id].streamed && !planner.pinned(id))
                    planner.addScore(id, kSeenScore);
            }
        }
    }

    void TextureStream::upload()
    {
        if (!active_ || stagedCount == 0)
            return;
        for (int k = 0; k < stagedCount; ++k)
        {
            const Staged &s = staged[k];
            NEA_Material *material = nullptr;
            NEA_Palette *palette = nullptr;
            if (TextureCache::create(s.header, s.body != nullptr ? s.body : stage + s.at, false, material, palette))
            {
                textureCache.attachStreamed(static_cast<unsigned>(s.id), material, palette);
                planner.loaded(s.id);
            }
            else
            {
                // NEA has no room where the plan thought there was.
                planner.failed(s.id);
                planner.shrinkCapacity();
                ++refusals;
            }
        }
        stagedCount = 0;
        stageUsed = 0;
    }

    void TextureStream::update(int budgetBytes)
    {
        if (!active_)
            return;
        ++frameNo;
        g_textureFrame = frameNo;
        if (stagedCount != 0)
            return;
        demand();
        if (frameNo % 8 == 0)
            planner.retryFailed();
        if (frameNo % 4 == 0 || frameNo < 16)
            planner.rebuild();

        StreamLoad loads[kMaxStaged];
        int evicted[kMaxEvicted], evictedCount = 0;
        const int made = planner.plan(static_cast<std::uint32_t>(budgetBytes), kStageBytes, loads, kMaxStaged, evicted,
                                      kMaxEvicted, evictedCount);
        // Those that make way are drawn flat from now on; their memory is
        // given back at once (nothing is written until the blank).
        for (int k = 0; k < evictedCount; ++k)
            textureCache.detachStreamed(static_cast<unsigned>(evicted[k]));
        for (int k = 0; k < made; ++k)
        {
            Staged &s = staged[stagedCount];
            s.id = loads[k].id;
            s.at = stageUsed;
            s.body = nullptr;
            if (ram[s.id] != nullptr)
            {
                // In main memory already: nothing to read.
                std::memcpy(&s.header, ram[s.id], sizeof(s.header));
                s.body = ram[s.id] + sizeof(s.header);
                ++stagedCount;
                continue;
            }
            if (!fileFor(s.id, s.header, stage + stageUsed, kStageBytes - stageUsed))
            {
                planner.failed(s.id);
                continue;
            }
            const std::uint32_t body = s.header.texelBytes + s.header.indexBytes + s.header.paletteBytes;
            stageUsed += (body + 31u) & ~31u;
            ++stagedCount;
        }
    }

    StreamStats TextureStream::stats() const
    {
        StreamStats s;
        s.streamed = streamedTotal;
        s.resident = planner.residentCount();
        s.pinned = pinnedTotal;
        s.loads = planner.loads();
        s.evictions = planner.evictions();
        s.refused = refusals;
        s.flatNow = flatNow;
        s.copies = ramCount;
        s.copiesKb = static_cast<int>(ramBytes / 1024);
        s.texelKb = static_cast<int>(planner.texelBytesUsed() / 1024);
        s.capacityKb = static_cast<int>(planner.texelCapacity() / 1024);
        return s;
    }
} // namespace hlrt
