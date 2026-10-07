#pragma once

// Collision against brush entities as well as the world.
//
// A door, a grate or an invisible wall is its own little level: it has clip
// hulls of its own, in its own space, and that space sits somewhere in the
// world -- somewhere that changes, for a door. So a move is traced against the
// world's hull, then against each solid brush's hull with the segment carried
// into that brush's space, and the nearest thing hit wins. That is how the PC
// engine does it (xash3d pm_trace.c, PM_PlayerTraceExt).
//
// TWO DELIBERATE CHOICES:
//
//  * A brush the move STARTS inside is ignored. When a door closes on the
//    player there is a moment when they overlap; if that counted as "in
//    solid" the player would be frozen in the door. Ignoring it lets them walk
//    out, and pushing them out is the door's job, not the trace's.
//
//  * Each brush has a box in world space that is tested first. A map has up to
//    a couple of hundred brushes and a frame of movement makes a dozen traces;
//    nearly all of those pairs are nowhere near each other.
//
// std-only and inline, like HlHullTrace.hpp: the host tests run this code.

#include "HlHullTrace.hpp"
#include "HlPlaneMath.hpp"

namespace hldata
{
    /// A brush entity as the collision sees it, right now.
    struct MoverHull
    {
        Hull standing;
        Hull crouched;
        fx origin[3] = {0, 0, 0}; ///< where the brush's own space sits in the world
        /// How the brush is turned about its origin: the matrix that takes a
        /// direction in its own space to the world's, row by row, in Q30
        /// (anglesToAxes). `turned` is false for a brush that is not turned at
        /// all, which is nearly all of them and costs them nothing.
        std::int32_t axes[9] = {1 << 30, 0, 0, 0, 1 << 30, 0, 0, 0, 1 << 30};
        bool turned = false;
        /// Its bounds in the world at this position. Not grown by the player's
        /// size: the test below does that.
        fx mins[3] = {0, 0, 0};
        fx maxs[3] = {0, 0, 0};
        bool solid = false;       ///< present and in the way
        /// A brush that is a body of liquid (func_water): what it is full of
        /// (kContents*), or 0 for a brush that is not one. Whatever is inside
        /// its box is in that liquid while it is `present`.
        std::int8_t contents = 0;
        bool present = false;     ///< there at all, solid or not
    };

    /// A volume the player climbs in: a func_ladder. Not solid, never drawn.
    struct LadderVolume
    {
        fx mins[3] = {0, 0, 0};
        fx maxs[3] = {0, 0, 0};
        bool active = false;
    };

    /// Something solid that is just a box: an actor standing there. Its bounds
    /// are in the world; like a brush's, they are not grown by the player's
    /// size.
    struct SolidBox
    {
        fx mins[3] = {0, 0, 0};
        fx maxs[3] = {0, 0, 0};
        bool active = false;
    };

    /// Everything the player can collide with, and climb.
    struct MoveWorld
    {
        Hull standing;
        Hull crouched;
        const MoverHull *movers = nullptr;
        int moverCount = 0;
        const LadderVolume *ladders = nullptr;
        int ladderCount = 0;
        /// The world's render tree, which is where water is: the clip hulls
        /// the player collides with know only solid and empty. Null on a
        /// world given no tree (some tests): then nothing is water.
        const Node *nodes = nullptr;
        const Leaf *leaves = nullptr;
        std::int32_t headNode = 0;
        /// What each leaf of that tree can see (the level's Vis section and
        /// how many leaves have a row in it). May be none: then every leaf
        /// sees every other.
        const std::uint8_t *vis = nullptr;
        std::uint32_t visSize = 0;
        int visLeafCount = 0;
        /// The actors' boxes.
        const SolidBox *boxes = nullptr;
        int boxCount = 0;
        /// If set, what the player's movement traces with in place of
        /// traceAll: the same function, from wherever the caller has put a
        /// fast copy of it (the DS keeps one in ITCM).
        struct Trace (*trace)(const MoveWorld &world, bool crouched, const fx *start,
                              const fx *end) = nullptr;
    };

    /// The leaf of the world's tree `p` is in; 0 (the solid leaf) on a world
    /// given no tree.
    inline int worldLeaf(const MoveWorld &world, const fx *p)
    {
        if (world.nodes == nullptr)
            return 0;
        std::int32_t index = world.headNode;
        while (index >= 0)
        {
            const Node &node = world.nodes[index];
            const fx d = planeDistance(world.standing.planes[node.plane],
                                       world.standing.planeTypes[node.plane], p);
            index = node.children[d >= 0 ? 0 : 1];
        }
        return -1 - index;
    }

    /// Whether leaf `to` can be seen from leaf `from`, by the map's own
    /// visibility. A leaf with no row (the solid one, or a map that was
    /// never vised) sees everything.
    inline bool leafSees(const MoveWorld &world, int from, int to)
    {
        if (world.vis == nullptr || world.leaves == nullptr || from <= 0 || from == to)
            return true;
        const std::int32_t ofs = world.leaves[from].visOfs;
        if (ofs < 0 || to <= 0 || to > world.visLeafCount)
            return ofs < 0;
        // The BSP's run-length rows: a non-zero byte is eight leaves, a zero
        // byte is followed by how many zero bytes it stands for. Bit 0 of
        // the first byte is leaf 1.
        const std::uint8_t *in = world.vis + ofs;
        const std::uint8_t *end = world.vis + world.visSize;
        int index = 1;
        while (index <= to && in < end)
        {
            const std::uint8_t bits = *in++;
            if (bits == 0)
            {
                if (in >= end)
                    break;
                index += 8 * *in++;
                continue;
            }
            if (to < index + 8)
                return (bits >> (to - index)) & 1;
            index += 8;
        }
        return false;
    }

    /// What is at `p`: kContentsEmpty, Solid, Water, Slime or Lava. The
    /// world's own leaf first; a body of liquid that is a brush entity
    /// overrides it, as in the game.
    inline int pointContents(const MoveWorld &world, const fx *p)
    {
        int contents = kContentsEmpty;
        if (world.nodes != nullptr)
        {
            std::int32_t index = world.headNode;
            while (index >= 0)
            {
                const Node &node = world.nodes[index];
                const fx d = planeDistance(world.standing.planes[node.plane],
                                           world.standing.planeTypes[node.plane], p);
                index = node.children[d >= 0 ? 0 : 1];
            }
            contents = world.leaves[-1 - index].contents;
        }
        for (int m = 0; m < world.moverCount; ++m)
        {
            const MoverHull &mover = world.movers[m];
            if (mover.contents == 0 || !mover.present)
                continue;
            if (p[0] >= mover.mins[0] && p[0] <= mover.maxs[0] && p[1] >= mover.mins[1] &&
                p[1] <= mover.maxs[1] && p[2] >= mover.mins[2] && p[2] <= mover.maxs[2])
                contents = mover.contents;
        }
        return contents;
    }

    /// Water, slime or lava: something to swim in.
    constexpr bool isLiquid(int contents)
    {
        return contents <= kContentsWater && contents >= kContentsLava;
    }

    /// The matrix for a brush turned by `angles` (turns, 65536 to the circle):
    /// world = m * local, m[row * 3 + column], in Q30 (fxSinCosFine says why
    /// not Q12). It is the game's own (yaw, then pitch, then roll), so a fan
    /// cooked from a map turns the way it does on the PC.
    inline void anglesToAxes(const std::uint16_t *angles, std::int32_t *m)
    {
        std::int32_t sp, cp, sy, cy, sr, cr;
        fxSinCosFine(angles[kPitch], sp, cp);
        fxSinCosFine(angles[kYaw], sy, cy);
        fxSinCosFine(angles[kRoll], sr, cr);
        const auto mul = [](std::int64_t a, std::int64_t b)
        {
            return static_cast<std::int32_t>((a * b) >> kNormalShift);
        };
        const std::int32_t srsp = mul(sr, sp);
        const std::int32_t crsp = mul(cr, sp);
        m[0] = mul(cp, cy);
        m[1] = mul(srsp, cy) - mul(cr, sy);
        m[2] = mul(crsp, cy) + mul(sr, sy);
        m[3] = mul(cp, sy);
        m[4] = mul(srsp, sy) + mul(cr, cy);
        m[5] = mul(crsp, sy) - mul(sr, cy);
        m[6] = -sp;
        m[7] = mul(sr, cp);
        m[8] = mul(cr, cp);
    }

    namespace detail
    {
        // The player's half-size: 16 x 16, and 36 standing or 18 crouched.
        // The clip hulls are already grown by it; the brushes' boxes are not.
        inline constexpr fx kPlayerHalfWidth = fxFromInt(16);
        inline constexpr fx kPlayerHalfHeight[2] = {fxFromInt(36), fxFromInt(18)};
        /// A little more than the trace's own stand-off distance, so a move
        /// that would stop just short of a brush still gets tested against it.
        inline constexpr fx kMoverSlack = fxFromInt(1);

        /// Can the segment a..b, as the centre of the player's box, come near
        /// the brush at all?
        HL_HOT bool nearMover(const MoverHull &mover, bool crouched, const fx *a, const fx *b)
        {
            for (int i = 0; i < 3; ++i)
            {
                const fx grow = (i == 2 ? kPlayerHalfHeight[crouched ? 1 : 0] : kPlayerHalfWidth) +
                                kMoverSlack;
                const fx lo = a[i] < b[i] ? a[i] : b[i];
                const fx hi = a[i] < b[i] ? b[i] : a[i];
                if (hi < mover.mins[i] - grow || lo > mover.maxs[i] + grow)
                    return false;
            }
            return true;
        }

        /// A world point in the brush's own space: back along its origin,
        /// then turned back.
        HL_HOT void toMoverSpace(const MoverHull &mover, const fx *world, fx *local)
        {
            const fx d[3] = {world[0] - mover.origin[0], world[1] - mover.origin[1],
                             world[2] - mover.origin[2]};
            if (!mover.turned)
            {
                local[0] = d[0];
                local[1] = d[1];
                local[2] = d[2];
                return;
            }
            // The inverse of a rotation is its transpose.
            for (int j = 0; j < 3; ++j)
            {
                local[j] = static_cast<fx>((static_cast<std::int64_t>(d[0]) * mover.axes[j] +
                                            static_cast<std::int64_t>(d[1]) * mover.axes[3 + j] +
                                            static_cast<std::int64_t>(d[2]) * mover.axes[6 + j]) >>
                                           kNormalShift);
            }
        }

        /// A direction (or an offset) from the brush's space to the world's.
        /// Works on Q12 and Q30 values alike.
        HL_HOT void turnToWorld(const MoverHull &mover, const std::int32_t *local,
                                std::int32_t *world)
        {
            if (!mover.turned)
            {
                world[0] = local[0];
                world[1] = local[1];
                world[2] = local[2];
                return;
            }
            for (int i = 0; i < 3; ++i)
            {
                world[i] = static_cast<std::int32_t>(
                    (static_cast<std::int64_t>(local[0]) * mover.axes[i * 3] +
                     static_cast<std::int64_t>(local[1]) * mover.axes[i * 3 + 1] +
                     static_cast<std::int64_t>(local[2]) * mover.axes[i * 3 + 2]) >>
                    kNormalShift);
            }
        }
    } // namespace detail

    /// Move the player's origin from `start` towards `end` through the world
    /// and every solid brush.
    HL_HOT Trace traceAll(const MoveWorld &world, bool crouched, const fx *start, const fx *end)
    {
        Trace best = traceHull(crouched ? world.crouched : world.standing, start, end);
        for (int m = 0; m < world.moverCount; ++m)
        {
            const MoverHull &mover = world.movers[m];
            if (!mover.solid || !detail::nearMover(mover, crouched, start, end))
                continue;
            fx a[3], b[3];
            detail::toMoverSpace(mover, start, a);
            detail::toMoverSpace(mover, end, b);
            const Trace t = traceHull(crouched ? mover.crouched : mover.standing, a, b);
            if (t.startSolid || t.allSolid || t.fraction >= best.fraction)
                continue;
            // The nearer hit. Whether the move as a whole started in solid is
            // still the world's answer.
            const bool startSolid = best.startSolid;
            const bool allSolid = best.allSolid;
            best = t;
            best.startSolid = startSolid;
            best.allSolid = allSolid;
            // Back into the world: the plane it stopped on, and the end point.
            // For a brush that is turned the end point is taken along the
            // move as it was asked for, in the world -- not brought back from
            // the brush's space. There and back through a rotation is never
            // quite where it started, and a position that is put through it
            // every frame (the ground check does that) walks.
            detail::turnToWorld(mover, t.normal, best.normal);
            std::int64_t shift = 0;
            for (int i = 0; i < 3; ++i)
            {
                // (Rounded towards the start: never a hair further than the
                // trace allowed.)
                const std::int64_t along = static_cast<std::int64_t>(end[i] - start[i]) * t.fraction;
                best.endPos[i] =
                    mover.turned
                        ? start[i] + static_cast<fx>(along >= 0 ? along >> kFractionShift
                                                                : -((-along) >> kFractionShift))
                        : t.endPos[i] + mover.origin[i];
                shift += static_cast<std::int64_t>(best.normal[i]) * mover.origin[i];
            }
            best.dist = t.dist + static_cast<fx>(shift >> kNormalShift);
            best.mover = static_cast<std::int16_t>(m);
        }
        // The boxes: the move against each, grown by the player's size, one
        // pair of faces at a time. A move that starts inside one passes
        // through it, as with a brush: nobody is ever frozen inside an actor
        // who walked up to them.
        for (int n = 0; n < world.boxCount; ++n)
        {
            const SolidBox &box = world.boxes[n];
            if (!box.active)
                continue;
            std::int64_t enter = -1; // Q30; -1 = from before the start
            std::int64_t leave = kFractionOne;
            int axis = -1;
            bool inside = true, miss = false;
            for (int i = 0; i < 3 && !miss; ++i)
            {
                const fx grow = i == 2 ? detail::kPlayerHalfHeight[crouched ? 1 : 0]
                                       : detail::kPlayerHalfWidth;
                const fx lo = box.mins[i] - grow, hi = box.maxs[i] + grow;
                const fx d = end[i] - start[i];
                if (start[i] <= lo || start[i] >= hi)
                    inside = false;
                if (d == 0)
                {
                    miss = start[i] <= lo || start[i] >= hi;
                    continue;
                }
                std::int64_t t0 = (static_cast<std::int64_t>(lo - start[i]) << kFractionShift) / d;
                std::int64_t t1 = (static_cast<std::int64_t>(hi - start[i]) << kFractionShift) / d;
                if (t0 > t1)
                {
                    const std::int64_t swap = t0;
                    t0 = t1;
                    t1 = swap;
                }
                if (t0 > enter)
                {
                    enter = t0;
                    axis = i;
                }
                if (t1 < leave)
                    leave = t1;
                miss = enter > leave;
            }
            if (miss || inside || axis < 0 || enter < 0 || enter >= best.fraction)
                continue;
            // Stop a little short, as the hull trace does.
            const fx d = end[axis] - start[axis];
            const std::int64_t back =
                (static_cast<std::int64_t>(kFxOne / 32) << kFractionShift) / (d < 0 ? -d : d);
            const std::int64_t fraction = enter > back ? enter - back : 0;
            best.fraction = static_cast<std::int32_t>(fraction);
            for (int i = 0; i < 3; ++i)
            {
                best.endPos[i] = start[i] + static_cast<fx>(
                                                (static_cast<std::int64_t>(end[i] - start[i]) * fraction) >>
                                                kFractionShift);
                best.normal[i] = 0;
            }
            best.normal[axis] = d > 0 ? -(1 << kNormalShift) : (1 << kNormalShift);
            {
                const fx grow = axis == 2 ? detail::kPlayerHalfHeight[crouched ? 1 : 0]
                                          : detail::kPlayerHalfWidth;
                best.dist = d > 0 ? -(box.mins[axis] - grow) : box.maxs[axis] + grow;
            }
            best.mover = -1;
            best.hit = true;
        }
        return best;
    }

    /// May the player's origin be at `p`: not in the world's solid, and not
    /// inside any solid brush? This is the question for "is there room to
    /// stand up here" -- NOT for "am I stuck", which asks the world alone (see
    /// the first choice above).
    inline bool positionFree(const MoveWorld &world, bool crouched, const fx *p)
    {
        if (!hullPositionFree(crouched ? world.crouched : world.standing, p))
            return false;
        for (int m = 0; m < world.moverCount; ++m)
        {
            const MoverHull &mover = world.movers[m];
            if (!mover.solid || !detail::nearMover(mover, crouched, p, p))
                continue;
            fx local[3];
            detail::toMoverSpace(mover, p, local);
            if (!hullPositionFree(crouched ? mover.crouched : mover.standing, local))
                return false;
        }
        return true;
    }
} // namespace hldata
