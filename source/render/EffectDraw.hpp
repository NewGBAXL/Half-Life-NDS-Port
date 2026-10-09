#pragma once

// What is seen for a moment in a fight: an alien slave's bolt, spit and
// grenades in the air, a houndeye's blast, an explosion.
//
// Plain shapes in flat colour, facing the camera -- a handful of polygons a
// frame, with no textures (the game's own sprites are not converted). The
// things themselves are the game's (ActorWorld::effects and ::shots, kept
// by shared/HlMonsters.hpp); this only draws them.

#include <cstdint>

#include "HlFixed.hpp"

namespace hlrt
{
    class EffectDraw
    {
    public:
        /// List what there is to draw this pass, farthest first, and add
        /// what it will cost. Call once the parts of the map in view are
        /// known, before the world is chosen.
        void gather(int &polygons, int &vertices);
        /// How many things are in the list (the glows the models asked for
        /// since gather() among them). Then each is drawn by its place,
        /// under the polygon ID given; and end() when all are.
        int begin();
        std::int32_t distance(int n) const { return items_[n].distance; }
        void drawItem(int n, int id);
        /// Leaves the modelview as the camera's, no texture bound and the
        /// level's polygon format.
        void end();

        /// The marks on the walls in view (ActorWorld::decals): each a flat
        /// picture in its wall's plane, drawn only where the wall's own
        /// depth is. After everything solid, before what is see-through.
        /// Returns how many it drew.
        int drawDecals();
        /// What the marks may cost a pass, at most.
        static constexpr int kDecalPolygons = 24;

        /// A glow to be drawn with this frame's effects: at `at`, `alpha`
        /// solid (1..31), `half` units across either way. For whoever draws
        /// something that glows somewhere only it knows (a turret's eye).
        void glow(const hldata::fx *at, int alpha, std::uint16_t colour, int half);

    private:
        struct Glow
        {
            hldata::fx at[3];
            std::uint16_t colour;
            std::uint8_t alpha, half;
        };
        Glow glows_[8];
        int glowCount_ = 0;

        static constexpr int kMaxItems = 96;
        enum : std::uint8_t { kRecord, kShot, kEffect, kGlow, kMine, kPiece };
        struct Item
        {
            std::int32_t distance;
            std::uint16_t index;
            std::uint8_t type;
        };
        Item items_[kMaxItems];
        int count_ = 0;
        void drawRecord(int index);
        void drawShot(int index);
        void drawEffect(int index);
        void drawGlow(int index);
        void drawMine(int index);
        void drawPiece(int index);
    };

    extern EffectDraw effectDraw;
} // namespace hlrt
