#pragma once

// One map face to one run of geometry-engine commands.
//
// A BSP face is a convex polygon of any number of sides; the DS draws
// triangles, quads and strips of either. Two limits decide how a face is cut
// up -- 2048 polygons and 6144 vertices a frame -- and on real maps it is the
// VERTICES that run out first (hlds-convert info). So the cut is chosen to
// spend as few vertices as possible:
//
//   3 sides           one triangle             3 vertices, 1 polygon
//   4 sides           one quad                 4 vertices, 1 polygon
//   even, 6 or more   one quad strip           n vertices, n/2 - 1 polygons
//   odd, 5 or more    a quad strip + triangle  n + 2 vertices
//
// A quad strip zig-zags across the polygon: rows (p0,p1), (p[n-1],p2),
// (p[n-2],p3)... Each quad is a slice of a convex polygon, so it is convex and
// planar, which is all the hardware asks of a quad.
//
// COLLINEAR POINTS. The map compiler adds vertices along a face's edges
// wherever a neighbouring face ends, so that edges match up exactly. Those
// points must stay -- removing them is what makes cracks -- but a cut that
// starts on the wrong one produces a sliver with no area. The starting vertex
// is therefore rotated to whichever makes the smallest piece largest.
//
// WINDING. GoldSrc lists a face's vertices clockwise seen from in front; the
// DS draws counter-clockwise polygons as front-facing. The axis change from
// Half-Life's coordinates to the DS's is a rotation, which keeps handedness,
// so the order is reversed here, once.

#include <array>
#include <cstdint>
#include <vector>

#include "hlds/GxStream.hpp"
#include "hlds/TextureSize.hpp"

namespace hlds
{
    struct FacePoint
    {
        double pos[3]; ///< Half-Life units
        double s, t;   ///< source texels
        std::uint16_t colour; ///< RGB555: what is sent, and what "same vertex" compares
    };

    /// A position on the grid the DS draws on: quarter units.
    using GridPoint = std::array<std::int64_t, 3>;

    /// Whether m is ON the side a -> c, between its ends, as far as the grid
    /// can say (see the definition for why that is a distance and not "exactly
    /// in line").
    bool onGridSide(const GridPoint &a, const GridPoint &m, const GridPoint &c);

    struct BuiltSurface
    {
        std::vector<std::uint32_t> words;
        int polygons = 0;
        int vertices = 0;
        double area = 0; ///< of the polygons sent, square units
        std::int16_t center[3] = {0, 0, 0};
        std::uint16_t radius = 0;
        bool texCoordsClamped = false; ///< the face spans more texels than 12.4 holds
        bool outOfRange = false;       ///< a vertex does not fit a 16-bit coordinate
    };

    /// The rotation of a polygon (index of the vertex to start the zig-zag from)
    /// that makes its smallest piece largest. `points` is 3 doubles a vertex.
    int bestStripStart(const std::vector<FacePoint> &polygon);

    /// The zig-zag's pieces for a polygon of `n` vertices started at vertex
    /// `start`, as index lists: quads of four, and for an odd polygon one
    /// triangle. Each piece keeps the polygon's winding.
    std::vector<std::vector<int>> zigzagPieces(int n, int start);

    /// Cut a convex polygon (in the DS's counter-clockwise order) into quads
    /// and triangles, as index lists.
    ///
    /// The zig-zag is used whenever it leaves no piece without area. When it
    /// cannot -- a polygon with several mid-edge vertices can defeat every
    /// starting point -- the polygon is cut by clipping ears instead, never an
    /// ear with no area, and the triangles are paired back into quads. What
    /// matters is the guarantee: no piece's edge runs past a vertex of the
    /// polygon without stopping at it, because the face on the other side of
    /// that vertex does stop there, and the mismatch is a crack.
    std::vector<std::vector<int>> decompose(const std::vector<FacePoint> &polygon);

    /// Build the command run for one face. `polygon` is in the BSP's own
    /// (clockwise) order.
    BuiltSurface buildSurface(const std::vector<FacePoint> &polygon, TextureSize cooked,
                              std::uint32_t sourceWidth, std::uint32_t sourceHeight);
} // namespace hlds
