#pragma once

// Posing a model: from a clip and a moment in it, each bone's matrix in the
// model's own space.
//
// A clip holds, per bone, keys of a rotation (a quaternion) and a place, both
// relative to the bone's parent. A pose is: for each bone find the two keys
// the moment falls between, blend them, turn the result into a matrix, and
// put it on top of the parent's -- parents always come first in the list, so
// one pass does it.
//
// Everything is fixed point: quaternions in Q14, matrices' rotations in Q14,
// places in Q12 units. Blending two rotations is a straight line between the
// quaternions, made unit length again: between keys that the converter has
// already put close together, that is as good as the long way round the
// sphere and a fraction of the work.
//
// std-only and inline: the host tests hold it against the game's own
// floating-point maths.

#include "HlFixed.hpp"
#include "HlModel.hpp"

namespace hldata
{
    /// A bone's matrix: the rotation's nine in Q14, row by row
    /// (model = r * bone), and where the bone's origin is, in Q12 units.
    struct BoneMatrix
    {
        std::int32_t r[9];
        fx t[3];
    };

    inline constexpr int kBoneRotShift = 14;

    namespace detail
    {
        /// The matrix of a unit quaternion (x y z w, Q14), in Q14.
        inline void quatToMatrix(const std::int32_t *q, std::int32_t *m)
        {
            const auto mul = [](std::int32_t a, std::int32_t b)
            {
                return static_cast<std::int32_t>((static_cast<std::int64_t>(a) * b) >> 13); // 2ab, Q14
            };
            const std::int32_t xx = mul(q[0], q[0]), yy = mul(q[1], q[1]), zz = mul(q[2], q[2]);
            const std::int32_t xy = mul(q[0], q[1]), xz = mul(q[0], q[2]), yz = mul(q[1], q[2]);
            const std::int32_t wx = mul(q[3], q[0]), wy = mul(q[3], q[1]), wz = mul(q[3], q[2]);
            constexpr std::int32_t one = 1 << 14;
            m[0] = one - yy - zz;
            m[1] = xy - wz;
            m[2] = xz + wy;
            m[3] = xy + wz;
            m[4] = one - xx - zz;
            m[5] = yz - wx;
            m[6] = xz - wy;
            m[7] = yz + wx;
            m[8] = one - xx - yy;
        }

        /// Which two of a run of keys `frame` (Q8) falls between, and how
        /// far along (Q12). Keys sit at 0, step, 2 * step ... and the last at
        /// the clip's last frame, which is usually a shorter stretch.
        inline std::uint32_t keySpan(std::uint32_t count, std::uint32_t step, std::uint32_t frame,
                                     std::uint32_t lastFrame, std::int32_t &along)
        {
            std::uint32_t index = (frame >> 8) / step;
            if (index >= count - 1)
                index = count - 2;
            const std::uint32_t from = index * step;
            const std::uint32_t to = index + 2 == count ? lastFrame : from + step;
            const std::uint32_t span = (to > from ? to - from : 1) << 8;
            std::uint32_t done = frame - (from << 8);
            if (done > span)
                done = span;
            along = static_cast<std::int32_t>((static_cast<std::uint64_t>(done) << 12) / span);
            return index;
        }

        /// A stored rotation, whole: the w that was left out put back.
        inline void fullQuat(const ModelKey &key, std::int32_t *q)
        {
            q[0] = key.v[0];
            q[1] = key.v[1];
            q[2] = key.v[2];
            const std::int32_t rest = (1 << 28) - q[0] * q[0] - q[1] * q[1] - q[2] * q[2];
            q[3] = rest > 0 ? static_cast<std::int32_t>(isqrt64(static_cast<std::uint64_t>(rest))) : 0;
        }

        /// One bone's rotation and place at `frame` (Q8 frames) of a track.
        inline void sampleTrack(const ModelTrack &track, const ModelKey *keys, std::uint32_t frame,
                                std::uint32_t lastFrame, std::int32_t *quat, fx *pos)
        {
            const ModelKey *rot = keys + track.firstRot;
            if (track.rotCount == 1)
            {
                fullQuat(*rot, quat);
            }
            else
            {
                std::int32_t s;
                const std::uint32_t index = keySpan(track.rotCount, track.rotStep, frame, lastFrame, s);
                std::int32_t a[4], b[4];
                fullQuat(rot[index], a);
                fullQuat(rot[index + 1], b);
                // The same turn is two quaternions, q and -q: blend towards
                // the nearer.
                std::int32_t dot = 0;
                for (int i = 0; i < 4; ++i)
                    dot += a[i] * b[i];
                const std::int32_t sign = dot < 0 ? -1 : 1;
                std::int64_t length2 = 0;
                for (int i = 0; i < 4; ++i)
                {
                    quat[i] = a[i] + (((sign * b[i] - a[i]) * s) >> 12);
                    length2 += static_cast<std::int64_t>(quat[i]) * quat[i];
                }
                // Back to unit length (Q14).
                const auto length = static_cast<std::int32_t>(isqrt64(static_cast<std::uint64_t>(length2)));
                if (length > 0)
                {
                    for (int i = 0; i < 4; ++i)
                        quat[i] = (quat[i] << 14) / length;
                }
            }

            const ModelKey *place = keys + track.firstPos;
            if (track.posCount == 1)
            {
                for (int i = 0; i < 3; ++i)
                    pos[i] = static_cast<fx>(place->v[i]) << (kFxShift - kModelPosShift);
                return;
            }
            std::int32_t s;
            const std::uint32_t index = keySpan(track.posCount, track.posStep, frame, lastFrame, s);
            for (int i = 0; i < 3; ++i)
            {
                const std::int32_t p =
                    place[index].v[i] + (((place[index + 1].v[i] - place[index].v[i]) * s) >> 12);
                pos[i] = static_cast<fx>(p) << (kFxShift - kModelPosShift);
            }
        }
    } // namespace detail

    /// The model's pose at `frame` (Q8, in the clip's own frames; clamped to
    /// its last) of clip `clip`: `out[bone]` for every bone, in model space.
    ///
    /// `slideBone`, if not negative, is a bone moved `slide` further along
    /// its own second axis than the clip has it: what the game calls a bone
    /// controller, and uses to let a barnacle's tongue out.
    inline void poseModel(const ModelView &model, int clip, std::uint32_t frame, BoneMatrix *out,
                          int slideBone = -1, fx slide = 0)
    {
        const ModelClip &c = model.clips[clip];
        const std::uint32_t lastFrame = c.frameCount - 1u;
        if (frame > (lastFrame << 8))
            frame = lastFrame << 8;
        const ModelTrack *tracks = model.tracks + c.firstTrack;
        const int bones = model.header->boneCount;
        for (int b = 0; b < bones; ++b)
        {
            std::int32_t quat[4];
            fx pos[3];
            detail::sampleTrack(tracks[b], model.keys, frame, lastFrame, quat, pos);
            if (b == slideBone)
                pos[1] += slide;
            std::int32_t local[9];
            detail::quatToMatrix(quat, local);
            const int parent = model.parents[b];
            BoneMatrix &m = out[b];
            if (parent < 0)
            {
                for (int i = 0; i < 9; ++i)
                    m.r[i] = local[i];
                for (int i = 0; i < 3; ++i)
                    m.t[i] = pos[i];
                continue;
            }
            const BoneMatrix &p = out[parent];
            for (int row = 0; row < 3; ++row)
            {
                for (int column = 0; column < 3; ++column)
                {
                    m.r[row * 3 + column] = static_cast<std::int32_t>(
                        (static_cast<std::int64_t>(p.r[row * 3]) * local[column] +
                         static_cast<std::int64_t>(p.r[row * 3 + 1]) * local[3 + column] +
                         static_cast<std::int64_t>(p.r[row * 3 + 2]) * local[6 + column]) >>
                        kBoneRotShift);
                }
                m.t[row] = p.t[row] + static_cast<fx>((static_cast<std::int64_t>(p.r[row * 3]) * pos[0] +
                                                      static_cast<std::int64_t>(p.r[row * 3 + 1]) * pos[1] +
                                                      static_cast<std::int64_t>(p.r[row * 3 + 2]) * pos[2]) >>
                                                     kBoneRotShift);
            }
        }
    }

    /// How far into a clip `ms` more milliseconds takes `frame` (Q8). A clip
    /// that loops goes round; one that does not stops on its last frame, and
    /// `finished` says so.
    inline std::uint32_t advanceClip(const ModelClip &clip, std::uint32_t frame, int ms, bool &finished)
    {
        finished = false;
        const std::uint32_t end = (clip.frameCount - 1u) << 8;
        // fps is Q8 frames a second, so frames * 256 a millisecond is fps / 1000.
        std::uint64_t next = frame + (static_cast<std::uint64_t>(clip.fps) * static_cast<std::uint32_t>(ms)) / 1000;
        if (end == 0)
        {
            finished = !(clip.flags & kClipLoops);
            return 0;
        }
        if (next >= end)
        {
            if (clip.flags & kClipLoops)
                return static_cast<std::uint32_t>(next % end);
            finished = true;
            return end;
        }
        return static_cast<std::uint32_t>(next);
    }
} // namespace hldata
