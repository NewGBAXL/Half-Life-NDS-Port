#pragma once

// The holes between the map's leaves, worked out again from its BSP tree (a
// .bsp no longer carries them: the compiler threw them away once the PVS was
// made). Each node's plane, as far as the node's own space reaches, is cut
// into pieces by the planes of what lies on either side of it, down to the
// leaves; a piece with a leaf on each side that is not solid is a portal.
//
// What is done with them: shared/HlPortals.hpp's flood, here (seenWorst).
// They are not in the cooked level.

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "HlPortals.hpp"
#include "hlds/BspFile.hpp"
#include "hlds/LevelWriter.hpp"

namespace hlds
{
    struct LeafPortal
    {
        int leaves[2] = {0, 0};
        std::vector<std::array<double, 3>> winding; ///< a convex polygon, in order
    };

    /// Every portal of the world model's tree (hull 0).
    std::vector<LeafPortal> buildPortals(const BspFile &bsp);

    /// ... as the flood takes them: one for each two leaves that have holes
    /// between them, the box round them all, in whole units and a unit
    /// larger all round (never smaller than the holes); each leaf's run of
    /// them.
    struct CookedPortals
    {
        std::vector<hldata::Portal> portals;
        std::vector<std::uint16_t> refs;
        std::vector<hldata::LeafPortals> leaves;
    };
    /// False, with `error`, if there are more than 16-bit indices hold.
    bool cookPortals(const std::vector<LeafPortal> &portals, std::size_t leafCount, CookedPortals &out,
                     std::string &error);

    /// The worst a pass can be asked for, as it is seen: from the middle of
    /// every leaf, looking eight ways round and three up and down at the
    /// game's field of view, what the portal flood reaches (HlPortals.hpp)
    /// and the walk would send -- what faces the eye and is in the view,
    /// within the "far" setting's distance.
    struct SeenWorst
    {
        int polygons = 0, vertices = 0;
        /// How far the farthest of what is seen is, the leaves taken
        /// together: what the map's far distance is cut to (whole units, a
        /// multiple of 256; hldata::kLevelFarLeast..kLevelFarMost).
        int farUnits = 0;
        int givenUp = 0; ///< floods that gave up (the PVS was counted)
        struct Hotspot
        {
            int leaf = 0;
            int at[3] = {0, 0, 0};
            int yaw = 0, pitch = 0; ///< degrees
            int polygons = 0, vertices = 0;
        };
        std::vector<Hotspot> hotspots; ///< the worst leaves, worst first (at most 10)
    };
    SeenWorst seenWorst(const BspFile &bsp, const CookedLevel &level, const CookedPortals &portals);
} // namespace hlds
