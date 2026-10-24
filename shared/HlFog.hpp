#pragma once

// What the player may say about how the picture is drawn: the fog, and how
// much detail is given up for speed. The numbers each choice stands for are
// here, where the host tests can see them; who uses them is the console's
// renderer (WorldDraw, ModelDraw, Camera).
//
// The fog is the hardware's: by depth, in 32 steps of density over a band,
// towards one colour. It was only ever there to hide where a view too heavy
// to draw is cut short, and was solid black up to the cut -- a wall. Now it
// is the player's: off, there when a view is cut ("when needed"), or always
// there at a distance of their choosing; as solid as they like at its far
// end; and in a colour.
//
// std-only and inline.

#include <cstdint>

namespace hldata
{
    inline constexpr std::uint8_t kFogOff = 0;
    inline constexpr std::uint8_t kFogWhenNeeded = 1;
    inline constexpr std::uint8_t kFogAlways = 2;
    inline constexpr int kFogTypes = 3;
    inline constexpr int kFogColours = 5;
    inline constexpr int kDetailLevels = 3; ///< low, normal, high
    inline constexpr int kDrawDistances = 3; ///< far, medium, near
    inline constexpr int kGraphicsMost = 10; ///< a slider's top

    struct GraphicsSettings
    {
        std::uint8_t fog = kFogWhenNeeded;
        std::uint8_t fogStrength = 7; ///< 0..10: how solid it is at its far end
        std::uint8_t fogDistance = 5; ///< 0..10: how far off (see fogAlwaysEnd, fogCutEnd)
        std::uint8_t fogColour = 0;   ///< into kFogColour
        std::uint8_t modelDetail = 1; ///< 0 low, 1 normal, 2 high
        std::uint8_t worldDetail = 1;
        std::uint8_t modelLimit = 0;  ///< how many models at once; 0: as many as the machine is good for
        std::uint8_t drawDistance = 0; ///< 0 far, 1 medium, 2 near

        constexpr bool valid() const
        {
            return fog < kFogTypes && fogStrength <= kGraphicsMost && fogDistance <= kGraphicsMost &&
                   fogColour < kFogColours && modelDetail < kDetailLevels && worldDetail < kDetailLevels &&
                   (modelLimit == 0 || (modelLimit >= 4 && modelLimit <= 12)) && drawDistance < kDrawDistances;
        }
    };

    /// The colours the fog may be (five bits each of red, green, blue), and
    /// what the menu calls them.
    inline constexpr std::uint16_t kFogColour[kFogColours] = {
        0, 10 | 10 << 5 | 11 << 10, 6 | 9 << 5 | 14 << 10, 12 | 9 << 5 | 6 << 10, 5 | 10 << 5 | 6 << 10};
    inline constexpr const char *const kFogColourName[kFogColours] = {"black", "grey", "blue", "brown", "green"};
    inline constexpr const char *const kFogTypeName[kFogTypes] = {"off", "when needed", "always"};
    inline constexpr const char *const kDetailName[kDetailLevels] = {"low", "normal", "high"};
    inline constexpr const char *const kDrawDistanceName[kDrawDistances] = {"far", "medium", "near"};

    /// The hardware's 32 densities (0..127) for a fog of `strength` (0..10):
    /// none at its near edge, `strength` tenths of whole at its far one,
    /// coming on slowly at first (a curve: a straight ramp reads as a wall).
    inline constexpr void fogTable(int strength, std::uint8_t *out)
    {
        strength = strength < 0 ? 0 : (strength > kGraphicsMost ? kGraphicsMost : strength);
        const int top = (127 * strength) / kGraphicsMost;
        for (int i = 0; i < 32; ++i)
            out[i] = static_cast<std::uint8_t>((top * i * i) / (31 * 31));
    }

    /// How far the world is drawn at all (units).
    inline constexpr int drawDistanceUnits(int setting)
    {
        return setting == 2 ? 1536 : (setting == 1 ? 3072 : 8192);
    }
    /// ... and on a map that can be seen no farther than `mapFar` (the
    /// level's LevelHeader::farUnits; 0: not known), no farther than that.
    inline constexpr int drawDistanceFor(int setting, int mapFar)
    {
        const int far = drawDistanceUnits(setting);
        return mapFar > 0 && mapFar < far ? mapFar : far;
    }

    /// Where a fog that is always there is at its most (units): from 768
    /// to 6144 by `distance` (0..10), and never past what is drawn.
    inline constexpr int fogAlwaysEnd(int distance, int drawDistance, int mapFar = 0)
    {
        distance = distance < 0 ? 0 : (distance > kGraphicsMost ? kGraphicsMost : distance);
        const int end = 768 + (distance * (6144 - 768)) / kGraphicsMost;
        const int far = drawDistanceFor(drawDistance, mapFar);
        return end > far ? far : end;
    }

    /// Where the fog that hides a cut is at its most, for a view cut off
    /// `cut` units away: from seven eighths of the way there to all of it,
    /// by `distance` (0..10) -- the farther, the less of the view is lost
    /// to fog and the harder the edge behind it. Never nearer than 192.
    inline constexpr int fogCutEnd(int cut, int distance)
    {
        distance = distance < 0 ? 0 : (distance > kGraphicsMost ? kGraphicsMost : distance);
        const int end = (cut * (280 + distance * 4)) / 320;
        return end < 192 ? 192 : end;
    }

    /// Model detail: a model may be drawn at a coarser level while that
    /// level's error (sixteenths of a unit) times this is no more than its
    /// distance (units) times 6 -- 16 is the rule the levels were made
    /// for; low gives up detail at half the distance, high at twice.
    inline constexpr int modelDetailWeight(int setting)
    {
        return setting == 0 ? 8 : (setting == 2 ? 32 : 16);
    }

    /// World detail: under `pressure` (0..3), something smaller than its
    /// distance over this is left out; 0: nothing is. Low leaves the
    /// smallest out even with no pressure; high never leaves anything out
    /// (and the view is cut shorter instead).
    inline constexpr int worldDetailRatio(int setting, int pressure)
    {
        constexpr int kRatio[kDetailLevels][4] = {{64, 40, 26, 16}, {0, 48, 32, 20}, {0, 0, 0, 0}};
        return kRatio[setting < 0 || setting >= kDetailLevels ? 1 : setting][pressure < 0 ? 0 : (pressure > 3 ? 3 : pressure)];
    }
} // namespace hldata
