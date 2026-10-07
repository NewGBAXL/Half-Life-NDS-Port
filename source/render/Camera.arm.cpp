#include "render/Camera.hpp"

namespace hlrt
{
    Camera camera;

    namespace
    {
        // The cull planes are a little WIDER than the picture (47 and 39
        // degrees of half-angle against the 45.1 and 37 actually drawn): the
        // hardware clips exactly, so all a loose plane costs is a few polygons,
        // while a tight one drops geometry at the screen's edge.
        constexpr std::int32_t kSinH = 2996; // sin 47, Q12
        constexpr std::int32_t kCosH = 2793;
        constexpr std::int32_t kSinV = 2578; // sin 39
        constexpr std::int32_t kCosV = 3183;
        constexpr std::int32_t kSinMid = 143; // sin 2: the two-pass seam
        constexpr std::int32_t kCosMid = 4094;

        // Pitch stops at 89 degrees.
        constexpr std::int16_t kMaxPitch = 16202;

        std::int32_t mulQ12(std::int32_t a, std::int32_t b)
        {
            return (a * b) >> 12;
        }

        /// dot(v, pos) with v in Q12 and pos in Q12 units: the result is Q12
        /// (v) times whole units, the scale every CullPlane::dist is in.
        std::int32_t dotPos(const std::int32_t *v, const hldata::fx *pos)
        {
            const std::int64_t d = static_cast<std::int64_t>(v[0]) * pos[0] +
                                   static_cast<std::int64_t>(v[1]) * pos[1] +
                                   static_cast<std::int64_t>(v[2]) * pos[2];
            return static_cast<std::int32_t>(d >> 12);
        }
    } // namespace

    void Camera::clampPitch()
    {
        if (pitch > kMaxPitch)
            pitch = kMaxPitch;
        if (pitch < -kMaxPitch)
            pitch = -kMaxPitch;
    }

    void Camera::update()
    {
        // libnds angles are 32768 to the circle; ours are 65536.
        const std::int32_t sy = sinLerp(static_cast<s16>(yaw >> 1));
        const std::int32_t cy = cosLerp(static_cast<s16>(yaw >> 1));
        const std::int32_t sp = sinLerp(static_cast<s16>(pitch >> 1));
        const std::int32_t cp = cosLerp(static_cast<s16>(pitch >> 1));

        forward[0] = mulQ12(cp, cy);
        forward[1] = mulQ12(cp, sy);
        forward[2] = sp;
        right[0] = sy;
        right[1] = -cy;
        right[2] = 0;
        up[0] = -mulQ12(sp, cy);
        up[1] = -mulQ12(sp, sy);
        up[2] = cp;

        // The hardware multiplies a ROW vector by the matrix, so row i is where
        // Half-Life axis i ends up: (right, up, -forward) components, times the
        // scale of 8 (see the header). The last row is the translation.
        for (int i = 0; i < 3; ++i)
        {
            view.m[i * 3 + 0] = right[i] << 3;
            view.m[i * 3 + 1] = up[i] << 3;
            view.m[i * 3 + 2] = -(forward[i] << 3);
        }
        // dotPos gives Q12 * units; view units are 128 units, so >> 7 more.
        view.m[9] = -(dotPos(right, pos) >> 7);
        view.m[10] = -(dotPos(up, pos) >> 7);
        view.m[11] = dotPos(forward, pos) >> 7;

        buildPlanes(ViewHalf::Whole);
    }

    void Camera::buildPlanes(ViewHalf half)
    {
        // Side planes: forward tilted towards the middle by the half-angle.
        for (int i = 0; i < 3; ++i)
        {
            const std::int32_t fh = mulQ12(forward[i], kSinH);
            const std::int32_t rh = mulQ12(right[i], kCosH);
            const std::int32_t fv = mulQ12(forward[i], kSinV);
            const std::int32_t uv = mulQ12(up[i], kCosV);
            planes[0].normal[i] = fh + rh; // left edge: inside is to its right
            planes[1].normal[i] = fh - rh;
            planes[2].normal[i] = fv + uv; // bottom
            planes[3].normal[i] = fv - uv;
            planes[4].normal[i] = -forward[i]; // far

            // A half screen ends at the middle column, which is straight
            // ahead: the plane through the eye whose normal is `right`. It is
            // tilted two degrees past the middle, for the same reason the
            // outer planes are loose.
            const std::int32_t fm = mulQ12(forward[i], kSinMid);
            const std::int32_t rm = mulQ12(right[i], kCosMid);
            if (half == ViewHalf::Left)
                planes[1].normal[i] = fm - rm; // right edge moves to the middle
            else if (half == ViewHalf::Right)
                planes[0].normal[i] = fm + rm;
        }
        for (int p = 0; p < 4; ++p)
            planes[p].dist = dotPos(planes[p].normal, pos);
        planes[4].dist = dotPos(planes[4].normal, pos) - (kFarUnits << 12);
    }

    void Camera::apply() const
    {
        glMatrixMode(GL_MODELVIEW);
        glLoadMatrix4x3(&view);
    }
} // namespace hlrt
