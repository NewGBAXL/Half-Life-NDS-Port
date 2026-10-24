#pragma once

// What of the potentially visible set the eye can really see into: the leaves
// reached from the eye's own through the holes between leaves (the portals
// the converter works out: Portals.cpp), each with the part of the screen it
// can be seen through. The converter's: what a map's worst view and its far
// distance are, as they are seen (Portals.cpp's seenWorst). (It was the DS's
// too, each pass; there it cost more time than it saved -- about 3 ms a pass
// on a DSi, for little of the PVS left out where a pass is short of room.)
//
// The PVS says which leaves can be seen from ANYWHERE in a leaf, looking any
// way. Most of those are behind a wall from where the eye is, or behind it.
// The flood starts at the eye's leaf with the whole screen. Through each of a
// leaf's portals that is in front of the eye, the leaf beyond can be seen in
// the part of the screen both the portal and the leaf cover; the leaf beyond
// takes that in (as a rectangle: the smallest that holds every way into it),
// and if what it has grew, it is looked at again. A leaf the flood never
// reaches is behind something solid.
//
// It is never less than what is seen: a rectangle holds more than the views
// it is made of; a portal is a box round its holes (a unit larger all round:
// Portals.cpp); one the eye is all but in, or partly behind the eye, passes
// the whole of what it comes from. Where it would take too long -- more than
// `stepsMost` leaves looked at or `testsMost` portals tested, or more leaves
// reached than it has room for -- it gives up, and the caller takes the
// whole PVS: what that costs is known before it starts.
//
// Integers only, no division: a point's place on the screen is the ratio of
// its sideways (or upwards) distance to its distance ahead, and two such are
// compared by multiplying across. Its working space is the caller's:
// nothing is allocated.

#include <cstddef>
#include <cstdint>

#include "HlLevel.hpp"

namespace hldata
{
    /// What lies between two leaves that are not solid, through which one can
    /// be seen from the other: the box round every hole between them (whole
    /// units, a unit larger all round: never smaller than the holes). Most of
    /// the game's walls are square to the axes, and their holes flat: the box
    /// is then the hole itself.
    struct Portal
    {
        std::uint16_t leaves[2];
        std::int16_t mins[3];
        std::int16_t maxs[3];
    };
    struct LeafPortals
    {
        std::uint16_t first; ///< into PortalRefs
        std::uint16_t count;
    };

    /// The portals, as the flood reads them.
    struct PortalSet
    {
        const Portal *portals = nullptr;
        const std::uint16_t *refs = nullptr; ///< portal indices, grouped per leaf
        const LeafPortals *leaves = nullptr; ///< per leaf: its run of refs
        int leafCount = 0;
        bool present() const { return portals != nullptr && leaves != nullptr && leafCount > 0; }
    };

    /// The eye: where it is (whole units) and its axes (unit length, Q12),
    /// and the edges of the part of the screen that is drawn, each as a
    /// direction (across or up, ahead): the sine and cosine of its angle off
    /// the middle, as the camera's cull planes have them -- left and bottom
    /// with a negative first.
    struct PortalEye
    {
        std::int32_t at[3];
        std::int32_t forward[3], right[3], up[3];
        std::int32_t left[2], rightEdge[2], bottom[2], top[2];
    };

    /// A slope on the screen, x / z with z > 0. (Both fit in 32 bits: a
    /// distance of at most a map across, times a Q12 axis; their products are
    /// taken in 64.)
    struct PortalSlope
    {
        std::int32_t x, z;
    };
    /// A part of the screen: [lo, hi] across ([0]) and up ([1]).
    struct PortalRect
    {
        PortalSlope lo[2], hi[2];
    };

    /// The flood's working space, the caller's: `capacity` leaves can be
    /// reached at once, and two words for every leaf of the map, kept from one
    /// flood to the next (nothing to clear between).
    struct PortalSpace
    {
        PortalRect *rects = nullptr;
        std::uint16_t *slotLeaf = nullptr;
        std::uint16_t *queue = nullptr;
        std::uint8_t *queued = nullptr;
        int capacity = 0;
        std::uint16_t *leafMark = nullptr;
        std::uint16_t *leafSlot = nullptr;
        std::uint16_t mark = 0;
        /// What the last flood did: leaves looked at, and portals tested.
        int steps = 0, tests = 0;
        /// Bytes a slot takes (the per-leaf words not counted).
        static constexpr std::size_t slotBytes() { return sizeof(PortalRect) + 2 + 2 + 1; }
    };

    namespace portaldetail
    {
        inline bool less(const PortalSlope &a, const PortalSlope &b)
        {
            return static_cast<std::int64_t>(a.x) * b.z < static_cast<std::int64_t>(b.x) * a.z;
        }

        /// `into` widened to hold `more`; whether it grew.
        inline bool grow(PortalRect &into, const PortalRect &more)
        {
            bool grew = false;
            for (int a = 0; a < 2; ++a)
            {
                if (less(more.lo[a], into.lo[a]))
                {
                    into.lo[a] = more.lo[a];
                    grew = true;
                }
                if (less(into.hi[a], more.hi[a]))
                {
                    into.hi[a] = more.hi[a];
                    grew = true;
                }
            }
            return grew;
        }

        /// The part of `here` the portal's box is seen in; false if none.
        inline bool seen(const Portal &portal, const PortalEye &eye, const PortalRect &here, PortalRect &out)
        {
            // An eye in the box, or within eight units of it: the portal is
            // gone through or not by a hair -- all it comes from passes.
            constexpr int kNearBox = 8;
            bool inBox = true;
            for (int a = 0; a < 3 && inBox; ++a)
                inBox = eye.at[a] >= portal.mins[a] - kNearBox && eye.at[a] <= portal.maxs[a] + kNearBox;
            if (inBox)
            {
                out = here;
                return true;
            }
            // Where the box is on the screen. Ahead (z), across (x) and up
            // (y) are each a sum of one term per axis of the world, taken at
            // the box's least or most along it -- so their least and most
            // over the box are sums too: eighteen multiplies, no corners.
            // (In 32 bits: within 65536 units of the eye on each axis, by a
            // unit axis in Q12, is under 2^31 all told.)
            std::int32_t zLo = 0, zHi = 0, xLo = 0, xHi = 0, yLo = 0, yHi = 0;
            for (int a = 0; a < 3; ++a)
            {
                const std::int32_t lo = portal.mins[a] - eye.at[a], hi = portal.maxs[a] - eye.at[a];
                const auto add = [&](std::int32_t axis, std::int32_t &least, std::int32_t &most)
                {
                    const std::int32_t p = lo * axis, q = hi * axis;
                    least += p < q ? p : q;
                    most += p < q ? q : p;
                };
                add(eye.forward[a], zLo, zHi);
                add(eye.right[a], xLo, xHi);
                add(eye.up[a], yLo, yHi);
            }
            constexpr std::int32_t kNear = 4 * 4096; // four units ahead
            if (zHi <= kNear)
                return false; // (all of it behind the eye, or as good as)
            if (zLo <= kNear)
            {
                // Some of it near the eye or behind it: it could cover any
                // of the screen -- all it comes from.
                out = here;
                return true;
            }
            // Every point of it ahead: its slopes lie between the least
            // across over the farthest ahead (or the nearest, if that least
            // is to the left) and the most over the nearest (or the
            // farthest). A little more than its corners would give; never
            // less.
            const PortalSlope lo[2] = {{xLo, xLo >= 0 ? zHi : zLo}, {yLo, yLo >= 0 ? zHi : zLo}};
            const PortalSlope hi[2] = {{xHi, xHi >= 0 ? zLo : zHi}, {yHi, yHi >= 0 ? zLo : zHi}};
            for (int a = 0; a < 2; ++a)
            {
                out.lo[a] = less(here.lo[a], lo[a]) ? lo[a] : here.lo[a];
                out.hi[a] = less(hi[a], here.hi[a]) ? hi[a] : here.hi[a];
                if (less(out.hi[a], out.lo[a]))
                    return false;
            }
            return true;
        }
    } // namespace portaldetail

    /// From `eyeLeaf`, as the header says: calls `reach(leaf)` once for each
    /// leaf reached, the eye's first; goes only into leaves for which
    /// `inPvs(leaf)` holds. Returns the leaves reached, or -1 if it gave up.
    template <class InPvs, class Reach>
    int portalFlood(const PortalSet &set, PortalSpace &space, int eyeLeaf, const PortalEye &eye, InPvs &&inPvs,
                    Reach &&reach, int stepsMost = 3000, int testsMost = 1 << 30)
    {
        using namespace portaldetail;
        if (!set.present() || space.capacity <= 0 || eyeLeaf <= 0 || eyeLeaf >= set.leafCount)
            return -1;
        if (++space.mark == 0)
        {
            // (Round again: old marks could be taken for this flood's.)
            for (int i = 0; i < set.leafCount; ++i)
                space.leafMark[i] = 0;
            space.mark = 1;
        }
        // (`waiting`, not head != tail: with every slot waiting, tail has
        // come round to head.)
        int used = 0, head = 0, tail = 0, waiting = 0;
        const int capacity = space.capacity;
        auto take = [&](int leaf, const PortalRect &rect)
        {
            if (used >= capacity)
                return false;
            const int slot = used++;
            space.leafMark[leaf] = space.mark;
            space.leafSlot[leaf] = static_cast<std::uint16_t>(slot);
            space.slotLeaf[slot] = static_cast<std::uint16_t>(leaf);
            space.rects[slot] = rect;
            space.queued[slot] = 1;
            space.queue[tail] = static_cast<std::uint16_t>(slot);
            if (++tail == capacity)
                tail = 0;
            ++waiting;
            reach(leaf);
            return true;
        };
        PortalRect whole;
        whole.lo[0] = {eye.left[0], eye.left[1]};
        whole.hi[0] = {eye.rightEdge[0], eye.rightEdge[1]};
        whole.lo[1] = {eye.bottom[0], eye.bottom[1]};
        whole.hi[1] = {eye.top[0], eye.top[1]};
        if (!take(eyeLeaf, whole))
            return -1;
        int steps = 0, tests = 0;
        // (What it did, whichever way it ends.)
        struct Count
        {
            PortalSpace &space;
            int &steps, &tests;
            ~Count()
            {
                space.steps = steps;
                space.tests = tests;
            }
        } count{space, steps, tests};
        while (waiting > 0)
        {
            if (++steps > stepsMost)
                return -1;
            const int slot = space.queue[head];
            if (++head == capacity)
                head = 0;
            --waiting;
            space.queued[slot] = 0;
            const int leaf = space.slotLeaf[slot];
            const PortalRect here = space.rects[slot];
            const LeafPortals &own = set.leaves[leaf];
            for (int k = 0; k < own.count; ++k)
            {
                const Portal &portal = set.portals[set.refs[own.first + k]];
                const int other = portal.leaves[0] == leaf ? portal.leaves[1] : portal.leaves[0];
                if (other <= 0 || other >= set.leafCount || other == leaf || !inPvs(other))
                    continue;
                PortalRect through;
                if (++tests > testsMost)
                    return -1;
                if (!seen(portal, eye, here, through))
                    continue;
                if (space.leafMark[other] != space.mark)
                {
                    if (!take(other, through))
                        return -1;
                    continue;
                }
                // Reached before: it takes this way in too, and is looked at
                // again if that is more of the screen.
                const int there = space.leafSlot[other];
                if (grow(space.rects[there], through) && !space.queued[there])
                {
                    space.queued[there] = 1;
                    space.queue[tail] = static_cast<std::uint16_t>(there);
                    if (++tail == capacity)
                        tail = 0;
                    ++waiting;
                }
            }
        }
        return used;
    }

    /// A working space owned by its holder, for the PC (the converter, the
    /// tests): `capacity` slots and `leafCount` leaves.
    template <class Vector8, class Vector16, class VectorRect>
    inline PortalSpace portalSpaceOf(VectorRect &rects, Vector16 &slotLeaf, Vector16 &queue, Vector8 &queued,
                                     Vector16 &leafMark, Vector16 &leafSlot, int capacity, int leafCount)
    {
        rects.assign(capacity, PortalRect{});
        slotLeaf.assign(capacity, 0);
        queue.assign(capacity, 0);
        queued.assign(capacity, 0);
        leafMark.assign(leafCount, 0);
        leafSlot.assign(leafCount, 0);
        PortalSpace space;
        space.rects = rects.data();
        space.slotLeaf = slotLeaf.data();
        space.queue = queue.data();
        space.queued = queued.data();
        space.capacity = capacity;
        space.leafMark = leafMark.data();
        space.leafSlot = leafSlot.data();
        return space;
    }
} // namespace hldata
