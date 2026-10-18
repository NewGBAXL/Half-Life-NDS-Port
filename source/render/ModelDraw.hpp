#pragma once

// Drawing the map's actors: models posed on the CPU and skinned by the
// hardware.
//
// Every vertex of a Half-Life model belongs to one bone, and a model's cooked
// stream is "use the matrix in slot N" followed by the vertices of that bone.
// So drawing an actor is: work out, for each bone, the one matrix that takes
// a vertex in that bone's space all the way to view space; put those in the
// matrix stack's slots; send the stream.
//
// That matrix is built here in 64-bit arithmetic -- pose, then the actor's
// place and facing, then the camera -- rather than by asking the hardware to
// multiply three together, because the last of them has small numbers in it
// (a model's vertex is in 128ths of a unit, the view's unit is 128 units) and
// the hardware's 12 fractional bits would not leave enough of them.
//
// Not in ITCM: this is a few actors a frame, not thousands of surfaces.

#include "HlActors.hpp"
#include "HlAnim.hpp"

namespace hlrt
{
    class ModelDraw
    {
    public:
        /// The most actors that can be drawn in one pass, nearest first; and
        /// how many may be on this machine (setLimit): what there is time to
        /// pose, not what there is room to draw.
        static constexpr int kMaxDrawn = 16;
        int cutOf(int kind) const { return cutClass_[kind & 3]; }
        int grown() const { return grown_; }
        bool strained() const { return strained_; }
        void setLimit(int limit) { limit_ = limit < 1 ? 1 : (limit > kMaxDrawn ? kMaxDrawn : limit); }
        int limit() const { return limit_; }

        /// The most of a pass that models may take between them, what the
        /// player holds included. The world gets the rest -- which is all of
        /// it where there is nobody about.
        static constexpr int kMaxPolygons = 1200;
        static constexpr int kMaxVertices = 3400;
        /// A model whose finest level is of more polygons than this has the
        /// game's own mesh for it (a DSi's file): for the nearest model in
        /// a frame with room, and nobody else.
        static constexpr int kWholeModel = 420;

        /// Decide what will be drawn this pass -- what is held, and the
        /// actors the eye can see, nearest first, as many as fit -- and say
        /// what it costs. Call once the visible leaves are known and BEFORE
        /// the world is chosen: the world is given what is left.
        void plan(int &polygons, int &vertices);
        /// Work out the poses of what plan() chose, without sending anything:
        /// for the time the geometry engine is busy with something else.
        /// draw() and drawHeld() then find them done.
        void prepare();
        /// Draw the actors plan() chose. `drawn` and `cut` are added to.
        /// Leaves the modelview as the camera's and lighting off.
        void draw(int &drawn, int &cut);
        /// Draw what the player holds, at the eye and turned as the view is.
        /// Call last: it is nearer than anything.
        void drawHeld();
        /// The kept poses are of this map's models: forget them with it.
        void forgetPoses();
        /// For the readout: the levels the actors are drawn at this pass, a
        /// digit each, nearest first.
        void levelsText(char *out, int size) const;

    private:
        /// `turn` is the actor's own turning: its nine, row by row, Q12
        /// (world = turn * model).
        /// `level`: which of the model's levels of detail.
        void drawActor(const hldata::ActorState &actor, const hldata::ModelView &model,
                       unsigned modelIndex, const hldata::fx *turn, int id, int level);
        const hldata::BoneMatrix *posed(const hldata::ActorState &actor,
                                        const hldata::ModelView &model, unsigned modelIndex,
                                        int id);
        void lightOn();
        void lightOff();

        int order_[kMaxDrawn] = {};
        /// The level of detail each of those is drawn at this pass, and the
        /// one what is held is.
        std::uint8_t level_[kMaxDrawn] = {};
        std::uint8_t heldLevel_ = 0;
        /// What each actor was last drawn at, one more than the level (0:
        /// not lately), by its number: a model near the distance where two
        /// levels meet keeps the one it has.
        std::uint8_t lastLevel_[256] = {};
        int found_ = 0;
        int limit_ = 8;
        /// The pieces of what has come apart that plan() chose (into
        /// ActorWorld::gibs): after the actors, in what room they left.
        std::uint8_t gibOrder_[hldata::kMaxGibs] = {};
        int gibFound_ = 0;
        int cut_ = 0;
        /// ... by what they were (HlDrawPlan.hpp's kDraw*); how far the
        /// models went over their share for an enemy's sake; and whether
        /// one had to be made room for at all, at which the frame's
        /// effects give way.
        int cutClass_[4] = {};
        int grown_ = 0;
        bool strained_ = false;
        bool heldDrawn_ = false;
    };

    extern ModelDraw modelDraw;
} // namespace hlrt
