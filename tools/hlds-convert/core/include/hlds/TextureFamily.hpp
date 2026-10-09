#pragma once

// The game's textures that are more than one picture, by its own naming:
//
//   +0name .. +9name   a cycle, ten pictures a second;
//   +aname .. +jname   the same thing's other face -- what a brush entity
//                      shows when its "frame" is set (a light off, a button
//                      in, a charger empty) -- which may be a cycle too;
//   -0name .. -9name   variants of one texture: each face wears one, chosen
//                      by where the face is, so that a big floor does not
//                      visibly repeat.
//
// A map's texture list has every member of a family any of its faces uses.

#include <vector>

#include "hlds/BspFile.hpp"

namespace hlds
{
    struct TextureFamily
    {
        std::vector<int> frames; ///< the cycle, in order: indices into the map's textures
        std::vector<int> other;  ///< the other face's cycle; empty if it has none
    };

    /// The family of texture `miptex`. For a texture that is in none: itself
    /// alone. A texture named as some other picture of a cycle than its
    /// first stands for the whole cycle from its first; one named as an
    /// other-face picture is that cycle, with no other face of its own.
    TextureFamily textureFamily(const BspFile &bsp, int miptex);

    /// The texture a face wears: what its texinfo names, or -- for a '-'
    /// texture -- the variant of that family chosen for this face.
    int wornTexture(const BspFile &bsp, const BspFace &face);

    /// Whether '-' textures are varied face by face (they are, unless this
    /// is told otherwise: every variant is a texture to hold, and faces
    /// that wear different ones cannot be merged). For the whole process.
    void setRandomTiling(bool on);
} // namespace hlds
