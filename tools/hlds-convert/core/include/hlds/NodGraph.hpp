#pragma once

// The game's own node graphs: maps/graphs/<map>.nod.
//
// The first time a map is run, the game works out which of its nodes can be
// walked between -- by something small, by a person, by something large --
// and through which doors, and writes what it found beside the map. Those
// files come with the game. They are read here for what the port could not
// work out as well by itself: links through doors and things that break
// (the entity each passes through is named), links for small things where a
// person does not fit, and the long links.
//
// A .nod is the game's memory written out as it stood (version 16, from a
// 32-bit program): a header, a CGraph of 8396 bytes, then nodes of 88 bytes
// and links of 24, and tables the port has no use for. What was a pointer in
// memory is rubbish on disk; the rest is exact. (HL SDK, dlls/nodes.cpp.)

#include <cstdint>
#include <string>
#include <vector>

namespace hlds
{
    inline constexpr int kNodLand = 1;
    inline constexpr int kNodAir = 2;
    inline constexpr int kNodWater = 4;
    inline constexpr int kNodSmall = 1; ///< a link something 24 units across can take
    inline constexpr int kNodHuman = 2;
    inline constexpr int kNodLarge = 4;
    inline constexpr int kNodFly = 8;

    struct NodNode
    {
        float origin[3]; ///< on the floor, for a land node
        int info = 0;    ///< kNodLand / kNodAir / kNodWater
    };
    struct NodLink
    {
        int from = 0;
        int to = 0;
        int info = 0;      ///< kNodSmall | kNodHuman | ...
        float length = 0;
        /// The brush model the link passes through (a door, something that
        /// breaks): its number in the map, as in "*12"; 0 for none.
        int model = 0;
    };
    struct NodGraph
    {
        std::vector<NodNode> nodes;
        std::vector<NodLink> links;
    };

    /// False, with why, for anything that is not a whole version-16 graph.
    bool parseNodGraph(const std::vector<std::uint8_t> &bytes, NodGraph &out, std::string &error);
} // namespace hlds
