#pragma once

// A cooked model: mdl/<name>.hmd, made by the converter from one of the game's
// studio models, and its skin, mdl/<name>.htx.
//
// WHAT IS IN IT
//
//   Bones      the skeleton: each bone's parent.
//   Batches    the skin, as ready-made geometry-engine command streams (the
//              same idea as a level's surfaces). A batch is one sub-model of
//              one body part -- a head, a holster -- cut so that it uses at
//              most kModelBatchBones bones, and it lists them: the DS puts
//              each bone's matrix in a slot of the hardware's matrix stack,
//              and the stream says "use slot N" before the vertices that
//              belong to that bone. Every vertex of these models belongs to
//              exactly one bone, which is what makes this possible.
//   Clips      animations: for each bone a track of keys (a rotation and a
//              place relative to its parent), at a step chosen per bone by
//              how fast it moves. A map's models carry only the clips that
//              map uses.
//   Events     moments of a clip that fire a target by name.
//
// UNITS. A vertex is in its bone's space, 16-bit, 1/kModelVertexScale of a
// unit (or coarser for a model too big for that: `vertexShift`). The DS
// builds, for each bone, the matrix that takes such a vertex straight to view
// space, on the CPU and in 64-bit, so nothing is lost on the way.
//
// Like a level, the file is checked in full when it is opened
// (ModelView::open) so that drawing and animating need no bounds checks.

#include <string>
#include <cstdint>
#include <cstring>

#include "HlLevel.hpp"

namespace hldata
{
    inline constexpr char kModelMagic[4] = {'H', 'L', 'M', 'D'};
    inline constexpr std::uint16_t kModelVersion = 1;

    /// Bones a batch may use: the matrix stack has 31 slots, and the world's
    /// own drawing needs the bottom one.
    inline constexpr int kModelBatchBones = 30;
    inline constexpr int kModelMaxBones = 96;

    /// The file a model's skin is in, under mdl/: by the model's name and the
    /// skin's size, since a model may be cooked with a smaller skin for a map
    /// that is short of video memory.
    inline std::string modelSkinFile(const char *name, int width, int height)
    {
        return std::string(name) + "." + std::to_string(width) + "x" + std::to_string(height) + ".htx";
    }
    /// Vertex units to the Half-Life unit, before `vertexShift`.
    inline constexpr int kModelVertexShift = 7; // 128
    /// Key rotations are quaternions in Q14; key places are units in Q6.
    inline constexpr int kModelQuatShift = 14;
    inline constexpr int kModelPosShift = 6;

    struct ModelHeader
    {
        char magic[4];
        std::uint16_t version;
        std::uint16_t boneCount;
        std::uint32_t fileSize;
        std::uint32_t crc32; ///< of every byte after this header
        std::uint16_t batchCount;
        std::uint16_t clipCount;
        std::uint16_t eventCount;
        std::uint16_t bodyPartCount;
        std::uint32_t gxWordCount;
        std::uint32_t trackCount;
        std::uint32_t keyCount;
        std::uint16_t atlasWidth; ///< of its skin, mdl/<name>.htx; 0 = no skin
        std::uint16_t atlasHeight;
        /// How much coarser than 1/128 unit its vertices are: 0 for nearly
        /// everything, more for a model too large to fit 16 bits otherwise.
        std::uint8_t vertexShift;
        std::uint8_t pad[3];
        std::int16_t mins[3]; ///< its box at rest, whole units, model space
        std::int16_t maxs[3];
        std::uint16_t radius; ///< of a ball round its origin that holds it in any pose
        std::uint16_t polygons; ///< the most any choice of body draws
        std::uint32_t reserved;
    };

    /// One body part: how an entity's "body" number picks a sub-model of it,
    /// (body / base) % count.
    struct ModelBodyPart
    {
        std::uint16_t base;
        std::uint16_t count;
    };

    struct ModelBatch
    {
        std::uint32_t gxOfs; ///< first word in the stream
        std::uint16_t gxWords;
        std::uint16_t polygons;
        std::uint16_t vertices;
        std::uint8_t bodyPart;
        std::uint8_t subModel;
        std::uint8_t boneCount;
        std::uint8_t pad[3];
        /// The bones it uses. The stream's "use slot N" is N-th of these.
        std::uint8_t bones[kModelBatchBones + 2];
    };

    inline constexpr std::uint16_t kClipLoops = 1;

    struct ModelClip
    {
        std::uint32_t name;       ///< nameHash of the sequence's label
        std::uint16_t frameCount; ///< frames of the original; the clip runs 0..frameCount-1
        std::uint16_t fps;        ///< frames a second, Q8
        std::uint16_t flags;      ///< kClip*
        std::uint16_t firstEvent;
        std::uint16_t eventCount;
        std::uint16_t pad;
        std::uint32_t firstTrack; ///< boneCount tracks from here, a bone each
        /// How far one run of the clip carries the model along its own
        /// forward, left and up (Q12 units): a walk's stride. The animation
        /// itself stays where it is.
        fx travel[3];
        /// Where the root ends up relative to where it began, forward and
        /// left (Q12 units): what a scripted move leaves the actor at.
        fx endOffset[2];
    };

    /// One bone's part of a clip: its turning and its place, each as keys at
    /// frames 0, step, 2 * step, ... and a last one at the clip's last frame.
    /// One key: it does not change. (Nearly every bone only turns: its place
    /// against its parent is one key for the whole clip.)
    struct ModelTrack
    {
        std::uint32_t firstRot; ///< into Keys
        std::uint32_t firstPos;
        std::uint16_t rotCount;
        std::uint16_t rotStep;
        std::uint16_t posCount;
        std::uint16_t posStep;
    };

    /// Three numbers: a place in Q6 units, or the x y z of a unit quaternion
    /// in Q14 whose w is not negative (and so need not be stored).
    struct ModelKey
    {
        std::int16_t v[3];
    };

    struct ModelEvent
    {
        std::uint16_t frame;
        std::uint16_t pad;
        std::uint32_t target; ///< nameHash of a name to fire
    };

    static_assert(sizeof(ModelHeader) == 64);
    static_assert(sizeof(ModelBatch) == 48);
    static_assert(sizeof(ModelClip) == 40);
    static_assert(sizeof(ModelTrack) == 16);
    static_assert(sizeof(ModelKey) == 6);
    static_assert(sizeof(ModelEvent) == 8);

    // The geometry-engine commands a model's stream is made of.
    inline constexpr std::uint8_t kGxMtxRestore = 0x14;
    inline constexpr std::uint8_t kGxNormal = 0x21;
    inline constexpr std::uint8_t kGxTexCoord = 0x22;
    inline constexpr std::uint8_t kGxVtx16 = 0x23;
    inline constexpr std::uint8_t kGxVtx10 = 0x24;
    inline constexpr std::uint8_t kGxBegin = 0x40;
    /// The first matrix-stack slot bones go in: slot 0 is the world's.
    inline constexpr int kModelFirstSlot = 1;

    /// A model file, opened: pointers into the block it was read into.
    struct ModelView
    {
        const ModelHeader *header = nullptr;
        const std::int16_t *parents = nullptr; ///< per bone; -1 for a root
        const ModelBodyPart *bodyParts = nullptr;
        const ModelBatch *batches = nullptr;
        const std::uint32_t *gx = nullptr;
        const ModelClip *clips = nullptr;
        const ModelTrack *tracks = nullptr;
        const ModelKey *keys = nullptr;
        const ModelEvent *events = nullptr;

        /// False for anything that is not a whole, consistent model of this
        /// version.
        bool open(const std::uint8_t *data, std::uint32_t size)
        {
            *this = ModelView();
            if (size < sizeof(ModelHeader) || (reinterpret_cast<std::uintptr_t>(data) & 3) != 0)
                return false;
            const auto *h = reinterpret_cast<const ModelHeader *>(data);
            if (std::memcmp(h->magic, kModelMagic, 4) != 0 || h->version != kModelVersion ||
                h->fileSize != size || h->boneCount == 0 || h->boneCount > kModelMaxBones ||
                crc32(data + sizeof(ModelHeader), size - sizeof(ModelHeader)) != h->crc32)
                return false;
            // The sections follow the header in this order, each padded to
            // four bytes.
            std::uint64_t at = sizeof(ModelHeader);
            auto take = [&](std::uint64_t bytes) -> const std::uint8_t *
            {
                const std::uint8_t *p = data + at;
                at += (bytes + 3) & ~std::uint64_t(3);
                return p;
            };
            const auto *p = reinterpret_cast<const std::int16_t *>(take(h->boneCount * 2ull));
            const auto *bp = reinterpret_cast<const ModelBodyPart *>(
                take(h->bodyPartCount * sizeof(ModelBodyPart)));
            const auto *b = reinterpret_cast<const ModelBatch *>(take(h->batchCount * sizeof(ModelBatch)));
            const auto *g = reinterpret_cast<const std::uint32_t *>(take(h->gxWordCount * 4ull));
            const auto *c = reinterpret_cast<const ModelClip *>(take(h->clipCount * sizeof(ModelClip)));
            const auto *t = reinterpret_cast<const ModelTrack *>(take(h->trackCount * sizeof(ModelTrack)));
            const auto *k = reinterpret_cast<const ModelKey *>(take(h->keyCount * sizeof(ModelKey)));
            const auto *e = reinterpret_cast<const ModelEvent *>(take(h->eventCount * sizeof(ModelEvent)));
            if (at != size)
                return false;

            for (int i = 0; i < h->boneCount; ++i)
            {
                if (p[i] >= i || p[i] < -1) // parents first
                    return false;
            }
            for (int i = 0; i < h->bodyPartCount; ++i)
            {
                if (bp[i].base == 0 || bp[i].count == 0)
                    return false;
            }
            for (int i = 0; i < h->batchCount; ++i)
            {
                if (static_cast<std::uint64_t>(b[i].gxOfs) + b[i].gxWords > h->gxWordCount ||
                    b[i].boneCount > kModelBatchBones || b[i].bodyPart >= h->bodyPartCount ||
                    b[i].subModel >= bp[b[i].bodyPart].count)
                    return false;
                for (int n = 0; n < b[i].boneCount; ++n)
                {
                    if (b[i].bones[n] >= h->boneCount)
                        return false;
                }
            }
            for (int i = 0; i < h->clipCount; ++i)
            {
                if (c[i].frameCount == 0 || c[i].fps == 0 ||
                    static_cast<std::uint64_t>(c[i].firstTrack) + h->boneCount > h->trackCount ||
                    static_cast<std::uint32_t>(c[i].firstEvent) + c[i].eventCount > h->eventCount)
                    return false;
            }
            for (std::uint32_t i = 0; i < h->trackCount; ++i)
            {
                if (t[i].rotCount == 0 || t[i].rotStep == 0 || t[i].posCount == 0 ||
                    t[i].posStep == 0 ||
                    static_cast<std::uint64_t>(t[i].firstRot) + t[i].rotCount > h->keyCount ||
                    static_cast<std::uint64_t>(t[i].firstPos) + t[i].posCount > h->keyCount)
                    return false;
            }
            header = h;
            parents = p;
            bodyParts = bp;
            batches = b;
            gx = g;
            clips = c;
            tracks = t;
            keys = k;
            events = e;
            return true;
        }

        /// The clip called `name` (a nameHash), or -1.
        int findClip(std::uint32_t name) const
        {
            for (int i = 0; i < header->clipCount; ++i)
            {
                if (clips[i].name == name)
                    return i;
            }
            return -1;
        }

        /// Whether `batch` is part of the model for this "body" number.
        bool batchInBody(const ModelBatch &batch, int body) const
        {
            const ModelBodyPart &part = bodyParts[batch.bodyPart];
            return (body < 0 ? 0 : body) / part.base % part.count == batch.subModel;
        }
    };

    /// mdl/<name>.hmd and mdl/<name>.htx: the names a model's files go by.
    inline constexpr int kModelNameMax = 24;
} // namespace hldata
