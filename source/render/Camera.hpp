#pragma once

// The eye: where it is, where it looks, and the two things the renderer needs
// from that every frame -- the matrix handed to the geometry engine and the
// planes the BSP walk culls against.
//
// AXES. Positions stay in Half-Life's axes (X forward, Y left, Z up) all the
// way to the hardware; the level's vertices are in them too. The geometry
// engine wants X right, Y up and the view down -Z, and that change of axes is
// folded into the view matrix here rather than baked into any data.
//
// SCALE. A level vertex is a 16-bit quarter-unit (HlFixed.hpp) that the
// hardware reads as 4.12, i.e. as (units / 1024). View space is set so that one
// view unit is 128 Half-Life units, which keeps every product inside the
// hardware's 20.12 and leaves 1/32 unit of precision; the matrix therefore
// carries a scale of 8. The clip planes below are in those view units.

#include <nds.h>

#include "HlFixed.hpp"

namespace hlrt
{
    /// Inside when dot(normal, point) - dist >= 0. `normal` is Q12 and points
    /// are whole Half-Life units, so both sides of the comparison are Q12.
    struct CullPlane
    {
        std::int32_t normal[3];
        std::int32_t dist;
    };

    /// Which part of the screen the cull planes enclose. In two-pass mode
    /// each pass draws one half, and culling to that half is where the mode's
    /// extra capacity comes from: geometry in the other half is not sent.
    enum class ViewHalf
    {
        Whole,
        Left,
        Right,
    };

    class Camera
    {
    public:
        static constexpr int kPlaneCount = 5; ///< left, right, bottom, top, far

        hldata::fx pos[3] = {0, 0, 0};
        std::uint16_t yaw = 0;   ///< turns; 0 looks down +X, positive turns left
        std::int16_t pitch = 0;  ///< turns; positive looks up
        /// What is added to those for the picture only, and goes again:
        /// the kick of a weapon or a blow, the lean into a sidestep, the
        /// roll of a fall (shared/HlView.hpp). Turns.
        std::int16_t kickPitch = 0;
        std::int16_t kickYaw = 0;
        std::int16_t roll = 0;   ///< positive leans to the right

        // Derived by update(). Unit vectors in Q12.
        std::int32_t forward[3];
        std::int32_t right[3];
        std::int32_t up[3];
        m4x3 view;
        CullPlane planes[kPlaneCount];

        /// Vertical field of view handed to NEA, in degrees. 74 vertical is 90
        /// horizontal on a 4:3 screen, which is what Half-Life uses.
        static constexpr int kFovDegrees = 74;
        /// Clip planes in view units (128 Half-Life units each), 20.12.
        ///
        /// What is drawn ends at kFarUnits: that is the cull plane the walk
        /// tests against, and it is as far as any map of the game lets one
        /// see in practice (the longest sight line there could be is about
        /// 12,000 units). The HARDWARE's far plane is twice as far again, and
        /// on purpose: its place in the projection matrix is a number a few
        /// units of the last place wide, good to within tens of units at
        /// best, and it must never be the one that decides. Depth does not
        /// suffer for it: the build uses W-buffering, where depth is distance
        /// and as fine far away as near.
        static constexpr int kNearClip = 128;         ///< 4 units
        static constexpr int kFarClip = 128 << 12;    ///< 16384 units
        static constexpr int kFarUnits = 8192;

        /// Recompute the vectors, the matrix and the cull planes (for the whole
        /// screen) from pos, yaw and pitch.
        void update();
        /// Rebuild only the cull planes, for one half of the screen or all of
        /// it. The matrix is the same for both halves: NEA narrows the
        /// projection, not the view.
        void buildPlanes(ViewHalf half);
        /// Load the view matrix into the geometry engine's modelview.
        void apply() const;

        /// Limit pitch to just short of straight up and down, where yaw stops
        /// meaning anything.
        void clampPitch();
    };

    extern Camera camera;
} // namespace hlrt
