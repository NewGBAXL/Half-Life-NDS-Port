#pragma once

// Lightmaps to vertex colours.
//
// GoldSrc lights a face with a small texture, one RGB sample every 16 units.
// The DS has no second texture stage and nowhere near the texture memory to
// bake light into the first, so each vertex takes the lightmap's value at that
// vertex and the hardware interpolates across the polygon. Faces are at most
// 240 units across (the map compiler splits them for the lightmap's sake), so
// what is lost is detail inside a face, not the overall light.
//
// LIGHT STYLES. A face carries up to four lightmap layers, one per style. Style
// 0 is plain light. 1..31 flicker or pulse and are baked as if steady.
// 32 and up belong to lights a button switches: those are baked in when the
// light starts on and left out when it starts off. Changing them at run time
// is what the level format's LightPatch section is reserved for.

#include <cstdint>
#include <vector>

#include "hlds/BspFile.hpp"
#include "hlds/EntityParser.hpp"

namespace hlds
{
    struct Rgb
    {
        std::uint8_t r = 0, g = 0, b = 0;
    };

    /// A face's lightmap rectangle in texture space (xash3d's
    /// Mod_CalcSurfaceExtents): which 16-texel sample its corner sits on, and how
    /// many samples it spans.
    struct LightExtents
    {
        int minS = 0, minT = 0;   ///< in samples
        int width = 0, height = 0; ///< in samples
    };

    class LightBake
    {
    public:
        /// The curve applied to every baked colour: out = in^(1/gamma). Raw
        /// lightmap values are meant to go through the game's own gamma ramp
        /// and look far too dark without one.
        static constexpr double kDefaultGamma = 1.8;

        LightBake(const BspFile &bsp, const std::vector<Entity> &entities,
                  double gamma = kDefaultGamma);

        LightExtents extents(const BspFace &face) const;
        /// A face's texture coordinates at vertex i, in source texels.
        void texCoords(const BspFace &face, int i, double &s, double &t) const;
        /// The light at texture coordinate (s, t) of `face`, gamma applied.
        Rgb sample(const BspFace &face, double s, double t) const;

        bool styleIsOn(int style) const;

    private:
        const BspFile &bsp_;
        std::vector<char> styleOff_; ///< by style number; 1 = switched off at map start
        std::uint8_t gammaTable_[256];
    };
} // namespace hlds
