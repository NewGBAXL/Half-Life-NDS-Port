#pragma once

// What a map will cost the DS, measured before any DS code runs.
//
// The DS draws at most 2048 polygons and 6144 vertices a frame, and that is
// the constraint the whole renderer is built around. This answers the question
// it turns on: standing in each leaf of a real map, how much does the PVS say
// could be visible?
//
// The figures are an UPPER BOUND. They count everything the PVS lists, in every
// direction at once; the runtime also culls to the view frustum and to a far
// distance, and a 90-degree view usually sees well under half of it. So a map
// whose worst leaf fits is safe, and one that does not is a candidate for
// two-pass rendering rather than a failure.

#include <map>
#include <string>
#include <string_view>
#include <vector>

#include "hlds/BspFile.hpp"

namespace hlds
{
    /// A face with this texture is never drawn: tool brushes, and the sky, which
    /// the DS draws its own way rather than as world polygons.
    bool isUndrawnTexture(std::string_view lowerName);

    /// What one face costs once it is cut into the quads and triangles the
    /// geometry engine accepts.
    struct FaceCost
    {
        int triangles = 0; ///< if every polygon were a triangle
        int polygons = 0;  ///< pairing triangles into quads where possible
        int vertices = 0;  ///< vertex RAM used by those quads and triangles
    };
    FaceCost faceCost(int edgeCount);

    struct LeafLoad
    {
        int leaf = 0;
        int faces = 0;
        FaceCost cost;
    };

    struct MapStats
    {
        float mins[3] = {0, 0, 0};
        float maxs[3] = {0, 0, 0};

        int worldFaces = 0;      ///< faces of model 0
        int drawnWorldFaces = 0; ///< of those, the ones with a drawn texture
        int brushModelFaces = 0; ///< drawn faces of every other model (doors, ...)
        FaceCost worldCost;      ///< the whole world drawn at once

        int visLeaves = 0;
        LeafLoad worst;          ///< the leaf with the most polygons in its PVS
        int medianPolygons = 0;
        int p90Polygons = 0;
        /// Leaves by PVS polygon count, in buckets of kBucket.
        static constexpr int kBucket = 256;
        std::vector<int> histogram;

        int textureCount = 0;
        int embeddedTextures = 0;
        int usedTextures = 0;    ///< named by at least one drawn face
        std::map<std::string, int> classCounts;
    };

    MapStats computeMapStats(const BspFile &bsp);
} // namespace hlds
