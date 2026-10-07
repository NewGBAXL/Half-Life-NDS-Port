#pragma once

// The one calculation the BSP tree, the PVS walk and the hull trace all turn
// on: which side of a plane a point is, and how far.
//
// Positions are Q12 and normals Q30 (HlFixed.hpp), so the products are summed
// in 64 bits and shifted back to Q12. On the ARM9 that is three SMULL/SMLAL --
// when the file is built as ARM code, which is why every hot caller lives in a
// *.arm.cpp. Axial planes, which are most of a GoldSrc map, skip the multiplies
// altogether.

#include "HlLevel.hpp"

namespace hldata
{
    /// Signed distance from `plane` to `point`, Q12. Positive in front.
    HL_HOT fx planeDistance(const Plane &plane, std::uint8_t type, const fx *point)
    {
        if (type < kPlaneOther)
            return point[type] - plane.dist;
        const std::int64_t dot = static_cast<std::int64_t>(plane.normal[0]) * point[0] +
                                 static_cast<std::int64_t>(plane.normal[1]) * point[1] +
                                 static_cast<std::int64_t>(plane.normal[2]) * point[2];
        return static_cast<fx>(dot >> kNormalShift) - plane.dist;
    }
} // namespace hldata
