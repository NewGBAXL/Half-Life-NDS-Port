#include "render/ModelDraw.hpp"

#include <nds.h>

#include <NEAMain.h>

#include "DebugLog.hpp"
#include "data/ModelCache.hpp"
#include "game/Game.hpp"
#include "game/Player.hpp"
#include "render/Camera.hpp"
#include "render/WorldDraw.hpp"

namespace hlrt
{
    ModelDraw modelDraw;

    namespace
    {
        using namespace hldata;

        /// Where the light on an actor comes from, in the world: from above
        /// and a little to one side, as most of the game's rooms are lit. Q12,
        /// unit length, the way the light TRAVELS.
        constexpr fx kLightDir[3] = {1024, 683, -3908};

        /// Both sides of a model's triangles are drawn. Its front faces are
        /// what the hardware calls back faces (the game winds them the other
        /// way from the level), so NEA_CULL_FRONT would be the saving -- but
        /// the hardware judges a triangle by its first three corners after
        /// projection, gets thin ones wrong, and what it gets wrong it throws
        /// away: bits of people went missing. Drawn both ways, the depth
        /// buffer does the hiding.
        constexpr NEA_CullingEnum kCull = NEA_CULL_NONE;
    } // namespace

    namespace
    {
        struct Posed
        {
            int id = -2; ///< the actor's place in the map's list; -1: what is held
            unsigned model = 0;
            int clip = 0;
            std::uint32_t frame = 0;
            fx slide = 0;
            std::uint32_t used = 0;
            BoneMatrix bones[kModelMaxBones];
        };
        Posed posedSlots[ModelDraw::kMaxDrawn + 1];
        std::uint32_t posedClock = 0;
    } // namespace

    void ModelDraw::forgetPoses()
    {
        for (Posed &slot : posedSlots)
            slot.id = -2;
    }

    const BoneMatrix *ModelDraw::posed(const ActorState &actor, const ModelView &model,
                                       unsigned modelIndex, int id)
    {
        // To the nearest whole frame of the clip: the game's clips are 15 to
        // 30 frames a second, so a pose serves two to four of the screen's.
        const std::uint32_t frame = (actor.frame + 0x80u) & ~0xFFu;
        // A barnacle's tongue is a bone let out (the game's bone controller
        // on its twelfth), to the nearest unit.
        const bool tongue = actor.kind == static_cast<std::uint8_t>(ActorKind::Barnacle);
        const fx slide = tongue ? -(actor.reach & ~(kFxOne - 1)) : 0;
        Posed *oldest = &posedSlots[0];
        for (Posed &slot : posedSlots)
        {
            if (slot.id == id && slot.model == modelIndex && slot.clip == actor.clip &&
                slot.frame == frame && slot.slide == slide)
            {
                slot.used = ++posedClock;
                return slot.bones;
            }
            if (slot.used < oldest->used)
                oldest = &slot;
        }
        const DebugLog::Perf::Scope scope(DebugLog::Section::Pose);
        Posed &slot = *oldest;
        slot.id = id;
        slot.model = modelIndex;
        slot.clip = actor.clip;
        slot.frame = frame;
        slot.slide = slide;
        slot.used = ++posedClock;
        const int bones = model.header->boneCount;
        if (actor.clip >= 0)
        {
            poseModel(model, actor.clip, frame, slot.bones, tongue ? 11 : -1, slide);
        }
        else
        {
            // No clip at all: every bone at the origin, unturned. (Only a
            // model cooked with no sequences gets here.)
            for (int b = 0; b < bones; ++b)
            {
                for (int i = 0; i < 9; ++i)
                    slot.bones[b].r[i] = (i % 4 == 0) ? (1 << kBoneRotShift) : 0;
                slot.bones[b].t[0] = slot.bones[b].t[1] = slot.bones[b].t[2] = 0;
            }
        }
        return slot.bones;
    }

    void ModelDraw::drawActor(const ActorState &actor, const ModelView &model, unsigned modelIndex,
                              const fx *turn, int id)
    {
        // The pose is kept: a model is drawn twice for one moment in
        // two-pass mode, and one that is holding still (a body, someone
        // waiting on a frame) is the same from frame to frame.
        const BoneMatrix *pose = posed(actor, model, modelIndex, id);
        const DebugLog::Perf::Scope scope(DebugLog::Section::Models);

        const int *view = camera.view.m; // row k: where world axis k goes, times 8; row 3: translation
        const int shift = 5 - model.header->vertexShift; // see below

        // The actor's turn and the view together, once: row m is where the
        // MODEL's axis m goes in view space (times 8, Q12, as the view's
        // rows are). And where the actor's own origin goes.
        //
        // As camera.apply does for the level: a Q12 unit is 1024 of the
        // level's vertex units.
        int turned[9];
        int base[3];
        for (int j = 0; j < 3; ++j)
        {
            for (int m = 0; m < 3; ++m)
                turned[m * 3 + j] = static_cast<int>(
                    (static_cast<std::int64_t>(turn[m]) * view[j] +
                     static_cast<std::int64_t>(turn[3 + m]) * view[3 + j] +
                     static_cast<std::int64_t>(turn[6 + m]) * view[6 + j]) >>
                    12);
            const std::int64_t sum = static_cast<std::int64_t>(actor.pos[0] >> 10) * view[0 + j] +
                                     static_cast<std::int64_t>(actor.pos[1] >> 10) * view[3 + j] +
                                     static_cast<std::int64_t>(actor.pos[2] >> 10) * view[6 + j];
            base[j] = static_cast<int>(sum >> 12) + view[9 + j];
        }

        const NEA_Material *skin = modelCache.skin(modelIndex);
        if (skin != nullptr)
            NEA_MaterialUse(skin);
        else
            GFX_TEX_FORMAT = 0;
        // Lit by the one light: how bright the actor stands sets its colour.
        const int bright = actor.light > 31 ? 31 : actor.light;
        GFX_LIGHT_COLOR = RGB15(bright, bright, bright); // light 0
        GFX_DIFFUSE_AMBIENT = RGB15(20, 20, 20) | (RGB15(13, 13, 13) << 16);
        GFX_SPECULAR_EMISSION = 0;

        for (int n = 0; n < model.header->batchCount; ++n)
        {
            const ModelBatch &batch = model.batches[n];
            if (!model.batchInBody(batch, actor.body))
                continue;
            for (int k = 0; k < batch.boneCount; ++k)
            {
                const BoneMatrix &bone = pose[batch.bones[k]];
                // The matrix the hardware multiplies a ROW vector by: row i is
                // where the bone's axis i goes in view space.
                //
                // A vertex is 1/128 unit (coarser by vertexShift); the view
                // matrix's rows take a level vertex (1/4 unit) to view space.
                // So a model vertex needs the same rows, 32 times smaller:
                // that is the ">> 5", less the vertexShift.
                //
                // The same rotation at full length (the view's rows are 8
                // long; ">> 3") is the matrix for NORMALS, which must not
                // shrink with the vertices or the model goes dark.
                //
                // The sums fit 32 bits: a bone's column is a unit vector in
                // Q14 and a row of `turned` one of length 8 in Q12, so their
                // product is at most 2^29.
                m4x3 position, direction;
                for (int i = 0; i < 3; ++i)
                {
                    for (int j = 0; j < 3; ++j)
                    {
                        const int sum = bone.r[i] * turned[j] + bone.r[3 + i] * turned[3 + j] +
                                        bone.r[6 + i] * turned[6 + j]; // Q14 * (Q12 * 8)
                        position.m[i * 3 + j] = sum >> (kBoneRotShift + shift);
                        direction.m[i * 3 + j] = sum >> (kBoneRotShift + 3);
                    }
                }
                for (int j = 0; j < 3; ++j)
                {
                    // The bone's origin: Q12 units against rows that are for
                    // 1/1024ths of one.
                    const std::int64_t sum = static_cast<std::int64_t>(bone.t[0]) * turned[j] +
                                             static_cast<std::int64_t>(bone.t[1]) * turned[3 + j] +
                                             static_cast<std::int64_t>(bone.t[2]) * turned[6 + j];
                    position.m[9 + j] = base[j] + static_cast<int>(sum >> 22);
                    direction.m[9 + j] = 0;
                }
                // Both matrices at once takes the directions; then the
                // positions alone are replaced; the slot keeps the pair.
                MATRIX_CONTROL = GL_MODELVIEW;
                glLoadMatrix4x3(&direction);
                MATRIX_CONTROL = GL_POSITION;
                glLoadMatrix4x3(&position);
                MATRIX_STORE = static_cast<std::uint32_t>(kModelFirstSlot + k);
            }
            MATRIX_CONTROL = GL_MODELVIEW;
            const std::uint32_t *word = model.gx + batch.gxOfs;
            for (int count = batch.gxWords; count > 0; --count)
                GFX_FIFO = *word++;
        }
    }

    void ModelDraw::plan(int &polygons, int &vertices)
    {
        found_ = 0;
        cut_ = 0;
        heldDrawn_ = false;
        polygons = 0;
        vertices = 0;
        int &found = found_;
        int &cut = cut_;
        int *order = order_;
        const ActorWorld &actors = game.actors;

        // Who can be seen, nearest first.
        std::int32_t distance[kMaxDrawn];
        for (int i = 0; actors.states != nullptr && i < actors.count; ++i)
        {
            const ActorState &actor = actors.states[i];
            const ModelView *model = actor.active ? actorModel(actors, actor) : nullptr;
            if (model == nullptr)
                continue;
            const int radius = model->header->radius;
            // A ball round its middle, which for something standing is about
            // half its height up.
            const std::int16_t centre[3] = {
                static_cast<std::int16_t>(fxToInt(actor.pos[0])),
                static_cast<std::int16_t>(fxToInt(actor.pos[1])),
                static_cast<std::int16_t>(fxToInt(actor.pos[2]) + (model->header->maxs[2] >> 1))};
            // Seen if the leaf its middle is in can be seen, or the one its
            // head is in: someone standing in a doorway has their middle in
            // the frame and their head in the room.
            const std::int16_t top[3] = {
                centre[0], centre[1],
                static_cast<std::int16_t>(fxToInt(actor.pos[2]) + model->header->maxs[2])};
            if (!worldDraw.ballVisible(centre, radius) && !worldDraw.ballVisible(top, radius))
                continue;
            const std::int32_t dx = fxToInt(actor.pos[0] - camera.pos[0]);
            const std::int32_t dy = fxToInt(actor.pos[1] - camera.pos[1]);
            const std::int32_t dz = fxToInt(actor.pos[2] - camera.pos[2]);
            const std::int32_t far = (dx < 0 ? -dx : dx) + (dy < 0 ? -dy : dy) + (dz < 0 ? -dz : dz);
            int at = found < kMaxDrawn ? found : kMaxDrawn - 1;
            if (found >= kMaxDrawn && far >= distance[at])
            {
                ++cut;
                continue;
            }
            if (found >= kMaxDrawn)
                ++cut; // the furthest so far gives up its place
            while (at > 0 && distance[at - 1] > far)
            {
                order[at] = order[at - 1];
                distance[at] = distance[at - 1];
                --at;
            }
            order[at] = i;
            distance[at] = far;
            if (found < kMaxDrawn)
                ++found;
        }

        // What the player holds comes first: it is always in view.
        const ModelView *held = game.held.active && !player.noclip ? actorModel(actors, game.held) : nullptr;
        if (held != nullptr)
        {
            for (int b = 0; b < held->header->batchCount; ++b)
            {
                polygons += held->batches[b].polygons;
                vertices += held->batches[b].vertices;
            }
            heldDrawn_ = true;
        }
        // Then the actors, nearest first, for as long as they fit in the
        // share of a pass that models may have: the rest are not drawn this
        // frame.
        int kept = 0;
        for (int n = 0; n < found; ++n)
        {
            const ActorState &actor = actors.states[order[n]];
            const ModelView &model = *actorModel(actors, actor);
            int p = 0, v = 0;
            for (int b = 0; b < model.header->batchCount; ++b)
            {
                if (!model.batchInBody(model.batches[b], actor.body))
                    continue;
                p += model.batches[b].polygons;
                v += model.batches[b].vertices;
            }
            // (Under pressure the share is less -- WorldDraw::pressure -- but
            // whoever is nearest is always let in.)
            const int share = 4 - (worldDraw.pressure() > 2 ? 2 : worldDraw.pressure());
            if (polygons + p > kMaxPolygons ||
                vertices + v > kMaxVertices ||
                (kept > 0 && (polygons + p > kMaxPolygons * share / 4 ||
                              vertices + v > kMaxVertices * share / 4)))
            {
                ++cut;
                continue;
            }
            polygons += p;
            vertices += v;
            order[kept++] = order[n];
        }
        found = kept;
    }

    void ModelDraw::prepare()
    {
        const ActorWorld &actors = game.actors;
        for (int n = 0; n < found_; ++n)
        {
            const ActorState &actor = actors.states[order_[n]];
            posed(actor, *actorModel(actors, actor), actor.model, order_[n]);
        }
        const ActorState &held = game.held;
        const ModelView *model = held.active ? actorModel(game.actors, held) : nullptr;
        if (model != nullptr && heldDrawn_)
            posed(held, *model, held.model, -1);
    }

    void ModelDraw::draw(int &drawn, int &cut)
    {
        const ActorWorld &actors = game.actors;
        cut += cut_;
        if (found_ == 0)
            return;
        const int found = found_;
        const int *order = order_;

        lightOn();

        for (int n = 0; n < found; ++n)
        {
            const ActorState &actor = actors.states[order[n]];
            const ModelView &model = *actorModel(actors, actor);
            // Turned about the vertical, the way it faces.
            fx sine, cosine;
            fxSinCos(actor.yaw, sine, cosine);
            const fx turn[9] = {cosine, -sine, 0, sine, cosine, 0, 0, 0, kFxOne};
            drawActor(actor, model, actor.model, turn, order[n]);
            ++drawn;
        }
        lightOff();
    }

    void ModelDraw::lightOn()
    {
        // One light, set in view space: with the direction matrix at identity
        // the hardware takes the vector as it is given.
        MATRIX_CONTROL = GL_MODELVIEW;
        MATRIX_IDENTITY = 0;
        {
            const int *view = camera.view.m;
            int v[3];
            for (int j = 0; j < 3; ++j)
            {
                // The view's rows are 8 long: >> 15 leaves Q12; a light
                // vector is 10 bits, 1.0 = 512 (and may not quite be 1).
                const std::int64_t sum = static_cast<std::int64_t>(kLightDir[0]) * view[0 + j] +
                                         static_cast<std::int64_t>(kLightDir[1]) * view[3 + j] +
                                         static_cast<std::int64_t>(kLightDir[2]) * view[6 + j];
                v[j] = static_cast<int>(sum >> (15 + 3));
                v[j] = v[j] > 511 ? 511 : (v[j] < -511 ? -511 : v[j]);
            }
            GFX_LIGHT_VECTOR = (v[0] & 0x3FF) | ((v[1] & 0x3FF) << 10) | ((v[2] & 0x3FF) << 20);
        }
        NEA_PolyFormat(31, 0, NEA_LIGHT_0, kCull, worldDraw.polygonMode());
    }

    void ModelDraw::lightOff()
    {
        // As it was: the camera's matrix, no lights, the level's polygons.
        camera.apply();
        NEA_PolyFormat(31, 0, static_cast<NEA_LightEnum>(0), WorldDraw::kCull, worldDraw.polygonMode());
        GFX_COLOR = 0x7FFF;
    }

    void ModelDraw::drawHeld()
    {
        const ActorState &held = game.held;
        const ModelView *model = held.active ? actorModel(game.actors, held) : nullptr;
        if (model == nullptr || !heldDrawn_)
            return;
        // At the eye, turned as the view is: the model's forward, left and up
        // are the camera's.
        ActorState at = held;
        for (int a = 0; a < 3; ++a)
            at.pos[a] = camera.pos[a];
        const fx turn[9] = {camera.forward[0], -camera.right[0], camera.up[0],
                            camera.forward[1], -camera.right[1], camera.up[1],
                            camera.forward[2], -camera.right[2], camera.up[2]};
        lightOn();
        drawActor(at, *model, held.model, turn, -1);
        lightOff();
    }
} // namespace hlrt
