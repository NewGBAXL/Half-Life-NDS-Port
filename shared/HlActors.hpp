#pragma once

// Actors: the people and props of a map that are models, as they are right
// now.
//
// An actor is a place, a way it faces, and a clip of its model at some frame.
// Left alone it stands and loops its idle clip (or holds a pose, or cycles).
// A scripted sequence can take it over -- walk it to a mark, turn it, have it
// act something out -- and that half lives in HlLogic.hpp with the rest of
// what a map's wiring does; this file is the actor itself and the parts of it
// that need no wiring.
//
// There is no thinking here. An actor does not see, hear or decide; it goes
// where a script sends it, in a straight line.
//
// std-only and inline: the host tests run it.

#include "HlAnim.hpp"
#include "HlMovers.hpp"

namespace hldata
{
    /// Actors that may arrive from the map before, on top of the map's own.
    inline constexpr int kMaxVisitors = 4;

    // What a script is doing with the actor it has (ActorState::scriptPhase).
    inline constexpr std::uint8_t kActorFree = 0;
    inline constexpr std::uint8_t kActorHeld = 1;    ///< taken, waiting to be told to start
    inline constexpr std::uint8_t kActorMoving = 2;  ///< on its way to the mark
    inline constexpr std::uint8_t kActorPlaying = 3; ///< acting it out

    struct ActorState
    {
        bool active = false;
        fx pos[3] = {0, 0, 0}; ///< its feet
        std::uint16_t yaw = 0;
        std::uint16_t model = 0; ///< into the map's ModelRefs
        std::uint16_t name = 0;  ///< name id in THIS map; 0 = none
        std::uint32_t nameHash = 0;
        std::uint8_t body = 0;
        std::uint8_t kind = 0;   ///< ActorKind
        std::uint8_t flags = 0;  ///< kActor*
        std::uint8_t light = 24;
        std::int16_t clip = -1;  ///< of its model; -1: the model has none, it stands as built
        std::int16_t restClip = -1; ///< the clip it goes back to when nothing directs it
        std::uint32_t frame = 0; ///< Q8, in the clip's frames
        bool once = false;       ///< the clip is played to its end and held, loop or not
        bool finished = false;   ///< ... and has got there
        bool frozen = false;     ///< held on this frame
        std::int16_t script = -1; ///< the logic record directing it, or -1
        std::uint8_t scriptPhase = kActorFree;
        // While moving to a mark:
        fx target[3] = {0, 0, 0};
        fx speed = 0;        ///< units a second
        std::int32_t moveMs = 0; ///< left before it is simply put there
        // What fights (HlMonsters.hpp):
        std::int16_t health = 0;
        std::uint8_t ai = 0;     ///< kAi*
        std::uint8_t struck = 0; ///< which blows of the attack being played have landed
        std::int32_t thinkMs = 0;    ///< until it next looks about, or plans its way
        std::int32_t cooldownMs = 0; ///< until it may attack again
        std::int32_t stuckMs = 0;    ///< how long it has been getting nowhere
        fx velocity[3] = {0, 0, 0};  ///< in the air
        std::uint32_t lastFrame = 0; ///< of its clip, a tick ago
        std::int16_t goalNode = -1;  ///< the path node it is walking to
        std::int16_t fromNode = -1;  ///< ... and the one it last stood on
        fx goal[3] = {0, 0, 0};
        bool hasGoal = false;
        /// A barnacle's tongue: how far it hangs below the mouth.
        fx reach = 0;
        std::uint8_t attack = 0;     ///< which of its kind's attacks it is making (kAttack*)
        std::uint8_t item = 0;       ///< ItemKind, for something to pick up
        std::uint16_t fires = 0;     ///< ... and the name id of what taking it fires
    };

    /// Something in flight: a bullsquid's spit, a hand grenade.
    struct Projectile
    {
        bool active = false;
        std::uint8_t kind = 0; ///< kShot*
        fx pos[3] = {0, 0, 0};
        fx velocity[3] = {0, 0, 0};
        std::int32_t leftMs = 0; ///< a grenade's fuse; the longest spit may fly
    };
    inline constexpr std::uint8_t kShotSpit = 1;
    inline constexpr std::uint8_t kShotGrenade = 2;
    inline constexpr int kMaxProjectiles = 8;

    /// Something seen for a moment, that is nothing but seen: an alien
    /// slave's bolt (from `a` to `b`), a burst (at `a`, `size` units across
    /// at its widest). Whoever draws the frame draws these.
    struct Effect
    {
        std::uint8_t kind = 0; ///< kEffect*; 0 = none
        fx a[3] = {0, 0, 0};
        fx b[3] = {0, 0, 0};
        std::int16_t size = 0;
        std::int16_t leftMs = 0;
        std::int16_t wholeMs = 0;
    };
    inline constexpr std::uint8_t kEffectBolt = 1;
    inline constexpr std::uint8_t kEffectBurst = 2; ///< an explosion
    inline constexpr std::uint8_t kEffectRing = 3;  ///< a houndeye's blast
    inline constexpr std::uint8_t kEffectSplat = 4; ///< spit landing
    inline constexpr int kMaxEffects = 8;

    /// The actors of the loaded map and the models they are made of.
    struct ActorWorld
    {
        ActorState *states = nullptr;
        int count = 0;              ///< the map's own, then kMaxVisitors spare
        const ModelView *models = nullptr; ///< by ModelRefs index; header null if not loaded
        int modelCount = 0;
        /// The collision's copy: one box per state (MoveWorld::boxes).
        SolidBox *boxes = nullptr;
        /// How the map starts each of them (the first `recordCount`): what a
        /// monstermaker sets one of its own back to.
        const ActorRecord *records = nullptr;
        int recordCount = 0;
        const std::uint32_t *nameHashes = nullptr;
        /// Where something that walks may stand, and from each to each the
        /// next to go to (HlLevel.hpp, PathNext). May be none.
        const PathNode *nodes = nullptr;
        const std::uint8_t *next = nullptr;
        int nodeCount = 0;
        std::uint32_t seed = 12345; ///< what little chance there is in a fight
        /// Set by what happens to the player's things this tick (kGot*): the
        /// caller shows it, and clears it.
        std::uint32_t got = 0;
        Projectile shots[kMaxProjectiles];
        Effect effects[kMaxEffects];
    };

    inline const ModelView *actorModel(const ActorWorld &a, const ActorState &actor)
    {
        if (actor.model >= a.modelCount || a.models == nullptr ||
            a.models[actor.model].header == nullptr)
            return nullptr;
        return &a.models[actor.model];
    }

    /// Start `actor` on the clip called `clip` (a nameHash; 0 or unknown:
    /// leave it on what it is doing and say so).
    inline bool actorPlay(const ActorWorld &a, ActorState &actor, std::uint32_t clip, bool once)
    {
        const ModelView *model = actorModel(a, actor);
        const int index = (model != nullptr && clip != 0) ? model->findClip(clip) : -1;
        if (index < 0)
            return false;
        actor.clip = static_cast<std::int16_t>(index);
        actor.frame = 0;
        actor.once = once;
        actor.finished = false;
        actor.frozen = false;
        return true;
    }

    /// Back to what it does when nothing directs it.
    inline void actorRest(ActorState &actor)
    {
        actor.clip = actor.restClip;
        actor.frame = 0;
        // (Someone sitting plays each of his ways of sitting through, then
        // takes up another: actorAdvance.)
        actor.once = actor.kind == static_cast<std::uint8_t>(ActorKind::Sitter);
        actor.finished = false;
        actor.frozen = actor.kind == static_cast<std::uint8_t>(ActorKind::Pose) ||
                       actor.kind == static_cast<std::uint8_t>(ActorKind::Furniture);
    }

    /// A point let fall from `pos` (HlWalk.hpp, which knows about lines):
    /// onto the world or a brush under it, at most `down` lower. False, and
    /// nothing moved, if it starts inside something or nothing is there.
    inline bool pointToFloor(const MoveWorld &world, fx *pos, fx down);

    /// The five ways a scientist sits, in the order the game has them, and
    /// which he takes up after one is done: mostly plain sitting, as there.
    inline std::uint32_t sittingClip(int which)
    {
        static const char *const kNames[] = {"sitlookleft", "sitlookright", "sitscared", "sitting2",
                                             "sitting3"};
        return nameHash(kNames[which < 0 || which > 4 ? 4 : which]);
    }
    inline std::uint32_t g_sitRoll = 0x51773;
    inline int sittingNext()
    {
        g_sitRoll = g_sitRoll * 1664525u + 1013904223u;
        const int roll = static_cast<int>((g_sitRoll >> 16) % 100);
        return roll < 40 ? 4 : (roll < 50 ? 0 : (roll < 60 ? 1 : (roll < 80 ? 3 : 2)));
    }

    /// The box the player cannot walk into, kept where the actor is.
    inline void actorPlaceBox(const ActorWorld &a, int index)
    {
        if (a.boxes == nullptr)
            return;
        const ActorState &actor = a.states[index];
        SolidBox &box = a.boxes[index];
        box.active = actor.active && (actor.flags & kActorSolid) != 0;
        // A person's width and height; a prop's from its model, within
        // reason (a forklift is not a pillar 16 units wide, nor a wall).
        fx half = fxFromInt(16), height = fxFromInt(72);
        const ModelView *model = actorModel(a, actor);
        if (model != nullptr && actor.kind >= static_cast<std::uint8_t>(ActorKind::Generic))
        {
            int reach = 0;
            for (int i = 0; i < 2; ++i)
            {
                reach = reach > -model->header->mins[i] ? reach : -model->header->mins[i];
                reach = reach > model->header->maxs[i] ? reach : model->header->maxs[i];
            }
            half = fxFromInt(reach < 8 ? 8 : (reach > 48 ? 48 : reach));
            const int top = model->header->maxs[2];
            height = fxFromInt(top < 8 ? 8 : (top > 96 ? 96 : top));
        }
        for (int i = 0; i < 2; ++i)
        {
            box.mins[i] = actor.pos[i] - half;
            box.maxs[i] = actor.pos[i] + half;
        }
        box.mins[2] = actor.pos[2];
        box.maxs[2] = actor.pos[2] + height;
    }

    /// Stand an actor on the ground under it: looked for from `up` above its
    /// feet to `down` below them, as something the player's size (whose
    /// middle is 36 units above its feet). False, with the actor left where
    /// it is, if there is no ground in that reach or it stands where such a
    /// thing does not fit.
    inline bool actorToFloor(const MoveWorld &world, ActorState &actor, fx up, fx down)
    {
        const fx from[3] = {actor.pos[0], actor.pos[1], actor.pos[2] + fxFromInt(36) + up};
        const fx to[3] = {actor.pos[0], actor.pos[1], actor.pos[2] + fxFromInt(36) - down};
        const Trace tr = traceHull(world.standing, from, to);
        if (tr.startSolid || tr.allSolid || tr.fraction >= kFractionOne)
            return false;
        actor.pos[2] = tr.endPos[2] - fxFromInt(36);
        return true;
    }

    /// Set one actor to how its record starts it: where it stands (let down
    /// onto the floor under it), which way it faces, in which clip.
    inline void actorSetup(ActorWorld &a, int index, const ActorRecord &record, const MoveWorld &world)
    {
        ActorState &actor = a.states[index];
        actor = ActorState();
        actor.active = true;
        actor.model = record.model;
        actor.name = record.name;
        actor.nameHash = a.nameHashes != nullptr ? a.nameHashes[record.name] : 0;
        actor.yaw = record.yaw;
        actor.body = record.body;
        actor.kind = record.kind;
        actor.flags = record.flags;
        actor.light = record.light;
        actor.item = static_cast<std::uint8_t>(record.item);
        actor.fires = record.target;
        for (int k = 0; k < 3; ++k)
            actor.pos[k] = record.origin[k];
        // Onto the floor, as the game drops what it spawns -- each kind as
        // the game does it. A body, and a piece of furniture, fall as a
        // point (they have no size there until they have landed); someone
        // sitting comes down in the low box he is given, onto the floor his
        // chair is on; a cycler and what hangs from a ceiling stay put; the
        // rest come down in the box they walk about in.
        const bool point = record.kind == static_cast<std::uint8_t>(ActorKind::Pose) ||
                           record.kind == static_cast<std::uint8_t>(ActorKind::Furniture);
        const bool stays = record.kind == static_cast<std::uint8_t>(ActorKind::Cycler) ||
                           record.kind == static_cast<std::uint8_t>(ActorKind::Barnacle);
        if (stays)
        {
        }
        else if (point)
        {
            pointToFloor(world, actor.pos, fxFromInt(256));
        }
        else if (record.kind == static_cast<std::uint8_t>(ActorKind::Sitter))
        {
            const fx from[3] = {actor.pos[0], actor.pos[1], actor.pos[2] + fxFromInt(18 + 1)};
            const fx to[3] = {actor.pos[0], actor.pos[1], actor.pos[2] + fxFromInt(18 - 256)};
            // (18: that box's middle above its feet.)
            const Trace tr = traceHull(world.crouched, from, to);
            if (!tr.allSolid && tr.fraction < kFractionOne)
                actor.pos[2] = tr.endPos[2] - fxFromInt(18);
        }
        else if (!actorToFloor(world, actor, fxFromInt(1), fxFromInt(256)))
        {
            // No room where the map has it (a map's author goes by the
            // model, not by the box it moves in): a stair's height up, or a
            // little to one side, is tried before it is left as it is.
            static const int kTry[][3] = {{0, 0, 18}, {16, 0, 0},  {-16, 0, 0},  {0, 16, 0},
                                          {0, -16, 0}, {16, 0, 18}, {-16, 0, 18}, {0, 16, 18},
                                          {0, -16, 18}, {0, 0, 36}};
            for (const int *offset : kTry)
            {
                ActorState moved = actor;
                for (int k = 0; k < 3; ++k)
                    moved.pos[k] += fxFromInt(offset[k]);
                if (actorToFloor(world, moved, fxFromInt(1), fxFromInt(256)))
                {
                    for (int k = 0; k < 3; ++k)
                        actor.pos[k] = moved.pos[k];
                    break;
                }
            }
        }

        const ModelView *model = actorModel(a, actor);
        if (model != nullptr && model->header->clipCount != 0)
        {
            int clip = record.clip != 0 ? model->findClip(record.clip) : 0;
            // (Each of those sitting starts in a way of his own.)
            if (actor.kind == static_cast<std::uint8_t>(ActorKind::Sitter))
            {
                const int own = model->findClip(sittingClip(index % 5));
                clip = own >= 0 ? own : clip;
            }
            actor.restClip = static_cast<std::int16_t>(clip < 0 ? 0 : clip);
        }
        actorRest(actor);
        // A cycler runs its sequence; furniture and poses hold theirs.
        if (actor.kind == static_cast<std::uint8_t>(ActorKind::Cycler))
            actor.frozen = false;
        if (actor.kind == static_cast<std::uint8_t>(ActorKind::Item))
            actor.frozen = true;
    }

    /// Set every actor to how the map starts it.
    inline void actorsInit(ActorWorld &a, const ActorRecord *records, int recordCount,
                           const std::uint32_t *nameHashes, const MoveWorld &world)
    {
        a.records = records;
        a.recordCount = recordCount;
        a.nameHashes = nameHashes;
        a.got = 0;
        for (Projectile &shot : a.shots)
            shot = Projectile();
        for (Effect &effect : a.effects)
            effect = Effect();
        for (int i = 0; i < a.count; ++i)
        {
            a.states[i] = ActorState();
            if (i < recordCount)
            {
                actorSetup(a, i, records[i], world);
                // Kept for a monstermaker: not there yet.
                if (records[i].flags & kActorDormant)
                    a.states[i].active = false;
            }
            actorPlaceBox(a, i);
        }
    }

    /// Advance one actor's clip by `ms`. Returns the frame it was at before
    /// (Q8), for whoever watches for the clip's events.
    inline std::uint32_t actorAdvance(const ActorWorld &a, ActorState &actor, int ms)
    {
        const std::uint32_t before = actor.frame;
        const ModelView *model = actorModel(a, actor);
        if (model == nullptr || actor.clip < 0 || actor.frozen || actor.finished)
            return before;
        ModelClip clip = model->clips[actor.clip];
        // A cycler goes round whatever the sequence says; a clip played
        // "once" stops at its end whatever it says.
        if (actor.kind == static_cast<std::uint8_t>(ActorKind::Cycler))
            clip.flags |= kClipLoops;
        if (actor.once)
            clip.flags &= static_cast<std::uint16_t>(~kClipLoops);
        bool finished = false;
        actor.frame = advanceClip(clip, actor.frame, ms, finished);
        actor.finished = finished;
        // Someone sitting, left to himself, has done with one way of
        // sitting: another.
        if (finished && actor.kind == static_cast<std::uint8_t>(ActorKind::Sitter) &&
            actor.scriptPhase == kActorFree)
        {
            const int next = model->findClip(sittingClip(sittingNext()));
            if (next >= 0)
            {
                actor.clip = static_cast<std::int16_t>(next);
                actor.frame = 0;
                actor.finished = false;
            }
        }
        return before;
    }

    /// How fast `clip` of the actor's model carries it, in Q12 units a
    /// second: its stride over its length. `fallback` if it says nothing.
    inline fx actorClipSpeed(const ActorWorld &a, const ActorState &actor, std::uint32_t clip,
                             fx fallback)
    {
        const ModelView *model = actorModel(a, actor);
        const int index = model != nullptr ? model->findClip(clip) : -1;
        if (index < 0)
            return fallback;
        const ModelClip &c = model->clips[index];
        const fx stride = fxLength(c.travel);
        if (stride <= 0 || c.frameCount < 2)
            return fallback;
        // stride * fps / (frames - 1); fps is Q8.
        return static_cast<fx>((static_cast<std::int64_t>(stride) * c.fps) / ((c.frameCount - 1) * 256));
    }
} // namespace hldata
