#include "render/ModelDraw.hpp"

#include <nds.h>

#include <NEAMain.h>

#include "DebugLog.hpp"
#include "HlFog.hpp"
#include "HlLamp.hpp"
#include "Settings.hpp"
#include "data/ModelCache.hpp"
#include "game/Game.hpp"
#include "game/Player.hpp"
#include "render/Camera.hpp"
#include "render/EffectDraw.hpp"
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
            std::int32_t controls[2] = {0, 0}; ///< what its bone controllers are set to
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
        // What the game sets by hand on top of the clip: a barnacle's tongue
        // let out, a turret's gun turned (HlActors.hpp).
        std::int32_t controls[kModelControls];
        const bool controlled = actorControls(actor, controls);
        Posed *oldest = &posedSlots[0];
        for (Posed &slot : posedSlots)
        {
            if (slot.id == id && slot.model == modelIndex && slot.clip == actor.clip &&
                slot.frame == frame && slot.controls[0] == controls[0] &&
                slot.controls[1] == controls[1])
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
        slot.controls[0] = controls[0];
        slot.controls[1] = controls[1];
        slot.used = ++posedClock;
        const int bones = model.header->boneCount;
        if (actor.clip >= 0)
        {
            poseModel(model, actor.clip, frame, slot.bones, controlled ? controls : nullptr);
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

    void ModelDraw::levelsText(char *out, int size) const
    {
        int n = 0;
        for (; n < found_ && n + 1 < size; ++n)
            out[n] = static_cast<char>('0' + level_[n]);
        if (size > 0)
            out[n] = '\0';
    }

    void ModelDraw::drawActor(const ActorState &actor, const ModelView &model, unsigned modelIndex,
                              const fx *turn, int id, int level)
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
        // The lamp is a light of the hardware's own on whatever is in its
        // beam -- the second: it shines the way the view looks -- and what
        // it falls on shines back a little where it is turned to the eye
        // (the material's specular colour, by the shininess table). Not on
        // what is in hand, which is behind it.
        int lamp = 0;
        if (game.lamp.on && id != -1)
        {
            // By how near it is and how near the beam's middle (its chest,
            // more or less, is what counts).
            const int d[3] = {fxToInt(actor.pos[0] - camera.pos[0]), fxToInt(actor.pos[1] - camera.pos[1]),
                              fxToInt(actor.pos[2] - camera.pos[2]) + 36};
            const int along = (d[0] * camera.forward[0] + d[1] * camera.forward[1] + d[2] * camera.forward[2]) >> 12;
            if (along > 0 && along < kLampReach)
            {
                const int square = d[0] * d[0] + d[1] * d[1] + d[2] * d[2] - along * along;
                lamp = lampOn(along, static_cast<int>(isqrt64(static_cast<std::uint64_t>(square > 0 ? square : 0))));
            }
        }
        GFX_LIGHT_COLOR = RGB15(bright, bright, bright); // light 0
        GFX_LIGHT_COLOR = RGB15(lamp, lamp * 30 / 31, lamp * 25 / 31) | (1u << 30); // light 1
        GFX_DIFFUSE_AMBIENT = RGB15(20, 20, 20) | (RGB15(13, 13, 13) << 16);
        // (Bit 15: the specular light goes by the shininess table.)
        GFX_SPECULAR_EMISSION = lamp > 0 ? (RGB15(lamp * 2 / 3, lamp * 2 / 3, lamp * 5 / 9) | 0x8000u) : 0u;

        for (int n = 0; n < model.header->batchCount; ++n)
        {
            const ModelBatch &batch = model.batches[n];
            if (!model.batchInBody(batch, actor.body, actor.skin, level))
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

        // The big turret's eye: a red glow where the model has it (its
        // second attachment, which turns with the gun), as bright as the
        // turret says. Drawn with the effects.
        fx eye[3];
        if (actor.kind == static_cast<std::uint8_t>(ActorKind::Turret) && actor.reach > 0 &&
            attachmentPoint(model, pose, 1, eye))
        {
            fx at[3];
            for (int k = 0; k < 3; ++k)
                at[k] = actor.pos[k] + fxMul(turn[k * 3], eye[0]) + fxMul(turn[k * 3 + 1], eye[1]) +
                        fxMul(turn[k * 3 + 2], eye[2]);
            effectDraw.glow(at, static_cast<int>(actor.reach) * 31 / 255, RGB15(31, 2, 2), 12);
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
        // How many at once: the player's say, or what the machine is good
        // for.
        const int limit = settings.graphics.modelLimit != 0
                              ? (settings.graphics.modelLimit > kMaxDrawn ? kMaxDrawn : settings.graphics.modelLimit)
                              : limit_;
        int *order = order_;
        const ActorWorld &actors = game.actors;

        // Who can be seen, nearest first. (`reach` is nearer the true
        // distance than the sum the order goes by: it is what a level of
        // detail is chosen from.)
        std::int32_t distance[kMaxDrawn], reach[kMaxDrawn];
        for (int i = 0; actors.states != nullptr && i < actors.count; ++i)
        {
            const ActorState &actor = actors.states[i];
            const ModelView *model = actor.active ? actorModel(actors, actor) : nullptr;
            if (model == nullptr)
                continue;
            const int radius = model->header->radius;
            // A ball round its middle, which for something standing is about
            // half its height up.
            // (Hung from a ceiling, it is all below where it is fixed.)
            const int up = isTurret(actor.kind) && (actor.flags & kActorCeiling) ? -model->header->maxs[2]
                                                                                 : model->header->maxs[2];
            const std::int16_t centre[3] = {
                static_cast<std::int16_t>(fxToInt(actor.pos[0])),
                static_cast<std::int16_t>(fxToInt(actor.pos[1])),
                static_cast<std::int16_t>(fxToInt(actor.pos[2]) + (up >> 1))};
            // Seen if the leaf its middle is in can be seen, or the one its
            // head is in: someone standing in a doorway has their middle in
            // the frame and their head in the room.
            const std::int16_t top[3] = {
                centre[0], centre[1],
                static_cast<std::int16_t>(fxToInt(actor.pos[2]) + up)};
            if (!worldDraw.ballVisible(centre, radius) && !worldDraw.ballVisible(top, radius))
                continue;
            const std::int32_t dx = fxToInt(actor.pos[0] - camera.pos[0]);
            const std::int32_t dy = fxToInt(actor.pos[1] - camera.pos[1]);
            const std::int32_t dz = fxToInt(actor.pos[2] - camera.pos[2]);
            const std::int32_t far = (dx < 0 ? -dx : dx) + (dy < 0 ? -dy : dy) + (dz < 0 ? -dz : dz);
            int at = found < limit ? found : limit - 1;
            if (found >= limit && far >= distance[at])
            {
                ++cut;
                continue;
            }
            if (found >= limit)
                ++cut; // the furthest so far gives up its place
            while (at > 0 && distance[at - 1] > far)
            {
                order[at] = order[at - 1];
                distance[at] = distance[at - 1];
                reach[at] = reach[at - 1];
                --at;
            }
            order[at] = i;
            distance[at] = far;
            {
                const std::int32_t ax = dx < 0 ? -dx : dx, ay = dy < 0 ? -dy : dy, az = dz < 0 ? -dz : dz;
                const std::int32_t most = ax > ay ? (ax > az ? ax : az) : (ay > az ? ay : az);
                reach[at] = most + (far - most) / 2;
            }
            if (found < limit)
                ++found;
        }

        // What the player holds comes first: it is always in view.
        const ModelView *held = game.held.active && !player.noclip ? actorModel(actors, game.held) : nullptr;
        // (At its finest, unless that is the game's own mesh and the frame
        // is already too full: then the level under it.)
        // (The player's say: with model detail high, a step of pressure is
        // borne before any model is made coarser for it.)
        const int weight = modelDetailWeight(settings.graphics.modelDetail);
        const int pressure = settings.graphics.modelDetail == 2 ? (worldDraw.pressure() > 0 ? worldDraw.pressure() - 1 : 0)
                                                                : worldDraw.pressure();
        heldLevel_ = 0;
        if (held != nullptr)
        {
            if (pressure > 0 && held->levelCount() > 1 && held->header->polygons > kWholeModel)
                heldLevel_ = 1;
            for (int b = 0; b < held->header->batchCount; ++b)
            {
                if (held->batches[b].level != heldLevel_)
                    continue;
                polygons += held->batches[b].polygons;
                vertices += held->batches[b].vertices;
            }
            heldDrawn_ = true;
        }
        // Then the actors, nearest first, for as long as they fit in the
        // share of a pass that models may have: the rest are not drawn this
        // frame.
        //
        // Each at a level of detail: the coarsest whose error is under a
        // pixel and a half at the model's distance (a unit is 127 pixels
        // over the distance in units, so a sixteenth of a unit of error is
        // good from five and a third times as many units away); a level
        // coarser again for each step of pressure, for all but the nearest;
        // and coarser still, if that is what it takes to fit.
        int kept = 0;
        for (int n = 0; n < found; ++n)
        {
            const ActorState &actor = actors.states[order[n]];
            const ModelView &model = *actorModel(actors, actor);
            const int levels = model.levelCount();
            int level = 0;
            for (int l = levels - 1; l > 0; --l)
            {
                if (model.header->levelError[l] * weight <= reach[n] * 3)
                {
                    level = l;
                    break;
                }
            }
            // (A model that was at a coarser level a moment ago stays at
            // it until it is a tenth nearer than where it would take it up:
            // otherwise one standing at the line flickers between two.)
            const int last = lastLevel_[order[n] & 255] - 1;
            if (last > level && last < levels && model.header->levelError[last] * weight * 9 <= reach[n] * 3 * 10)
                level = last;
            // The game's own mesh (a DSi's finest) is for whoever is
            // nearest, in a frame with room.
            if (level == 0 && levels > 1 && model.header->polygons > kWholeModel && (kept > 0 || pressure > 0))
                level = 1;
            if (kept > 0)
                level = level + pressure < levels ? level + pressure : levels - 1;
            // (Under pressure the share is less -- WorldDraw::pressure -- but
            // whoever is nearest is always let in.)
            const int share = 4 - (pressure > 2 ? 2 : pressure);
            int p = 0, v = 0;
            bool fits = false;
            for (; level < levels && !fits; ++level)
            {
                p = v = 0;
                for (int b = 0; b < model.header->batchCount; ++b)
                {
                    if (!model.batchInBody(model.batches[b], actor.body, actor.skin, level))
                        continue;
                    p += model.batches[b].polygons;
                    v += model.batches[b].vertices;
                }
                fits = !(polygons + p > kMaxPolygons || vertices + v > kMaxVertices ||
                         (kept > 0 &&
                          (polygons + p > kMaxPolygons * share / 4 || vertices + v > kMaxVertices * share / 4)));
            }
            if (!fits)
            {
                lastLevel_[order[n] & 255] = 0;
                ++cut;
                continue;
            }
            --level;
            polygons += p;
            vertices += v;
            lastLevel_[order[n] & 255] = static_cast<std::uint8_t>(level + 1);
            level_[kept] = static_cast<std::uint8_t>(level);
            order[kept++] = order[n];
        }
        found = kept;

        // The pieces of what has come apart: those in view, for as long as
        // there is room in the models' share (they are the first thing to
        // go when there is not: nothing under pressure, and never more than
        // a third of the share).
        gibFound_ = 0;
        if (worldDraw.pressure() < 2)
        {
            int gibPolygons = 0;
            for (int i = 0; i < kMaxGibs; ++i)
            {
                const Gib &gib = actors.gibs[i];
                if (!gib.active || gib.model >= actors.modelCount || actors.models[gib.model].header == nullptr)
                    continue;
                const ModelView &model = actors.models[gib.model];
                const std::int16_t centre[3] = {static_cast<std::int16_t>(fxToInt(gib.pos[0])),
                                                static_cast<std::int16_t>(fxToInt(gib.pos[1])),
                                                static_cast<std::int16_t>(fxToInt(gib.pos[2]))};
                if (!worldDraw.ballVisible(centre, 12))
                    continue;
                int p = 0, v = 0;
                for (int b = 0; b < model.header->batchCount; ++b)
                {
                    // (A piece is small and soon gone: the coarsest there is.)
                    if (!model.batchInBody(model.batches[b], gib.body, 0, model.levelCount() - 1))
                        continue;
                    p += model.batches[b].polygons;
                    v += model.batches[b].vertices;
                }
                if (polygons + p > kMaxPolygons || vertices + v > kMaxVertices ||
                    gibPolygons + p > kMaxPolygons / 3)
                    continue;
                polygons += p;
                vertices += v;
                gibPolygons += p;
                gibOrder_[gibFound_++] = static_cast<std::uint8_t>(i);
            }
        }
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
        // (A gib model has one pose, shared by all its pieces: nothing to
        // work out worth doing ahead.)
    }

    void ModelDraw::draw(int &drawn, int &cut)
    {
        const ActorWorld &actors = game.actors;
        cut += cut_;
        if (found_ == 0 && gibFound_ == 0)
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
            // A turret under a ceiling is the same model upside down: turned
            // over about the way it faces (the game's half turn of pitch,
            // and of yaw to make up for it).
            const fx over[9] = {cosine, sine, 0, sine, -cosine, 0, 0, 0, -kFxOne};
            const bool ceiling = isTurret(actor.kind) && (actor.flags & kActorCeiling) != 0;
            drawActor(actor, model, actor.model, ceiling ? over : turn, order[n], level_[n]);
            ++drawn;
        }
        // The pieces: each a model of its own for the moment, turned as it
        // has tumbled. (One pose serves all the pieces of a model: they are
        // asked for under one number.)
        for (int n = 0; n < gibFound_; ++n)
        {
            const Gib &gib = actors.gibs[gibOrder_[n]];
            if (!gib.active || gib.model >= actors.modelCount || actors.models[gib.model].header == nullptr)
                continue;
            ActorState piece;
            piece.active = true;
            piece.kind = static_cast<std::uint8_t>(ActorKind::Generic);
            piece.model = gib.model;
            piece.body = gib.body;
            piece.light = 22;
            piece.clip = actors.models[gib.model].header->clipCount != 0 ? 0 : -1;
            for (int k = 0; k < 3; ++k)
                piece.pos[k] = gib.pos[k];
            fx sy, cy, sp, cp;
            fxSinCos(gib.yaw, sy, cy);
            fxSinCos(gib.pitch, sp, cp);
            const fx turn[9] = {fxMul(cy, cp), -sy, fxMul(cy, sp), fxMul(sy, cp), cy, fxMul(sy, sp), -sp, 0, cp};
            drawActor(piece, actors.models[gib.model], gib.model, turn, -3 - static_cast<int>(gib.model),
                      actors.models[gib.model].levelCount() - 1);
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
        // The lamp's: straight along the view, which in view space is
        // always the same way. (Its colour is each model's own: none, for
        // one that is not in the beam.)
        GFX_LIGHT_VECTOR = ((static_cast<std::uint32_t>(-511) & 0x3FF) << 20) | (1u << 30);
        // How the shine falls off as a face turns from the eye: steeply (a
        // highlight, not a wash). The table is 128 steps of 0..255.
        if (game.lamp.on)
        {
            for (int w = 0; w < 32; ++w)
            {
                std::uint32_t word = 0;
                for (int b = 0; b < 4; ++b)
                {
                    const std::uint32_t x = static_cast<std::uint32_t>(w * 4 + b) * 2; // 0..254
                    const std::uint32_t x2 = x * x >> 8, x4 = x2 * x2 >> 8;
                    word |= (x4 * x2 >> 8) << (b * 8);
                }
                GFX_SHININESS = word;
            }
        }
        NEA_PolyFormat(31, 0, static_cast<NEA_LightEnum>(NEA_LIGHT_0 | NEA_LIGHT_1), kCull, worldDraw.polygonMode());
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
        // (Not in a dead hand: the camera has fallen from it.)
        if (model == nullptr || !heldDrawn_ || player.state.health <= 0)
            return;
        // At the eye, turned as the view is: the model's forward, left and up
        // are the camera's.
        // (... and with the walk's bob, pushed a little forward by it, as
        // the game's is.)
        ActorState at = held;
        for (int a = 0; a < 3; ++a)
            at.pos[a] = camera.pos[a] + static_cast<fx>((static_cast<std::int64_t>(camera.forward[a]) * player.view.bob * 2 / 5) >> 12);
        const fx turn[9] = {camera.forward[0], -camera.right[0], camera.up[0],
                            camera.forward[1], -camera.right[1], camera.up[1],
                            camera.forward[2], -camera.right[2], camera.up[2]};
        lightOn();
        drawActor(at, *model, held.model, turn, -1, heldLevel_);
        lightOff();
    }
} // namespace hlrt
