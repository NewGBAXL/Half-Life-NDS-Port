#pragma once

// What is seen for a moment in a fight: an alien slave's bolt, spit and
// grenades in the air, a houndeye's blast, an explosion.
//
// Plain shapes in flat colour, facing the camera -- a handful of polygons a
// frame, with no textures (the game's own sprites are not converted). The
// things themselves are the game's (ActorWorld::effects and ::shots, kept
// by shared/HlMonsters.hpp); this only draws them.

namespace hlrt
{
    class EffectDraw
    {
    public:
        /// Draw them all. Call inside the frame, after the models and the
        /// brushes; leaves the modelview as the camera's, no texture bound
        /// and the level's polygon format.
        void draw();
    };

    extern EffectDraw effectDraw;
} // namespace hlrt
