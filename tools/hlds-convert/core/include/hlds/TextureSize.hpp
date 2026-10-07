#pragma once

// How big a texture is on the DS.
//
// The geometry engine only takes power-of-two textures, and the whole of a
// map's world textures has to fit one compressed-texture bank pair (about half
// a million texels), so nothing is kept at the size the game shipped it. This
// is the starting size; the budget pass may halve it further.
//
// Geometry depends on the answer -- texture coordinates are in cooked texels --
// so the sizes are settled before any face is built.

#include <cstdint>

namespace hlds
{
    struct TextureSize
    {
        std::uint16_t width = 0;
        std::uint16_t height = 0;
    };

    inline constexpr int kMinTextureSide = 16;
    inline constexpr int kMaxTextureSide = 128;

    /// Half the source size, each axis rounded to the nearest power of two (in
    /// ratio, so 48 goes up to 64 and 40 down to 32) and clamped to
    /// kMinTextureSide..kMaxTextureSide.
    TextureSize defaultTextureSize(std::uint32_t sourceWidth, std::uint32_t sourceHeight);
} // namespace hlds
