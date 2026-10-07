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
        /// The most actors drawn in one pass, nearest first.
        static constexpr int kMaxDrawn = 8;

        /// The most of a pass that models may take between them, what the
        /// player holds included. The world gets the rest -- which is all of
        /// it where there is nobody about.
        static constexpr int kMaxPolygons = 1200;
        static constexpr int kMaxVertices = 3400;

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

    private:
        /// `turn` is the actor's own turning: its nine, row by row, Q12
        /// (world = turn * model).
        void drawActor(const hldata::ActorState &actor, const hldata::ModelView &model,
                       unsigned modelIndex, const hldata::fx *turn, int id);
        const hldata::BoneMatrix *posed(const hldata::ActorState &actor,
                                        const hldata::ModelView &model, unsigned modelIndex,
                                        int id);
        void lightOn();
        void lightOff();

        int order_[kMaxDrawn] = {};
        int found_ = 0;
        int cut_ = 0;
        bool heldDrawn_ = false;
    };

    extern ModelDraw modelDraw;
} // namespace hlrt
