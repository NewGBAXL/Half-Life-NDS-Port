#pragma once

// What fights, what it is fought with, and what is picked up on the way.
//
// MONSTERS are actors (HlActors.hpp) with a little mind. Each is a state --
// idle, chasing, attacking, in the air, flinching, dying -- and moves by the
// game's own rule for things that walk: a step forward is allowed if, lifted
// a stair's height, nothing is in the way, and there is floor under where it
// ends. They find the player in a straight line when one can be walked, and
// otherwise from path node to path node, by a table the converter worked out
// (PathNext): there is no searching on the console.
//
//   headcrab  runs at the player and leaps from 256 units; it hurts what it
//             lands on.
//   zombie    walks up and swings; each swing hurts if the player is still
//             in reach when it lands.
//   barnacle  hangs, with a tongue down to the floor; what walks into the
//             tongue is pulled up and bitten.
//
// A scripted sequence that has taken a monster has it: it does not think
// until the script lets it go.
//
// WEAPONS are the crowbar, the pistol and the shotgun, with the game's
// numbers at its middle difficulty. A shot is a line: through the world's
// own tree (not the clip hulls, which are the world grown by the player's
// size), against the doors and boxes, and against the box each monster
// stands in.
//
// ITEMS are actors that do nothing but lie there until walked over.
//
// std-only and inline: the host tests run it.

#include "HlLogic.hpp"
#include "HlMaterial.hpp"

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

    // What happened to the player's things (ActorWorld::got).
    inline constexpr std::uint32_t kGotItem = 1;   ///< something was picked up
    inline constexpr std::uint32_t kGotWeapon = 2; ///< ... and it was a weapon not had before
    inline constexpr std::uint32_t kGotHurt = 4;   ///< a monster hurt the player

    // What a weapon did this tick (weaponsTick's result).
    inline constexpr std::uint32_t kWeaponFired = 1;  ///< a shot, or a swing that missed
    inline constexpr std::uint32_t kWeaponHit = 2;    ///< ... that landed on something
    inline constexpr std::uint32_t kWeaponReload = 4; ///< began to reload
    inline constexpr std::uint32_t kWeaponDrawn = 8;  ///< another weapon is in hand

    inline constexpr std::uint8_t kHaveSuit = 1; ///< in Inventory::have

    inline bool isMonster(std::uint8_t kind)
    {
        return kind == static_cast<std::uint8_t>(ActorKind::Headcrab) ||
               kind == static_cast<std::uint8_t>(ActorKind::Zombie) ||
               kind == static_cast<std::uint8_t>(ActorKind::Barnacle) ||
               kind == static_cast<std::uint8_t>(ActorKind::Slave) ||
               kind == static_cast<std::uint8_t>(ActorKind::Bullsquid) ||
               kind == static_cast<std::uint8_t>(ActorKind::Houndeye);
    }

    inline bool isGrunt(std::uint8_t kind)
    {
        return kind == static_cast<std::uint8_t>(ActorKind::Grunt);
    }
    /// The people of the place, who can be hurt (the man in the suit
    /// cannot).
    inline bool isPerson(std::uint8_t kind)
    {
        return kind == static_cast<std::uint8_t>(ActorKind::Scientist) ||
               kind == static_cast<std::uint8_t>(ActorKind::Guard) ||
               kind == static_cast<std::uint8_t>(ActorKind::Sitter);
    }
    /// What comes out of something that is hit, as the game has it: red
    /// from people (and from the barnacle, which is made of what it has
    /// eaten), yellow from the aliens, nothing from a machine.
    inline constexpr int kBloodNone = 0;
    inline constexpr int kBloodRed = 1;
    inline constexpr int kBloodYellow = 2;
    inline int bloodOf(std::uint8_t kind)
    {
        if (isGrunt(kind) || isPerson(kind) || kind == static_cast<std::uint8_t>(ActorKind::Barnacle))
            return kBloodRed;
        return isMonster(kind) ? kBloodYellow : kBloodNone;
    }
    /// How much a person takes before they are dead (the game's, at its
    /// middle difficulty).
    inline int personHealth(std::uint8_t kind)
    {
        return kind == static_cast<std::uint8_t>(ActorKind::Guard) ? 35 : 20;
    }
    inline bool isMine(std::uint8_t kind)
    {
        return kind == static_cast<std::uint8_t>(ActorKind::Tripmine);
    }

    /// What each kind of monster is, in numbers. The game's, at "normal".
    struct MonsterKind
    {
        int health;
        bool small;  ///< moves as the crouched hull (32 x 32 x 36), not the standing one
        fx half;     ///< half its width, for what hits it
        fx height;
        fx speed;    ///< units a second, if its clip does not say
        const char *idle;
        const char *move;
        const char *flinch;
        const char *die;
    };

    inline const MonsterKind &monsterKind(std::uint8_t kind)
    {
        static const MonsterKind kHeadcrab = {10, true, fxFromInt(12), fxFromInt(24), fxFromInt(140),
                                              "idle1", "run", "flinch", "dieback"};
        static const MonsterKind kZombie = {50, false, fxFromInt(16), fxFromInt(72), fxFromInt(45),
                                            "idle1", "walk", "flinchsmall", "diesimple"};
        static const MonsterKind kBarnacle = {25, true, fxFromInt(16), fxFromInt(32), 0,
                                              "idle1", "idle1", "flinch_sm", "death"};
        static const MonsterKind kSlave = {30, false, fxFromInt(16), fxFromInt(64), fxFromInt(90),
                                           "idle1", "walk1", "flinch2", "diesimple"};
        // (It goes only where something 64 units across has room:
        // monsterHull.)
        static const MonsterKind kBullsquid = {40, false, fxFromInt(20), fxFromInt(48), fxFromInt(110),
                                               "idle", "run", "flinchs", "die1"};
        static const MonsterKind kHoundeye = {20, true, fxFromInt(16), fxFromInt(36), fxFromInt(160),
                                              "idle1", "run", "flinch_small", "die"};
        // (A soldier: HlGrunt.hpp has the rest of him.)
        static const MonsterKind kGrunt = {50, false, fxFromInt(16), fxFromInt(72), fxFromInt(170),
                                           "idle1", "run", "smflinch", "diesimple"};
        if (kind == static_cast<std::uint8_t>(ActorKind::Grunt))
            return kGrunt;
        if (kind == static_cast<std::uint8_t>(ActorKind::Slave))
            return kSlave;
        if (kind == static_cast<std::uint8_t>(ActorKind::Bullsquid))
            return kBullsquid;
        if (kind == static_cast<std::uint8_t>(ActorKind::Houndeye))
            return kHoundeye;
        if (kind == static_cast<std::uint8_t>(ActorKind::Zombie))
            return kZombie;
        if (kind == static_cast<std::uint8_t>(ActorKind::Barnacle))
            return kBarnacle;
        return kHeadcrab;
    }

    inline constexpr int kHeadcrabBite = 10;
    inline constexpr int kZombieSlash = 20;
    inline constexpr int kZombieBothSlash = 40;
    inline constexpr int kBarnacleBite = 15; ///< each second it has hold of the player
    // What the newer kinds do to the player, and from how far.
    inline constexpr int kSlaveZap = 10;
    inline constexpr int kSlaveClaw = 8;
    inline constexpr int kSquidSpit = 10;
    inline constexpr int kSquidBite = 25;
    inline constexpr int kSquidWhip = 35;
    inline constexpr int kHoundBlast = 15;
    inline constexpr int kHoundBlastRadius = 256;
    inline constexpr int kGrenadeBlast = 100;
    inline constexpr int kGrenadeRadius = 250;
    inline constexpr fx kSpitSpeed = fxFromInt(900);

    // The attacks of the newer kinds (ActorState::attack): a clip, and the
    // frames of it on which something happens (the model's own events).
    inline constexpr std::uint8_t kAttackNone = 0;
    inline constexpr std::uint8_t kAttackSlaveZap = 1;
    inline constexpr std::uint8_t kAttackSlaveClaw = 2;
    inline constexpr std::uint8_t kAttackSquidSpit = 3;
    inline constexpr std::uint8_t kAttackSquidBite = 4;
    inline constexpr std::uint8_t kAttackSquidWhip = 5;
    inline constexpr std::uint8_t kAttackHoundBlast = 6;

    struct MonsterAttack
    {
        const char *clip;
        int frames[3]; ///< -1: none
        int cooldownMs;
    };
    inline const MonsterAttack &monsterAttack(std::uint8_t attack)
    {
        static const MonsterAttack kAttacks[] = {
            {"", {-1, -1, -1}, 0},
            {"zapattack1", {24, -1, -1}, 2000},
            {"attack1", {7, 13, 21}, 500},
            {"range", {11, -1, -1}, 2500},
            {"bite", {7, -1, -1}, 500},
            {"whip", {23, -1, -1}, 500},
            {"attack", {58, -1, -1}, 1500},
        };
        return kAttacks[attack <= kAttackHoundBlast ? attack : 0];
    }

    inline constexpr fx kSightRange = fxFromInt(1024);
    /// The size a kind walks about as (HlWalk.hpp kWalk*).
    inline int monsterHull(std::uint8_t kind)
    {
        if (kind == static_cast<std::uint8_t>(ActorKind::Bullsquid))
            return kWalkLarge;
        return monsterKind(kind).small ? kWalkCrouched : kWalkStanding;
    }

    inline constexpr fx kLeapRange = fxFromInt(256);
    inline constexpr fx kZombieReach = fxFromInt(70);

    // ------------------------------------------------------------- the player

    /// A monster's blow. Armour takes four fifths of it, and half of what it
    /// takes comes off the armour.
    /// `from`: where it came from, if it came from somewhere.
    inline void playerHurt(PlayerState &player, int damage, const fx *from = nullptr)
    {
        if (damage > 0)
        {
            player.hurt += damage;
            player.hurtKnown = from != nullptr;
            for (int i = 0; from != nullptr && i < 3; ++i)
                player.hurtFrom[i] = from[i];
        }
        int taken = (damage * 4) / 5;
        int cost = taken / 2;
        if (cost > player.inv.armor)
        {
            cost = player.inv.armor;
            taken = cost * 2;
        }
        player.inv.armor = static_cast<std::int16_t>(player.inv.armor - cost);
        player.health -= damage - taken;
    }

    namespace detail
    {
        /// Something heard from where a monster is.
        inline void monsterSound(LogicWorld &w, const ActorState &actor, CommonSound sound)
        {
            if (w.heard != nullptr)
                w.heard->add(sound, actor.pos, 127, kRadiusLarge);
        }
        /// ... the one that belongs to its kind: `which` is 0 for being
        /// roused, 1 for being hurt, 2 for dying.
        inline void monsterVoice(LogicWorld &w, const ActorState &actor, int which)
        {
            static const CommonSound kVoices[][3] = {
                {CommonSound::HeadcrabAlert, CommonSound::HeadcrabPain, CommonSound::HeadcrabDie},
                {CommonSound::ZombieAlert, CommonSound::ZombiePain, CommonSound::ZombiePain},
                {CommonSound::BarnacleAlert, CommonSound::BarnacleChew, CommonSound::BarnacleDie},
                {CommonSound::SlaveAlert, CommonSound::SlavePain, CommonSound::SlaveDie},
                {CommonSound::SquidAlert, CommonSound::SquidPain, CommonSound::SquidDie},
                {CommonSound::HoundAlert, CommonSound::HoundPain, CommonSound::HoundDie},
            };
            int row = 0;
            switch (static_cast<ActorKind>(actor.kind))
            {
            case ActorKind::Zombie:    row = 1; break;
            case ActorKind::Barnacle:  row = 2; break;
            case ActorKind::Slave:     row = 3; break;
            case ActorKind::Bullsquid: row = 4; break;
            case ActorKind::Houndeye:  row = 5; break;
            default:                   row = 0; break;
            }
            monsterSound(w, actor, kVoices[row][which]);
        }

        inline std::uint32_t chance(ActorWorld &a)
        {
            a.seed = a.seed * 1664525u + 1013904223u;
            return a.seed >> 8;
        }

        inline fx playerFeet(const PlayerState &player)
        {
            return player.origin[2] - (player.ducked ? kCrouchedFeet : kStandingFeet);
        }

        /// Whether a box with its feet at `feet` is in the player.
        inline bool touchesPlayer(const PlayerState &player, const fx *feet, fx half, fx height)
        {
            const fx mins[3] = {feet[0] - half, feet[1] - half, feet[2]};
            const fx maxs[3] = {feet[0] + half, feet[1] + half, feet[2] + height};
            return playerTouchesBox(player, mins, maxs, 0);
        }

    } // namespace detail

    // ------------------------------------------------------------- monsters

    namespace detail
    {
        inline void monsterClip(ActorWorld &a, ActorState &actor, const char *clip, bool once)
        {
            const ModelView *model = actorModel(a, actor);
            const int index = model != nullptr ? model->findClip(nameHash(clip)) : -1;
            // (Already in it, and it loops: let it run on.)
            if (index >= 0 && index == actor.clip && !once && !actor.once)
                return;
            if (!actorPlay(a, actor, nameHash(clip), once) && once)
            {
                // A clip this map's model was not given: it is over at once.
                actor.once = true;
                actor.finished = true;
            }
            actor.lastFrame = 0;
        }

        inline void monsterWake(ActorWorld &a, ActorState &actor)
        {
            if (actor.ai != kAiIdle)
                return;
            actor.ai = kAiChase;
            actor.thinkMs = 0;
            actor.hasGoal = false;
            actor.goalNode = actor.fromNode = -1;
            // (Where it was: where it goes back to if it loses them.)
            for (int i = 0; i < 3; ++i)
                actor.home[i] = actor.pos[i];
            actor.hasSeen = false;
            actor.goingHome = false;
            actor.lostMs = 0;
            actor.lookMs = 0;
            monsterClip(a, actor, monsterKind(actor.kind).move, false);
        }

        /// Whether the frame `at` of the actor's clip went by this tick.
        inline bool passedFrame(const ActorState &actor, int at)
        {
            const std::uint32_t frame = static_cast<std::uint32_t>(at) << 8;
            return actor.lastFrame < frame && frame <= actor.frame;
        }
    } // namespace detail

    // ------------------------------------------------ things seen, things thrown

    inline void effectsTick(LogicWorld &w, PlayerState &player, int ms);
    inline bool gibBurst(LogicWorld &w, int index);
    inline bool lineEntersBox(const fx *from, const fx *to, const fx *mins, const fx *maxs, std::int64_t &enter);
    inline void shakeStart(LogicWorld &w, fx amplitude, int ms);
    inline void explodeAt(LogicWorld &w, PlayerState &player, const fx *at, int size, bool fireball, bool sound,
                          bool hurts);
    inline void monsterHurt(LogicWorld &w, int index, int damage, bool bullet, int group = kHitBody);
    inline void turretHurt(LogicWorld &w, int index, int damage, bool bullet, int group);
    inline void gruntHurt(LogicWorld &w, int index, int damage, bool bullet, int group);
    inline void mineHurt(LogicWorld &w, int index);
    namespace detail
    {
        inline void gruntTick(LogicWorld &w, PlayerState &player, int index, int ms);
        inline void personTick(LogicWorld &w, PlayerState &player, int index, int ms);
        inline void mineTick(LogicWorld &w, PlayerState &player, int index, int ms);
    }
    inline bool fights(std::uint8_t kind);
    /// What a shot, a blow or a blast can hurt: what fights, and people.
    inline bool canBeHurt(std::uint8_t kind);
    inline void targetBox(const ActorState &actor, fx *mins, fx *maxs);
    /// Blood where something that bleeds was hurt at `at`: a puff of its
    /// colour, and a mark on the wall or floor behind it along `dir` (null:
    /// on the floor under it) -- more of it, and a pool, for the blow that
    /// killed.
    inline void bleed(LogicWorld &w, int index, const fx *at, const fx *dir, bool heavy);

    /// A blast at `origin`: hurts the player and every monster within
    /// `radius` units that it can reach in a straight line, less with
    /// distance (all of `damage` at the middle, none at the edge), and breaks
    /// what breaks. `except` is a monster it leaves alone (its own maker),
    /// or -1.
    inline void radiusDamage(LogicWorld &w, PlayerState &player, const fx *origin, int damage, int radius,
                             int except)
    {
        ActorWorld &a = *w.actors;
        auto share = [&](const fx *at) -> int
        {
            const fx d[3] = {at[0] - origin[0], at[1] - origin[1], at[2] - origin[2]};
            const int distance = fxToInt(fxLength(d));
            if (distance >= radius || !lineClear(*w.world, origin, at))
                return 0;
            return (damage * (radius - distance)) / radius;
        };
        if (player.health > 0)
        {
            const int hurt = share(player.origin);
            if (hurt > 0)
            {
                playerHurt(player, hurt);
                a.got |= kGotHurt;
            }
        }
        for (int i = 0; i < a.count; ++i)
        {
            const ActorState &actor = a.states[i];
            if (i == except || !actor.active || !canBeHurt(actor.kind))
                continue;
            fx mins[3], maxs[3];
            targetBox(actor, mins, maxs);
            const fx middle[3] = {actor.pos[0], actor.pos[1], (mins[2] + maxs[2]) / 2};
            if (actor.ai >= kAiDying)
            {
                // Something lying dead, caught in it: it comes apart (the
                // game's rule: a blast of more than what is left of it).
                if ((isMonster(actor.kind) || isGrunt(actor.kind)) && share(middle) >= 40)
                    gibBurst(w, i);
                continue;
            }
            const int hurt = share(middle);
            if (hurt > 0)
            {
                monsterHurt(w, i, hurt, false);
                // (Away from the blast.)
                fx away[3] = {middle[0] - origin[0], middle[1] - origin[1], middle[2] - origin[2]};
                const fx far = fxLength(away);
                for (int k = 0; k < 3 && far > 0; ++k)
                    away[k] = static_cast<fx>((static_cast<std::int64_t>(away[k]) << kFxShift) / far);
                bleed(w, i, middle, far > 0 ? away : nullptr, a.states[i].ai >= kAiDying || !a.states[i].active);
            }
        }
        for (int i = 0; i < w.recordCount; ++i)
        {
            const LogicRecord &record = w.records[i];
            if (w.states[i].dead || record.brush == kNoBrush || !detail::isBreakable(record))
                continue;
            const MoverHull &hull = w.movers[record.brush];
            const fx middle[3] = {(hull.mins[0] + hull.maxs[0]) / 2, (hull.mins[1] + hull.maxs[1]) / 2,
                                  (hull.mins[2] + hull.maxs[2]) / 2};
            const fx d[3] = {middle[0] - origin[0], middle[1] - origin[1], middle[2] - origin[2]};
            const int distance = fxToInt(fxLength(d));
            if (distance < radius)
                logicStrike(w, player, i, (damage * (radius - distance)) / radius);
        }
    }

    /// The monster at `index` comes apart: its body is gone, and the game's
    /// pieces fly -- an alien's four; a person's skull and four of the five
    /// others -- from inside its box, faster the further past dead it was
    /// taken. False (and nothing done) if the map has not got the pieces.
    inline bool gibBurst(LogicWorld &w, int index)
    {
        ActorWorld &a = *w.actors;
        ActorState &actor = a.states[index];
        const bool human = actor.kind == static_cast<std::uint8_t>(ActorKind::Grunt);
        const int model = human ? a.humanGibs : a.alienGibs;
        const int pieces = gibPieces(a, model);
        if (pieces == 0)
            return false;
        fx mins[3], maxs[3];
        targetBox(actor, mins, maxs);
        // (The game's: seven tenths as fast for something only just far
        // enough gone, twice for something well past, four times beyond.)
        const int scale = actor.health > -50 ? 7 : (actor.health > -200 ? 20 : 40);
        const int count = human ? 5 : 4;
        for (int n = 0; n < count; ++n)
        {
            // A person's first piece is the skull, from where the head was.
            const int body = human ? (n == 0 ? 0 : 1 + static_cast<int>(detail::chance(a) % 5)) % pieces
                                   : static_cast<int>(detail::chance(a) % static_cast<std::uint32_t>(pieces));
            fx at[3], velocity[3];
            for (int k = 0; k < 3; ++k)
            {
                const fx span = maxs[k] - mins[k];
                at[k] = mins[k] + static_cast<fx>((static_cast<std::int64_t>(span) * (detail::chance(a) & 255)) >> 8);
                velocity[k] = fxFromInt(static_cast<int>(detail::chance(a) % 201) - 100) * scale / 10;
            }
            if (human && n == 0)
                at[2] = maxs[2];
            velocity[2] = fxFromInt(200 + static_cast<int>(detail::chance(a) % 101)) * scale / 10;
            for (int k = 0; k < 3; ++k)
                velocity[k] = velocity[k] > fxFromInt(1500) ? fxFromInt(1500)
                                                           : (velocity[k] < -fxFromInt(1500) ? -fxFromInt(1500) : velocity[k]);
            Gib &gib = gibAdd(a, model, body, at, velocity,
                              (6000 + static_cast<int>(detail::chance(a) % 2000)) * a.lifePercent / 100);
            gib.yaw = static_cast<std::uint16_t>(detail::chance(a));
            gib.spin = static_cast<std::int16_t>(40 + detail::chance(a) % 60);
            effectAdd(a, kEffectBlood, at, nullptr, human ? 0 : 1, 300);
        }
        if (w.heard != nullptr)
            w.heard->add(CommonSound::BodySplat, actor.pos, 127, kRadiusLarge);
        if (actor.script >= 0 && actor.script < w.recordCount)
        {
            w.states[actor.script].dead = true;
            actor.script = -1;
            actor.scriptPhase = kActorFree;
        }
        actor.ai = kAiDead;
        actor.active = false;
        actor.flags &= static_cast<std::uint8_t>(~kActorSolid);
        actorPlaceBox(a, index);
        return true;
    }

    /// Hurt the monster at `index`. `bullet`: by a bullet, which a zombie
    /// hardly feels (three tenths, as in the game).
    /// `group`: the part of it that was hit (kHit*).
    inline void monsterHurt(LogicWorld &w, int index, int damage, bool bullet, int group)
    {
        ActorWorld &a = *w.actors;
        ActorState &actor = a.states[index];
        if (actor.active && isTurret(actor.kind))
        {
            turretHurt(w, index, damage, bullet, group);
            return;
        }
        if (actor.active && isGrunt(actor.kind))
        {
            gruntHurt(w, index, damage, bullet, group);
            return;
        }
        if (actor.active && isMine(actor.kind))
        {
            mineHurt(w, index);
            return;
        }
        if (actor.active && isPerson(actor.kind))
        {
            // One of the people of the place. They do not fight back: they
            // cry out, and die of enough of it -- the scene that had them
            // has lost them, and they lie where they fell.
            if (actor.ai >= kAiDying)
                return;
            if (group == kHitHead)
                damage *= 3;
            actor.health = static_cast<std::int16_t>(actor.health - damage);
            const bool guard = actor.kind == static_cast<std::uint8_t>(ActorKind::Guard);
            if (-actor.health < personHealth(actor.kind))
            {
                // (A cry no more than twice a second or so, as the game's
                // guard: not one for every pellet.)
                if (actor.painMs <= 0)
                {
                    actor.painMs = 600;
                    detail::monsterSound(w, actor, guard ? CommonSound::GuardPain : CommonSound::ScientistPain);
                }
                return;
            }
            detail::monsterSound(w, actor, guard ? CommonSound::GuardDie : CommonSound::ScientistDie);
            if (actor.script >= 0 && actor.script < w.recordCount)
            {
                w.states[actor.script].dead = true;
                actor.script = -1;
                actor.scriptPhase = kActorFree;
            }
            actor.ai = kAiDead;
            actor.flags &= static_cast<std::uint8_t>(~kActorSolid);
            actor.velocity[0] = actor.velocity[1] = actor.velocity[2] = 0;
            // (Falling, if this map's model was given how: else as they
            // stood, which is at least still.)
            detail::monsterClip(a, actor, "diesimple", true);
            actorPlaceBox(a, index);
            return;
        }
        if (!actor.active || !isMonster(actor.kind) || actor.ai >= kAiDying)
            return;
        // A hit to the head counts three times over (before whatever else
        // the kind makes of it).
        if (group == kHitHead)
            damage *= 3;
        if (bullet && actor.kind == static_cast<std::uint8_t>(ActorKind::Zombie))
            damage = (damage * 3 + 9) / 10;
        actor.health = static_cast<std::int16_t>(actor.health - damage);
        const MonsterKind &kind = monsterKind(actor.kind);
        // Taken far past dead (the game's thirty under): it comes apart.
        if (actor.health < -30 && gibBurst(w, index))
            return;
        if (actor.health <= 0)
        {
            // A script that had it has lost it.
            if (actor.script >= 0 && actor.script < w.recordCount)
            {
                w.states[actor.script].dead = true;
                actor.script = -1;
                actor.scriptPhase = kActorFree;
            }
            actor.ai = kAiDying;
            detail::monsterVoice(w, actor, 2);
            actor.flags &= static_cast<std::uint8_t>(~kActorSolid);
            actor.velocity[0] = actor.velocity[1] = actor.velocity[2] = 0;
            actor.reach = 0;
            detail::monsterClip(a, actor, kind.die, true);
            actorPlaceBox(a, index);
            return;
        }
        detail::monsterVoice(w, actor, 1);
        if (actor.script >= 0 || (actor.flags & kActorPrisoner))
            return;
        const bool wasIdle = actor.ai == kAiIdle;
        detail::monsterWake(a, actor);
        // It flinches, if it was doing nothing that cannot be broken off.
        if ((wasIdle || actor.ai == kAiChase) && (detail::chance(a) & 1) &&
            actor.kind != static_cast<std::uint8_t>(ActorKind::Barnacle))
        {
            actor.ai = kAiFlinch;
            detail::monsterClip(a, actor, kind.flinch, true);
        }
    }

    namespace detail
    {
        inline void headcrabLeap(LogicWorld &w, const PlayerState &player, int index)
        {
            ActorWorld &a = *w.actors;
            ActorState &actor = a.states[index];
            // At the player's eyes, by the game's sum: up fast enough to
            // reach their height (16 units at the least), and across in the
            // time that takes; not faster than 650 in all.
            const fx to[3] = {player.origin[0] - actor.pos[0], player.origin[1] - actor.pos[1],
                              player.origin[2] + player.viewHeight - actor.pos[2]};
            fx height = to[2] < fxFromInt(16) ? fxFromInt(16) : to[2];
            // v = sqrt(2 g h); both in units, so the root is too.
            const fx up = fxFromInt(static_cast<int>(
                isqrt64(static_cast<std::uint64_t>(2) * 800 * static_cast<std::uint64_t>(fxToInt(height)))));
            // The time to the top, in milliseconds.
            const std::int64_t ms = (static_cast<std::int64_t>(up) * 1000) / kGravity;
            actor.velocity[0] = static_cast<fx>((static_cast<std::int64_t>(to[0]) * 1000) / (ms > 0 ? ms : 1));
            actor.velocity[1] = static_cast<fx>((static_cast<std::int64_t>(to[1]) * 1000) / (ms > 0 ? ms : 1));
            actor.velocity[2] = up;
            const fx length = fxLength(actor.velocity);
            if (length > fxFromInt(650))
            {
                for (int i = 0; i < 3; ++i)
                    actor.velocity[i] = static_cast<fx>(
                        (static_cast<std::int64_t>(actor.velocity[i]) * fxFromInt(650)) / length);
            }
            actor.yaw = fxAtan2Turns(to[1], to[0]);
            actor.ai = kAiLeap;
            actor.struck = 0;
            actor.thinkMs = 3000; // the longest a leap may last
            monsterSound(w, actor, CommonSound::HeadcrabAttack);
            monsterClip(a, actor, "jump", true);
        }

        inline void headcrabFly(LogicWorld &w, PlayerState &player, int index, int ms)
        {
            ActorWorld &a = *w.actors;
            ActorState &actor = a.states[index];
            const MonsterKind &kind = monsterKind(actor.kind);
            const fx from[3] = {actor.pos[0], actor.pos[1], actor.pos[2] + kCrouchedFeet};
            fx to[3];
            for (int i = 0; i < 3; ++i)
                to[i] = from[i] + static_cast<fx>((static_cast<std::int64_t>(actor.velocity[i]) * ms) / 1000);
            actor.velocity[2] -= static_cast<fx>((static_cast<std::int64_t>(kGravity) * ms) / 1000);
            const Trace t = walkerTrace(*w.world, &a, index, true, from, to);
            bool landed = false;
            if (!t.startSolid)
            {
                actor.pos[0] = t.endPos[0];
                actor.pos[1] = t.endPos[1];
                actor.pos[2] = t.endPos[2] - kCrouchedFeet;
            }
            if (t.startSolid || t.fraction < kFractionOne)
            {
                // The floor ends it; a wall or a ceiling only stops it going
                // that way.
                if (t.startSolid || t.normal[2] > kFloorNormalZ)
                    landed = true;
                else
                {
                    actor.velocity[0] = actor.velocity[1] = 0;
                    if (actor.velocity[2] > 0)
                        actor.velocity[2] = 0;
                }
            }
            if (!actor.struck && touchesPlayer(player, actor.pos, kind.half, kind.height))
            {
                actor.struck = 1;
                monsterSound(w, actor, CommonSound::HeadcrabBite);
                playerHurt(player, kHeadcrabBite, actor.pos);
                a.got |= kGotHurt;
                actor.velocity[0] = actor.velocity[1] = 0;
            }
            actor.thinkMs -= ms;
            if (landed || actor.thinkMs <= 0)
            {
                actor.ai = kAiChase;
                actor.cooldownMs = 2000;
                actor.thinkMs = 0;
                actor.velocity[0] = actor.velocity[1] = actor.velocity[2] = 0;
                monsterClip(a, actor, kind.move, false);
            }
        }

        inline void barnacleTick(LogicWorld &w, PlayerState &player, int index, int ms)
        {
            ActorWorld &a = *w.actors;
            ActorState &actor = a.states[index];
            // Its feet are its mouth, on the ceiling; the tongue hangs from
            // there to the floor (found once, when it first looks).
            if (actor.ai == kAiIdle && actor.reach == 0)
            {
                const fx from[3] = {actor.pos[0], actor.pos[1], actor.pos[2] - fxFromInt(4)};
                const fx to[3] = {actor.pos[0], actor.pos[1], actor.pos[2] - fxFromInt(1024)};
                const Shot down = traceShot(*w.world, from, to);
                actor.reach = static_cast<fx>((static_cast<std::int64_t>(fxFromInt(1020)) * down.fraction) >>
                                              kFractionShift) -
                              fxFromInt(8);
                if (actor.reach < fxFromInt(8))
                    actor.reach = fxFromInt(8);
                actor.goal[2] = actor.reach; // how long it is when let all the way down
            }
            const fx dx = player.origin[0] - actor.pos[0];
            const fx dy = player.origin[1] - actor.pos[1];
            const bool under = dx < fxFromInt(20) && dx > -fxFromInt(20) && dy < fxFromInt(20) &&
                               dy > -fxFromInt(20);
            const fx head = player.origin[2] + (player.ducked ? kCrouchedFeet : kStandingFeet);
            if (actor.ai == kAiIdle)
            {
                // Let down again, if it was pulled up.
                if (actor.reach < actor.goal[2])
                    actor.reach += static_cast<fx>((static_cast<std::int64_t>(fxFromInt(60)) * ms) / 1000);
                // Touched: anywhere between the mouth and the tip.
                if (under && head > actor.pos[2] - actor.reach - fxFromInt(8) && head < actor.pos[2])
                {
                    actor.ai = kAiAttack;
                    actor.thinkMs = 0;
                    monsterSound(w, actor, CommonSound::BarnacleAlert);
                    monsterClip(a, actor, "attack1", false);
                }
                return;
            }
            // It has them: up they go, and at the top it bites.
            if (!under || player.health <= 0)
            {
                actor.ai = kAiIdle;
                monsterClip(a, actor, "idle1", false);
                return;
            }
            const fx top = actor.pos[2] - fxFromInt(36);
            if (head < top)
            {
                const fx lift = static_cast<fx>((static_cast<std::int64_t>(fxFromInt(50)) * ms) / 1000);
                fx to[3] = {actor.pos[0], actor.pos[1], player.origin[2] + lift};
                const Trace t = traceAll(*w.world, player.ducked, player.origin, to);
                if (!t.startSolid)
                {
                    for (int i = 0; i < 3; ++i)
                        player.origin[i] = t.endPos[i];
                }
                player.velocity[0] = player.velocity[1] = player.velocity[2] = 0;
                player.onGround = false;
                actor.reach = actor.pos[2] - head;
                if (t.fraction >= kFractionOne)
                    return;
            }
            player.velocity[0] = player.velocity[1] = 0;
            player.velocity[2] = 0;
            actor.thinkMs -= ms;
            if (actor.thinkMs <= 0)
            {
                actor.thinkMs = 1000;
                monsterClip(a, actor, "chewcycle", false);
                monsterSound(w, actor, CommonSound::BarnacleChew);
                playerHurt(player, kBarnacleBite, actor.pos);
                a.got |= kGotHurt;
            }
        }

        /// Which of its attacks one of the newer kinds makes from where it
        /// is, if any: `flat` and `dz` are how far off the player is, `sees`
        /// whether there is a clear line to them.
        inline std::uint8_t chooseAttack(ActorWorld &a, const ActorState &actor, fx flat, fx dz, bool sees)
        {
            const bool level = dz < fxFromInt(64) && dz > -fxFromInt(64);
            switch (static_cast<ActorKind>(actor.kind))
            {
            case ActorKind::Slave:
                if (flat < fxFromInt(60) && level)
                    return kAttackSlaveClaw;
                if (sees && actor.cooldownMs <= 0 && flat < fxFromInt(784))
                    return kAttackSlaveZap;
                break;
            case ActorKind::Bullsquid:
                if (flat < fxFromInt(85) && level)
                    return (chance(a) & 1) ? kAttackSquidBite : kAttackSquidWhip;
                if (sees && actor.cooldownMs <= 0 && flat > fxFromInt(128) && flat < fxFromInt(784))
                    return kAttackSquidSpit;
                break;
            case ActorKind::Houndeye:
                if (actor.cooldownMs <= 0 && flat < fxFromInt(100) && level)
                    return kAttackHoundBlast;
                break;
            default:
                break;
            }
            return kAttackNone;
        }

        /// The moment of an attack: frame `n` of its frames has come.
        inline void attackLands(LogicWorld &w, PlayerState &player, int index, fx flat, fx dz,
                                const fx *eye, const fx *theirs)
        {
            ActorWorld &a = *w.actors;
            ActorState &actor = a.states[index];
            const bool reach = flat < kZombieReach && dz < fxFromInt(64) && dz > -fxFromInt(64);
            auto hurt = [&](int damage)
            {
                playerHurt(player, damage, actor.pos);
                a.got |= kGotHurt;
            };
            switch (actor.attack)
            {
            case kAttackSlaveZap:
            {
                // The bolt goes where they are now: if they are still to be
                // seen, it has them.
                monsterSound(w, actor, CommonSound::SlaveZap);
                const Shot shot = traceShot(*w.world, eye, theirs);
                fx end[3];
                for (int i = 0; i < 3; ++i)
                    end[i] = eye[i] + static_cast<fx>((static_cast<std::int64_t>(theirs[i] - eye[i]) *
                                                       shot.fraction) >> kFractionShift);
                // (Where it lands on someone is their chest, not the eye they
                // see it with: a bolt drawn into the eye itself is a smear
                // across the whole picture.)
                if (shot.fraction >= kFractionOne)
                    end[2] -= fxFromInt(20);
                effectAdd(a, kEffectBolt, eye, end, 0, 200);
                if (shot.fraction >= kFractionOne && player.health > 0)
                    hurt(kSlaveZap);
                break;
            }
            case kAttackSlaveClaw:
                monsterSound(w, actor, reach ? CommonSound::ClawStrike : CommonSound::ClawMiss);
                if (reach)
                    hurt(kSlaveClaw);
                break;
            case kAttackSquidSpit:
            {
                monsterSound(w, actor, CommonSound::SquidSpit);
                const fx d[3] = {theirs[0] - eye[0], theirs[1] - eye[1], theirs[2] - eye[2]};
                const fx length = fxLength(d);
                if (length > 0)
                {
                    fx velocity[3];
                    for (int i = 0; i < 3; ++i)
                        velocity[i] = static_cast<fx>((static_cast<std::int64_t>(d[i]) * kSpitSpeed) / length);
                    projectileAdd(a, kShotSpit, eye, velocity, 3000);
                }
                break;
            }
            case kAttackSquidBite:
                monsterSound(w, actor, CommonSound::SquidBite);
                if (reach)
                    hurt(kSquidBite);
                break;
            case kAttackSquidWhip:
                monsterSound(w, actor, CommonSound::SquidBite);
                if (reach)
                {
                    // The tail throws them: away from it, and up.
                    hurt(kSquidWhip);
                    fx sine, cosine;
                    fxSinCos(actor.yaw, sine, cosine);
                    player.velocity[0] += cosine * 250;
                    player.velocity[1] += sine * 250;
                    player.velocity[2] = fxFromInt(250);
                    player.onGround = false;
                }
                break;
            case kAttackHoundBlast:
            {
                monsterSound(w, actor, CommonSound::HoundBlast);
                const fx middle[3] = {actor.pos[0], actor.pos[1], actor.pos[2] + fxFromInt(16)};
                effectAdd(a, kEffectRing, middle, nullptr, kHoundBlastRadius, 300);
                radiusDamage(w, player, middle, kHoundBlast, kHoundBlastRadius, index);
                break;
            }
            default:
                break;
            }
        }

        inline void monsterTick(LogicWorld &w, PlayerState &player, int index, int ms)
        {
            ActorWorld &a = *w.actors;
            ActorState &actor = a.states[index];
            const MonsterKind &kind = monsterKind(actor.kind);
            const MoveWorld &world = *w.world;
            if (actor.health == 0 && actor.ai == kAiIdle)
            {
                // Its first tick: what it has to lose, and how fast it goes.
                actor.health = static_cast<std::int16_t>(kind.health);
                actor.speed = actorClipSpeed(a, actor, nameHash(kind.move), kind.speed);
                actor.thinkMs = (index * 37) % 300;
                actor.restClip = actor.clip;
            }
            if (actor.ai == kAiDead)
                return;
            if (actor.ai == kAiDying)
            {
                if (actor.finished)
                    actor.ai = kAiDead;
                return;
            }
            // A script's, for now.
            if (actor.script >= 0)
                return;
            if (actor.cooldownMs > 0)
                actor.cooldownMs -= ms;
            if (actor.kind == static_cast<std::uint8_t>(ActorKind::Barnacle))
            {
                barnacleTick(w, player, index, ms);
                return;
            }

            const fx target[3] = {player.origin[0], player.origin[1], playerFeet(player)};
            const fx d[3] = {target[0] - actor.pos[0], target[1] - actor.pos[1], 0};
            const fx flat = fxLength(d);
            const fx dz = target[2] - actor.pos[2];
            const fx eye[3] = {actor.pos[0], actor.pos[1], actor.pos[2] + kind.height - fxFromInt(4)};
            const fx theirs[3] = {player.origin[0], player.origin[1],
                                  player.origin[2] + player.viewHeight};

            switch (actor.ai)
            {
            case kAiIdle:
            {
                actor.thinkMs -= ms;
                if (actor.thinkMs > 0)
                    break;
                actor.thinkMs = 300;
                // (A prisoner goes for nobody.)
                if (player.health <= 0 || flat > kSightRange || dz > fxFromInt(512) ||
                    dz < -fxFromInt(512) || (actor.flags & kActorPrisoner))
                    break;
                // In front of it, or near enough to be heard.
                fx sine, cosine;
                fxSinCos(actor.yaw, sine, cosine);
                const std::int64_t ahead = static_cast<std::int64_t>(d[0]) * cosine +
                                           static_cast<std::int64_t>(d[1]) * sine;
                // (Someone moving is heard from twice as far as someone
                // standing still.)
                const fx speed[3] = {player.velocity[0], player.velocity[1], 0};
                const fx heard = fxLength(speed) > fxFromInt(100) ? fxFromInt(400) : fxFromInt(192);
                if (ahead < 0 && flat > heard)
                    break;
                if (lineClear(world, eye, theirs))
                {
                    monsterWake(a, actor);
                    monsterVoice(w, actor, 0);
                }
                break;
            }
            case kAiChase:
            {
                if (player.health <= 0)
                {
                    actor.ai = kAiIdle;
                    monsterClip(a, actor, kind.idle, false);
                    break;
                }
                // In reach?
                if (actor.kind == static_cast<std::uint8_t>(ActorKind::Zombie))
                {
                    if (flat < fxFromInt(56) && dz < fxFromInt(48) && dz > -fxFromInt(48))
                    {
                        actor.ai = kAiAttack;
                        actor.struck = 0;
                        actor.yaw = fxAtan2Turns(d[1], d[0]);
                        monsterSound(w, actor, CommonSound::ZombieAttack);
                        monsterClip(a, actor, (chance(a) & 1) ? "attack1" : "attack2", true);
                        break;
                    }
                }
                else if (actor.kind == static_cast<std::uint8_t>(ActorKind::Headcrab))
                {
                    if (actor.cooldownMs <= 0 && flat < kLeapRange && dz < fxFromInt(160) &&
                        dz > -fxFromInt(96) && lineClear(world, eye, theirs))
                    {
                        headcrabLeap(w, player, index);
                        break;
                    }
                }
                else
                {
                    // The newer kinds: an attack from a distance needs them
                    // in sight, which is looked for only when it could be
                    // made (a line through the world is not free).
                    const bool far = actor.cooldownMs <= 0 && flat < fxFromInt(784) &&
                                     actor.kind != static_cast<std::uint8_t>(ActorKind::Houndeye);
                    const bool sees = far && lineClear(world, eye, theirs);
                    const std::uint8_t attack = chooseAttack(a, actor, flat, dz, sees);
                    if (attack != kAttackNone)
                    {
                        actor.ai = kAiAttack;
                        actor.attack = attack;
                        actor.struck = 0;
                        actor.yaw = fxAtan2Turns(d[1], d[0]);
                        if (attack == kAttackHoundBlast)
                            monsterSound(w, actor, CommonSound::HoundAttack);
                        monsterClip(a, actor, monsterAttack(attack).clip, true);
                        break;
                    }
                }
                const fx speed = actor.speed > 0 ? actor.speed : kind.speed;
                // Does it see them? Looked at twice a second. While it does,
                // it is them it makes for; once it does not, where it last
                // did -- and it gets there, looks, and after a while gives
                // them up and goes back to where it was.
                actor.lookMs -= ms;
                if (actor.lookMs <= 0)
                {
                    actor.lookMs = 500;
                    if (lineClear(world, eye, theirs))
                    {
                        for (int i = 0; i < 3; ++i)
                            actor.seen[i] = target[i];
                        actor.hasSeen = true;
                        actor.lostMs = 0;
                        actor.goingHome = false;
                    }
                    else
                    {
                        actor.lostMs += 500;
                    }
                }
                if (actor.goingHome)
                {
                    const fx back[3] = {actor.home[0] - actor.pos[0], actor.home[1] - actor.pos[1], 0};
                    const int went = fxLength(back) < fxFromInt(40)
                                         ? kWalkNoWay
                                         : walkTick(a, world, index, monsterHull(actor.kind), actor.home, speed, ms,
                                                    &player, kind.half, kind.height);
                    if (went == kWalkNoWay || actor.lostMs > 40000)
                    {
                        // Home (or as near as there is a way): as it was.
                        actor.ai = kAiIdle;
                        actor.goingHome = false;
                        monsterClip(a, actor, kind.idle, false);
                        break;
                    }
                    monsterClip(a, actor, kind.move, false);
                    break;
                }
                const bool lost = actor.hasSeen && actor.lostMs > 0;
                const fx *making = lost ? actor.seen : target;
                const fx gap[3] = {making[0] - actor.pos[0], making[1] - actor.pos[1], 0};
                const bool there = lost && fxLength(gap) < fxFromInt(48);
                const int went = there ? kWalkNoWay
                                       : walkTick(a, world, index, monsterHull(actor.kind), making, speed, ms, &player,
                                                  kind.half, kind.height);
                if (went == kWalkNoWay)
                {
                    // Nowhere to go, or where it last saw them and they are
                    // not there: it waits, looking -- and not for ever.
                    faceTo(actor, d[0], d[1], ms, 32768);
                    monsterClip(a, actor, kind.idle, false);
                    if (actor.lostMs > 6000)
                    {
                        actor.goingHome = true;
                        actor.thinkMs = 0;
                        actor.hasGoal = false;
                        actor.goalNode = actor.fromNode = -1;
                    }
                    break;
                }
                monsterClip(a, actor, kind.move, false);
                break;
            }
            case kAiAttack:
            {
                if (actor.attack != kAttackNone)
                {
                    // One of the newer kinds': whatever its frames bring.
                    const MonsterAttack &attack = monsterAttack(actor.attack);
                    faceTo(actor, d[0], d[1], ms, 32768);
                    for (int n = 0; n < 3; ++n)
                    {
                        if (attack.frames[n] < 0 || (actor.struck & (1 << n)) ||
                            !passedFrame(actor, attack.frames[n]))
                            continue;
                        actor.struck |= static_cast<std::uint8_t>(1 << n);
                        attackLands(w, player, index, flat, dz, eye, theirs);
                    }
                    if (actor.finished)
                    {
                        actor.cooldownMs = attack.cooldownMs;
                        actor.attack = kAttackNone;
                        actor.ai = kAiChase;
                        actor.thinkMs = 0;
                        monsterClip(a, actor, kind.move, false);
                    }
                    break;
                }
                // A zombie's swing: one arm then the other, or both at once.
                // The frames are the model's own events.
                const ModelView *model = actorModel(a, actor);
                const bool both = model != nullptr && actor.clip >= 0 &&
                                  actor.clip == model->findClip(nameHash("attack2"));
                const int at[2] = {both ? 6 : 10, both ? -1 : 19};
                for (int n = 0; n < 2; ++n)
                {
                    if (at[n] < 0 || (actor.struck & (1 << n)) || !passedFrame(actor, at[n]))
                        continue;
                    actor.struck |= static_cast<std::uint8_t>(1 << n);
                    const bool lands = flat < kZombieReach && dz < fxFromInt(64) && dz > -fxFromInt(64);
                    monsterSound(w, actor, lands ? CommonSound::ClawStrike : CommonSound::ClawMiss);
                    if (lands)
                    {
                        playerHurt(player, both ? kZombieBothSlash : kZombieSlash, actor.pos);
                        a.got |= kGotHurt;
                    }
                }
                if (actor.finished)
                {
                    actor.ai = kAiChase;
                    actor.thinkMs = 0;
                    monsterClip(a, actor, kind.move, false);
                }
                break;
            }
            case kAiLeap:
                headcrabFly(w, player, index, ms);
                break;
            case kAiFlinch:
                if (actor.finished)
                {
                    actor.ai = kAiChase;
                    actor.thinkMs = 0;
                    monsterClip(a, actor, kind.move, false);
                }
                break;
            default:
                break;
            }
        }

        // ----------------------------------------------------------- items

        /// The player walks over an item: true if it was taken.
        inline bool itemTake(PlayerState &player, std::uint8_t item, std::uint32_t &got)
        {
            Inventory &inv = player.inv;
            auto add = [](std::int16_t &what, int amount, int most)
            {
                if (what >= most)
                    return false;
                what = static_cast<std::int16_t>(what + amount > most ? most : what + amount);
                return true;
            };
            auto weapon = [&](std::uint8_t which)
            {
                if (inv.have & (1u << which))
                    return false;
                inv.have |= static_cast<std::uint8_t>(1u << which);
                inv.current = which;
                got |= kGotWeapon;
                return true;
            };
            switch (static_cast<ItemKind>(item))
            {
            case ItemKind::Crowbar:
                return weapon(kWeaponCrowbar);
            case ItemKind::Glock:
                // A new one comes loaded; another is its rounds.
                if (weapon(kWeaponGlock))
                {
                    inv.clipGlock = 17;
                    return true;
                }
                return add(inv.ammo9mm, 17, 250);
            case ItemKind::Shotgun:
                if (weapon(kWeaponShotgun))
                {
                    inv.clipShotgun = 8;
                    add(inv.ammoBuck, 4, 125);
                    return true;
                }
                return add(inv.ammoBuck, 12, 125);
            case ItemKind::GlockClip:
                return add(inv.ammo9mm, 17, 250);
            case ItemKind::Mp5:
                // (Twenty-five rounds with it, as the game gives.)
                if (weapon(kWeaponMp5))
                {
                    inv.clipMp5 = 25;
                    return true;
                }
                return add(inv.ammo9mm, 25, 250);
            case ItemKind::Python:
                if (weapon(kWeaponPython))
                {
                    inv.clipPython = 6;
                    return true;
                }
                return add(inv.ammo357, 6, 36);
            case ItemKind::Mp5Clip:
                return add(inv.ammo9mm, 50, 250);
            case ItemKind::AmmoBox:
                return add(inv.ammo9mm, 200, 250);
            case ItemKind::ArGrenades:
                return add(inv.ammoAr, 2, 10);
            case ItemKind::Ammo357:
                return add(inv.ammo357, 6, 36);
            case ItemKind::Buckshot:
                return add(inv.ammoBuck, 12, 125);
            case ItemKind::HealthKit:
                if (player.health >= 100)
                    return false;
                player.health = player.health + 15 > 100 ? 100 : player.health + 15;
                return true;
            case ItemKind::Battery:
                // Armour is the suit's: without one a battery is left lying.
                return (inv.have & kHaveSuit) != 0 && add(inv.armor, 15, 100);
            case ItemKind::Grenades:
                if (!add(inv.grenades, 5, 10))
                    return false;
                if (!(inv.have & (1u << kWeaponGrenade)))
                {
                    inv.have |= static_cast<std::uint8_t>(1u << kWeaponGrenade);
                    got |= kGotWeapon;
                }
                return true;
            case ItemKind::Suit:
                if (inv.have & kHaveSuit)
                    return false;
                inv.have |= kHaveSuit;
                return true;
            default:
                return false;
            }
        }

        // --------------------------------------------- makers and chargers

        inline void makerTick(LogicWorld &w, PlayerState &player, int index, int ms)
        {
            const LogicRecord &record = w.records[index];
            LogicState &state = w.states[index];
            ActorWorld &a = *w.actors;
            if (state.dead)
                return;
            bool make = false;
            if (record.flags & kLogicToggle)
            {
                make = state.phase > 0;
            }
            else if (state.running)
            {
                state.speed -= ms;
                make = state.speed <= 0;
            }
            if (!make)
                return;
            // A place of its own that is free: never used, or whose last
            // tenant is dead and has lain a while.
            const int first = static_cast<int>(record.extra[0]);
            const int count = static_cast<int>(record.extra[1]);
            int slot = -1;
            for (int n = 0; n < count && first + n < a.recordCount; ++n)
            {
                const ActorState &there = a.states[first + n];
                if (!there.active || there.ai == kAiDead)
                {
                    slot = first + n;
                    break;
                }
            }
            if (slot < 0)
            {
                // All of its children are about: it waits for one to die.
                state.speed = 500;
                return;
            }
            // Not on top of the player.
            const ActorRecord &born = a.records[slot];
            const MonsterKind &kind = monsterKind(born.kind);
            if (touchesPlayer(player, born.origin, kind.half + fxFromInt(8), kind.height))
            {
                state.speed = 500;
                return;
            }
            actorSetup(a, slot, born, *w.world);
            a.states[slot].flags &= static_cast<std::uint8_t>(~kActorDormant);
            actorPlaceBox(a, slot);
            if (state.phase > 0)
                --state.phase;
            state.speed = record.waitMs > 0 ? record.waitMs : 1000;
            ++state.bits;
            logicUseTargets(w, player, index, kUseToggle, 0);
            if (record.extra[2] != 0 && state.bits >= record.extra[2])
                state.dead = true;
        }

        inline void chargerTick(LogicWorld &w, PlayerState &player, int index, int ms)
        {
            const LogicRecord &record = w.records[index];
            LogicState &state = w.states[index];
            if (!state.running)
                return;
            const bool suit = record.kind == static_cast<std::uint8_t>(LogicKind::SuitCharger);
            // For as long as they stay at it, it has something left, and
            // they have room for it.
            bool near = record.brush != kNoBrush;
            if (near)
            {
                const MoverHull &mover = w.movers[record.brush];
                near = playerTouchesBox(player, mover.mins, mover.maxs, kUseRange);
            }
            const bool full = suit ? (player.inv.armor >= 100 || !(player.inv.have & kHaveSuit))
                                   : player.health >= 100;
            if (!near || full || state.bits >= record.extra[0] || player.health <= 0)
            {
                state.running = false;
                return;
            }
            // (Its sound once a second while it gives.)
            if (w.heard != nullptr && state.bits % 10 == 0 && state.speed - ms <= 0)
                w.heard->add(CommonSound::Charge, nullptr, 100);
            state.speed -= ms;
            while (state.speed <= 0 && state.bits < record.extra[0])
            {
                state.speed += 100; // a point every tenth of a second
                ++state.bits;
                if (suit)
                    player.inv.armor = static_cast<std::int16_t>(player.inv.armor < 100 ? player.inv.armor + 1 : 100);
                else
                    player.health = player.health < 100 ? player.health + 1 : 100;
            }
        }
    } // namespace detail

    namespace detail
    {
        inline void projectilesTick(LogicWorld &w, PlayerState &player, int ms)
        {
            ActorWorld &a = *w.actors;
            for (Effect &effect : a.effects)
            {
                if (effect.kind == 0)
                    continue;
                effect.leftMs = static_cast<std::int16_t>(effect.leftMs - ms);
                if (effect.leftMs <= 0)
                    effect.kind = 0;
            }
            // The pieces of what has come apart: they fall, bounce (losing
            // nearly half their speed each time), turn while they fly, lie
            // where they stop, and are gone when their time is up.
            for (Gib &gib : a.gibs)
            {
                if (!gib.active)
                    continue;
                if ((gib.leftMs -= ms) <= 0)
                {
                    gib.active = false;
                    continue;
                }
                if (gib.resting)
                    continue;
                gib.velocity[2] -= static_cast<fx>((static_cast<std::int64_t>(kGravity) * ms) / 1000);
                fx to[3];
                for (int i = 0; i < 3; ++i)
                    to[i] = gib.pos[i] + static_cast<fx>((static_cast<std::int64_t>(gib.velocity[i]) * ms) / 1000);
                gib.yaw = static_cast<std::uint16_t>(gib.yaw + gib.spin * ms);
                gib.pitch = static_cast<std::uint16_t>(gib.pitch + (gib.spin / 2) * ms);
                const Trace t = traceAll(*w.world, true, gib.pos, to);
                if (t.startSolid)
                {
                    // Thrown from inside something (a monster hard against a
                    // wall): moved towards the open a little at a time, and
                    // given up if it never gets there.
                    gib.pos[2] += fxFromInt(4);
                    if (gib.leftMs > 600)
                        gib.leftMs -= 2000;
                    continue;
                }
                if (t.fraction < kFractionOne)
                {
                    std::int64_t into = 0;
                    for (int i = 0; i < 3; ++i)
                        into += static_cast<std::int64_t>(gib.velocity[i]) * t.normal[i];
                    into >>= kNormalShift;
                    for (int i = 0; i < 3; ++i)
                    {
                        const fx off = static_cast<fx>((into * t.normal[i] * 2) >> kNormalShift);
                        gib.velocity[i] = ((gib.velocity[i] - off) * 11) / 20;
                        to[i] = t.endPos[i];
                    }
                    gib.spin = static_cast<std::int16_t>(gib.spin / 2);
                    // On something flat enough to lie on, and slow: it lies.
                    if (t.normal[2] > kNormalOne / 10 * 7 && fxLength(gib.velocity) < fxFromInt(60))
                    {
                        gib.resting = true;
                        gib.pitch = 0;
                        // (Where a piece of someone lands, it leaves some.)
                        const bool alien = static_cast<int>(gib.model) == a.alienGibs;
                        if (static_cast<int>(gib.model) == a.humanGibs || alien)
                            markFloor(a, *w.world, gib.pos, 24, alien ? kDecalAlien2 : kDecalBlood, 13,
                                      alien ? kMarkAlien : kMarkBlood);
                    }
                }
                for (int i = 0; i < 3; ++i)
                    gib.pos[i] = to[i];
            }
            for (Projectile &shot : a.shots)
            {
                if (!shot.active)
                    continue;
                shot.leftMs -= ms;
                const bool gib = shot.kind == kShotGib;
                if (shot.kind == kShotContact)
                {
                    // A launcher's grenade: an arc at half gravity, and off
                    // at the first thing it meets -- the world, or anything
                    // that fights, or (a soldier's) the player.
                    shot.velocity[2] -= static_cast<fx>((static_cast<std::int64_t>(kGravity / 2) * ms) / 1000);
                    fx to[3];
                    for (int i = 0; i < 3; ++i)
                        to[i] = shot.pos[i] + static_cast<fx>((static_cast<std::int64_t>(shot.velocity[i]) * ms) / 1000);
                    const Shot line = traceShot(*w.world, shot.pos, to);
                    std::int64_t reach = line.fraction;
                    bool met = line.fraction < kFractionOne || shot.leftMs <= 0;
                    for (int i = 0; i < a.count; ++i)
                    {
                        const ActorState &actor = a.states[i];
                        if (!actor.active || !fights(actor.kind) || actor.ai >= kAiDying)
                            continue;
                        fx mins[3], maxs[3];
                        targetBox(actor, mins, maxs);
                        std::int64_t enter;
                        if (lineEntersBox(shot.pos, to, mins, maxs, enter) && enter < reach && shot.leftMs < 9900)
                        {
                            reach = enter;
                            met = true;
                        }
                    }
                    if (shot.sprite == 1 && player.health > 0 && playerTouchesBox(player, to, to, fxFromInt(8)))
                        met = true;
                    for (int i = 0; i < 3; ++i)
                        shot.pos[i] += static_cast<fx>((static_cast<std::int64_t>(to[i] - shot.pos[i]) * reach) >>
                                                       kFractionShift);
                    if (met)
                    {
                        shot.active = false;
                        explodeAt(w, player, shot.pos, kGrenadeBlast, true, true, true);
                    }
                    continue;
                }
                const bool grenade = shot.kind == kShotGrenade || gib;
                if (grenade)
                    shot.velocity[2] -= static_cast<fx>((static_cast<std::int64_t>(kGravity / 2) * ms) / 1000);
                fx to[3];
                for (int i = 0; i < 3; ++i)
                    to[i] = shot.pos[i] + static_cast<fx>((static_cast<std::int64_t>(shot.velocity[i]) * ms) / 1000);
                if (grenade)
                {
                    // It bounces: off what a crouched person could not pass
                    // (near enough for something the size of a fist), losing
                    // a third of its speed each time, until its fuse is out.
                    const Trace t = traceAll(*w.world, true, shot.pos, to);
                    if (!t.startSolid && t.fraction < kFractionOne)
                    {
                        std::int64_t into = 0;
                        for (int i = 0; i < 3; ++i)
                            into += static_cast<std::int64_t>(shot.velocity[i]) * t.normal[i];
                        into >>= kNormalShift;
                        for (int i = 0; i < 3; ++i)
                        {
                            const fx off = static_cast<fx>((into * t.normal[i] * 2) >> kNormalShift);
                            shot.velocity[i] = ((shot.velocity[i] - off) * 2) / 3;
                            to[i] = t.endPos[i];
                        }
                        if (!gib && w.heard != nullptr && fxLength(shot.velocity) > fxFromInt(60))
                        {
                            // (One of its three knocks, and quiet: the
                            // game's are a quarter of full.)
                            w.actors->seed = w.actors->seed * 1664525u + 1013904223u;
                            const int which = static_cast<int>((w.actors->seed >> 12) % 3);
                            w.heard->add(which == 0 ? CommonSound::GrenadeBounce
                                                    : (which == 1 ? CommonSound::GrenadeBounce2 : CommonSound::GrenadeBounce3),
                                         shot.pos, 48, kRadiusMedium);
                        }
                    }
                    for (int i = 0; i < 3; ++i)
                        shot.pos[i] = t.startSolid ? shot.pos[i] : to[i];
                    if (shot.leftMs <= 0)
                    {
                        shot.active = false;
                        if (gib)
                            continue; // (what was thrown out is simply gone)
                        effectAdd(a, kEffectBurst, shot.pos, nullptr, 96, 450);
                        markBlast(a, *w.world, shot.pos, 48);
                        if (w.heard != nullptr)
                            w.heard->add(CommonSound::Explode, shot.pos, 127, 2000);
                        radiusDamage(w, player, shot.pos, kGrenadeBlast, kGrenadeRadius, -1);
                        // (Felt, near to.)
                        const fx d[3] = {player.origin[0] - shot.pos[0], player.origin[1] - shot.pos[1],
                                         player.origin[2] - shot.pos[2]};
                        const int far = fxToInt(fxLength(d));
                        if (far < 500)
                            shakeStart(w, fxFromInt(5) * (500 - far) / 500, 450);
                    }
                    continue;
                }
                // Spit: straight on until it meets the world or the player.
                const Shot line = traceShot(*w.world, shot.pos, to);
                fx end[3];
                for (int i = 0; i < 3; ++i)
                    end[i] = shot.pos[i] + static_cast<fx>((static_cast<std::int64_t>(to[i] - shot.pos[i]) *
                                                            line.fraction) >> kFractionShift);
                // (The player is a box; the spit a point taken at the middle
                // and the end of this tick's flight.)
                const fx middle[3] = {(shot.pos[0] + end[0]) / 2, (shot.pos[1] + end[1]) / 2,
                                      (shot.pos[2] + end[2]) / 2};
                const bool hits = player.health > 0 &&
                                  (playerTouchesBox(player, middle, middle, fxFromInt(4)) ||
                                   playerTouchesBox(player, end, end, fxFromInt(4)));
                if (hits)
                {
                    playerHurt(player, kSquidSpit, shot.pos);
                    a.got |= kGotHurt;
                }
                if (hits || line.fraction < kFractionOne || shot.leftMs <= 0)
                {
                    shot.active = false;
                    effectAdd(a, kEffectSplat, end, nullptr, 12, 200);
                    if (w.heard != nullptr)
                        w.heard->add(CommonSound::SpitHit, end, 110, kRadiusMedium);
                    continue;
                }
                for (int i = 0; i < 3; ++i)
                    shot.pos[i] = end[i];
            }
        }
    } // namespace detail


    // ------------------------------------------------------------- turrets
    //
    // The guns that mind a place: the game's turret.cpp, think for think
    // (ten times a second). Folded away until one sees someone (if it is the
    // kind that looks) or is switched on by name; then out, sweeping round
    // and pinging; on whoever it sees, turning to them and firing a round a
    // think while they are within thirty degrees of where it points; folded
    // away again when it has seen nobody for a while. Angles are 65536 to
    // the circle.

    inline constexpr std::uint8_t kTurretOff = kAiIdle;
    inline constexpr std::uint8_t kTurretDeploy = 1;
    inline constexpr std::uint8_t kTurretSearch = 2;
    inline constexpr std::uint8_t kTurretActive = 3;
    inline constexpr std::uint8_t kTurretRetire = 4;

    /// What each kind of turret is, in numbers: the game's, at "normal".
    struct TurretKind
    {
        int health;
        int half;     ///< of its box, across
        int offHigh;  ///< ... and up and down from where it is fixed, folded away
        int onHigh;   ///< ... and out
        int eye;      ///< how far its gun is from where it is fixed
        int minPitch; ///< how far down it can point, in degrees (a negative number)
        int maxSpinMs; ///< how long its barrels turn with nobody to fire at (0: for good)
        int maxWaitMs; ///< how long it looks for someone before it folds away (0: for good)
        int damage;   ///< of a round
        CommonSound fire;
        bool spinsUp; ///< its barrels have to get going before it fires
        bool tough;   ///< armoured; shrugs off a tenth while folded away; goes wild when nearly done for
    };

    inline const TurretKind &turretKind(std::uint8_t kind)
    {
        static const TurretKind kMini = {40, 16, 16, 32, 13, -15, 0, 15000, 5, CommonSound::SentryFire, false, true};
        static const TurretKind kSentry = {40, 16, 64, 64, 48, -60, 0, 0, 4, CommonSound::SentryFire, false, false};
        static const TurretKind kBig = {50, 32, 16, 32, 13, -15, 5000, 15000, 10, CommonSound::TurretFire, true, true};
        if (kind == static_cast<std::uint8_t>(ActorKind::Sentry))
            return kSentry;
        return kind == static_cast<std::uint8_t>(ActorKind::Turret) ? kBig : kMini;
    }

    inline constexpr int kTurretRange = 1200;
    inline constexpr int kTurretTurnRate = 30; ///< degrees a second, to begin with
    inline constexpr std::int32_t kDegree = 65536 / 360;

    /// The box a shot at a turret is tried against (and a sentry's touch).
    inline void turretBox(const ActorState &actor, fx *mins, fx *maxs)
    {
        const TurretKind &kind = turretKind(actor.kind);
        const fx high = fxFromInt(actor.ai == kTurretOff ? kind.offHigh : kind.onHigh);
        for (int i = 0; i < 2; ++i)
        {
            mins[i] = actor.pos[i] - fxFromInt(kind.half);
            maxs[i] = actor.pos[i] + fxFromInt(kind.half);
        }
        mins[2] = actor.pos[2] - high;
        maxs[2] = actor.pos[2] + high;
    }

    /// Where a line from `from` to `to` enters the box, as a fraction of the
    /// way (Q30); false if it does not.
    inline bool lineEntersBox(const fx *from, const fx *to, const fx *mins, const fx *maxs,
                              std::int64_t &enter)
    {
        std::int64_t leave = kFractionOne;
        enter = 0;
        for (int k = 0; k < 3; ++k)
        {
            const fx d = to[k] - from[k];
            if (d == 0)
            {
                if (from[k] < mins[k] || from[k] > maxs[k])
                    return false;
                continue;
            }
            std::int64_t t0 = (static_cast<std::int64_t>(mins[k] - from[k]) << kFractionShift) / d;
            std::int64_t t1 = (static_cast<std::int64_t>(maxs[k] - from[k]) << kFractionShift) / d;
            if (t0 > t1)
            {
                const std::int64_t swap = t0;
                t0 = t1;
                t1 = swap;
            }
            enter = t0 > enter ? t0 : enter;
            leave = t1 < leave ? t1 : leave;
            if (enter > leave)
                return false;
        }
        return true;
    }

    namespace detail
    {
        inline void turretEye(const ActorState &actor, fx *eye)
        {
            const fx up = fxFromInt(turretKind(actor.kind).eye);
            eye[0] = actor.pos[0];
            eye[1] = actor.pos[1];
            eye[2] = actor.pos[2] + ((actor.flags & kActorCeiling) ? -up : up);
        }

        /// The way the gun points (Q12, unit).
        inline void turretForward(const ActorState &actor, fx *forward)
        {
            fx sinYaw, cosYaw, sinPitch, cosPitch;
            fxSinCos(static_cast<std::uint16_t>(actor.aimYaw), sinYaw, cosYaw);
            fxSinCos(static_cast<std::uint16_t>(actor.aimPitch), sinPitch, cosPitch);
            forward[0] = fxMul(cosPitch, cosYaw);
            forward[1] = fxMul(cosPitch, sinYaw);
            forward[2] = sinPitch;
        }

        inline bool turretSees(const LogicWorld &w, const PlayerState &player, const fx *eye)
        {
            if (player.health <= 0)
                return false;
            const fx d[3] = {player.origin[0] - eye[0], player.origin[1] - eye[1], player.origin[2] - eye[2]};
            if (fxLength(d) > fxFromInt(kTurretRange))
                return false;
            const fx head[3] = {player.origin[0], player.origin[1], player.origin[2] + player.viewHeight};
            return lineClear(*w.world, eye, player.origin) || lineClear(*w.world, eye, head);
        }

        /// One think's turning towards where it is wanted. The game's rule:
        /// up and down at the rate it has; round by the shorter way, the
        /// rate building while there is far to go (to ten times what it
        /// began at) and easing off close to. False when it is there.
        inline bool turretTurn(ActorState &actor)
        {
            bool moved = false;
            if (actor.aimPitch != actor.wantPitch)
            {
                const std::int32_t step = actor.turnRate * kDegree / 10;
                const std::int32_t left = actor.wantPitch - actor.aimPitch;
                actor.aimPitch += left > 0 ? (left < step ? left : step) : (-left < step ? left : -step);
                moved = true;
            }
            const std::int32_t left = static_cast<std::int16_t>(static_cast<std::uint32_t>(actor.wantYaw - actor.aimYaw));
            if (left != 0)
            {
                const std::int32_t far = left < 0 ? -left : left;
                if (far > 30 * kDegree)
                {
                    if (actor.turnRate < kTurretTurnRate * 10)
                        actor.turnRate += kTurretTurnRate;
                }
                else if (actor.turnRate > 45)
                {
                    actor.turnRate -= kTurretTurnRate;
                }
                else
                {
                    actor.turnRate += kTurretTurnRate;
                }
                const std::int32_t step = actor.turnRate * kDegree / 10;
                // (There, or as good as: a degree and a half.)
                if (step >= far || far < kTurretTurnRate * kDegree / 20)
                    actor.aimYaw = actor.wantYaw;
                else
                    actor.aimYaw += left > 0 ? step : -step;
                actor.aimYaw &= 0xFFFF;
                actor.wantYaw &= 0xFFFF;
                moved = true;
            }
            if (!moved)
                actor.turnRate = kTurretTurnRate;
            return moved;
        }

        /// Its barrels: got going (the big one takes two seconds and more
        /// over it; the others are ready at once) ...
        inline void turretSpinUp(LogicWorld &w, ActorState &actor, const TurretKind &kind)
        {
            if (actor.struck)
                return;
            if (!kind.spinsUp)
            {
                actor.struck = 1;
                return;
            }
            if (actor.speed == 0)
            {
                monsterSound(w, actor, CommonSound::TurretSpinUp);
                actor.speed = kFxOne / 10;
                actor.thinkMs -= 900; // (it waits a second over the first of it)
                return;
            }
            actor.speed += kFxOne * 3 / 40;
            if (actor.speed >= kFxOne)
            {
                actor.speed = kFxOne;
                actor.struck = 1;
            }
        }
        /// ... and let run down (five seconds, for the big one).
        inline void turretSpinDown(LogicWorld &w, ActorState &actor, const TurretKind &kind)
        {
            if (!actor.struck && actor.speed == 0)
                return;
            if (!kind.spinsUp)
            {
                actor.struck = 0;
                return;
            }
            if (actor.speed >= kFxOne)
                monsterSound(w, actor, CommonSound::TurretSpinDown);
            actor.speed -= kFxOne / 50;
            if (actor.speed <= 0)
            {
                actor.speed = 0;
                actor.struck = 0;
            }
        }

        /// A round, along the way the gun points: it hurts the player if
        /// that line meets them before it meets anything else.
        inline void turretShoot(LogicWorld &w, PlayerState &player, ActorState &actor, const TurretKind &kind,
                                const fx *eye, const fx *forward)
        {
            ActorWorld &a = *w.actors;
            monsterSound(w, actor, kind.fire);
            const fx end[3] = {eye[0] + forward[0] * kTurretRange * 2, eye[1] + forward[1] * kTurretRange * 2,
                               eye[2] + forward[2] * kTurretRange * 2};
            const Shot shot = traceShot(*w.world, eye, end);
            std::int64_t reach = shot.fraction;
            const fx tall = player.ducked ? kCrouchedFeet : kStandingFeet;
            const fx mins[3] = {player.origin[0] - fxFromInt(16), player.origin[1] - fxFromInt(16),
                                player.origin[2] - tall};
            const fx maxs[3] = {player.origin[0] + fxFromInt(16), player.origin[1] + fxFromInt(16),
                                player.origin[2] + tall};
            std::int64_t enter;
            if (player.health > 0 && lineEntersBox(eye, end, mins, maxs, enter) && enter < reach)
            {
                reach = enter;
                playerHurt(player, kind.damage, eye);
                a.got |= kGotHurt;
            }
            fx at[3];
            for (int i = 0; i < 3; ++i)
                at[i] = eye[i] + static_cast<fx>((static_cast<std::int64_t>(end[i] - eye[i]) * reach) >> kFractionShift);
            effectAdd(a, kEffectTracer, eye, at, 0, 60);
            effectAdd(a, kEffectMuzzle, eye, nullptr, 0, 50);
            // (What misses marks the wall behind.)
            if (reach == shot.fraction && shot.fraction < kFractionOne)
                markShot(a, *w.world, eye, end, false);
        }

        inline void turretDeploy(LogicWorld &w, PlayerState &player, ActorState &actor, bool alarmed)
        {
            ActorWorld &a = *w.actors;
            actor.ai = kTurretDeploy;
            monsterClip(a, actor, "deploy", true);
            if (alarmed)
                monsterSound(w, actor, CommonSound::TurretAlert);
            monsterSound(w, actor, CommonSound::TurretDeploy);
            // What it sets off as it comes out (a tripped sentry wakes the
            // others of its name this way).
            if (actor.fires != 0)
                logicFire(w, player, actor.fires, -1, kUseOn, 0);
        }

        inline void turretThink(LogicWorld &w, PlayerState &player, int index)
        {
            ActorWorld &a = *w.actors;
            ActorState &actor = a.states[index];
            const TurretKind &kind = turretKind(actor.kind);
            const bool ceiling = (actor.flags & kActorCeiling) != 0;
            fx eye[3];
            turretEye(actor, eye);
            // The big turret's eye (ActorState::reach, 0..255): lit at each
            // ping, it goes out again over the next second.
            actor.reach = actor.reach > 30 ? actor.reach - 30 : 0;

            // Switched by name: on if it is off, off if it is on (unless it
            // was told which, and is that already).
            if (actor.switched != 0)
            {
                const int how = actor.switched - 1;
                actor.switched = 0;
                const bool on = actor.ai != kTurretOff;
                if (how == kUseToggle || how == kUseSet || (how == kUseOn) != on)
                {
                    if (on && actor.ai != kTurretRetire)
                    {
                        actor.hasGoal = false; // told to stop: it does not come out by itself again
                        actor.ai = kTurretRetire;
                    }
                    else if (!on)
                    {
                        actor.hasGoal = (actor.flags & kActorAuto) != 0;
                        turretDeploy(w, player, actor, false);
                        return;
                    }
                }
            }

            switch (actor.ai)
            {
            case kTurretOff:
            {
                // One that looks out for itself comes out at the sight of
                // someone; a sentry at a touch.
                fx mins[3], maxs[3];
                turretBox(actor, mins, maxs);
                const bool touched = actor.kind == static_cast<std::uint8_t>(ActorKind::Sentry) &&
                                     player.health > 0 && playerTouchesBox(player, mins, maxs, fxFromInt(2));
                if (touched || (actor.hasGoal && turretSees(w, player, eye)))
                    turretDeploy(w, player, actor, true);
                break;
            }
            case kTurretDeploy:
                if (!actor.finished)
                    break;
                actor.aimYaw = actor.wantYaw = actor.yaw;
                actor.aimPitch = actor.wantPitch = 0;
                actor.turnRate = kTurretTurnRate;
                actor.stuckMs = kind.maxWaitMs;
                actor.goal[0] = kind.maxSpinMs;
                actor.moveMs = 0;
                actor.cooldownMs = 0;
                actor.ai = kTurretSearch;
                monsterClip(a, actor, "spin", false);
                break;
            case kTurretSearch:
                monsterClip(a, actor, "spin", false);
                actor.cooldownMs -= 100;
                if (actor.cooldownMs <= 0)
                {
                    actor.cooldownMs = 1000;
                    monsterSound(w, actor, CommonSound::TurretPing);
                    actor.reach = 255; // (the big one's eye lights with it)
                }
                if (turretSees(w, player, eye))
                {
                    actor.moveMs = 0;
                    actor.goal[0] = kind.maxSpinMs;
                    actor.ai = kTurretActive;
                    break;
                }
                if (kind.maxWaitMs != 0 && (actor.stuckMs -= 100) <= 0)
                {
                    actor.ai = kTurretRetire;
                    break;
                }
                if (kind.maxSpinMs != 0 && (actor.goal[0] -= 100) <= 0)
                    turretSpinDown(w, actor, kind);
                // Round and round, looking.
                actor.wantYaw = (actor.wantYaw + actor.turnRate * kDegree / 10) & 0xFFFF;
                turretTurn(actor);
                break;
            case kTurretActive:
            {
                const bool visible = turretSees(w, player, eye);
                if (!visible)
                {
                    // Half a second more of it, then back to looking.
                    if (actor.moveMs == 0)
                    {
                        actor.moveMs = 500;
                    }
                    else if ((actor.moveMs -= 100) <= 0)
                    {
                        actor.moveMs = 0;
                        actor.stuckMs = kind.maxWaitMs;
                        actor.ai = kTurretSearch;
                        break;
                    }
                }
                else
                {
                    actor.moveMs = 0;
                }
                fx forward[3];
                turretForward(actor, forward);
                fx to[3] = {player.origin[0] - eye[0], player.origin[1] - eye[1], player.origin[2] - eye[2]};
                const fx far = fxLength(to);
                // Within thirty degrees of where it points.
                const bool onTarget =
                    far > 0 && fxMul(to[0], forward[0]) + fxMul(to[1], forward[1]) + fxMul(to[2], forward[2]) >
                                   fxMul(far, fxFromDouble(0.866));
                if (actor.struck && (onTarget || actor.attack))
                {
                    monsterClip(a, actor, "fire", false);
                    turretShoot(w, player, actor, kind, eye, forward);
                }
                else
                {
                    monsterClip(a, actor, "spin", false);
                }
                if (actor.attack)
                {
                    // Nearly done for, and wild: now and then it swings off
                    // somewhere, and it does not last.
                    if (chance(a) % 10 == 0)
                    {
                        actor.wantYaw = static_cast<std::int32_t>(chance(a) & 0xFFFF);
                        actor.wantPitch = static_cast<std::int32_t>(chance(a) % 90) * kDegree * (ceiling ? -1 : 1);
                        actor.health = static_cast<std::int16_t>(actor.health - 10);
                        if (actor.health <= 0)
                        {
                            actor.ai = kAiDying;
                            actor.switched = 0xFF;
                        }
                        break;
                    }
                }
                else if (visible)
                {
                    const fx flat[3] = {to[0], to[1], 0};
                    actor.wantYaw = fxAtan2Turns(to[1], to[0]);
                    std::int32_t pitch = static_cast<std::int16_t>(fxAtan2Turns(to[2], fxLength(flat)));
                    // As far up and down as it is built to point (upside
                    // down, the other way up).
                    const std::int32_t low = (ceiling ? -90 : kind.minPitch) * kDegree;
                    const std::int32_t high = (ceiling ? -kind.minPitch : 90) * kDegree;
                    actor.wantPitch = pitch < low ? low : (pitch > high ? high : pitch);
                }
                turretSpinUp(w, actor, kind);
                turretTurn(actor);
                break;
            }
            case kTurretRetire:
            {
                // Gun back to where it started; barrels stopped; then away.
                actor.wantPitch = 0;
                actor.wantYaw = actor.yaw;
                const ModelView *model = actorModel(a, actor);
                const int retire = model != nullptr ? model->findClip(nameHash("retire")) : -1;
                if (turretTurn(actor))
                {
                    monsterClip(a, actor, "spin", false);
                }
                else if (actor.struck || actor.speed != 0)
                {
                    turretSpinDown(w, actor, kind);
                }
                else if (retire >= 0 && actor.clip != retire)
                {
                    // (The game plays it backwards: turretTick runs it.)
                    actor.clip = static_cast<std::int16_t>(retire);
                    actor.frame = (model->clips[retire].frameCount - 1u) << 8;
                    actor.frozen = true;
                    actor.once = true;
                    actor.finished = false;
                    monsterSound(w, actor, CommonSound::TurretDeploy);
                    if (actor.fires != 0)
                        logicFire(w, player, actor.fires, -1, kUseOff, 0);
                }
                else if (retire < 0 || actor.frame == 0)
                {
                    actor.ai = kTurretOff;
                    actor.attack = 0;
                    monsterClip(a, actor, "idle_off", false);
                }
                break;
            }
            default:
                break;
            }
        }

        inline void turretTick(LogicWorld &w, PlayerState &player, int index, int ms)
        {
            ActorWorld &a = *w.actors;
            ActorState &actor = a.states[index];
            const TurretKind &kind = turretKind(actor.kind);
            if (actor.health == 0 && actor.ai == kTurretOff && actor.switched != 0xFF)
            {
                // Its first tick. (Health in tenths: see turretHurt.)
                actor.health = static_cast<std::int16_t>(kind.health * 10);
                actor.hasGoal = (actor.flags & kActorAuto) != 0;
                actor.aimYaw = actor.wantYaw = actor.yaw;
                actor.turnRate = kTurretTurnRate;
                actor.thinkMs = -200 - (index * 37) % 100;
            }
            if (actor.ai == kAiDead)
                return;
            if (actor.ai == kAiDying)
            {
                if (actor.switched == 0xFF)
                {
                    // Destroyed, a moment ago: the gun droops (a sentry's
                    // falls straight), and what it was to set off goes off.
                    actor.switched = 0;
                    actor.reach = 255; // (its eye, a last time)
                    monsterSound(w, actor, CommonSound::TurretDie);
                    monsterClip(a, actor, "die", true);
                    actor.struck = 0;
                    actor.speed = 0;
                    if (actor.kind == static_cast<std::uint8_t>(ActorKind::Sentry))
                    {
                        actor.aimYaw = actor.wantYaw = actor.yaw;
                        actor.aimPitch = actor.wantPitch = 0;
                        actor.flags &= static_cast<std::uint8_t>(~kActorSolid);
                    }
                    else
                    {
                        actor.wantPitch = ((actor.flags & kActorCeiling) ? -90 : -15) * kDegree;
                    }
                    if (actor.fires != 0)
                        logicFire(w, player, actor.fires, -1, kUseOn, 0);
                    return;
                }
                if ((actor.thinkMs += ms) >= 100)
                {
                    actor.thinkMs -= 100;
                    actor.reach = actor.reach > 30 ? actor.reach - 30 : 0;
                    actor.wantYaw = actor.aimYaw;
                    if (!turretTurn(actor) && actor.finished)
                        actor.ai = kAiDead;
                }
                return;
            }
            // Folding away is its coming out, run backwards.
            if (actor.ai == kTurretRetire && actor.frozen)
            {
                const ModelView *model = actorModel(a, actor);
                const std::uint32_t back =
                    model != nullptr && actor.clip >= 0
                        ? static_cast<std::uint32_t>(model->clips[actor.clip].fps) * static_cast<std::uint32_t>(ms) / 1000u
                        : actor.frame;
                actor.frame = actor.frame > back ? actor.frame - back : 0;
            }
            actor.thinkMs += ms;
            // (Not more than two thinks a tick, however long the tick.)
            for (int n = 0; n < 2 && actor.thinkMs >= 100 && actor.ai < kAiDying; ++n)
            {
                actor.thinkMs -= 100;
                turretThink(w, player, index);
            }
            if (actor.thinkMs > 200)
                actor.thinkMs = 200;
        }
    } // namespace detail

    /// Hurt the turret at `index`, with `damage` to a hit box of `group`.
    /// The game's rules: its plating turns a bullet (a tenth gets through);
    /// folded away it feels a tenth of anything (not a sentry, which is
    /// woken by it instead); nearly done for, and out, it goes wild. Its
    /// health is kept in tenths, for the tenths.
    inline void turretHurt(LogicWorld &w, int index, int damage, bool bullet, int group)
    {
        ActorState &actor = w.actors->states[index];
        if (!actor.active || actor.ai >= kAiDying)
            return;
        const TurretKind &kind = turretKind(actor.kind);
        int tenths = damage * 10;
        if (bullet && group == kHitArmour && kind.tough)
            tenths /= 10;
        if (kind.tough && actor.ai == kTurretOff)
            tenths /= 10;
        if (tenths <= 0)
            return;
        actor.health = static_cast<std::int16_t>(actor.health - tenths);
        if (actor.health <= 0)
        {
            actor.health = 0;
            actor.ai = kAiDying;
            actor.switched = 0xFF; // (turretTick says so, and sets off what it is to)
            actor.thinkMs = 0;
            return;
        }
        if (!kind.tough)
        {
            if (actor.ai == kTurretOff)
                actor.switched = static_cast<std::uint8_t>(1 + kUseOn);
        }
        else if (actor.health <= 100 && actor.ai != kTurretOff && actor.ai != kTurretDeploy)
        {
            actor.attack = 1;
            actor.ai = kTurretSearch;
        }
    }

    // ------------------------------------------------------------- tripmines
    //
    // The game's tripmine.cpp. A mine on a wall: a while after the map
    // starts (two and a half seconds; one, if the map says so) its beam
    // comes on, out along the way it points to the first thing in the way
    // within 2048 units. Ten times a second the beam is measured again --
    // against the world and what moves in it, the player, and anything that
    // fights -- and if it has got any shorter or longer, or the mine is hurt
    // by anything at all, the mine goes off a moment later: 150, to 375
    // units.

    inline constexpr std::uint8_t kMineArmed = 1; ///< ActorState::ai, between kAiIdle and kAiDying
    inline constexpr int kMineBlast = 150;
    inline constexpr int kMineRange = 2048;

    namespace detail
    {
        /// How far along its beam (Q30, of kMineRange) the first thing is.
        inline std::int32_t mineReach(const LogicWorld &w, const PlayerState &player, int index, const fx *end)
        {
            const ActorWorld &a = *w.actors;
            const ActorState &mine = a.states[index];
            std::int64_t reach = traceShot(*w.world, mine.pos, end).fraction;
            std::int64_t enter;
            if (player.health > 0)
            {
                const fx tall = player.ducked ? kCrouchedFeet : kStandingFeet;
                const fx mins[3] = {player.origin[0] - fxFromInt(16), player.origin[1] - fxFromInt(16),
                                    player.origin[2] - tall};
                const fx maxs[3] = {player.origin[0] + fxFromInt(16), player.origin[1] + fxFromInt(16),
                                    player.origin[2] + tall};
                if (lineEntersBox(mine.pos, end, mins, maxs, enter) && enter < reach)
                    reach = enter;
            }
            for (int i = 0; i < a.count; ++i)
            {
                const ActorState &actor = a.states[i];
                if (i == index || !actor.active || !fights(actor.kind) || isMine(actor.kind) || actor.ai >= kAiDying)
                    continue;
                fx mins[3], maxs[3];
                targetBox(actor, mins, maxs);
                if (lineEntersBox(mine.pos, end, mins, maxs, enter) && enter < reach)
                    reach = enter;
            }
            return static_cast<std::int32_t>(reach);
        }

        inline void mineTick(LogicWorld &w, PlayerState &player, int index, int ms)
        {
            ActorWorld &a = *w.actors;
            ActorState &mine = a.states[index];
            if (mine.ai == kAiDead)
                return;
            fx forward[3];
            turretForward(mine, forward);
            if (mine.ai == kAiDying)
            {
                // Set off: it goes a moment after.
                if ((mine.thinkMs -= ms) > 0)
                    return;
                mine.ai = kAiDead;
                mine.active = false;
                actorPlaceBox(a, index);
                const fx at[3] = {mine.pos[0] + forward[0] * 8, mine.pos[1] + forward[1] * 8,
                                  mine.pos[2] + forward[2] * 8};
                explodeAt(w, player, at, kMineBlast, true, true, true);
                return;
            }
            const fx end[3] = {mine.pos[0] + forward[0] * kMineRange, mine.pos[1] + forward[1] * kMineRange,
                               mine.pos[2] + forward[2] * kMineRange};
            if (mine.ai != kMineArmed)
            {
                if (mine.health == 0)
                {
                    mine.health = 1;
                    mine.thinkMs = (mine.flags & kActorQuick) ? 1000 : 2500;
                }
                if ((mine.thinkMs -= ms) > 0)
                    return;
                // Its beam comes on: as long as it is now is as long as it
                // has to stay. (ActorState::reach: Q30 of the range, not
                // Q12; ::goal: where it ends.)
                mine.reach = mineReach(w, player, index, end);
                for (int k = 0; k < 3; ++k)
                    mine.goal[k] = mine.pos[k] + static_cast<fx>((static_cast<std::int64_t>(end[k] - mine.pos[k]) *
                                                                   mine.reach) >> kFractionShift);
                mine.ai = kMineArmed;
                mine.thinkMs = 0;
                monsterSound(w, mine, CommonSound::MineActivate);
                return;
            }
            if ((mine.thinkMs += ms) < 100)
                return;
            mine.thinkMs = mine.thinkMs > 200 ? 0 : mine.thinkMs - 100;
            const std::int32_t reach = mineReach(w, player, index, end);
            const std::int32_t off = reach > mine.reach ? reach - mine.reach : mine.reach - reach;
            // (A thousandth of the range, as the game: two units.)
            if (off > kFractionOne / 1000)
                mineHurt(w, index);
        }
    } // namespace detail

    /// Anything at all sets a mine off: a tenth to three tenths of a second
    /// later.
    inline void mineHurt(LogicWorld &w, int index)
    {
        ActorWorld &a = *w.actors;
        ActorState &mine = a.states[index];
        if (!mine.active || mine.ai >= kAiDying)
            return;
        mine.ai = kAiDying;
        mine.thinkMs = 100 + static_cast<int>(detail::chance(a) % 200);
    }

    /// What can be shot, and stays destroyed: the monsters, the turrets,
    /// the soldiers and the mines.
    inline bool fights(std::uint8_t kind)
    {
        return isMonster(kind) || isTurret(kind) || isGrunt(kind) || isMine(kind);
    }
    inline bool canBeHurt(std::uint8_t kind)
    {
        return fights(kind) || isPerson(kind);
    }

    inline void bleed(LogicWorld &w, int index, const fx *at, const fx *dir, bool heavy)
    {
        if (w.actors == nullptr || w.world == nullptr)
            return;
        const int blood = bloodOf(w.actors->states[index].kind);
        if (blood == kBloodNone)
            return;
        const bool alien = blood == kBloodYellow;
        effectAdd(*w.actors, kEffectBlood, at, nullptr, alien ? 1 : 0, 250);
        static const fx kDown[3] = {0, 0, -kFxOne};
        markBlood(*w.actors, *w.world, at, dir != nullptr ? dir : kDown, alien, heavy);
        if (heavy)
            markFloor(*w.actors, *w.world, at, 96, alien ? kDecalAlien : kDecalBlood, 22,
                      alien ? kMarkAlien : kMarkBlood);
    }

    /// The box a shot at the actor is first tried against.
    inline void targetBox(const ActorState &actor, fx *mins, fx *maxs)
    {
        if (isTurret(actor.kind))
        {
            turretBox(actor, mins, maxs);
            return;
        }
        if (isMine(actor.kind))
        {
            for (int i = 0; i < 3; ++i)
            {
                mins[i] = actor.pos[i] - fxFromInt(8);
                maxs[i] = actor.pos[i] + fxFromInt(8);
            }
            return;
        }
        if (isPerson(actor.kind))
        {
            // (Seated, a scientist is half the height.)
            const bool seated = actor.kind == static_cast<std::uint8_t>(ActorKind::Sitter);
            for (int i = 0; i < 2; ++i)
            {
                mins[i] = actor.pos[i] - fxFromInt(14);
                maxs[i] = actor.pos[i] + fxFromInt(14);
            }
            mins[2] = actor.pos[2];
            maxs[2] = actor.pos[2] + fxFromInt(seated ? 48 : 70);
            return;
        }
        const MonsterKind &kind = monsterKind(actor.kind);
        // (A barnacle's body is above its "feet".)
        const bool hangs = actor.kind == static_cast<std::uint8_t>(ActorKind::Barnacle);
        for (int i = 0; i < 2; ++i)
        {
            mins[i] = actor.pos[i] - kind.half;
            maxs[i] = actor.pos[i] + kind.half;
        }
        mins[2] = hangs ? actor.pos[2] - fxFromInt(8) : actor.pos[2];
        maxs[2] = actor.pos[2] + kind.height;
    }

    /// Everything in this file that happens by itself, for `ms` more
    /// milliseconds. After logicTick (which moves the actors' clips on).
    namespace detail
    {
        /// One of the people and what is under them. They stand on brushes
        /// as on the world -- a lift's floor, a pane -- go where a brush
        /// they stand on goes, and FALL when it breaks or is no longer
        /// there, as far as there is to fall. (Someone the map stood where
        /// nothing at all is under them is left there, as before: only
        /// whoever had ground can lose it.)
        inline void peopleGround(LogicWorld &w, ActorState &actor, int ms)
        {
            const MoveWorld &world = *w.world;
            if (actor.hurtMs > 0)
                actor.hurtMs -= ms;
            if (actor.groundMover == -2)
            {
                const fx from[3] = {actor.pos[0], actor.pos[1], actor.pos[2] + fxFromInt(36 + 1)};
                const fx to[3] = {actor.pos[0], actor.pos[1], actor.pos[2] + fxFromInt(36 - 24)};
                const Trace tr = traceAll(world, false, from, to);
                actor.groundMover = -1;
                if (!tr.startSolid && !tr.allSolid && tr.fraction < kFractionOne && tr.mover >= 0)
                {
                    actor.groundMover = tr.mover;
                    actor.pos[2] = tr.endPos[2] - fxFromInt(36);
                    for (int k = 0; k < 3; ++k)
                        actor.groundAt[k] = w.brushStates[tr.mover].pos[k];
                }
            }
            if (actor.airborne)
            {
                // (The game's gravity, 800 units a second each second.)
                actor.velocity[2] -= fxFromInt(800) * ms / 1000;
                const fx step = static_cast<fx>((static_cast<std::int64_t>(actor.velocity[2]) * ms) / 1000);
                const fx from[3] = {actor.pos[0], actor.pos[1], actor.pos[2] + fxFromInt(36)};
                const fx to[3] = {from[0], from[1], from[2] + step};
                const Trace tr = traceAll(world, false, from, to);
                if (tr.startSolid || tr.allSolid)
                {
                    actor.airborne = false; // (nowhere to fall: it stays)
                    actor.velocity[2] = 0;
                    actor.groundMover = -1;
                }
                else if (tr.fraction < kFractionOne)
                {
                    actor.pos[2] = tr.endPos[2] - fxFromInt(36);
                    actor.airborne = false;
                    actor.velocity[2] = 0;
                    actor.groundMover = tr.mover;
                    if (tr.mover >= 0)
                    {
                        for (int k = 0; k < 3; ++k)
                            actor.groundAt[k] = w.brushStates[tr.mover].pos[k];
                    }
                }
                else
                {
                    actor.pos[2] += step;
                }
                return;
            }
            if (actor.groundMover < 0 || actor.groundMover >= w.brushCount)
                return;
            const BrushState &under = w.brushStates[actor.groundMover];
            if (!under.active || !w.movers[actor.groundMover].solid)
            {
                // What they stood on is gone.
                actor.groundMover = -1;
                actor.airborne = true;
                actor.velocity[2] = 0;
                return;
            }
            // ... or has moved: so have they.
            for (int k = 0; k < 3; ++k)
            {
                actor.pos[k] += under.pos[k] - actor.groundAt[k];
                actor.groundAt[k] = under.pos[k];
            }
        }

        /// The places that hurt (trigger_hurt) hurt whoever is in them, not
        /// the player alone: half a second's worth every half second.
        inline void hurtPlaces(LogicWorld &w)
        {
            ActorWorld &a = *w.actors;
            for (int r = 0; r < w.recordCount; ++r)
            {
                const LogicRecord &record = w.records[r];
                if (record.kind != static_cast<std::uint8_t>(LogicKind::TriggerHurt))
                    continue;
                const LogicState &state = w.states[r];
                if (state.dead || state.running || !logicMasterAllows(w, r))
                    continue;
                const int damage = static_cast<std::int32_t>(record.extra[0]) / 2;
                if (damage <= 0)
                    continue;
                for (int i = 0; i < a.count; ++i)
                {
                    ActorState &actor = a.states[i];
                    if (!actor.active || actor.hurtMs > 0 || actor.ai >= kAiDying || actor.script >= 0 ||
                        !canBeHurt(actor.kind))
                        continue;
                    // (Its box against the place's: about a person's.)
                    const int x = fxToInt(actor.pos[0]), y = fxToInt(actor.pos[1]), z = fxToInt(actor.pos[2]);
                    if (x + 16 < record.mins[0] || x - 16 > record.maxs[0] || y + 16 < record.mins[1] ||
                        y - 16 > record.maxs[1] || z + 72 < record.mins[2] || z > record.maxs[2])
                        continue;
                    actor.hurtMs = 500;
                    monsterHurt(w, i, damage, false);
                }
            }
        }
    } // namespace detail

    inline void monstersTick(LogicWorld &w, PlayerState &player, int ms)
    {
        if (w.actors == nullptr || w.world == nullptr)
            return;
        ActorWorld &a = *w.actors;
        // (Looked at eight times a second: nothing stays in such a place
        // for less.)
        a.hurtLookMs -= ms;
        if (a.hurtLookMs <= 0 && w.records != nullptr && w.states != nullptr)
        {
            a.hurtLookMs = 125;
            detail::hurtPlaces(w);
        }
        for (int k = 0; k < 3; ++k)
            a.foe[k] = player.origin[k];
        if (a.noiseMs > 0)
            a.noiseMs -= ms;
        if (a.speakMs > 0)
            a.speakMs -= ms;
        // The small things in flight; a casing rings as it lands.
        if (piecesTick(a, *w.world, ms) > 0 && w.heard != nullptr)
            w.heard->add(CommonSound::Shell, nullptr, 70, kRadiusSmall);
        decalsTick(a, ms);
        for (int i = 0; i < w.recordCount; ++i)
        {
            const auto kind = static_cast<LogicKind>(w.records[i].kind);
            if (kind == LogicKind::Maker)
                detail::makerTick(w, player, i, ms);
            else if (kind == LogicKind::HealthCharger || kind == LogicKind::SuitCharger)
                detail::chargerTick(w, player, i, ms);
        }
        for (int i = 0; i < a.count; ++i)
        {
            ActorState &actor = a.states[i];
            if (!actor.active)
                continue;
            if (actor.kind == static_cast<std::uint8_t>(ActorKind::Item))
            {
                if (player.health > 0 &&
                    detail::touchesPlayer(player, actor.pos, fxFromInt(12), fxFromInt(24)) &&
                    detail::itemTake(player, actor.item, a.got))
                {
                    a.got |= kGotItem;
                    if (w.heard != nullptr)
                    {
                        const auto item = static_cast<ItemKind>(actor.item);
                        w.heard->add(item == ItemKind::HealthKit ? CommonSound::MedKit
                                     : (item == ItemKind::GlockClip || item == ItemKind::Buckshot ||
                                        item == ItemKind::Mp5Clip || item == ItemKind::AmmoBox ||
                                        item == ItemKind::ArGrenades || item == ItemKind::Ammo357)
                                         ? CommonSound::AmmoPickup
                                         : CommonSound::GunPickup,
                                     nullptr);
                    }
                    actor.active = false;
                    actorPlaceBox(a, i);
                    // What taking it sets off (the suit opens the way on).
                    if (actor.fires != 0)
                        logicFire(w, player, actor.fires, -1, kUseToggle, 0);
                }
                continue;
            }
            if (isTurret(actor.kind))
            {
                detail::turretTick(w, player, i, ms);
                actor.lastFrame = actor.frame;
                actorPlaceBox(a, i);
                continue;
            }
            if (isMine(actor.kind))
            {
                detail::mineTick(w, player, i, ms);
                continue;
            }
            if (isGrunt(actor.kind))
            {
                detail::gruntTick(w, player, i, ms);
                actor.lastFrame = actor.frame;
                actorPlaceBox(a, i);
                continue;
            }
            if (isPerson(actor.kind))
            {
                // (No scene has them: they stand on what is under them, and
                // follow, run, or shoot.)
                if (actor.script < 0 && actor.kind != static_cast<std::uint8_t>(ActorKind::Sitter) &&
                    w.brushStates != nullptr && w.movers != nullptr)
                {
                    detail::peopleGround(w, actor, ms);
                    if (actor.airborne)
                    {
                        actorPlaceBox(a, i);
                        continue;
                    }
                }
                if (actor.script < 0 && actor.ai < kAiDying)
                    detail::personTick(w, player, i, ms);
                continue;
            }
            if (!isMonster(actor.kind))
                continue;
            detail::monsterTick(w, player, i, ms);
            actor.lastFrame = actor.frame;
            actorPlaceBox(a, i);
        }
        detail::projectilesTick(w, player, ms);
        effectsTick(w, player, ms);
    }

    // ------------------------------------------------------------- weapons

    /// What each gun is, in numbers: the game's (its weapons' own source,
    /// dlls/*.cpp and cl_dll/ev_hldm.cpp).
    struct GunKind
    {
        int holds;     ///< rounds in a clip
        int reloadMs;  ///< (the shotgun's is by the shell: see weaponsTick)
        int refireMs;
        int damage;    ///< of a round, or of a pellet
        int pellets;
        int cone;      ///< how far a round may go off line, in 4096ths: the game's cone
        int range;
        bool wet;      ///< fires under water (the pistol alone does)
        int emptyMs;   ///< the wait after a click
        CommonSound fire[2]; ///< one or the other
        int pitch, pitchWide; ///< of the shot: from `pitch` percent, `pitchWide` steps up
        const char *shoot;    ///< the hand's clip for a shot
    };
    // The game's cones, in 4096ths: 0.01 and 0.1 (the pistol's two), one
    // degree, three, ten.
    inline constexpr int kConePistol = 41, kConePistolFast = 410, kCone1 = 36, kCone3 = 107, kCone10 = 357;
    inline const GunKind &gunKind(std::uint8_t weapon)
    {
        static const GunKind kGlock = {17,  1500, 300, 8, 1, kConePistol, 4096, true, 200,
                                       {CommonSound::Pistol, CommonSound::Pistol}, 98, 4, "shoot"};
        static const GunKind kShotgun = {8,   500, 750, 5, 6, kCone10, 2048, false, 150,
                                         {CommonSound::Shotgun, CommonSound::Shotgun}, 93, 32, "shoot"};
        static const GunKind kMp5 = {50,  1500, 100, 5, 1, kCone3, 4096, false, 150,
                                     {CommonSound::Mp5Fire1, CommonSound::Mp5Fire}, 94, 16, "shoot"};
        static const GunKind kPython = {6,   2000, 750, 40, 1, kCone1, 4096, false, 150,
                                        {CommonSound::PythonFire, CommonSound::PythonFire2}, 100, 1, "fire1"};
        return weapon == kWeaponShotgun ? kShotgun
                                        : (weapon == kWeaponMp5 ? kMp5 : (weapon == kWeaponPython ? kPython : kGlock));
    }
    /// The clip of the gun in hand, and what it is refilled from; null for
    /// what is not a gun.
    inline std::int16_t *gunClip(Inventory &inv)
    {
        return inv.current == kWeaponGlock ? &inv.clipGlock
               : inv.current == kWeaponShotgun ? &inv.clipShotgun
               : inv.current == kWeaponMp5 ? &inv.clipMp5
               : inv.current == kWeaponPython ? &inv.clipPython : nullptr;
    }
    inline std::int16_t *gunSpare(Inventory &inv)
    {
        return inv.current == kWeaponGlock || inv.current == kWeaponMp5 ? &inv.ammo9mm
               : inv.current == kWeaponShotgun ? &inv.ammoBuck
               : inv.current == kWeaponPython ? &inv.ammo357 : nullptr;
    }
    inline constexpr std::uint32_t kWeaponSecondary = 16; ///< weaponsTick: its other fire (the launcher, both barrels)
    /// A spent case leaves the gun now (WeaponState::shells of them): at
    /// the shot for the pistol and the MP5, at the pump for the shotgun.
    inline constexpr std::uint32_t kWeaponShell = 32;

    struct WeaponState
    {
        std::int32_t cooldownMs = 0; ///< until it can be used again
        std::int32_t secondMs = 0;   ///< ... and its other fire
        std::int32_t reloadMs = 0;   ///< left of a reload under way
        std::uint32_t seed = 777;    ///< where the shotgun's pellets go
        /// The clip the hand should start, said by weaponsTick as it
        /// happens (null: none this tick). Whoever draws the hand plays it
        /// and sets this back to null.
        const char *anim = nullptr;
        // The crowbar, as the game keeps it:
        std::uint8_t swingMiss = 0;  ///< which of its three swings at the air is next
        std::uint8_t swingHit = 0;   ///< ... and of its two that land
        std::int32_t readyMs = 1000; ///< how long it has been ready: a blow after a second's rest is a whole one
        std::int32_t retryMs = 0;    ///< a swing that met nothing is tried again when this runs out
        std::uint8_t blow = 10;      ///< what the swing under way does
        std::int32_t dentMs = 0;     ///< until the mark of the last blow on a wall is made
        fx dentAt[3] = {0, 0, 0};
        fx dentNormal[3] = {0, 0, 0};
        std::int32_t holsterMs = 0;  ///< while it is being put away
        std::uint8_t taking = 0;     ///< ... for this weapon
        // The guns and the grenade, as the game keeps them:
        std::uint8_t shells = 0;     ///< how many spent cases kWeaponShell is for
        std::int32_t pumpMs = 0;     ///< the shotgun: until it is pumped after a shot
        std::uint8_t pumpShells = 0; ///< ... and how many cases come out then
        /// The shotgun's loading, a shell at a time: 0 not loading; 1
        /// between shells (the next goes in when `loadMs` runs out, or the
        /// pump ends it); 2 a shell is going in.
        std::uint8_t loading = 0;
        std::int32_t loadMs = 0;
        /// The grenade: how long ago its pin was pulled (-1: it is not).
        /// It goes off three seconds after, in the hand or out of it.
        std::int32_t pinMs = -1;
        bool redraw = false;         ///< the next grenade is to be taken out
        std::int32_t idleMs = 1000;  ///< until the hand next does something of its own
        bool clicked = false;        ///< the click has been heard for this pull of the trigger
        /// The weapon these belong to (a kWeapon*): another in hand, by
        /// whatever means, and they start afresh.
        std::uint8_t minded = 0;
    };
    inline constexpr int kGrenadeFuse = 3000;  ///< from the pin, as the game has it
    inline constexpr int kGrenadePull = 500;   ///< the pin takes this long: no throw sooner
    inline constexpr int kCrowbarReach = 32;  ///< the game's: units along the view
    inline constexpr int kCrowbarBlow = 10;   ///< a whole blow; a follow-up is half
    // What a swing of the crowbar met (detail::crowbarBlow).
    inline constexpr int kMetNothing = 0;
    inline constexpr int kMetFlesh = 1;   ///< something that bleeds
    inline constexpr int kMetThing = 2;   ///< a machine, or something that breaks
    inline constexpr int kMetWall = 3;
    inline constexpr int kMetKilled = 4;  ///< flesh, and that was the end of it

    /// Which part of the actor a line from `from` to `to` strikes first
    /// (kHit*): the model is posed as the actor stands, and the line tried
    /// against each of its hit boxes, in the box's bone's own space. A line
    /// that goes between them all, or a model that has none, is a hit to
    /// the body: whoever fired was near enough.
    inline int hitGroup(const ActorWorld &a, const ActorState &actor, const fx *from, const fx *to)
    {
        const ModelView *model = actorModel(a, actor);
        if (model == nullptr || model->header->hitboxCount == 0 || actor.clip < 0)
            return kHitBody;
        static BoneMatrix bones[kModelMaxBones];
        std::int32_t controls[kModelControls];
        const bool controlled = actorControls(actor, controls);
        poseModel(*model, actor.clip, actor.frame, bones, controlled ? controls : nullptr);
        // Into the model's space: back through the way the actor is turned
        // (a turret under a ceiling is turned over as well: ModelDraw).
        fx sine, cosine;
        fxSinCos(actor.yaw, sine, cosine);
        const bool over = isTurret(actor.kind) && (actor.flags & kActorCeiling) != 0;
        fx ends[2][3];
        for (int e = 0; e < 2; ++e)
        {
            const fx *p = e == 0 ? from : to;
            const fx d[3] = {p[0] - actor.pos[0], p[1] - actor.pos[1], p[2] - actor.pos[2]};
            ends[e][0] = fxMul(d[0], cosine) + fxMul(d[1], sine);
            ends[e][1] = over ? fxMul(d[0], sine) - fxMul(d[1], cosine) : fxMul(d[1], cosine) - fxMul(d[0], sine);
            ends[e][2] = over ? -d[2] : d[2];
        }
        int group = kHitBody;
        std::int64_t nearest = kFractionOne + 1;
        for (int n = 0; n < model->header->hitboxCount; ++n)
        {
            const ModelHitbox &box = model->hitboxes[n];
            const BoneMatrix &bone = bones[box.bone];
            fx local[2][3];
            for (int e = 0; e < 2; ++e)
            {
                const fx d[3] = {ends[e][0] - bone.t[0], ends[e][1] - bone.t[1], ends[e][2] - bone.t[2]};
                for (int k = 0; k < 3; ++k)
                    local[e][k] = static_cast<fx>((static_cast<std::int64_t>(bone.r[k]) * d[0] +
                                                   static_cast<std::int64_t>(bone.r[3 + k]) * d[1] +
                                                   static_cast<std::int64_t>(bone.r[6 + k]) * d[2]) >>
                                                  kBoneRotShift);
            }
            fx mins[3], maxs[3];
            for (int k = 0; k < 3; ++k)
            {
                mins[k] = static_cast<fx>(box.mins[k]) << (kFxShift - kHitboxShift);
                maxs[k] = static_cast<fx>(box.maxs[k]) << (kFxShift - kHitboxShift);
            }
            std::int64_t enter;
            if (lineEntersBox(local[0], local[1], mins, maxs, enter) && enter < nearest)
            {
                nearest = enter;
                group = box.group;
            }
        }
        return group;
    }

    /// What a shot along `dir` (Q12, unit) from `eye` lands on within
    /// `range` units, and hurts: a monster, or something that breaks. True
    /// if it hurt anything.
    inline bool fireLine(LogicWorld &w, PlayerState &player, const fx *eye, const fx *dir, int range,
                         int damage, bool bullet, bool mark = true, int *met = nullptr, int from = -1)
    {
        if (met != nullptr)
            *met = kMetNothing;
        const fx end[3] = {eye[0] + dir[0] * range, eye[1] + dir[1] * range, eye[2] + dir[2] * range};
        const Shot shot = traceShot(*w.world, eye, end);
        // The nearest monster the line goes through before that.
        int struck = -1;
        std::int64_t nearest = shot.fraction;
        const ActorWorld *a = w.actors;
        for (int i = 0; a != nullptr && i < a->count; ++i)
        {
            const ActorState &actor = a->states[i];
            // (`from`: the actor whose shot it is, if it is not the
            // player's -- who does not shoot himself.)
            if (!actor.active || !canBeHurt(actor.kind) || actor.ai >= kAiDying || i == from)
                continue;
            fx mins[3], maxs[3];
            targetBox(actor, mins, maxs);
            std::int64_t enter = 0;
            const bool miss = !lineEntersBox(eye, end, mins, maxs, enter);
            if (!miss && enter < nearest)
            {
                nearest = enter;
                struck = i;
            }
        }
        if (struck >= 0)
        {
            // Where it landed, what bleeds bleeds (people red, the aliens
            // yellow) -- and on the wall or the floor behind it, if there
            // is one near: more of it for the blow that kills.
            const std::uint8_t kind = a->states[struck].kind;
            if (bloodOf(kind) != kBloodNone)
            {
                fx at[3];
                for (int i = 0; i < 3; ++i)
                    at[i] = eye[i] + static_cast<fx>((static_cast<std::int64_t>(end[i] - eye[i]) * nearest) >>
                                                     kFractionShift);
                if (bullet && w.heard != nullptr)
                    w.heard->add(CommonSound::BulletFlesh, at, 90, kRadiusSmall);
                monsterHurt(w, struck, damage, bullet, hitGroup(*a, a->states[struck], eye, end));
                const bool killed = a->states[struck].ai >= kAiDying || !a->states[struck].active;
                if (met != nullptr)
                    *met = killed ? kMetKilled : kMetFlesh;
                // A guard and the player's shots, by the game's rule: in a
                // fight it is taken for a stray one; with nothing else to
                // shoot at, the first makes him wary -- unless he was
                // plainly aimed at -- and the next an enemy, who follows
                // nobody. (`struck`: 0 easy, 1 wary, 2 an enemy.)
                if (kind == static_cast<std::uint8_t>(ActorKind::Guard) && from < 0)
                {
                    ActorState &him = w.actors->states[struck];
                    if (him.foe < 0 && him.struck < 2)
                    {
                        const fx to[3] = {him.pos[0] - eye[0], him.pos[1] - eye[1], him.pos[2] + fxFromInt(36) - eye[2]};
                        const fx far = fxLength(to);
                        const std::int64_t along = static_cast<std::int64_t>(to[0]) * dir[0] +
                                                   static_cast<std::int64_t>(to[1]) * dir[1] +
                                                   static_cast<std::int64_t>(to[2]) * dir[2];
                        // (Within about fifteen degrees of where the shot went.)
                        const bool aimed = far > 0 && along * 100 > static_cast<std::int64_t>(far) * fxLength(dir) * 96;
                        him.struck = static_cast<std::uint8_t>(him.struck >= 1 || aimed ? 2 : 1);
                        if (him.struck >= 2)
                        {
                            him.ai = 0; // (kPersonIdle: he follows no longer)
                            him.wasFollowing = false;
                        }
                    }
                }
                bleed(w, struck, at, dir, killed || damage >= 40);
                return true;
            }
            monsterHurt(w, struck, damage, bullet, hitGroup(*a, a->states[struck], eye, end));
            if (met != nullptr)
                *met = kMetThing;
            return true;
        }
        if (shot.mover >= 0)
        {
            for (int i = 0; i < w.recordCount; ++i)
            {
                if (w.records[i].brush == static_cast<std::uint16_t>(shot.mover) && !w.states[i].dead &&
                    detail::isBreakable(w.records[i]))
                {
                    w.struckFrom[0] = dir[0];
                    w.struckFrom[1] = dir[1];
                    w.struckFrom[2] = dir[2];
                    logicStrike(w, player, i, damage);
                    if (met != nullptr)
                        *met = kMetThing;
                    // Glass that takes it and stands (the marks are the
                    // brush's: they go with it). Glass that will break is
                    // cracked where it was hit -- one crack to a pane, moved
                    // by the next hit. Glass that never breaks is not: a
                    // shot leaves its small mark on it, a blow nothing but
                    // dust.
                    const std::uint32_t made = w.records[i].extra[1];
                    fx at[3], normal[3];
                    if (w.actors != nullptr && !w.states[i].dead && (made == 0 || made == 7) &&
                        w.brushStates != nullptr && traceShotWall(*w.world, eye, end, at, normal))
                    {
                        fx local[3];
                        for (int k = 0; k < 3; ++k)
                            local[k] = at[k] - w.brushStates[shot.mover].pos[k];
                        if (made == 0)
                            decalAdd(*w.actors, kDecalCrack, local, normal, 20, kMarkCrack, shot.mover);
                        else if (bullet)
                            decalAdd(*w.actors, static_cast<std::uint8_t>(kDecalShot + detail::chance(*w.actors) % kDecalShots),
                                     local, normal, 4, kMarkCrack, shot.mover);
                        specksAdd(*w.actors, at, normal, 2, static_cast<std::uint16_t>(24 | 26 << 5 | 27 << 10), 60);
                    }
                    return true;
                }
            }
        }
        // On the world, or on something that does not break: its mark, its
        // dust, and now and then the whine of it glancing off.
        if (met != nullptr && shot.fraction < kFractionOne)
            *met = kMetWall;
        if (mark && w.actors != nullptr && shot.fraction < kFractionOne && markShot(*w.actors, *w.world, eye, end, !bullet) &&
            bullet && w.heard != nullptr && detail::chance(*w.actors) % 3 == 0)
        {
            fx at[3];
            for (int i = 0; i < 3; ++i)
                at[i] = eye[i] + static_cast<fx>((static_cast<std::int64_t>(end[i] - eye[i]) * shot.fraction) >> kFractionShift);
            w.heard->add(detail::chance(*w.actors) & 1 ? CommonSound::Ricochet1 : CommonSound::Ricochet2, at, 100,
                         kRadiusMedium);
        }
        return false;
    }

    /// The next weapon the player has after the one in hand; kWeaponNone if
    /// they have none.
    inline std::uint8_t weaponAfter(const Inventory &inv)
    {
        for (int n = 1; n <= kWeaponCount; ++n)
        {
            const auto which = static_cast<std::uint8_t>((inv.current + n) % kWeaponCount);
            if (which != kWeaponNone && (inv.have & (1u << which)))
                return which;
        }
        return kWeaponNone;
    }

    namespace detail
    {
        /// The crowbar's blow, as the game makes it: along the view for
        /// kCrowbarReach units; and if that meets nothing, a box the size of
        /// someone crouching swept the same way, which is what lets a swing
        /// land on what is beside the line and not on it. Hurts what it
        /// meets by `damage`; sounds as what it met sounds; a wall's dent is
        /// left for a fifth of a second later (WeaponState::dentMs). Returns
        /// a kMet*.
        inline int crowbarBlow(LogicWorld &w, PlayerState &player, WeaponState &state, const fx *eye,
                               const fx *forward, int damage)
        {
            int met = kMetNothing;
            fireLine(w, player, eye, forward, kCrowbarReach, damage, false, false, &met);
            const fx end[3] = {eye[0] + forward[0] * kCrowbarReach, eye[1] + forward[1] * kCrowbarReach,
                               eye[2] + forward[2] * kCrowbarReach};
            fx probe[3] = {eye[0] + forward[0] * (kCrowbarReach + 16), eye[1] + forward[1] * (kCrowbarReach + 16),
                           eye[2] + forward[2] * (kCrowbarReach + 16)};
            bool breaks = false;
            if (met == kMetNothing && w.actors != nullptr)
            {
                // The box: whoever stands within half its width of the line.
                ActorWorld &a = *w.actors;
                int best = -1;
                std::int64_t nearest = kFractionOne;
                for (int i = 0; i < a.count; ++i)
                {
                    const ActorState &actor = a.states[i];
                    if (!actor.active || !canBeHurt(actor.kind) || actor.ai >= kAiDying)
                        continue;
                    fx mins[3], maxs[3];
                    targetBox(actor, mins, maxs);
                    for (int k = 0; k < 3; ++k)
                    {
                        mins[k] -= fxFromInt(k == 2 ? 18 : 16);
                        maxs[k] += fxFromInt(k == 2 ? 18 : 16);
                    }
                    std::int64_t enter = 0;
                    if (lineEntersBox(eye, end, mins, maxs, enter) && enter < nearest)
                    {
                        nearest = enter;
                        best = i;
                    }
                }
                if (best >= 0)
                {
                    monsterHurt(w, best, damage, false);
                    const ActorState &struck = a.states[best];
                    fx mins[3], maxs[3];
                    targetBox(struck, mins, maxs);
                    const fx at[3] = {struck.pos[0], struck.pos[1], (mins[2] + maxs[2]) / 2};
                    const bool killed = struck.ai >= kAiDying || !struck.active;
                    bleed(w, best, at, forward, killed);
                    met = bloodOf(struck.kind) == kBloodNone ? kMetThing : (killed ? kMetKilled : kMetFlesh);
                }
            }
            if (met == kMetNothing)
            {
                // ... and the world, and what stands in it.
                const Trace sweep = traceAll(*w.world, true, eye, end);
                if (!sweep.startSolid && sweep.fraction < kFractionOne)
                {
                    met = kMetWall;
                    // (Where on it: on from where the box stopped, into
                    // what stopped it.)
                    for (int k = 0; k < 3; ++k)
                        probe[k] = sweep.endPos[k] - static_cast<fx>((static_cast<std::int64_t>(sweep.normal[k]) * 28) >> 18);
                    if (sweep.mover >= 0)
                    {
                        for (int i = 0; i < w.recordCount; ++i)
                        {
                            if (w.records[i].brush == static_cast<std::uint16_t>(sweep.mover) && !w.states[i].dead &&
                                isBreakable(w.records[i]))
                            {
                                w.struckFrom[0] = forward[0];
                                w.struckFrom[1] = forward[1];
                                w.struckFrom[2] = forward[2];
                                logicStrike(w, player, i, damage);
                                met = kMetThing;
                            }
                        }
                    }
                }
            }
            else if (met == kMetThing)
            {
                breaks = true;
            }
            if (w.heard == nullptr || met == kMetNothing)
                return met;

            if (met == kMetFlesh || met == kMetKilled)
            {
                // One of its three thuds.
                static constexpr CommonSound kThuds[] = {CommonSound::CrowbarFlesh, CommonSound::CrowbarFlesh2,
                                                         CommonSound::CrowbarFlesh3};
                w.heard->add(kThuds[w.actors != nullptr ? chance(*w.actors) % 3 : 0], nullptr, 127, 0);
                return met;
            }
            // A wall, or a thing: where, and what it is made of. Each
            // material has its own sound, as loud as the game has it, and
            // lets the crowbar's own ring more or less.
            fx at[3] = {end[0], end[1], end[2]}, normal[3] = {0, 0, 0};
            const bool found = traceShotWall(*w.world, eye, probe, at, normal);
            Material made = Material::Concrete;
            if (found && w.level != nullptr && met == kMetWall)
                made = wallMaterial(*w.level, at, normal);
            static constexpr struct
            {
                CommonSound first;
                std::uint8_t count;
                std::uint8_t step; ///< (the grate's two are its first and its fourth)
                std::uint8_t loud; ///< of 127: the material's own sound
                std::uint8_t ring; ///< of 127: the crowbar's
            } kStruck[static_cast<int>(Material::Count)] = {
                {CommonSound::Step1, 2, 1, 114, 76},     // concrete
                {CommonSound::Metal1, 2, 1, 114, 38},    // metal
                {CommonSound::Dirt1, 3, 1, 114, 13},     // dirt
                {CommonSound::Duct1, 1, 1, 64, 38},      // vent
                {CommonSound::Grate1, 2, 3, 114, 64},    // grate
                {CommonSound::Tile1, 4, 1, 102, 25},     // tile
                {CommonSound::Slosh1, 4, 1, 114, 0},     // slosh
                {CommonSound::WoodHit1, 3, 1, 114, 25},  // wood
                {CommonSound::GlassHit1, 3, 1, 102, 25}, // computer
                {CommonSound::GlassHit1, 3, 1, 102, 25}, // glass
            };
            const auto &struck = kStruck[static_cast<int>(made)];
            std::uint32_t roll = w.actors != nullptr ? chance(*w.actors) : 0;
            // (On something that breaks, both are quieter.)
            const int loud = breaks ? struck.loud * 2 / 3 : struck.loud;
            const int ring = breaks ? struck.ring / 2 : struck.ring;
            w.heard->add(static_cast<std::uint16_t>(static_cast<int>(struck.first) + static_cast<int>(roll % struck.count) * struck.step),
                         at, loud, kRadiusMedium, 0, 96 + static_cast<int>((roll >> 8) & 15));
            if (ring > 0)
                w.heard->add((roll >> 16) & 1 ? CommonSound::CrowbarHit : CommonSound::CrowbarHit2, nullptr, ring, 0);
            if (ring > 0 && w.heard->count > 0)
                w.heard->events[w.heard->count - 1].pitch = static_cast<std::uint8_t>(98 + ((roll >> 20) & 3));
            // The dent, a moment later -- on a wall; a thing that breaks has
            // its own marks.
            if (found && met == kMetWall)
            {
                state.dentMs = 200;
                for (int k = 0; k < 3; ++k)
                {
                    state.dentAt[k] = at[k];
                    state.dentNormal[k] = normal[k];
                }
            }
            // Loud enough to turn a head.
            if (w.actors != nullptr)
            {
                for (int k = 0; k < 3; ++k)
                    w.actors->noise[k] = player.origin[k];
                w.actors->noiseMs = 600;
            }
            return met;
        }

        /// A swing of the crowbar has landed: the hand's other swing (the
        /// game has two for a blow that lands, turn about), and a quarter
        /// of a second -- unless it killed, which in the game ends the
        /// matter there and then.
        inline void crowbarLanded(WeaponState &state, int met)
        {
            static const char *const kHits[] = {"attack2hit", "attack3hit"};
            state.anim = kHits[state.swingHit++ % 2];
            state.retryMs = 0;
            if (met != kMetKilled)
                state.cooldownMs = 250;
        }
    } // namespace detail

    /// The weapon in hand, for `ms` more milliseconds: `attack` is the
    /// trigger held, `next` a press of "the other weapon", `second` the
    /// other button held: the MP5's launcher, and for any other gun
    /// "reload"; `pick` a weapon asked for by name (a kWeapon*; 0 none),
    /// taken if it is carried -- and if it is the one in hand already,
    /// reloaded; `reload` a reload asked for outright, of any gun (the
    /// MP5 too, which the other button cannot reload while it has
    /// grenades to launch). `forward`, `right` and `up` are the view's (Q12). Returns
    /// kWeapon* for what it did, for whoever draws it.
    inline std::uint32_t weaponsTick(LogicWorld &w, PlayerState &player, WeaponState &state,
                                     const fx *eye, const fx *forward, const fx *right, const fx *up,
                                     bool attack, bool next, int ms, bool second = false, int pick = 0,
                                     bool reload = false)
    {
        Inventory &inv = player.inv;
        std::uint32_t did = 0;
        if (state.cooldownMs > 0)
            state.cooldownMs -= ms;
        else if (state.readyMs < 100000)
            state.readyMs += ms; // (how long it has been ready: the crowbar's whole blow)
        if (state.secondMs > 0)
            state.secondMs -= ms;
        // The mark of the crowbar's last blow on a wall, a moment after it.
        if (state.dentMs > 0 && (state.dentMs -= ms) <= 0 && w.actors != nullptr)
            decalAdd(*w.actors, kDecalDent, state.dentAt, state.dentNormal, 8, kMarkShot);
        // A weapon being put away: when it is, the other is taken out.
        if (state.holsterMs > 0)
        {
            state.holsterMs -= ms;
            if (state.holsterMs > 0)
                return did;
            if (state.taking != kWeaponNone && (inv.have & (1u << state.taking)))
                inv.current = state.taking;
            state.taking = kWeaponNone;
            state.cooldownMs = 500;
            state.reloadMs = 0;
            return did | kWeaponDrawn;
        }
        // A swing of the crowbar that met nothing is made again a tenth of
        // a second on, without a sound: it may land then.
        if (state.retryMs > 0 && (state.retryMs -= ms) <= 0 && inv.current == kWeaponCrowbar)
        {
            const int met = detail::crowbarBlow(w, player, state, eye, forward, state.blow);
            if (met != kMetNothing)
            {
                detail::crowbarLanded(state, met);
                did |= kWeaponHit;
            }
        }
        // Something not held is not in hand (a save from before it was).
        if (inv.current != kWeaponNone && !(inv.have & (1u << inv.current)))
            inv.current = weaponAfter(inv);

        std::int16_t *clip = gunClip(inv);
        std::int16_t *spare = gunSpare(inv);
        const GunKind &gun = gunKind(inv.current);
        const int holds = gun.holds;
        const bool picked = pick > kWeaponNone && pick < kWeaponCount && pick != inv.current &&
                            (inv.have & (1u << pick)) != 0;
        reload = reload || (pick > kWeaponNone && pick == inv.current);

        if (state.reloadMs > 0)
        {
            state.reloadMs -= ms;
            if (state.reloadMs > 0 && !next && !picked)
                return did;
            if (state.reloadMs <= 0 && clip != nullptr)
            {
                const int take = holds - *clip < *spare ? holds - *clip : *spare;
                *clip = static_cast<std::int16_t>(*clip + take);
                *spare = static_cast<std::int16_t>(*spare - take);
            }
            state.reloadMs = 0;
        }
        // (A grenade with its pin out is not put down.)
        if ((next || picked) && state.pinMs < 0)
        {
            const std::uint8_t other = picked ? static_cast<std::uint8_t>(pick) : weaponAfter(inv);
            if (other != inv.current)
            {
                state.retryMs = 0;
                if (inv.current == kWeaponCrowbar)
                {
                    // Put away first, as the game puts it away: half a
                    // second, and then the other comes out.
                    state.anim = "holster";
                    state.holsterMs = 500;
                    state.taking = other;
                    state.reloadMs = 0;
                    return did;
                }
                inv.current = other;
                state.cooldownMs = 500;
                state.reloadMs = 0;
                return did | kWeaponDrawn;
            }
        }
        // ---- the guns and the grenade, as the game has them ----
        if (state.minded != inv.current)
        {
            // Another weapon in hand, however it got there.
            state.minded = inv.current;
            state.pumpMs = 0;
            state.loading = 0;
            state.loadMs = 0;
            state.pinMs = -1;
            state.redraw = false;
            state.idleMs = 1000;
            state.clicked = false;
        }
        auto chance = [&]() -> std::uint32_t
        {
            state.seed = state.seed * 1664525u + 1013904223u;
            return state.seed >> 8;
        };
        auto say = [&](CommonSound sound, int pitch = 100, int volume = 127)
        {
            if (w.heard != nullptr)
                w.heard->add(static_cast<std::uint16_t>(sound), nullptr, volume, kRadiusLarge, 0, pitch);
        };
        // (Once for each pull of the trigger, as the game's is.)
        auto click = [&]()
        {
            if (!state.clicked)
                say(CommonSound::Click, 100, 100);
            state.clicked = true;
        };
        if (!attack && !second)
            state.clicked = false;
        const bool under = player.waterLevel == 3;
        const bool isGun = clip != nullptr;

        // The shotgun is pumped a moment after it fires: that is when the
        // spent case comes out.
        if (state.pumpMs > 0 && (state.pumpMs -= ms) <= 0)
        {
            state.pumpMs = 0;
            say(CommonSound::ShotgunPump, 95 + static_cast<int>(chance() % 32));
            state.shells = state.pumpShells;
            did |= kWeaponShell;
        }

        // A reload begun: the shotgun's a shell at a time, the others' all
        // at once when the hand has done.
        auto startReload = [&]() -> bool
        {
            if (!isGun || *clip >= holds || *spare <= 0 || state.cooldownMs > 0)
                return false;
            if (inv.current == kWeaponShotgun)
            {
                state.anim = "start_reload";
                state.loading = 1;
                state.loadMs = 600;
                state.cooldownMs = 1000;
            }
            else
            {
                // (The sounds of it are the hand's own: its clip has them
                // where the magazine comes out and goes in.)
                state.anim = inv.current == kWeaponGlock && *clip > 0 ? "reload_noshot" : "reload";
                state.reloadMs = gun.reloadMs;
            }
            state.idleMs = 3000;
            did |= kWeaponReload;
            return true;
        };

        if (state.loading != 0)
        {
            // Firing breaks it off, with what is in it.
            if ((attack || second) && *clip > 0 && state.cooldownMs <= 0)
            {
                state.loading = 0;
            }
            else
            {
                state.loadMs -= ms;
                while (state.loading != 0 && state.loadMs <= 0)
                {
                    if (state.loading == 2)
                    {
                        *clip = static_cast<std::int16_t>(*clip + 1);
                        *spare = static_cast<std::int16_t>(*spare - 1);
                        state.loading = 1;
                    }
                    if (*clip < holds && *spare > 0)
                    {
                        state.anim = "reload";
                        say(chance() & 1 ? CommonSound::ShotgunReload : CommonSound::ShotgunReload3,
                            85 + static_cast<int>(chance() % 32));
                        state.loading = 2;
                        state.loadMs += 500;
                    }
                    else
                    {
                        // Full, or nothing more to put in: pumped, and done.
                        state.anim = "pump";
                        say(CommonSound::ShotgunPump, 95 + static_cast<int>(chance() % 32));
                        state.loading = 0;
                        state.loadMs = 0;
                        state.idleMs = 1500;
                    }
                }
                return did;
            }
        }

        if (inv.current == kWeaponGrenade)
        {
            if (state.pinMs < 0)
            {
                if (state.redraw && state.cooldownMs <= 0)
                {
                    // The next one out of the pocket.
                    state.redraw = false;
                    state.anim = "draw";
                    state.idleMs = 10000 + static_cast<int>(chance() % 5000);
                }
                if (attack && state.cooldownMs <= 0 && inv.grenades > 0 && w.actors != nullptr)
                {
                    state.pinMs = 0;
                    state.anim = "pinpull";
                    return did;
                }
            }
            else
            {
                state.pinMs += ms;
                const bool gone = state.pinMs >= kGrenadeFuse;
                if (gone || (!attack && state.pinMs >= kGrenadePull))
                {
                    // How it is thrown, by how high it is aimed: the
                    // game's own sum. (Looking down is a positive angle
                    // there; ten degrees are added upward, and the range
                    // above and below squeezed to fit.)
                    const fx flat[3] = {forward[0], forward[1], 0};
                    const fx along = fxLength(flat);
                    const int upTurns = static_cast<std::int16_t>(fxAtan2Turns(forward[2], along));
                    const int down = -upTurns * 360 * 16 / 65536; // sixteenths of a degree
                    const int aimed = -10 * 16 + (down < 0 ? down * 80 / 90 : down * 100 / 90);
                    int speed = (90 * 16 - aimed) * 13 / 32; // ... times six and a half
                    speed = speed > 1000 ? 1000 : speed;
                    fx sine = 0, cosine = 0;
                    fxSinCos(static_cast<std::uint16_t>(-aimed * 65536 / (360 * 16)), sine, cosine);
                    fx dir[3] = {0, 0, sine};
                    if (along > 0)
                    {
                        dir[0] = static_cast<fx>((static_cast<std::int64_t>(forward[0]) * cosine) / along);
                        dir[1] = static_cast<fx>((static_cast<std::int64_t>(forward[1]) * cosine) / along);
                    }
                    fx from[3], velocity[3];
                    for (int i = 0; i < 3; ++i)
                    {
                        from[i] = eye[i] + dir[i] * 16;
                        velocity[i] = gone ? 0 : dir[i] * speed + player.velocity[i];
                    }
                    const int left = kGrenadeFuse - state.pinMs;
                    state.pinMs = -1;
                    if (w.actors == nullptr || !projectileAdd(*w.actors, kShotGrenade, gone ? eye : from, velocity, left < 1 ? 1 : left))
                        return did;
                    inv.grenades = static_cast<std::int16_t>(inv.grenades - 1);
                    state.anim = speed < 500 ? "throw1" : (speed < 1000 ? "throw2" : "throw3");
                    state.cooldownMs = 500;
                    state.redraw = inv.grenades > 0;
                    did |= kWeaponFired;
                    if (inv.grenades == 0)
                    {
                        // The last one: nothing left to hold.
                        inv.have &= static_cast<std::uint8_t>(~(1u << kWeaponGrenade));
                        inv.current = weaponAfter(inv);
                        did |= kWeaponDrawn;
                    }
                    return did;
                }
                return did;
            }
            // Nothing doing: now and then it is turned over in the hand.
            if ((state.idleMs -= ms) <= 0 && state.cooldownMs <= 0 && !state.redraw)
            {
                const bool fidget = chance() % 4 == 3;
                state.anim = fidget ? "fidget" : "idle";
                state.idleMs = fidget ? 2500 : 10000 + static_cast<int>(chance() % 5000);
            }
            return did;
        }

        if (inv.current == kWeaponCrowbar)
        {
            if (!attack || state.cooldownMs > 0)
                return did;
            // A swing, in the game's order: the swish and one of the three
            // swings at the air, always; then the blow -- a whole one after
            // a second's rest, half of one on the heels of the last -- and
            // if it lands, one of the two swings that land takes the
            // other's place.
            static const char *const kMisses[] = {"attack1miss", "attack2", "attack3"};
            state.anim = kMisses[state.swingMiss++ % 3];
            state.blow = static_cast<std::uint8_t>(state.readyMs >= 1000 ? kCrowbarBlow : kCrowbarBlow / 2);
            state.readyMs = 0;
            if (w.heard != nullptr)
                w.heard->add(CommonSound::CrowbarMiss, nullptr);
            did |= kWeaponFired;
            const int met = detail::crowbarBlow(w, player, state, eye, forward, state.blow);
            if (met == kMetNothing)
            {
                state.cooldownMs = 500;
                state.retryMs = 100;
                return did;
            }
            detail::crowbarLanded(state, met);
            return did | kWeaponHit;
        }
        if (!isGun)
            return did;

        // A reload asked for outright. (The other button too, for the .357,
        // which has nothing else to do with it.)
        if ((reload || (second && inv.current == kWeaponPython)) && startReload())
            return did;

        // Rounds out of the gun: `rounds` of them, `pellets` in all, each
        // off line by the game's cone -- the sum of two draws either way,
        // so that most go near the middle.
        auto fire = [&](int rounds, int pellets, int cone, int wait, CommonSound sound, int pitch, const char *anim)
        {
            *clip = static_cast<std::int16_t>(*clip - rounds);
            say(sound, pitch);
            if (w.actors != nullptr)
            {
                // (Loud enough to turn a head.)
                for (int i = 0; i < 3; ++i)
                    w.actors->noise[i] = eye[i];
                w.actors->noiseMs = 400;
                // The flash at the gun's end: ahead of the eye, to the
                // right and a little down, where the gun is held.
                fx at[3];
                for (int i = 0; i < 3; ++i)
                    at[i] = eye[i] + forward[i] * 30 + right[i] * 6 - up[i] * 5;
                effectAdd(*w.actors, kEffectMuzzle, at, nullptr, 0, 50);
            }
            bool hit = false;
            for (int n = 0; n < pellets; ++n)
            {
                fx dir[3] = {forward[0], forward[1], forward[2]};
                if (cone != 0)
                {
                    const int x = (static_cast<int>(chance() % 4097) + static_cast<int>(chance() % 4097) - 4096) * cone / 4096;
                    const int y = (static_cast<int>(chance() % 4097) + static_cast<int>(chance() % 4097) - 4096) * cone / 4096;
                    for (int i = 0; i < 3; ++i)
                        dir[i] = forward[i] + ((right[i] * x + up[i] * y) >> kFxShift);
                }
                hit = fireLine(w, player, eye, dir, gun.range, gun.damage, true) || hit;
            }
            // (What the wait ran over by comes off the next: a gun that
            // fires ten times a second does, whatever the length of a tick.)
            state.cooldownMs = wait + (state.cooldownMs > -ms ? state.cooldownMs : -ms);
            state.anim = anim;
            state.idleMs = 10000 + static_cast<int>(chance() % 5000);
            did |= kWeaponFired | (hit ? kWeaponHit : 0);
        };

        const bool fires = attack || (second && (inv.current == kWeaponGlock || inv.current == kWeaponShotgun ||
                                                 inv.current == kWeaponMp5));
        if (fires && state.cooldownMs <= 0 && !(second && inv.current == kWeaponMp5 && !attack && state.secondMs > 0))
        {
            // Under water all but the pistol only click.
            if (under && !gun.wet)
            {
                click();
                state.cooldownMs = gun.emptyMs;
                return did;
            }
            // The other trigger first, as the game looks at it first.
            if (second && inv.current == kWeaponMp5)
            {
                // The launcher: a grenade that goes off at what it touches,
                // once a second.
                if (state.secondMs > 0)
                    return did;
                if (inv.ammoAr <= 0)
                {
                    click();
                    return did;
                }
                if (w.actors == nullptr)
                    return did;
                fx from[3], velocity[3];
                for (int i = 0; i < 3; ++i)
                {
                    from[i] = eye[i] + forward[i] * 16;
                    velocity[i] = forward[i] * 800;
                }
                if (!projectileAdd(*w.actors, kShotContact, from, velocity, 10000))
                    return did;
                inv.ammoAr = static_cast<std::int16_t>(inv.ammoAr - 1);
                state.secondMs = 1000;
                state.cooldownMs = state.cooldownMs > 1000 ? state.cooldownMs : 1000;
                say(chance() & 1 ? CommonSound::Launcher : CommonSound::Launcher2, 94 + static_cast<int>(chance() % 16));
                state.anim = "grenade";
                state.idleMs = 5000;
                return did | kWeaponSecondary;
            }
            if (second && inv.current == kWeaponShotgun)
            {
                // Both barrels: two shells, twelve pellets -- or, with one
                // shell or none, more shells first.
                if (*clip <= 1)
                {
                    if (!startReload())
                    {
                        click();
                        state.cooldownMs = gun.emptyMs;
                    }
                    return did;
                }
                fire(2, gun.pellets * 2, gun.cone, 1500, CommonSound::ShotgunDouble, 85 + static_cast<int>(chance() % 32),
                     "shoot_big");
                state.pumpMs = 950;
                state.pumpShells = 2;
                state.idleMs = *clip > 0 ? 6000 : 1500;
                return did | kWeaponSecondary;
            }
            if (*clip <= 0)
            {
                // Nothing in it: more put in, if there is more; or the click.
                if (!startReload())
                {
                    click();
                    state.cooldownMs = gun.emptyMs;
                }
                return did;
            }
            const bool fast = second && !attack && inv.current == kWeaponGlock;
            const char *anim = inv.current == kWeaponGlock && *clip == 1 && !fast ? "shoot_empty" : gun.shoot;
            fire(1, gun.pellets, fast ? kConePistolFast : gun.cone, fast ? 200 : gun.refireMs,
                 gun.fire[chance() & 1], gun.pitch + static_cast<int>(chance() % gun.pitchWide), anim);
            if (inv.current == kWeaponShotgun)
            {
                state.pumpMs = 500;
                state.pumpShells = 1;
                state.idleMs = *clip > 0 ? 5000 : 750;
            }
            else if (inv.current != kWeaponPython)
            {
                state.shells = 1;
                did |= kWeaponShell;
            }
            return did;
        }

        // Nothing doing: the hand does something of its own now and then,
        // each weapon by its own odds (the game's).
        if (!attack && !second && state.cooldownMs <= 0 && (state.idleMs -= ms) <= 0)
        {
            const int roll = static_cast<int>(chance() % 100);
            switch (inv.current)
            {
            case kWeaponGlock:
                // (Not with nothing in it: its slide is back.)
                if (*clip != 0)
                {
                    state.anim = roll < 30 ? "idle3" : (roll < 60 ? "idle1" : "idle2");
                    state.idleMs = roll < 30 ? 3060 : (roll < 60 ? 3750 : 2500);
                }
                else
                {
                    state.idleMs = 1000;
                }
                break;
            case kWeaponPython:
                state.anim = roll < 50 ? "idle1" : (roll < 70 ? "idle2" : (roll < 90 ? "idle3" : "fidget1"));
                state.idleMs = roll < 50 ? 2330 : (roll < 70 ? 2000 : (roll < 90 ? 2930 : 5670));
                break;
            case kWeaponShotgun:
                state.anim = roll < 80 ? "deepidle" : (roll < 95 ? "sm_idle" : "idle4");
                state.idleMs = roll < 80 ? 5000 : 2220;
                break;
            default: // the MP5
                state.anim = roll < 50 ? "longidle" : "idle1";
                state.idleMs = 10000 + static_cast<int>(chance() % 5000);
                break;
            }
        }
        return did;
    }

    /// The weapon in hand's own clip moved on by `ms`, and the sounds its
    /// clip has in it played as they are passed: the game keeps a reload's
    /// sounds in the animation, where the magazine comes out and goes in.
    inline void weaponHandTick(LogicWorld &w, ActorState &held, int ms)
    {
        if (w.actors == nullptr || !held.active)
            return;
        const std::int16_t clipBefore = held.clip;
        const std::uint32_t before = actorAdvance(*w.actors, held, ms);
        const ModelView *model = actorModel(*w.actors, held);
        if (model == nullptr || held.clip < 0 || held.clip != clipBefore || w.heard == nullptr || w.modelSounds == nullptr)
            return;
        const ModelClip &clip = model->clips[held.clip];
        const bool wrapped = held.frame < before;
        for (int e = 0; e < clip.eventCount; ++e)
        {
            const ModelEvent &event = model->events[clip.firstEvent + e];
            if (event.kind != kEventSound)
                continue;
            const std::uint32_t at = static_cast<std::uint32_t>(event.frame) << 8;
            const bool passed = wrapped ? (at > before || at <= held.frame)
                                        : ((before < at && at <= held.frame) || (at == 0 && before == 0 && held.frame > 0));
            if (!passed)
                continue;
            for (std::uint32_t n = 0; n < w.modelSoundCount; ++n)
            {
                if (w.modelSounds[n].hash == event.target)
                {
                    w.heard->add(w.modelSounds[n].sound, nullptr, 127, kRadiusLarge);
                    break;
                }
            }
        }
    }
} // namespace hldata

#include "HlEffects.hpp"
#include "HlGrunt.hpp"
#include "HlPeople.hpp"
