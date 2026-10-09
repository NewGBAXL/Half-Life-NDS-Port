#pragma once

// The game's sprites -- glows, flashes, explosions, the pictures beams are
// made of -- as the DS draws them.
//
// A sprite is cooked to ONE texture: its frames side by side in rows (an
// atlas), each shrunk to a few dozen texels, in the A3I5 format: 32 colours
// and 8 levels of see-through. The level's Sprites section (SpriteRef) says
// how the atlas is cut up and how big the sprite is in the world.
//
// MOST OF THE GAME'S SPRITES ARE "ADDITIVE": drawn by adding their colour to
// what is behind, so that black is nothing and white is a glare. The DS
// cannot add. They are cooked so that how bright a texel was becomes how
// solid it is, and its colour is made as bright as it will go: a glow is then
// a patch of its own colour, see-through at its edges. Against something
// dark -- nearly everything in this game -- that looks the same; against
// something bright it dims a little where the game would have flared.
//
// The file: <card>/_nds/hl_nds/data/spr/<name>.<w>x<h>.htx, a cooked texture
// (HlTexture.hpp) like any other.

#include <cstdint>
#include <string>

namespace hldata
{
    // SpriteRef::kind: which way the picture faces (the game's sprite types).
    inline constexpr std::uint8_t kSpriteFacesEye = 0;  ///< turned to the eye: nearly all of them
    inline constexpr std::uint8_t kSpriteUpright = 1;   ///< turned to the eye about the vertical only
    inline constexpr std::uint8_t kSpriteOriented = 2;  ///< lies as its entity's angles say

    /// The sprites the port's own effects are drawn with: every map's
    /// Sprites section begins with these, in this order.
    enum class CommonSprite : std::uint16_t
    {
        Explosion, ///< a grenade's, and env_explosion's
        Muzzle,    ///< at the end of a gun that fires
        Blood,     ///< where a shot lands on something that bleeds
        Glow,      ///< the big turret's eye; a spark
        Beam,      ///< a turret's tracer, a laser
        Lightning, ///< an alien slave's bolt
        Spit,      ///< a bullsquid's
        Shock,     ///< a houndeye's blast
        Count
    };

    /// Where each is in the game's folder, and whether it is drawn as light
    /// whatever its file says (a beam's picture always is).
    inline const char *commonSpritePath(CommonSprite sprite)
    {
        static const char *const kPaths[] = {"sprites/zerogxplode.spr", "sprites/muzzleflash1.spr",
                                             "sprites/bloodspray.spr",  "sprites/flare3.spr",
                                             "sprites/laserbeam.spr",   "sprites/lgtning.spr",
                                             "sprites/bigspit.spr",     "sprites/shockwave.spr"};
        static_assert(sizeof(kPaths) / sizeof(kPaths[0]) == static_cast<std::size_t>(CommonSprite::Count));
        return kPaths[static_cast<int>(sprite)];
    }

    inline std::string spriteFile(const char *name, int width, int height)
    {
        return std::string(name) + "." + std::to_string(width) + "x" + std::to_string(height) + ".htx";
    }
} // namespace hldata
