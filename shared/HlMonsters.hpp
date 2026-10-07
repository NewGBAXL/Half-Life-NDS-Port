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
        // (The game moves a bullsquid as its large hull, which is not
        // cooked: it goes as a person does, and fits through what it should
        // not.)
        static const MonsterKind kBullsquid = {40, false, fxFromInt(20), fxFromInt(48), fxFromInt(110),
                                               "idle", "run", "flinchs", "die1"};
        static const MonsterKind kHoundeye = {20, true, fxFromInt(16), fxFromInt(36), fxFromInt(160),
                                              "idle1", "run", "flinch_small", "die"};
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
    inline constexpr fx kLeapRange = fxFromInt(256);
    inline constexpr fx kZombieReach = fxFromInt(70);

    // ------------------------------------------------------------- the player

    /// A monster's blow. Armour takes four fifths of it, and half of what it
    /// takes comes off the armour.
    inline void playerHurt(PlayerState &player, int damage)
    {
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

    /// Something to be seen for `ms` milliseconds (kEffect*). When all the
    /// places are taken the oldest gives way.
    inline void effectAdd(ActorWorld &a, std::uint8_t kind, const fx *from, const fx *to, int size, int ms)
    {
        Effect *slot = &a.effects[0];
        for (Effect &effect : a.effects)
        {
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
    }

    /// Something set flying. False if there is no room for another.
    inline bool projectileAdd(ActorWorld &a, std::uint8_t kind, const fx *pos, const fx *velocity, int ms)
    {
        for (Projectile &shot : a.shots)
        {
            if (shot.active)
                continue;
            shot.active = true;
            shot.kind = kind;
            for (int i = 0; i < 3; ++i)
            {
                shot.pos[i] = pos[i];
                shot.velocity[i] = velocity[i];
            }
            shot.leftMs = ms;
            return true;
        }
        return false;
    }

    inline void monsterHurt(LogicWorld &w, int index, int damage, bool bullet);

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
            if (i == except || !actor.active || !isMonster(actor.kind) || actor.ai >= kAiDying)
                continue;
            const fx middle[3] = {actor.pos[0], actor.pos[1],
                                  actor.pos[2] + monsterKind(actor.kind).height / 2};
            const int hurt = share(middle);
            if (hurt > 0)
                monsterHurt(w, i, hurt, false);
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

    /// Hurt the monster at `index`. `bullet`: by a bullet, which a zombie
    /// hardly feels (three tenths, as in the game).
    inline void monsterHurt(LogicWorld &w, int index, int damage, bool bullet)
    {
        ActorWorld &a = *w.actors;
        ActorState &actor = a.states[index];
        if (!actor.active || !isMonster(actor.kind) || actor.ai >= kAiDying)
            return;
        if (bullet && actor.kind == static_cast<std::uint8_t>(ActorKind::Zombie))
            damage = (damage * 3 + 9) / 10;
        actor.health = static_cast<std::int16_t>(actor.health - damage);
        const MonsterKind &kind = monsterKind(actor.kind);
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
        if (actor.script >= 0)
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
                playerHurt(player, kHeadcrabBite);
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
                playerHurt(player, kBarnacleBite);
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
                playerHurt(player, damage);
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
                if (player.health <= 0 || flat > kSightRange || dz > fxFromInt(512) ||
                    dz < -fxFromInt(512))
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
                if (walkTick(a, world, index, kind.small, target, speed, ms, &player, kind.half,
                             kind.height) == kWalkNoWay)
                {
                    // Nowhere to go: it waits, facing them.
                    faceTo(actor, d[0], d[1], ms, 32768);
                    monsterClip(a, actor, kind.idle, false);
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
                        playerHurt(player, both ? kZombieBothSlash : kZombieSlash);
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
            for (Projectile &shot : a.shots)
            {
                if (!shot.active)
                    continue;
                shot.leftMs -= ms;
                const bool grenade = shot.kind == kShotGrenade;
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
                        if (w.heard != nullptr && fxLength(shot.velocity) > fxFromInt(60))
                            w.heard->add(CommonSound::GrenadeBounce, shot.pos, 100, kRadiusMedium);
                    }
                    for (int i = 0; i < 3; ++i)
                        shot.pos[i] = t.startSolid ? shot.pos[i] : to[i];
                    if (shot.leftMs <= 0)
                    {
                        shot.active = false;
                        effectAdd(a, kEffectBurst, shot.pos, nullptr, 96, 350);
                        if (w.heard != nullptr)
                            w.heard->add(CommonSound::Explode, shot.pos, 127, 2000);
                        radiusDamage(w, player, shot.pos, kGrenadeBlast, kGrenadeRadius, -1);
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
                    playerHurt(player, kSquidSpit);
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

    /// Everything in this file that happens by itself, for `ms` more
    /// milliseconds. After logicTick (which moves the actors' clips on).
    inline void monstersTick(LogicWorld &w, PlayerState &player, int ms)
    {
        if (w.actors == nullptr || w.world == nullptr)
            return;
        ActorWorld &a = *w.actors;
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
                                     : (item == ItemKind::GlockClip || item == ItemKind::Buckshot)
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
            if (!isMonster(actor.kind))
                continue;
            detail::monsterTick(w, player, i, ms);
            actor.lastFrame = actor.frame;
            actorPlaceBox(a, i);
        }
        detail::projectilesTick(w, player, ms);
    }

    // ------------------------------------------------------------- weapons

    struct WeaponState
    {
        std::int32_t cooldownMs = 0; ///< until it can be used again
        std::int32_t reloadMs = 0;   ///< left of a reload under way
        std::uint32_t seed = 777;    ///< where the shotgun's pellets go
    };

    /// What a shot along `dir` (Q12, unit) from `eye` lands on within
    /// `range` units, and hurts: a monster, or something that breaks. True
    /// if it hurt anything.
    inline bool fireLine(LogicWorld &w, PlayerState &player, const fx *eye, const fx *dir, int range,
                         int damage, bool bullet)
    {
        const fx end[3] = {eye[0] + dir[0] * range, eye[1] + dir[1] * range, eye[2] + dir[2] * range};
        const Shot shot = traceShot(*w.world, eye, end);
        // The nearest monster the line goes through before that.
        int struck = -1;
        std::int64_t nearest = shot.fraction;
        const ActorWorld *a = w.actors;
        for (int i = 0; a != nullptr && i < a->count; ++i)
        {
            const ActorState &actor = a->states[i];
            if (!actor.active || !isMonster(actor.kind) || actor.ai >= kAiDying)
                continue;
            const MonsterKind &kind = monsterKind(actor.kind);
            // (A barnacle's body is above its "feet".)
            const bool hangs = actor.kind == static_cast<std::uint8_t>(ActorKind::Barnacle);
            const fx mins[3] = {actor.pos[0] - kind.half, actor.pos[1] - kind.half,
                                hangs ? actor.pos[2] - fxFromInt(8) : actor.pos[2]};
            const fx maxs[3] = {actor.pos[0] + kind.half, actor.pos[1] + kind.half,
                                actor.pos[2] + kind.height};
            std::int64_t enter = 0, leave = kFractionOne;
            bool miss = false;
            for (int k = 0; k < 3 && !miss; ++k)
            {
                const fx d = end[k] - eye[k];
                if (d == 0)
                {
                    miss = eye[k] < mins[k] || eye[k] > maxs[k];
                    continue;
                }
                std::int64_t t0 = (static_cast<std::int64_t>(mins[k] - eye[k]) << kFractionShift) / d;
                std::int64_t t1 = (static_cast<std::int64_t>(maxs[k] - eye[k]) << kFractionShift) / d;
                if (t0 > t1)
                {
                    const std::int64_t swap = t0;
                    t0 = t1;
                    t1 = swap;
                }
                enter = t0 > enter ? t0 : enter;
                leave = t1 < leave ? t1 : leave;
                miss = enter > leave;
            }
            if (!miss && enter < nearest)
            {
                nearest = enter;
                struck = i;
            }
        }
        if (struck >= 0)
        {
            monsterHurt(w, struck, damage, bullet);
            return true;
        }
        if (shot.mover >= 0)
        {
            for (int i = 0; i < w.recordCount; ++i)
            {
                if (w.records[i].brush == static_cast<std::uint16_t>(shot.mover) && !w.states[i].dead &&
                    detail::isBreakable(w.records[i]))
                {
                    logicStrike(w, player, i, damage);
                    return true;
                }
            }
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

    /// The weapon in hand, for `ms` more milliseconds: `attack` is the
    /// trigger held, `next` a press of "the other weapon". `forward`,
    /// `right` and `up` are the view's (Q12). Returns kWeapon* for what it
    /// did, for whoever draws it.
    inline std::uint32_t weaponsTick(LogicWorld &w, PlayerState &player, WeaponState &state,
                                     const fx *eye, const fx *forward, const fx *right, const fx *up,
                                     bool attack, bool next, int ms)
    {
        Inventory &inv = player.inv;
        std::uint32_t did = 0;
        if (state.cooldownMs > 0)
            state.cooldownMs -= ms;
        // Something not held is not in hand (a save from before it was).
        if (inv.current != kWeaponNone && !(inv.have & (1u << inv.current)))
            inv.current = weaponAfter(inv);

        std::int16_t *clip = inv.current == kWeaponGlock ? &inv.clipGlock
                             : inv.current == kWeaponShotgun ? &inv.clipShotgun : nullptr;
        std::int16_t *spare = inv.current == kWeaponGlock ? &inv.ammo9mm
                              : inv.current == kWeaponShotgun ? &inv.ammoBuck : nullptr;
        const int holds = inv.current == kWeaponGlock ? 17 : 8;

        if (state.reloadMs > 0)
        {
            state.reloadMs -= ms;
            if (state.reloadMs > 0 && !next)
                return did;
            if (state.reloadMs <= 0 && clip != nullptr)
            {
                const int take = holds - *clip < *spare ? holds - *clip : *spare;
                *clip = static_cast<std::int16_t>(*clip + take);
                *spare = static_cast<std::int16_t>(*spare - take);
            }
            state.reloadMs = 0;
        }
        if (next)
        {
            const std::uint8_t other = weaponAfter(inv);
            if (other != inv.current)
            {
                inv.current = other;
                state.cooldownMs = 500;
                state.reloadMs = 0;
                return did | kWeaponDrawn;
            }
        }
        if (!attack || state.cooldownMs > 0 || inv.current == kWeaponNone)
            return did;

        if (inv.current == kWeaponGrenade)
        {
            // Thrown the way they look, a little up: three seconds to live.
            if (inv.grenades <= 0 || w.actors == nullptr)
                return did;
            fx from[3], velocity[3];
            for (int i = 0; i < 3; ++i)
            {
                from[i] = eye[i] + forward[i] * 16;
                velocity[i] = forward[i] * 500 + up[i] * 120;
            }
            if (!projectileAdd(*w.actors, kShotGrenade, from, velocity, 3000))
                return did;
            inv.grenades = static_cast<std::int16_t>(inv.grenades - 1);
            state.cooldownMs = 1000;
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
        if (inv.current == kWeaponCrowbar)
        {
            // What is in reach: a monster first, else something that breaks
            // (found as USE finds a button, and hit twice as hard, as the
            // game doubles a blunt blow on a thing).
            bool hit = fireLine(w, player, eye, forward, 48, 10, false);
            // A swing is not a shot: it lands on what is close and roughly
            // ahead, the nearest such (the game sweeps a box for the same
            // reason).
            if (!hit && w.actors != nullptr)
            {
                int best = -1;
                fx bestD = fxFromInt(72);
                for (int i = 0; i < w.actors->count; ++i)
                {
                    const ActorState &actor = w.actors->states[i];
                    if (!actor.active || !isMonster(actor.kind) || actor.ai >= kAiDying)
                        continue;
                    const fx half = monsterKind(actor.kind).height / 2;
                    const fx to[3] = {actor.pos[0] - eye[0], actor.pos[1] - eye[1],
                                      actor.pos[2] + half - eye[2]};
                    const fx distance = fxLength(to);
                    const std::int64_t ahead = static_cast<std::int64_t>(to[0]) * forward[0] +
                                               static_cast<std::int64_t>(to[1]) * forward[1] +
                                               static_cast<std::int64_t>(to[2]) * forward[2];
                    // Within 72 units and 50 degrees or so of straight ahead.
                    if (distance < bestD && ahead * 10 > static_cast<std::int64_t>(distance) * kFxOne * 6)
                    {
                        bestD = distance;
                        best = i;
                    }
                }
                if (best >= 0)
                {
                    monsterHurt(w, best, 10, false);
                    hit = true;
                }
            }
            if (!hit)
            {
                const int target = logicFindUsable(w, eye, forward, true);
                if (target >= 0)
                {
                    logicStrike(w, player, target, 20);
                    hit = true;
                }
            }
            state.cooldownMs = hit ? 250 : 500;
            if (w.heard != nullptr)
                w.heard->add(hit ? CommonSound::CrowbarHit : CommonSound::CrowbarMiss, nullptr);
            return did | kWeaponFired | (hit ? kWeaponHit : 0);
        }
        if (*clip <= 0)
        {
            if (*spare > 0)
            {
                state.reloadMs = inv.current == kWeaponGlock ? 1500 : 500 + 500 * (holds < *spare ? holds : *spare);
                if (w.heard != nullptr)
                    w.heard->add(inv.current == kWeaponGlock ? CommonSound::Reload
                                                             : CommonSound::ShotgunReload,
                                 nullptr);
                return did | kWeaponReload;
            }
            state.cooldownMs = 300; // a click
            if (w.heard != nullptr)
                w.heard->add(CommonSound::Click, nullptr);
            return did;
        }
        *clip = static_cast<std::int16_t>(*clip - 1);
        if (w.heard != nullptr)
            w.heard->add(inv.current == kWeaponGlock ? CommonSound::Pistol : CommonSound::Shotgun, nullptr);
        bool hit = false;
        if (inv.current == kWeaponGlock)
        {
            hit = fireLine(w, player, eye, forward, 4096, 8, true);
            state.cooldownMs = 300;
        }
        else
        {
            // Six pellets in a cone of five degrees or so.
            for (int n = 0; n < 6; ++n)
            {
                state.seed = state.seed * 1664525u + 1013904223u;
                const int x = static_cast<int>((state.seed >> 8) % 715) - 357;
                state.seed = state.seed * 1664525u + 1013904223u;
                const int y = static_cast<int>((state.seed >> 8) % 715) - 357;
                fx dir[3];
                for (int i = 0; i < 3; ++i)
                    dir[i] = forward[i] + ((right[i] * x + up[i] * y) >> kFxShift);
                hit = fireLine(w, player, eye, dir, 2048, 5, true) || hit;
            }
            state.cooldownMs = 750;
        }
        return did | kWeaponFired | (hit ? kWeaponHit : 0);
    }
} // namespace hldata
