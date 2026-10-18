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

#include <cstring>

#include "HlAnim.hpp"
#include "HlMovers.hpp"

namespace hldata
{
    // What a monster is doing (ActorState::ai).
    inline constexpr std::uint8_t kAiIdle = 0;
    inline constexpr std::uint8_t kAiChase = 1;
    inline constexpr std::uint8_t kAiAttack = 2;
    inline constexpr std::uint8_t kAiLeap = 3;
    inline constexpr std::uint8_t kAiFlinch = 4;
    inline constexpr std::uint8_t kAiDying = 5;
    inline constexpr std::uint8_t kAiDead = 6;

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
        std::uint8_t skin = 0;   ///< which of its model's skins
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
        /// A monster chasing (with `seen` and `lookMs` below, which a
        /// soldier uses too): whether it has seen what it is after at all,
        /// how long ago it last did; and where it was when it set out,
        /// which is where it goes back to when it gives up.
        bool hasSeen = false;
        std::int32_t lostMs = 0;
        fx home[3] = {0, 0, 0};
        bool goingHome = false;
        /// One of the people (HlPeople.hpp): what he is afraid of or
        /// shooting at (an actor, or -1), and whether he was following the
        /// player before he ran.
        std::int16_t foe = -1;
        bool wasFollowing = false;
        /// A guard (HlPeople.hpp): where what he is after was when he last
        /// saw it, whether he sees it now and has a clear shot at it; his
        /// pistol (0 in its holster, 1 coming out, 2 out). (`painMs`, a
        /// soldier's too, is how long until he may cry out again.)
        fx foeAt[3] = {0, 0, 0};
        bool foeSeen = false;
        bool clearShot = false;
        std::uint8_t gun = 0;
        std::int32_t gunMs = 0;
        /// A barnacle's tongue: how far it hangs below the mouth. (The big
        /// turret's eye, too: how bright, 0..255 -- not Q12.)
        fx reach = 0;
        /// What one of the people stands on (HlMonsters.hpp's peopleGround):
        /// a brush, by its number; -1 the world, or nothing the map gave
        /// them; -2 not yet looked at. Where that brush was a tick ago, so
        /// that they go where it goes; and whether they are falling --
        /// because it broke, or went from under them.
        std::int16_t groundMover = -2;
        fx groundAt[3] = {0, 0, 0};
        bool airborne = false;
        std::int32_t hurtMs = 0;     ///< until a place that hurts may hurt them again
        std::uint8_t attack = 0;     ///< which of its kind's attacks it is making (kAttack*)
        std::uint8_t item = 0;       ///< ItemKind, for something to pick up
        std::uint16_t fires = 0;     ///< ... and the name id of what taking it fires
        // A turret (HlMonsters.hpp). Angles are 65536 to the circle: which
        // way the gun points in the world, and how far up; and where it is
        // being turned to.
        std::int32_t aimYaw = 0, aimPitch = 0;
        std::int32_t wantYaw = 0, wantPitch = 0;
        std::int32_t turnRate = 0; ///< degrees a second
        /// Something has switched it by name since it last thought: 1 + how
        /// (kUse*). 0: nothing has.
        std::uint8_t switched = 0;
        // A soldier (HlGrunt.hpp). (What he carries is in `item`: kGrunt*.)
        std::uint8_t ammo = 0;      ///< rounds in his gun
        std::uint8_t mood = 0;      ///< kMood*
        std::uint8_t bursts = 0;    ///< left of the volley he is firing
        std::int16_t squad = -1;    ///< the actor who leads his squad (himself, if he does); -1: on his own
        std::int32_t grenadeMs = 0; ///< until he may think of a grenade again
        std::int32_t painMs = 0;    ///< until he may cry out again
        std::int32_t lookMs = 0;    ///< until he next looks about
        std::int32_t coverMs = 0;   ///< until it is worth looking for cover again
        fx seen[3] = {0, 0, 0};     ///< where whoever he is after was last seen
    };

    /// Something in flight: a bullsquid's spit, a hand grenade.
    struct Projectile
    {
        bool active = false;
        std::uint8_t kind = 0; ///< kShot*
        /// For kShotGib: the sprite it is drawn as (a shooter that throws
        /// sprites), or 0xFFFF for a plain scrap.
        std::uint16_t sprite = 0xFFFF;
        fx pos[3] = {0, 0, 0};
        fx velocity[3] = {0, 0, 0};
        std::int32_t leftMs = 0; ///< a grenade's fuse; the longest spit may fly
    };
    inline constexpr std::uint8_t kShotSpit = 1;
    inline constexpr std::uint8_t kShotGrenade = 2;
    inline constexpr std::uint8_t kShotGib = 3; ///< something thrown out (HlEffects.hpp): falls, bounces, goes
    inline constexpr std::uint8_t kShotContact = 4; ///< a launcher's grenade: goes off at what it touches
    /// A crossbow's bolt: straight on, fast; into what it meets, or into the
    /// wall, where it stays a while (`sprite` 1: it is in the wall).
    inline constexpr std::uint8_t kShotBolt = 5;
    /// A satchel charge: thrown, it lies where it stops until it is set off
    /// (`sprite` 1).
    inline constexpr std::uint8_t kShotSatchel = 6;
    /// A tripmine the player put on a wall: `velocity` is the way its beam
    /// goes (unit, Q12). `sprite` 0 while it charges (`leftMs` to go), 1 armed
    /// (`leftMs` then the length its beam was, Q30 of kMineRange), 2 set off
    /// (`leftMs` until it goes).
    inline constexpr std::uint8_t kShotMine = 7;
    /// A snark: hops at whatever it can find to bite, and bursts when its
    /// time is up. `sprite`: how many bites (low byte), and until its next
    /// bite (high byte, in twentieths of a second).
    inline constexpr std::uint8_t kShotSnark = 8;
    /// How many may be in flight at once: all of these on a DSi, half on a
    /// DS (ActorWorld::shotLimit).
    inline constexpr int kMaxProjectiles = 16;

    /// A piece of something that has come apart: one sub-model of a gib
    /// model (the game's hgibs, agibs, woodgibs ...), thrown, tumbling,
    /// bouncing, lying, and gone after a while. Seen only: it is in nobody's
    /// way.
    struct Gib
    {
        bool active = false;
        bool resting = false;
        std::uint16_t model = 0; ///< into the map's ModelRefs
        std::uint8_t body = 0;   ///< which piece of it
        fx pos[3] = {0, 0, 0};
        fx velocity[3] = {0, 0, 0};
        std::uint16_t yaw = 0, pitch = 0; ///< how it is turned
        std::int16_t spin = 0;            ///< ... and turns, 65536ths of a circle a millisecond
        std::int32_t leftMs = 0;
    };
    /// How many there may be at once: all of these on a DSi, half on a DS
    /// (ActorWorld::gibLimit).
    inline constexpr int kMaxGibs = 32;

    /// A mark left on a wall: a bullet's hole, blood, the scorch of a
    /// blast, the cracks in glass that has been shot (the game's decals:
    /// HlUi.hpp says which picture each is). `at` is in the world, or --
    /// for one on a brush, which moves with it and goes when it does --
    /// from the brush's place.
    struct Decal
    {
        bool active = false;
        std::uint8_t picture = 0;
        std::uint8_t half = 4;  ///< units from its middle to its edge
        std::uint8_t turn = 0;  ///< quarter turns, so that no two look alike
        std::int16_t brush = -1;
        std::uint16_t colour = 0;
        fx at[3] = {0, 0, 0};
        fx normal[3] = {0, 0, kFxOne}; ///< Q12: the way the wall faces
        /// Until it is gone (its last second it fades). A crack lasts as
        /// long as its pane.
        std::int32_t leftMs = 0;
    };
    /// Marks of one kind are one family: a new one where one of its
    /// family already is takes that one's place (decalAdd).
    constexpr int decalFamily(std::uint8_t picture)
    {
        // (16 to 20: the hand, the pools and the splashes, which are all
        // blood. 21 and 22: a wall's own damage.)
        return picture < 4 ? 0
               : ((picture >= 5 && picture <= 7) || (picture >= 16 && picture <= 20) ? 5 : (picture == 13 ? 8 : picture));
    }
    /// How long each lasts (ms): holes and dents a third of a minute,
    /// blood and scorches two thirds, a crack for as long as its pane.
    constexpr std::int32_t decalLife(std::uint8_t picture)
    {
        const int family = decalFamily(picture);
        return family == 9 ? 0x3FFFFFFF : (family == 0 || family == 4 ? 20000 : 40000);
    }
    /// (All of these on a DSi; ActorWorld::decalLimit of them on a DS.)
    inline constexpr int kMaxDecals = 64;

    /// A small thing thrown, tumbling, falling and soon gone, that is
    /// nothing but seen: a shard of what has been broken (a patch of its
    /// own texture), a speck off a wall a shot has hit, a spent casing.
    struct Piece
    {
        std::uint8_t kind = 0;   ///< kPiece*; 0: none
        std::uint8_t half = 2;   ///< units from its middle to its edge
        std::uint8_t alpha = 31; ///< how solid it is drawn (glass is not)
        std::uint8_t bounces = 0;
        std::uint16_t texture = 0xFFFF; ///< kPieceShard: into the level's Textures
        std::uint8_t s = 0, t = 0;      ///< ... and where in it its patch is, in eighths
        std::uint16_t colour = 0x7FFF;
        std::uint16_t spin = 0;         ///< how it is turned, and how fast it turns
        std::int16_t spinRate = 0;      ///< 65536ths of a turn a millisecond
        std::int16_t weight = 256;      ///< how gravity takes it (256: as anything; less: it drifts)
        fx pos[3] = {0, 0, 0};
        fx velocity[3] = {0, 0, 0};
        std::int32_t leftMs = 0;
    };
    inline constexpr std::uint8_t kPieceShard = 1;
    inline constexpr std::uint8_t kPieceSpeck = 2; ///< plain, tiny, in `colour`
    inline constexpr std::uint8_t kPieceShell = 3; ///< brass; it rings once as it lands
    inline constexpr int kMaxPieces = 64;

    /// What something breaks into, by what it is made of (the game's
    /// func_breakable materials: 0 glass, 1 wood, 2 metal, 3 flesh, 4 cinder
    /// block, 5 ceiling tile, 6 computer, 7 the glass that never breaks, 8
    /// rock): how many pieces for each 4096 square units of its biggest
    /// face (and no fewer than `least`), how big, how fast they leave, how
    /// they fall, how solid they look and how long they lie.
    struct BreakStyle
    {
        std::uint8_t per, least, most, half, alpha;
        std::int16_t speed, weight;
        std::int16_t lastMs;
    };
    inline constexpr BreakStyle breakStyle(int material)
    {
        switch (material)
        {
        case 0:  return {6, 6, 16, 3, 18, 140, 256, 1200};  // glass: many, light, soon gone
        case 7:  return {6, 6, 16, 3, 18, 140, 256, 1200};
        case 1:  return {4, 5, 12, 4, 31, 170, 256, 1800};  // wood
        case 2:  return {3, 4, 10, 4, 31, 150, 256, 1800};  // metal: few and heavy
        case 5:  return {4, 4, 10, 4, 31, 60, 110, 1800};   // ceiling tiles drift down
        case 6:  return {4, 5, 12, 3, 31, 180, 256, 1800};  // a computer
        default: return {4, 5, 12, 4, 31, 160, 256, 1800};  // cinder block, rock, flesh
        }
    }

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
        /// For kEffectSprite: which of the map's sprites (played through
        /// once over its moment), and its colour (RGB555; 0x7FFF: its own).
        std::uint16_t sprite = 0xFFFF;
        std::uint16_t colour = 0x7FFF;
    };
    inline constexpr std::uint8_t kEffectBolt = 1;
    inline constexpr std::uint8_t kEffectBurst = 2; ///< an explosion
    inline constexpr std::uint8_t kEffectRing = 3;  ///< a houndeye's blast
    inline constexpr std::uint8_t kEffectSplat = 4; ///< spit landing
    inline constexpr std::uint8_t kEffectTracer = 5; ///< a round's line, from `a` to `b`
    inline constexpr std::uint8_t kEffectMuzzle = 6; ///< the flash at a gun's end
    inline constexpr std::uint8_t kEffectBlood = 7;  ///< where a shot lands on what bleeds (`size`: 1 if it bleeds yellow)
    inline constexpr std::uint8_t kEffectSprite = 8; ///< a sprite of the map's, `size` units across
    /// (All of these on a DSi, half on a DS: ActorWorld::effectLimit.)
    inline constexpr int kMaxEffects = 48;

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
        /// Where something that walks may stand, and the ways between
        /// (HlLevel.hpp, PathLink). May be none.
        const PathNode *nodes = nullptr;
        const PathLink *links = nullptr;
        int nodeCount = 0;
        int linkCount = 0;
        /// How each link is just now, a byte a link (kLink*), kept by the
        /// logic, which knows the doors; null: all of them are open.
        std::uint8_t *linkShut = nullptr;
        /// The doors those who walk have asked to have opened since the
        /// logic last looked (brush indices): it opens them.
        std::uint16_t doorWanted[4] = {};
        int doorsWanted = 0;
        std::uint32_t seed = 12345; ///< what little chance there is in a fight
        std::int32_t hurtLookMs = 0; ///< until the places that hurt are next looked at (HlMonsters.hpp)
        /// Set by what happens to the player's things this tick (kGot*): the
        /// caller shows it, and clears it.
        std::uint32_t got = 0;
        /// A game between consoles (HlNetGame.hpp): what lies about is
        /// everybody's. Touching something does not take it: it is noted
        /// in `wanted` (the actor; -1 none; -2 one has been asked for and
        /// no other is noted yet), and whoever runs the game says who has
        /// it.
        bool shared = false;
        std::int16_t wanted = -1;
        Projectile shots[kMaxProjectiles];
        Effect effects[kMaxEffects];
        Gib gibs[kMaxGibs];
        Decal decals[kMaxDecals];
        int decalLimit = 24;
        int decalNext = 0; ///< where the next mark goes: round and round
        Piece pieces[kMaxPieces];
        int pieceLimit = 24;
        /// How long the small things and the marks last, of what they are
        /// made to (a hundredth each): a DSi keeps them longer.
        int lifePercent = 100;
        int markLifePercent = 100;
        int gibLimit = kMaxGibs / 2;
        int shotLimit = kMaxProjectiles / 2;
        int effectLimit = kMaxEffects / 2;
        /// Something loud enough to turn a head (a shot, a blast): where,
        /// and for how much longer it counts as just heard.
        fx noise[3] = {0, 0, 0};
        std::int32_t noiseMs = 0;
        /// Where the player is (kept each tick, for what is not told).
        fx foe[3] = {0, 0, 0};
        /// Until any soldier may speak again (they do not talk over each
        /// other), and which question one has asked that wants an answer
        /// (0: none; 1: "check in"; 2: any other).
        std::int32_t speakMs = 0;
        std::uint8_t asked = 0;
        /// The models a person and an alien come apart into (into `models`),
        /// or -1 where the map has not got them: then they do not come apart.
        std::int16_t humanGibs = -1, alienGibs = -1;
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

    /// What the actor's model has its bone controllers set to (poseModel's
    /// `controls`): how far a barnacle's tongue is out, to the nearest unit;
    /// which way a turret's gun is turned from the way the turret faces, and
    /// how far up (the game's signs: a turret on a ceiling is upside down,
    /// and both go the other way). False if they are all at rest.
    inline bool actorControls(const ActorState &actor, std::int32_t *controls)
    {
        controls[0] = controls[1] = controls[2] = controls[3] = 0;
        if (isTurret(actor.kind))
        {
            const bool ceiling = (actor.flags & kActorCeiling) != 0;
            controls[0] = ceiling ? actor.yaw - actor.aimYaw : actor.aimYaw - actor.yaw;
            controls[1] = ceiling ? actor.aimPitch : -actor.aimPitch;
            return true;
        }
        if (actor.kind == static_cast<std::uint8_t>(ActorKind::Barnacle))
        {
            controls[0] = -(actor.reach & ~(kFxOne - 1));
            return true;
        }
        return false;
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
    /// Which way of sitting comes next for `actor`: by chance, from his
    /// own count of them -- which is his, and goes into a save with him
    /// (nothing the game does by chance is kept anywhere but in its state).
    inline int sittingNext(ActorState &actor)
    {
        ++actor.struck;
        std::uint32_t roll = (static_cast<std::uint32_t>(actor.pos[0]) ^ (static_cast<std::uint32_t>(actor.pos[1]) << 7)) +
                             actor.struck * 2654435761u;
        roll = roll * 1664525u + 1013904223u;
        const int of = static_cast<int>((roll >> 16) % 100);
        return of < 40 ? 4 : (of < 50 ? 0 : (of < 60 ? 1 : (of < 80 ? 3 : 2)));
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
        // (A turret is where it is fixed, as much below as above; under a
        // ceiling, all below.)
        if (isTurret(actor.kind))
        {
            box.mins[2] = actor.pos[2] - ((actor.flags & kActorCeiling) ? height : fxFromInt(16));
            if (actor.flags & kActorCeiling)
                box.maxs[2] = actor.pos[2] + fxFromInt(16);
        }
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
        if (record.kind != static_cast<std::uint8_t>(ActorKind::Item))
            actor.skin = static_cast<std::uint8_t>(record.item >> kActorSkinShift);
        actor.kind = record.kind;
        actor.flags = record.flags;
        actor.light = record.light;
        actor.item = record.kind == static_cast<std::uint8_t>(ActorKind::Item) ||
                             record.kind == static_cast<std::uint8_t>(ActorKind::Grunt)
                         ? static_cast<std::uint8_t>(record.item)
                         : std::uint8_t(0);
        if (record.kind == static_cast<std::uint8_t>(ActorKind::Tripmine))
        {
            // Which way it points (as a turret's gun: up is positive).
            actor.aimYaw = record.yaw;
            actor.aimPitch = static_cast<std::int32_t>(static_cast<std::int8_t>(record.item & 0xFF)) * 256;
        }
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
                           record.kind == static_cast<std::uint8_t>(ActorKind::Barnacle) ||
                           record.kind == static_cast<std::uint8_t>(ActorKind::Tripmine) ||
                           isTurret(record.kind);
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
        a.noiseMs = a.speakMs = 0;
        a.asked = 0;
        for (Projectile &shot : a.shots)
            shot = Projectile();
        for (Effect &effect : a.effects)
            effect = Effect();
        for (Gib &gib : a.gibs)
            gib = Gib();
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
    HL_HOT std::uint32_t actorAdvance(const ActorWorld &a, ActorState &actor, int ms)
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
            actor.scriptPhase == kActorFree && actor.ai == 0)
        {
            const int next = model->findClip(sittingClip(sittingNext(actor)));
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
    /// Something to be seen for `ms` milliseconds (kEffect*). When all the
    /// places are taken the oldest gives way.
    inline Effect &effectAdd(ActorWorld &a, std::uint8_t kind, const fx *from, const fx *to, int size, int ms)
    {
        Effect *slot = &a.effects[0];
        const int limit = a.effectLimit < 1 ? 1 : (a.effectLimit > kMaxEffects ? kMaxEffects : a.effectLimit);
        for (int i = 0; i < limit; ++i)
        {
            Effect &effect = a.effects[i];
            if (effect.kind == 0)
            {
                slot = &effect;
                break;
            }
            if (effect.leftMs < slot->leftMs)
                slot = &effect;
        }
        slot->kind = kind;
        for (int i = 0; i < 3; ++i)
        {
            slot->a[i] = from[i];
            slot->b[i] = to != nullptr ? to[i] : from[i];
        }
        slot->size = static_cast<std::int16_t>(size);
        slot->leftMs = slot->wholeMs = static_cast<std::int16_t>(ms);
        slot->sprite = 0xFFFF;
        slot->colour = 0x7FFF;
        return *slot;
    }

    inline constexpr std::uint8_t kLinkOpen = 0;   ///< nothing in the way
    inline constexpr std::uint8_t kLinkDoor = 1;   ///< a door that is shut, and opens for whoever can open doors
    inline constexpr std::uint8_t kLinkNoWay = 2;  ///< locked, or not broken yet

    /// Leave a mark: `picture` (a kDecal*), `half` units from its middle to
    /// its edge, in `colour`, on the wall at `at` that faces `normal`
    /// (Q12). The oldest gives way.
    inline Decal &decalAdd(ActorWorld &a, std::uint8_t picture, const fx *at, const fx *normal, int half,
                           std::uint16_t colour, int brush = -1, bool exact = false)
    {
        const int limit = a.decalLimit < 1 ? 1 : (a.decalLimit > kMaxDecals ? kMaxDecals : a.decalLimit);
        const std::int32_t life = decalLife(picture) / 100 * a.markLifePercent;
        half = half < 1 ? 1 : (half > 64 ? 64 : half);
        // One of its family there already -- on the same thing, facing the
        // same way, its middle within this one's reach (anywhere on the
        // pane, for a crack) -- is this one: moved to the new place and
        // begun again. Ten blows on one spot are one mark, not ten sheets
        // one on another for the hardware to draw.
        for (int i = 0; i < limit; ++i)
        {
            Decal &old = a.decals[i];
            if (!old.active || old.brush != brush || decalFamily(old.picture) != decalFamily(picture))
                continue;
            // (A mark the map put there is itself and no other: it is this
            // one only if it is this picture in this very place. Two of
            // the map's stains side by side stay two.)
            if (exact)
            {
                const fx gap[3] = {old.at[0] - at[0], old.at[1] - at[1], old.at[2] - at[2]};
                if (old.picture != picture || fxLength(gap) > fxFromInt(1))
                    continue;
                old.leftMs = decalLife(picture) / 100 * a.markLifePercent;
                return old;
            }
            const std::int64_t facing = static_cast<std::int64_t>(old.normal[0]) * normal[0] +
                                        static_cast<std::int64_t>(old.normal[1]) * normal[1] +
                                        static_cast<std::int64_t>(old.normal[2]) * normal[2];
            if (facing < (static_cast<std::int64_t>(kFxOne) * kFxOne * 9) / 10)
                continue;
            const fx d[3] = {old.at[0] - at[0], old.at[1] - at[1], old.at[2] - at[2]};
            const int reach = old.half > half ? old.half : half;
            if (decalFamily(picture) != 9 && fxLength(d) > fxFromInt(reach))
                continue;
            old.half = static_cast<std::uint8_t>(reach);
            old.leftMs = life;
            for (int k = 0; k < 3; ++k)
            {
                old.at[k] = at[k];
                old.normal[k] = normal[k];
            }
            return old;
        }
        Decal &decal = a.decals[a.decalNext % limit];
        a.decalNext = (a.decalNext + 1) % limit;
        decal.active = true;
        decal.leftMs = life;
        decal.picture = picture;
        decal.half = static_cast<std::uint8_t>(half);
        a.seed = a.seed * 1664525u + 1013904223u;
        decal.turn = static_cast<std::uint8_t>((a.seed >> 12) & 3);
        decal.brush = static_cast<std::int16_t>(brush);
        decal.colour = colour;
        for (int i = 0; i < 3; ++i)
        {
            decal.at[i] = at[i];
            decal.normal[i] = normal[i];
        }
        return decal;
    }

    /// Throw a small thing. The one with least time left gives way.
    inline Piece &pieceAdd(ActorWorld &a, std::uint8_t kind, const fx *pos, const fx *velocity, int ms)
    {
        const int limit = a.pieceLimit < 1 ? 1 : (a.pieceLimit > kMaxPieces ? kMaxPieces : a.pieceLimit);
        Piece *slot = &a.pieces[0];
        for (int i = 0; i < limit; ++i)
        {
            if (a.pieces[i].kind == 0)
            {
                slot = &a.pieces[i];
                break;
            }
            if (a.pieces[i].leftMs < slot->leftMs)
                slot = &a.pieces[i];
        }
        *slot = Piece();
        slot->kind = kind;
        a.seed = a.seed * 1664525u + 1013904223u;
        slot->spin = static_cast<std::uint16_t>(a.seed >> 8);
        slot->spinRate = static_cast<std::int16_t>(static_cast<int>((a.seed >> 12) % 161) - 80);
        for (int i = 0; i < 3; ++i)
        {
            slot->pos[i] = pos[i];
            slot->velocity[i] = velocity[i];
        }
        slot->leftMs = ms * a.lifePercent / 100;
        return *slot;
    }

    /// The marks, for `ms` more milliseconds: each goes when its time is
    /// up. Returns how many there are.
    inline int decalsTick(ActorWorld &a, int ms)
    {
        int live = 0;
        for (Decal &decal : a.decals)
        {
            if (!decal.active)
                continue;
            decal.leftMs -= ms;
            if (decal.leftMs <= 0)
                decal.active = false;
            else
                ++live;
        }
        return live;
    }

    /// Something made of `material` and as big as the box `mins`..`maxs`
    /// has broken: its pieces, each a patch of one of `textures` (the
    /// thing's own; none: plain), thrown out from its middle.
    inline int piecesBurst(ActorWorld &a, const fx *mins, const fx *maxs, int material, const std::uint16_t *textures,
                           int textureCount)
    {
        const BreakStyle style = breakStyle(material);
        // Its biggest face, in units of 64 by 64.
        const int size[3] = {fxToInt(maxs[0] - mins[0]), fxToInt(maxs[1] - mins[1]), fxToInt(maxs[2] - mins[2])};
        const int thin = size[0] <= size[1] && size[0] <= size[2] ? 0 : (size[1] <= size[2] ? 1 : 2);
        const int face = (size[(thin + 1) % 3] * size[(thin + 2) % 3]) / 4096;
        int count = style.per * face;
        count = count < style.least ? style.least : (count > style.most ? style.most : count);
        auto chance = [&a]() -> std::uint32_t
        {
            a.seed = a.seed * 1664525u + 1013904223u;
            return a.seed >> 8;
        };
        for (int n = 0; n < count; ++n)
        {
            fx at[3], velocity[3];
            for (int k = 0; k < 3; ++k)
            {
                const fx span = maxs[k] - mins[k];
                at[k] = mins[k] + static_cast<fx>((static_cast<std::int64_t>(span) * (chance() & 255)) >> 8);
                // Out from the middle, and anyhow besides.
                const fx out = at[k] - (mins[k] + span / 2);
                velocity[k] = out * 3 + fxFromInt(static_cast<int>(chance() % static_cast<std::uint32_t>(style.speed * 2 + 1)) - style.speed);
            }
            velocity[2] += fxFromInt(style.speed / 2);
            Piece &piece = pieceAdd(a, kPieceShard, at, velocity, style.lastMs + static_cast<int>(chance() % 400));
            piece.half = static_cast<std::uint8_t>(style.half + chance() % 3);
            piece.alpha = style.alpha;
            piece.weight = style.weight;
            piece.texture = textureCount > 0 ? textures[chance() % static_cast<std::uint32_t>(textureCount)] : 0xFFFF;
            piece.s = static_cast<std::uint8_t>(chance() & 7);
            piece.t = static_cast<std::uint8_t>(chance() & 7);
            piece.colour = textureCount > 0 ? 0x7FFF : static_cast<std::uint16_t>(14 | 14 << 5 | 14 << 10);
        }
        return count;
    }

    /// Something set flying. Null if there is no room for another.
    inline Projectile *projectileAdd(ActorWorld &a, std::uint8_t kind, const fx *pos, const fx *velocity, int ms)
    {
        const int limit = a.shotLimit < 1 ? 1 : (a.shotLimit > kMaxProjectiles ? kMaxProjectiles : a.shotLimit);
        for (int i = 0; i < limit; ++i)
        {
            Projectile &shot = a.shots[i];
            if (shot.active)
                continue;
            shot.active = true;
            shot.kind = kind;
            shot.sprite = 0xFFFF;
            for (int i = 0; i < 3; ++i)
            {
                shot.pos[i] = pos[i];
                shot.velocity[i] = velocity[i];
            }
            shot.leftMs = ms;
            return &shot;
        }
        return nullptr;
    }

    /// Throw a piece. When all the places are taken, the one with least
    /// time left gives way.
    inline Gib &gibAdd(ActorWorld &a, int model, int body, const fx *pos, const fx *velocity, int ms)
    {
        const int limit = a.gibLimit < 1 ? 1 : (a.gibLimit > kMaxGibs ? kMaxGibs : a.gibLimit);
        Gib *slot = &a.gibs[0];
        for (int i = 0; i < limit; ++i)
        {
            if (!a.gibs[i].active)
            {
                slot = &a.gibs[i];
                break;
            }
            if (a.gibs[i].leftMs < slot->leftMs)
                slot = &a.gibs[i];
        }
        *slot = Gib();
        slot->active = true;
        slot->model = static_cast<std::uint16_t>(model);
        slot->body = static_cast<std::uint8_t>(body);
        for (int i = 0; i < 3; ++i)
        {
            slot->pos[i] = pos[i];
            slot->velocity[i] = velocity[i];
        }
        slot->leftMs = ms;
        return *slot;
    }

    /// Find, among the map's models, the two that things come apart into
    /// (by their names: the game's hgibs and agibs).
    inline void gibModelsFind(ActorWorld &a, const ModelRef *refs, int count)
    {
        a.humanGibs = a.alienGibs = -1;
        for (int i = 0; i < count; ++i)
        {
            if (std::strcmp(refs[i].name, "hgibs") == 0)
                a.humanGibs = static_cast<std::int16_t>(i);
            else if (std::strcmp(refs[i].name, "agibs") == 0)
                a.alienGibs = static_cast<std::int16_t>(i);
        }
    }

    /// How many pieces the gib model `model` has (its first body part's
    /// sub-models); 0 if the map has not got it.
    inline int gibPieces(const ActorWorld &a, int model)
    {
        if (model < 0 || model >= a.modelCount || a.models == nullptr || a.models[model].header == nullptr ||
            a.models[model].header->bodyPartCount == 0)
            return 0;
        return a.models[model].bodyParts[0].count;
    }
} // namespace hldata
