#pragma once

// The small things both halves of the world renderer use: the half that runs
// from fast memory (WorldDraw.itcm.arm.cpp: choosing what is seen and sending
// it) and the half that does not need to (WorldDrawRest.arm.cpp: setting up,
// brushes, the sky). Inline, so each half has its own copy where it runs.

#include <cstdint>

#include "HlPlaneMath.hpp"
#include "data/LevelFile.hpp"
#include "render/Camera.hpp"

namespace hlrt::worldparts
{

        using namespace hldata;

        // Deeper than any tree the map compilers produce; a walk that would
        // overflow it drops the far branch instead of the stack.
        constexpr int kWalkStack = 512;
        inline std::int16_t walkStack[kWalkStack];

        /// True when the box is entirely behind one of the planes.
        inline bool boxOutside(const CullPlane *planes, const std::int16_t *mins,
                               const std::int16_t *maxs)
        {
            for (int p = 0; p < Camera::kPlaneCount; ++p)
            {
                const CullPlane &plane = planes[p];
                // The corner furthest along the normal: if even that is behind
                // the plane, the whole box is.
                const std::int32_t d =
                    plane.normal[0] * (plane.normal[0] >= 0 ? maxs[0] : mins[0]) +
                    plane.normal[1] * (plane.normal[1] >= 0 ? maxs[1] : mins[1]) +
                    plane.normal[2] * (plane.normal[2] >= 0 ? maxs[2] : mins[2]);
                if (d < plane.dist)
                    return true;
            }
            return false;
        }

        inline bool sphereOutside(const CullPlane *planes, const std::int16_t *center,
                                  std::int32_t radius)
        {
            const std::int32_t reach = radius << 12;
            for (int p = 0; p < Camera::kPlaneCount; ++p)
            {
                const CullPlane &plane = planes[p];
                const std::int32_t d = plane.normal[0] * center[0] + plane.normal[1] * center[1] +
                                       plane.normal[2] * center[2];
                if (d - plane.dist < -reach)
                    return true;
            }
            return false;
        }

        /// Descend the render tree to the leaf holding `pos`.
        inline int findLeaf(const LevelView &view, const fx *pos)
        {
            std::int32_t index = view.models[0].headNode;
            while (index >= 0)
            {
                const Node &node = view.nodes[index];
                const fx d = planeDistance(view.planes[node.plane], view.planeTypes[node.plane], pos);
                index = node.children[d >= 0 ? 0 : 1];
            }
            return -1 - index;
        }
} // namespace hlrt::worldparts
