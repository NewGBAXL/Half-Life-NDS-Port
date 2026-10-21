#pragma once

// Round 3 of the monsters, each from its file in the game's code, kept as
// close to it as the port allows and named as it is, so the two can be read
// side by side:
//
//   cockroach   roach.cpp          its own think: no schedules
//   snark       squeakgrenade.cpp  a monstermaker's: let go as a snark that
//                                  hunts the player (HlMonsters.hpp's shots)
//   alien grunt agrunt.cpp         a squad, the hornet gun, its fists
//   controller  controller.cpp     flies (its Move: aiFlyTick, here); a ball
//                                  that hunts, and a stream of small ones
//   leech       leech.cpp          its own swimming think: no schedules
//
// std-only and inline (HlMonsters.hpp includes it last): the host tests run
// it.

#include <algorithm>

namespace hldata
{
    // ============================================================= cockroach

    namespace roach
    {
        // m_iMode (count1).
        inline constexpr int kIdle = 0;
        inline constexpr int kBored = 1;
        inline constexpr int kScaredByEnt = 2;
        inline constexpr int kScaredByLight = 3;
        inline constexpr int kSmellFood = 4;
        inline constexpr int kEat = 5;
        // Where it runs to: vec1 (m_Route[0]); whether it is going: flag1;
        // when it last thought: time1.

        /// Eat: it will not think of food again for `seconds`.
        inline void eat(AiCtx &c, int seconds)
        {
            c.m.hungryTime = c.ai.now + seconds * 1000;
        }
        inline bool shouldEat(const AiCtx &c)
        {
            return c.m.hungryTime <= c.ai.now;
        }

        /// The food it smells (Listen, for a roach: a body or a gib within
        /// what the sound carries), nearest first; null if none.
        inline const AiSound *smellFood(AiCtx &c)
        {
            const AiSound *best = nullptr;
            fx nearest = 0;
            for (std::int8_t i = c.ai.sounds.active; i != kSoundListEmpty; i = c.ai.sounds.pool[i].next)
            {
                const AiSound &s = c.ai.sounds.pool[i];
                if (!(s.type & (kSoundCarcass | kSoundMeat)))
                    continue;
                const fx far = aiDist(s.origin, c.me.pos);
                if (far > fxFromInt(s.volume) || (best != nullptr && far >= nearest))
                    continue;
                best = &s;
                nearest = far;
            }
            return best;
        }

        /// PickNewDest: food, if that is what it is going for; else
        /// somewhere 256 to 511 away, and at least 128 in the plane.
        inline void pickNewDest(AiCtx &c, int mode)
        {
            c.m.count1 = mode;
            c.m.flag1 = true;
            if (mode == kSmellFood)
            {
                if (const AiSound *food = smellFood(c))
                {
                    c.m.vec1[0] = food->origin[0] + fxFromInt(3 - aiRandomLong(c.a, 0, 5));
                    c.m.vec1[1] = food->origin[1] + fxFromInt(3 - aiRandomLong(c.a, 0, 5));
                    c.m.vec1[2] = food->origin[2];
                    return;
                }
            }
            for (int tries = 0; tries < 8; ++tries)
            {
                const fx dir[2] = {fxFromInt(aiRandomLong(c.a, -1000, 1000)) / 1000,
                                   fxFromInt(aiRandomLong(c.a, -1000, 1000)) / 1000};
                const int far = 256 + aiRandomLong(c.a, 0, 255);
                const fx to[3] = {c.me.pos[0] + dir[0] * far, c.me.pos[1] + dir[1] * far, c.me.pos[2]};
                c.m.vec1[0] = to[0];
                c.m.vec1[1] = to[1];
                c.m.vec1[2] = to[2];
                if (aiDist2D(to, c.me.pos) >= fxFromInt(128))
                    break;
            }
            // Now and then, a skitter as it goes.
            if (aiRandomLong(c.a, 0, 9) == 1)
                detail::aiSay(c, CommonSound::RoachWalk);
        }

        /// WALK_MOVE for something two units across: a step through the
        /// world as a point a few units up -- or up a stair -- then down to
        /// the floor, a stair's height or two below. (The console's smallest
        /// walk is a crouching person's, and the game's roaches are in the
        /// vents and under things that never let one through.)
        inline bool tinyStep(AiCtx &c, fx *feet, fx dx, fx dy)
        {
            for (const int up : {4, 18})
            {
                const fx from[3] = {feet[0], feet[1], feet[2] + fxFromInt(up)};
                const fx to[3] = {feet[0] + dx, feet[1] + dy, feet[2] + fxFromInt(up)};
                if (traceShot(*c.w.world, from, to).fraction < kFractionOne)
                    continue;
                const fx down[3] = {to[0], to[1], to[2] - fxFromInt(up + 36)};
                const Shot floor = traceShot(*c.w.world, to, down);
                if (floor.fraction >= kFractionOne)
                    return false; // (no floor: it does not walk off an edge)
                feet[0] = to[0];
                feet[1] = to[1];
                feet[2] = to[2] + static_cast<fx>((static_cast<std::int64_t>(down[2] - to[2]) * floor.fraction) >> kFractionShift);
                return true;
            }
            return false;
        }

        inline void stop(AiCtx &c)
        {
            c.m.flag1 = false;
            aiSetActivity(c, kActIdle);
            c.m.count1 = c.m.count1 == kSmellFood ? kEat : kIdle;
        }

        /// Move: a step along its way -- it may overshoot -- and a new way
        /// if it is held up.
        inline void move(AiCtx &c, int ms)
        {
            const fx far = aiDist2D(c.m.vec1, c.me.pos);
            c.me.yaw = fxAtan2Turns(c.m.vec1[1] - c.me.pos[1], c.m.vec1[0] - c.me.pos[0]);
            fx forward[3];
            aiForward(c.me.yaw, forward);
            const fx speed = c.m.groundSpeed > 0 ? c.m.groundSpeed : monsterKind(c.me.kind).speed;
            // (Not past where it is going -- the game's may overshoot, into
            // the wall behind -- and a long while unseen goes in tenths.)
            if (ms > 100)
            {
                for (int done = 0; done < ms && c.m.flag1; done += 100)
                    move(c, ms - done > 100 ? 100 : ms - done);
                return;
            }
            fx step = static_cast<fx>((static_cast<std::int64_t>(speed) * ms) / 1000);
            const bool arrives = far <= step;
            if (arrives)
                step = far;
            // (A look for what is in the way only now and then: the game's
            // own load balancing.)
            if (aiRandomLong(c.a, 0, 7) == 1)
            {
                fx probe[3] = {c.me.pos[0], c.me.pos[1], c.me.pos[2]};
                if (!tinyStep(c, probe, forward[0] * 4, forward[1] * 4))
                {
                    pickNewDest(c, c.m.count1);
                    return;
                }
            }
            fx feet[3] = {c.me.pos[0], c.me.pos[1], c.me.pos[2]};
            const fx dx = static_cast<fx>((static_cast<std::int64_t>(forward[0]) * step) >> kFxShift);
            const fx dy = static_cast<fx>((static_cast<std::int64_t>(forward[1]) * step) >> kFxShift);
            if (tinyStep(c, feet, dx, dy))
            {
                for (int k = 0; k < 3; ++k)
                    c.me.pos[k] = feet[k];
                actorPlaceBox(c.a, c.index);
            }
            if (arrives)
            {
                stop(c);
                return;
            }
            if (aiRandomLong(c.a, 0, 149) == 1 && c.m.count1 != kScaredByLight && c.m.count1 != kSmellFood)
                pickNewDest(c, kIdle);
        }

        /// Look, for a roach: all round, 150 units, at whatever it fears --
        /// only while the player is somewhere it can be seen from.
        inline bool seesFear(AiCtx &c, int distance)
        {
            const fx reach = fxFromInt(distance);
            AiTarget t;
            if (c.player.health > 0 && !unseen(c.player) && aiDist(c.player.origin, c.me.pos) < reach + kStandingFeet &&
                aiTargetOf(c.w, c.player, kAiPlayer, t) && aiRelationship(c, t) == kRelFear)
                return true;
            for (int i = 0; i < c.a.count; ++i)
            {
                const ActorState &other = c.a.states[i];
                if (i == c.index || !other.active || other.ai >= kAiDying || other.kind == c.me.kind ||
                    !canBeHurt(other.kind))
                    continue;
                const fx d[2] = {other.pos[0] - c.me.pos[0], other.pos[1] - c.me.pos[1]};
                if (d[0] > reach || d[0] < -reach || d[1] > reach || d[1] < -reach ||
                    aiDist(other.pos, c.me.pos) >= reach)
                    continue;
                if (aiTargetOf(c.w, c.player, static_cast<std::int16_t>(i), t) && aiRelationship(c, t) == kRelFear)
                    return true;
            }
            return false;
        }

        /// MonsterThink, the roach's: ten times a second where the player
        /// may see it, once a second or so otherwise.
        inline void think(AiCtx &c)
        {
            const int ms = c.m.time1 != 0 && c.ai.now > c.m.time1 ? static_cast<int>(std::min<std::int32_t>(c.ai.now - c.m.time1, 1500)) : 100;
            c.m.time1 = c.ai.now;
            if (c.me.ai >= kAiDying)
                return;
            const bool seen = aiClientInPvs(c);
            if (!seen)
                c.m.thinkMs += aiRandomMs(c.a, 900, 1400);
            switch (c.m.count1)
            {
            case kIdle:
            case kEat:
                // Not going anywhere: now and then, a look round.
                if (aiRandomLong(c.a, 0, 3) == 1)
                {
                    if (seen && seesFear(c, 150))
                    {
                        eat(c, 30 + aiRandomLong(c.a, 0, 14));
                        pickNewDest(c, kScaredByEnt);
                        aiSetActivity(c, kActWalk);
                    }
                    else if (aiRandomLong(c.a, 0, 149) == 1)
                    {
                        // (Bored.)
                        pickNewDest(c, kBored);
                        aiSetActivity(c, kActWalk);
                        if (c.m.count1 == kEat)
                            eat(c, 30 + aiRandomLong(c.a, 0, 14));
                    }
                }
                // Food, if it is hungry and is only standing about. (The
                // game's roach also runs from a light that comes on: the
                // console has no light to measure where it stands.)
                if (c.m.count1 == kIdle && shouldEat(c))
                {
                    const AiSound *food = smellFood(c);
                    if (food != nullptr && food->origin[2] - c.me.pos[2] <= fxFromInt(3) &&
                        food->origin[2] - c.me.pos[2] >= -fxFromInt(3))
                    {
                        pickNewDest(c, kSmellFood);
                        aiSetActivity(c, kActWalk);
                    }
                }
                break;
            default:
                break;
            }
            if (c.m.flag1)
                move(c, ms);
        }

        /// Touch: the player walking over it is the end of it.
        inline void frame(AiCtx &c, int)
        {
            if (c.me.ai >= kAiDying || c.player.health <= 0)
                return;
            const fx moving = c.player.velocity[0] | c.player.velocity[1] | c.player.velocity[2];
            if (moving == 0)
                return;
            const fx at[3] = {c.me.pos[0], c.me.pos[1], c.me.pos[2] + fxFromInt(2)};
            if (detail::playerTouchesBox(c.player, at, at, fxFromInt(2)))
            {
                markFloor(c.a, *c.w.world, c.me.pos, 24, kDecalAlien2, 6, kMarkAlien);
                monsterHurt(c.w, c.index, c.me.health, false, kHitGeneric, kDmgCrush, kAiPlayer);
            }
        }

        /// Killed: a squash, or now and then its cry; and gone.
        inline bool killed(AiCtx &c)
        {
            detail::aiSay(c, aiRandomLong(c.a, 0, 4) == 1 ? CommonSound::RoachDie : CommonSound::RoachSmash);
            soundInsert(c.ai.sounds, c.ai.now, kSoundWorld, c.me.pos, 128, 1000);
            c.me.ai = kAiDead;
            c.m.deadflag = 2;
            c.m.state = c.m.idealState = kStateDead;
            c.me.active = false;
            actorPlaceBox(c.a, c.index);
            return true;
        }

        inline const AiKind &kind()
        {
            static const AiKind k = []
            {
                AiKind k;
                k.kind = static_cast<std::uint8_t>(ActorKind::Cockroach);
                k.health = 1;
                k.eye = fxFromInt(1);
                k.fov = fxDot(0.5);
                k.blood = kBloodYellow;
                k.think = think;
                k.frame = frame;
                k.killed = killed;
                return k;
            }();
            return k;
        }
    } // namespace roach

    // ================================================================= snark

    namespace snark
    {
        /// A monstermaker's snark: let go as the snark a player throws is
        /// (HlMonsters.hpp: kShotSnarkFoe), after the player and their side,
        /// and the place it was made in is free -- its maker makes as many
        /// as it makes, a few, all told.
        inline void think(AiCtx &c)
        {
            if (!c.me.active)
                return;
            // (A point a few units up, as it is: HlMonsters.hpp. Off toward
            // the player.)
            const fx at[3] = {c.me.pos[0], c.me.pos[1], c.me.pos[2] + fxFromInt(4)};
            fx toward[3] = {c.player.origin[0] - at[0], c.player.origin[1] - at[1], 0};
            const fx far = fxLength(toward);
            for (int k = 0; k < 2; ++k)
                toward[k] = far > 0 ? static_cast<fx>((static_cast<std::int64_t>(toward[k]) << kFxShift) / far) : 0;
            const fx velocity[3] = {toward[0] * 120 + fxFromInt(aiRandomLong(c.a, -30, 30)),
                                    toward[1] * 120 + fxFromInt(aiRandomLong(c.a, -30, 30)), fxFromInt(60)};
            Projectile *shot = projectileAdd(c.a, kShotSnarkFoe, at, velocity, 15000);
            if (shot == nullptr)
                return; // (no room yet: again at its next think)
            shot->sprite = 0;
            detail::aiSay(c, CommonSound::SnarkDeploy);
            c.me.ai = kAiDead;
            c.m.state = c.m.idealState = kStateDead;
            c.me.active = false;
            actorPlaceBox(c.a, c.index);
        }

        inline const AiKind &kind()
        {
            static const AiKind k = []
            {
                AiKind k;
                k.kind = static_cast<std::uint8_t>(ActorKind::Snark);
                k.health = 2;
                k.blood = kBloodYellow;
                k.think = think;
                return k;
            }();
            return k;
        }
    } // namespace snark

    // =========================================================== alien grunt

    namespace agrunt
    {
        inline constexpr int kHornet1 = 1, kHornet5 = 5, kLeftPunch = 12, kRightPunch = 13;
        inline constexpr int kMeleeDist = 100; ///< AGRUNT_MELEE_DIST
        inline constexpr int kDmgPunch = 20;   ///< sk_agrunt_dmg_punch
        inline constexpr int kArmour = 10;     ///< the hit group of its armour
        // m_flNextHornetAttackCheck: time1; m_fCanHornetAttack: flag1;
        // m_flNextSpeakTime, m_flNextWordTime: time2, time3; m_iLastWord:
        // count1; m_flNextPainTime: value2 (ms).

        enum : std::uint8_t
        {
            kSchedThreatDisplay = kSchedLastCommon + 1,
        };

        inline constexpr AiTask tlAGruntFail[] = {
            {kTaskStopMoving, 0}, {kTaskSetActivity, kActIdle}, {kTaskWait, 2000}, {kTaskWaitPvs, 0}};
        inline constexpr AiSchedule slAGruntFail = {tlAGruntFail, 4, kCondCanRangeAttack1 | kCondCanMeleeAttack1, 0,
                                                    "AGrunt Fail"};
        inline constexpr AiTask tlAGruntCombatFail[] = {
            {kTaskStopMoving, 0}, {kTaskSetActivity, kActIdle}, {kTaskWaitFaceEnemy, 2000}, {kTaskWaitPvs, 0}};
        inline constexpr AiSchedule slAGruntCombatFail = {tlAGruntCombatFail, 4,
                                                          kCondCanRangeAttack1 | kCondCanMeleeAttack1, 0,
                                                          "AGrunt Combat Fail"};
        inline constexpr AiTask tlAGruntStandoff[] = {
            {kTaskStopMoving, 0}, {kTaskSetActivity, kActIdle}, {kTaskWaitFaceEnemy, 2000}};
        inline constexpr AiSchedule slAGruntStandoff = {
            tlAGruntStandoff, 3,
            kCondCanRangeAttack1 | kCondCanMeleeAttack1 | kCondSeeEnemy | kCondNewEnemy | kCondHearSound,
            kSoundDanger, "Agrunt Standoff"};
        inline constexpr AiTask tlAGruntRangeAttack1[] = {
            {kTaskStopMoving, 0}, {kTaskFaceEnemy, 0}, {kTaskRangeAttack1, 0}};
        inline constexpr AiSchedule slAGruntRangeAttack1 = {tlAGruntRangeAttack1, 3,
                                                            kCondNewEnemy | kCondEnemyDead | kCondHeavyDamage, 0,
                                                            "AGrunt Range Attack1"};
        inline constexpr AiTask tlAGruntTakeCoverFromEnemy[] = {
            {kTaskStopMoving, 0},     {kTaskWait, 200},
            {kTaskFindCoverFromEnemy, 0}, {kTaskRunPath, 0},
            {kTaskWaitForMovement, 0}, {kTaskRemember, static_cast<std::int32_t>(kMemoryInCover)},
            {kTaskFaceEnemy, 0}};
        inline constexpr AiSchedule slAGruntTakeCoverFromEnemy = {tlAGruntTakeCoverFromEnemy, 7, kCondNewEnemy, 0,
                                                                  "AGruntTakeCoverFromEnemy"};
        inline constexpr AiTask tlAGruntVictoryDance[] = {
            {kTaskStopMoving, 0},
            {kTaskSetFailSchedule, kSchedThreatDisplay},
            {kTaskWait, 200},
            {kTaskGetPathToEnemyCorpse, 0},
            {kTaskWalkPath, 0},
            {kTaskWaitForMovement, 0},
            {kTaskFaceEnemy, 0},
            {kTaskPlaySequence, kActCrouch},
            {kTaskPlaySequence, kActVictoryDance},
            {kTaskPlaySequence, kActVictoryDance},
            {kTaskPlaySequence, kActStand},
            {kTaskPlaySequence, kActThreatDisplay},
            {kTaskPlaySequence, kActCrouch},
            {kTaskPlaySequence, kActVictoryDance},
            {kTaskPlaySequence, kActVictoryDance},
            {kTaskPlaySequence, kActStand}};
        inline constexpr AiSchedule slAGruntVictoryDance = {
            tlAGruntVictoryDance, 16, kCondNewEnemy | kCondLightDamage | kCondHeavyDamage, 0, "AGruntVictoryDance"};
        inline constexpr AiTask tlAGruntThreatDisplay[] = {
            {kTaskStopMoving, 0}, {kTaskFaceEnemy, 0}, {kTaskPlaySequence, kActThreatDisplay}};
        inline constexpr AiSchedule slAGruntThreatDisplay = {
            tlAGruntThreatDisplay, 3, kCondNewEnemy | kCondLightDamage | kCondHeavyDamage,
            kSoundPlayer | kSoundCombat | kSoundWorld, "AGruntThreatDisplay"};

        /// IRelationship: the soldiers are its nemesis.
        inline int relationship(AiCtx &c, std::int16_t target, int base)
        {
            if (target >= 0 && target < c.a.count && isGrunt(c.a.states[target].kind))
                return kRelNemesis;
            return base;
        }

        /// TraceAttack: its armour (hit group 10) turns a bullet, a blade or a
        /// club, all but a little.
        inline bool traceAttack(AiCtx &c, AiDamage &d)
        {
            if (d.group == kArmour && (d.type & (kDmgBullet | kDmgSlash | kDmgClub)))
            {
                if (d.hasFrom)
                    effectAdd(c.a, kEffectMuzzle, d.from, nullptr, 2, 100); // (its ricochet's spark)
                d.amount -= 20;
                if (d.amount <= 0)
                {
                    d.amount = 0;
                    d.graze = true; // (the game's 0.1: felt, nothing taken)
                }
            }
            return true;
        }

        inline void stopTalking(AiCtx &c)
        {
            c.m.time2 = c.m.time3 = c.ai.now + 10000 + aiRandomLong(c.a, 0, 10) * 1000;
        }

        /// PrescheduleThink: the words it says to itself.
        inline void prescheduleThink(AiCtx &c)
        {
            if (c.m.time2 > c.ai.now || c.m.time3 >= c.ai.now)
                return;
            detail::aiSay(c, aiRandomLong(c.a, 0, 1) == 0 ? CommonSound::AgruntIdle1 : CommonSound::AgruntIdle2);
            if (aiRandomLong(c.a, 1, 10) <= 1)
                stopTalking(c);
            else
                c.m.time3 = c.ai.now + aiRandomMs(c.a, 500, 1000);
        }

        inline void sound(AiCtx &c, int which)
        {
            switch (which)
            {
            case kAiSoundAlert:
                stopTalking(c);
                detail::aiSay(c, aiRandomLong(c.a, 0, 1) == 0 ? CommonSound::AgruntAlert1 : CommonSound::AgruntAlert2);
                break;
            case kAiSoundPain:
                if (c.m.value2 > c.ai.now)
                    return;
                c.m.value2 = c.ai.now + 600;
                stopTalking(c);
                detail::aiSay(c, aiRandomLong(c.a, 0, 1) == 0 ? CommonSound::AgruntPain1 : CommonSound::AgruntPain2);
                break;
            case kAiSoundDeath:
                stopTalking(c);
                detail::aiSay(c, aiRandomLong(c.a, 0, 1) == 0 ? CommonSound::AgruntDie1 : CommonSound::AgruntDie2);
                break;
            default:
                break;
            }
        }

        inline int yawSpeed(AiCtx &c)
        {
            return c.m.activity == kActTurnLeft || c.m.activity == kActTurnRight ? 110 : 100;
        }

        /// Where its gun arm is (the model's first attachment, as the game's
        /// m_HackedGunPos has it: 24 to its right, 64 ahead, 48 up -- made
        /// a little nearer, the console's model being drawn whole).
        inline void armPos(const AiCtx &c, fx *at)
        {
            fx forward[3], right[3];
            aiForward(c.me.yaw, forward);
            aiRight(c.me.yaw, right);
            for (int k = 0; k < 2; ++k)
                at[k] = c.me.pos[k] + right[k] * 24 + forward[k] * 32;
            at[2] = c.me.pos[2] + fxFromInt(48);
        }

        inline bool canCheckAttacks(AiCtx &c)
        {
            return !aiHas(c, kCondEnemyTooFar);
        }

        inline bool checkMelee1(AiCtx &c, fx dot, fx dist)
        {
            return aiHas(c, kCondSeeEnemy) && dist <= fxFromInt(kMeleeDist) && dot >= fxDot(0.6) &&
                   c.m.enemy != kAiNone;
        }

        /// NoFriendlyFire (the squad monster's: none of its squad ahead).
        inline bool noFriendlyFire(AiCtx &c)
        {
            if (!aiInSquad(c))
                return true;
            AiTarget t;
            if (!aiTargetOf(c.w, c.player, c.m.enemy, t))
                return false;
            const std::uint16_t yaw = fxAtan2Turns(t.center[1] - c.me.pos[1], t.center[0] - c.me.pos[0]);
            fx forward[3], right[3];
            aiForward(yaw, forward);
            aiRight(yaw, right);
            for (int i = 0; i < kMaxSquadMembers; ++i)
            {
                const std::int16_t id = aiSquadMember(c, i);
                if (id == kAiNone || id == c.index)
                    continue;
                const fx *p = c.a.states[id].pos;
                const fx d[2] = {p[0] - c.me.pos[0], p[1] - c.me.pos[1]};
                const std::int64_t ahead = static_cast<std::int64_t>(d[0]) * forward[0] + static_cast<std::int64_t>(d[1]) * forward[1];
                const std::int64_t side = static_cast<std::int64_t>(d[0]) * right[0] + static_cast<std::int64_t>(d[1]) * right[1];
                const std::int64_t width = static_cast<std::int64_t>(fxFromInt(48)) * kFxOne;
                if (ahead > 0 && side < width && side > -width)
                    return false;
            }
            return true;
        }

        /// CheckRangeAttack1: its enemy seen, 100 to 1024 off, more or less
        /// ahead, and a clear line to it from its arm -- looked at every two
        /// to five seconds once it is yes, every fifth of one while no.
        inline bool checkRange1(AiCtx &c, fx dot, fx dist)
        {
            if (c.ai.now < c.m.time1)
                return c.m.flag1;
            if (aiHas(c, kCondSeeEnemy) && dist >= fxFromInt(kMeleeDist) && dist <= fxFromInt(1024) &&
                dot >= fxDot(0.5) && noFriendlyFire(c))
            {
                AiTarget t;
                fx arm[3];
                armPos(c, arm);
                if (aiTargetOf(c.w, c.player, c.m.enemy, t))
                {
                    fx end[3];
                    const fx d[3] = {t.center[0] - arm[0], t.center[1] - arm[1], t.center[2] - arm[2]};
                    const fx far = fxLength(d);
                    fx dir[3] = {0, 0, 0};
                    for (int k = 0; far > 0 && k < 3; ++k)
                        dir[k] = static_cast<fx>((static_cast<std::int64_t>(d[k]) << kFxShift) / far);
                    const std::int16_t hit = aiTraceLine(c, arm, dir, fxToInt(far) + 8, end);
                    if (hit == c.m.enemy || aiDist(end, arm) + fxFromInt(8) >= far)
                    {
                        c.m.time1 = c.ai.now + aiRandomMs(c.a, 2000, 5000);
                        c.m.flag1 = true;
                        return true;
                    }
                }
            }
            c.m.time1 = c.ai.now + 200;
            c.m.flag1 = false;
            return false;
        }

        /// A hornet from its arm, at its enemy where it was last seen.
        inline void shootHornet(AiCtx &c)
        {
            fx arm[3];
            armPos(c, arm);
            fx dir[3];
            if (aiHas(c, kCondSeeEnemy))
            {
                const fx d[3] = {c.m.enemyLkp[0] - c.me.pos[0], c.m.enemyLkp[1] - c.me.pos[1],
                                 c.m.enemyLkp[2] - c.me.pos[2]};
                const fx far = fxLength(d);
                for (int k = 0; k < 3; ++k)
                    dir[k] = far > 0 ? static_cast<fx>((static_cast<std::int64_t>(d[k]) << kFxShift) / far) : 0;
            }
            else
            {
                aiForward(c.me.yaw, dir);
            }
            for (int k = 0; k < 3; ++k)
                arm[k] += dir[k] * 32;
            effectAdd(c.a, kEffectMuzzle, arm, nullptr, 6, 100);
            const fx velocity[3] = {dir[0] * 300, dir[1] * 300, dir[2] * 300};
            if (Projectile *shot = projectileAdd(c.a, kShotHornetFoe, arm, velocity, 5000))
                shot->sprite = aiRandomLong(c.a, 1, 5) <= 2 ? 0 : 2; // (red, or the faster orange)
            detail::aiSay(c, static_cast<CommonSound>(static_cast<int>(CommonSound::HornetFire1) + aiRandomLong(c.a, 0, 2)));
        }

        /// A fist: what is 100 ahead takes it, and the player is thrown to
        /// the side the blow came from.
        inline void punch(AiCtx &c, int side)
        {
            const std::int16_t hit = aiHullAttack(c, kMeleeDist, kDmgPunch, kDmgClub);
            if (hit == kAiNone)
            {
                detail::aiSay(c, CommonSound::ClawMiss);
                return;
            }
            detail::aiPunch(c, hit, fxFromInt(8), fxFromInt(25) * side);
            fx right[3];
            aiRight(c.me.yaw, right);
            const fx shove[3] = {right[0] * 250 * side, right[1] * 250 * side, 0};
            detail::aiShove(c, hit, shove);
            detail::aiSay(c, CommonSound::ClawStrike);
        }

        inline void animEvent(AiCtx &c, int event)
        {
            if (event >= kHornet1 && event <= kHornet5)
                shootHornet(c);
            else if (event == kLeftPunch)
                punch(c, -1);
            else if (event == kRightPunch)
                punch(c, 1);
        }

        inline const AiSchedule *getSchedule(AiCtx &c)
        {
            if (aiHas(c, kCondHearSound))
            {
                const AiSound *heard = aiBestSound(c);
                if (heard != nullptr && (heard->type & kSoundDanger))
                    return aiScheduleOfType(c, kSchedTakeCoverFromBestSound);
            }
            if (c.m.state == kStateCombat)
            {
                if (aiHas(c, kCondEnemyDead))
                    return aiBaseGetSchedule(c);
                if (aiHas(c, kCondNewEnemy))
                    return aiScheduleOfType(c, kSchedWakeAngry);
                if (aiHas(c, kCondCanMeleeAttack1))
                {
                    stopTalking(c);
                    detail::aiSay(c, CommonSound::AgruntAttack);
                    return aiScheduleOfType(c, kSchedMeleeAttack1);
                }
                if (aiHas(c, kCondHeavyDamage))
                    return aiScheduleOfType(c, kSchedSmallFlinch);
                if (aiHas(c, kCondCanRangeAttack1) && aiOccupySlot(c, kSlotsAgruntHornet))
                    return aiScheduleOfType(c, kSchedRangeAttack1);
                if (aiOccupySlot(c, kSlotAgruntChase))
                    return aiScheduleOfType(c, kSchedChaseEnemy);
                return aiScheduleOfType(c, kSchedStandoff);
            }
            return aiBaseGetSchedule(c);
        }

        inline const AiSchedule *scheduleOfType(AiCtx &c, int type)
        {
            switch (type)
            {
            case kSchedTakeCoverFromEnemy: return &slAGruntTakeCoverFromEnemy;
            case kSchedRangeAttack1:       return &slAGruntRangeAttack1;
            case kSchedThreatDisplay:      return &slAGruntThreatDisplay;
            case kSchedStandoff:           return &slAGruntStandoff;
            case kSchedVictoryDance:       return &slAGruntVictoryDance;
            case kSchedFail:               return c.m.enemy != kAiNone ? &slAGruntCombatFail : &slAGruntFail;
            default:                       break;
            }
            return aiSquadScheduleOfType(c, type);
        }

        inline void started(AiCtx &c)
        {
            stopTalking(c);
        }

        inline const AiKind &kind()
        {
            static const AiKind k = []
            {
                AiKind k;
                k.kind = static_cast<std::uint8_t>(ActorKind::AlienGrunt);
                k.health = 90;
                k.eye = fxFromInt(56);
                k.fov = fxDot(0.2);
                k.capability = kCapSquad;
                k.soundMask = kSoundWorld | kSoundCombat | kSoundPlayer | kSoundDanger;
                k.blood = kBloodYellow;
                k.squadMonster = true;
                k.relationship = relationship;
                k.traceAttack = traceAttack;
                k.prescheduleThink = prescheduleThink;
                k.sound = sound;
                k.yawSpeed = yawSpeed;
                k.canCheckAttacks = canCheckAttacks;
                k.checkMelee1 = checkMelee1;
                k.checkRange1 = checkRange1;
                k.animEvent = animEvent;
                k.getSchedule = getSchedule;
                k.scheduleOfType = scheduleOfType;
                k.started = started;
                return k;
            }();
            return k;
        }
    } // namespace agrunt

    // ================================================================ flying

    namespace fly
    {
        /// Where a flier's middle is, for its traces: the standing hull's,
        /// above its feet (the game's controller is a box 64 across; the
        /// console has the person's box to try a way with).
        inline void middle(const fx *feet, fx *at)
        {
            at[0] = feet[0];
            at[1] = feet[1];
            at[2] = feet[2] + kStandingFeet;
        }

        /// A straight flight from `from` to `to` (feet), nothing in the way.
        inline bool clear(AiCtx &c, const fx *from, const fx *to)
        {
            fx a[3], b[3];
            middle(from, a);
            middle(to, b);
            const Trace t = detail::walkerTrace(*c.w.world, &c.a, c.index, false, a, b);
            return !t.startSolid && t.fraction >= kFractionOne;
        }

        /// The node a flier is at, or nearest it can see (the ones in the
        /// air too).
        inline int nearest(AiCtx &c, const fx *at)
        {
            // (Not pathNearest: that is a walker's -- near in the plane, and
            // somewhere it can walk to. A flier's is the nearest it can see,
            // up or down, in the air or on the ground; of the eight nearest.)
            constexpr int kTries = 8;
            int best[kTries];
            std::int64_t bestD[kTries];
            int count = 0;
            for (int n = 0; n < c.a.nodeCount; ++n)
            {
                const fx d[3] = {c.a.nodes[n].pos[0] - at[0], c.a.nodes[n].pos[1] - at[1], c.a.nodes[n].pos[2] - at[2]};
                const std::int64_t far = static_cast<std::int64_t>(fxLength(d)) * fxLength(d);
                if (far > static_cast<std::int64_t>(fxFromInt(2048)) * fxFromInt(2048))
                    continue;
                int k = count < kTries ? count++ : kTries;
                if (k == kTries)
                {
                    if (far >= bestD[kTries - 1])
                        continue;
                    k = kTries - 1;
                }
                for (; k > 0 && bestD[k - 1] > far; --k)
                {
                    best[k] = best[k - 1];
                    bestD[k] = bestD[k - 1];
                }
                best[k] = n;
                bestD[k] = far;
            }
            fx from[3];
            middle(at, from);
            for (int k = 0; k < count; ++k)
            {
                fx to[3];
                middle(c.a.nodes[best[k]].pos, to);
                if (lineClear(*c.w.world, from, to))
                    return best[k];
            }
            return count > 0 ? best[0] : -1;
        }

        /// BuildNearestRoute, for what flies: the first node, in the order
        /// cover is looked for, that `wantSeen` (or not) has a line to the
        /// threat's eyes, `minDist` to `maxDist` off it, that it can fly to;
        /// the way there begun. False if there is none.
        inline bool route(AiCtx &c, const fx *threat, fx viewOfs, int minDist, int maxDist, bool wantSeen)
        {
            const ActorWorld &a = c.a;
            if (a.nodeCount == 0)
                return false;
            const int mine = nearest(c, c.me.pos);
            if (mine < 0)
                return false;
            std::uint8_t reached[kMaxPathNodes];
            pathReach(a, mine, kPathLinkFly, false, reached);
            reached[mine] = 1;
            const fx looker[3] = {threat[0], threat[1], threat[2] + viewOfs};
            int pause = 0;
            for (int i = 0; i < a.nodeCount; ++i)
            {
                thinkPoint(pause, 8);
                const int n = (i + c.ai.lastCoverSearch) % a.nodeCount;
                c.ai.lastCoverSearch = n + 1;
                if (!reached[n])
                    continue;
                const fx *p = a.nodes[n].pos;
                const fx dist = aiDist(threat, p);
                if (dist <= fxFromInt(minDist) || dist >= fxFromInt(maxDist))
                    continue;
                fx from[3];
                middle(p, from);
                if (lineClear(*c.w.world, from, looker) != wantSeen)
                    continue;
                aiRouteNew(c, kMoveGoalLocation, p);
                c.m.movementActivity = kActWalk;
                return true;
            }
            return false;
        }
    } // namespace fly

    /// A frame of flight (the controller's Move and MoveExecute): towards
    /// its goal straight where it can, else by the ways for what flies, a
    /// node at a time; its speed eased, as the game's m_velocity is.
    inline void aiFlyTick(AiCtx &c, int ms)
    {
        if (c.me.ai >= kAiDying || c.m.inAir)
            return;
        fx want[3] = {0, 0, 0};
        const fx speed = fxFromInt(100);
        if (c.m.movementGoal != kMoveGoalNone && c.m.moveWaitFinished <= c.ai.now)
        {
            fx dest[3];
            aiMoveDest(c, dest);
            fx to[3] = {dest[0], dest[1], dest[2]};
            if (aiDist(dest, c.me.pos) < fxFromInt(24))
            {
                aiMovementComplete(c);
            }
            else
            {
                // (A way round, looked for twice a second while it has none
                // straight.)
                if (!fly::clear(c, c.me.pos, dest))
                {
                    c.me.thinkMs -= ms;
                    if (c.me.thinkMs <= 0 || c.me.goalNode < 0)
                    {
                        c.me.thinkMs = 500;
                        const int mine = fly::nearest(c, c.me.pos);
                        const int theirs = fly::nearest(c, dest);
                        const std::uint8_t next = mine >= 0 && theirs >= 0 && mine != theirs
                                                      ? pathStep(c.a, mine, theirs, kPathLinkFly, false)
                                                      : kNoPathNode;
                        c.me.goalNode = static_cast<std::int16_t>(next == kNoPathNode ? (mine >= 0 && aiDist(c.a.nodes[mine].pos, c.me.pos) > fxFromInt(32) ? mine : -1) : next);
                    }
                    if (c.me.goalNode >= 0)
                    {
                        for (int k = 0; k < 3; ++k)
                            to[k] = c.a.nodes[c.me.goalNode].pos[k];
                        if (aiDist(to, c.me.pos) < fxFromInt(24))
                            c.me.thinkMs = 0;
                    }
                }
                const fx d[3] = {to[0] - c.me.pos[0], to[1] - c.me.pos[1], to[2] - c.me.pos[2]};
                const fx far = fxLength(d);
                for (int k = 0; far > 0 && k < 3; ++k)
                    want[k] = static_cast<fx>((static_cast<std::int64_t>(d[k]) * speed) / far);
            }
        }
        // m_velocity = m_velocity * 0.8 + vecDir * speed * 0.2, a tenth of a
        // second at a time.
        const int keep = ms >= 100 ? 205 : 1024 - (ms * 819) / 100;
        for (int k = 0; k < 3; ++k)
            c.me.velocity[k] = static_cast<fx>((static_cast<std::int64_t>(c.me.velocity[k]) * keep +
                                                static_cast<std::int64_t>(want[k]) * (1024 - keep)) >> 10);
        fx step[3], from[3], to[3];
        for (int k = 0; k < 3; ++k)
            step[k] = static_cast<fx>((static_cast<std::int64_t>(c.me.velocity[k]) * ms) / 1000);
        if ((step[0] | step[1] | step[2]) == 0)
            return;
        fly::middle(c.me.pos, from);
        for (int k = 0; k < 3; ++k)
            to[k] = from[k] + step[k];
        const Trace t = detail::walkerTrace(*c.w.world, &c.a, c.index, false, from, to);
        if (t.startSolid)
            return;
        c.me.pos[0] = t.endPos[0];
        c.me.pos[1] = t.endPos[1];
        c.me.pos[2] = t.endPos[2] - kStandingFeet;
        actorPlaceBox(c.a, c.index);
        if (t.fraction < kFractionOne && (c.m.moveBlockedMs += ms) > 2000 && c.m.movementGoal != kMoveGoalNone)
        {
            c.m.moveBlockedMs = 0;
            aiRemember(c, kMemoryMoveFailed);
            aiTaskFail(c);
            aiRouteClear(c);
        }
        else if (t.fraction >= kFractionOne)
        {
            c.m.moveBlockedMs = 0;
        }
    }

    // ============================================================ controller

    namespace controller
    {
        inline constexpr int kHeadOpen = 1, kBallShoot = 2, kSmallShoot = 3, kPowerupFull = 4, kPowerupHalf = 5;
        inline constexpr int kDmgZap = 25;   ///< sk_controller_dmgzap: its head ball's jolt
        inline constexpr int kDmgBall = 4;   ///< sk_controller_dmgball: each small one
        inline constexpr int kSpeedBall = 800; ///< sk_controller_speedball
        // m_flShootTime, m_flShootEnd: time1, time2; m_fInCombat: flag1;
        // m_vecEstVelocity: vec1.

        inline constexpr AiTask tlControllerChaseEnemy[] = {{kTaskGetPathToEnemy, 128}, {kTaskWaitForMovement, 0}};
        inline constexpr AiSchedule slControllerChaseEnemy = {tlControllerChaseEnemy, 2,
                                                              kCondNewEnemy | kCondTaskFailed, 0,
                                                              "ControllerChaseEnemy"};
        inline constexpr AiTask tlControllerStrafe[] = {
            {kTaskWait, 200}, {kTaskGetPathToEnemy, 128}, {kTaskWaitForMovement, 0}, {kTaskWait, 1000}};
        inline constexpr AiSchedule slControllerStrafe = {tlControllerStrafe, 4, kCondNewEnemy, 0, "ControllerStrafe"};
        inline constexpr AiTask tlControllerTakeCover[] = {
            {kTaskWait, 200}, {kTaskFindCoverFromEnemy, 0}, {kTaskWaitForMovement, 0}, {kTaskWait, 1000}};
        inline constexpr AiSchedule slControllerTakeCover = {tlControllerTakeCover, 4, kCondNewEnemy, 0,
                                                             "ControllerTakeCover"};
        inline constexpr AiTask tlControllerFail[] = {
            {kTaskStopMoving, 0}, {kTaskSetActivity, kActIdle}, {kTaskWait, 2000}, {kTaskWaitPvs, 0}};
        inline constexpr AiSchedule slControllerFail = {tlControllerFail, 4, 0, 0, "ControllerFail"};

        inline void sound(AiCtx &c, int which)
        {
            switch (which)
            {
            case kAiSoundIdle:  detail::aiSay(c, CommonSound::ControllerIdle); break;
            case kAiSoundAlert: detail::aiSay(c, CommonSound::ControllerAlert); break;
            case kAiSoundPain:
                if (aiRandomLong(c.a, 0, 5) < 2)
                    detail::aiSay(c, aiRandomLong(c.a, 0, 1) == 0 ? CommonSound::ControllerPain1 : CommonSound::ControllerPain2);
                break;
            case kAiSoundDeath: detail::aiSay(c, CommonSound::ControllerDie); break;
            default:            break;
            }
        }

        inline int yawSpeed(AiCtx &) { return 120; }

        /// Its head, where the big ball comes from; its hands, the small ones.
        inline void headPos(const AiCtx &c, fx *at)
        {
            at[0] = c.me.pos[0];
            at[1] = c.me.pos[1];
            at[2] = c.me.pos[2] + fxFromInt(64);
        }
        inline void handPos(const AiCtx &c, fx *at)
        {
            fx forward[3];
            aiForward(c.me.yaw, forward);
            at[0] = c.me.pos[0] + forward[0] * 32;
            at[1] = c.me.pos[1] + forward[1] * 32;
            at[2] = c.me.pos[2] + fxFromInt(40);
        }

        inline bool checkRange1(AiCtx &, fx dot, fx dist)
        {
            return dot > fxDot(0.5) && dist > fxFromInt(256) && dist <= fxFromInt(2048);
        }
        inline bool checkRange2(AiCtx &, fx dot, fx dist)
        {
            return dot > fxDot(0.5) && dist > fxFromInt(64) && dist <= fxFromInt(2048);
        }
        inline bool checkMelee1(AiCtx &, fx, fx) { return false; }

        inline void animEvent(AiCtx &c, int event)
        {
            switch (event)
            {
            case kHeadOpen:
            {
                fx at[3];
                headPos(c, at);
                effectAdd(c.a, kEffectMuzzle, at, nullptr, 8, 400); // (its head lit)
                break;
            }
            case kBallShoot:
            {
                // The big one, from its head, slowly up and after its enemy.
                fx at[3];
                headPos(c, at);
                const fx velocity[3] = {0, 0, fxFromInt(32)};
                projectileAdd(c.a, kShotHeadBall, at, velocity, 5000);
                break;
            }
            case kSmallShoot:
                // A stream of small ones, five a second, for the next while
                // (the event's own length: a second, as the model has it).
                detail::aiSay(c, CommonSound::ControllerAttack);
                c.m.time1 = c.ai.now;
                c.m.time2 = c.ai.now + 1000;
                break;
            default:
                break;
            }
        }

        /// The small balls owed since it last looked: towards where its enemy
        /// will be, as near as the game's Intersect has it -- here, where it
        /// is, with a little spread.
        inline void shoot(AiCtx &c)
        {
            if (c.m.time2 <= c.ai.now && c.m.time1 >= c.m.time2)
                return;
            AiTarget t;
            const bool target = c.m.enemy != kAiNone && aiTargetOf(c.w, c.player, c.m.enemy, t);
            while (c.m.time1 < c.m.time2 && c.m.time1 < c.ai.now)
            {
                if (target)
                {
                    fx hand[3];
                    handPos(c, hand);
                    fx d[3] = {t.center[0] - hand[0], t.center[1] - hand[1], t.center[2] - hand[2]};
                    const fx far = fxLength(d);
                    fx velocity[3];
                    for (int k = 0; k < 3; ++k)
                    {
                        const fx unit = far > 0 ? static_cast<fx>((static_cast<std::int64_t>(d[k]) << kFxShift) / far) : 0;
                        velocity[k] = unit * kSpeedBall + fxFromInt(aiRandomLong(c.a, -28, 28));
                    }
                    projectileAdd(c.a, kShotZapBall, hand, velocity, 5000);
                }
                c.m.time1 += 200;
            }
            if (c.m.time1 >= c.m.time2)
                c.m.flag1 = false;
        }

        /// LookupFloat: its float clip by the way it is going.
        inline void floatClip(AiCtx &c)
        {
            const ModelView *model = actorModel(c.a, c.me);
            if (model == nullptr)
                return;
            const fx *v = c.me.velocity;
            const char *name = "up";
            if (fxLength(v) >= fxFromInt(32))
            {
                fx forward[3], right[3];
                aiForward(c.me.yaw, forward);
                aiRight(c.me.yaw, right);
                const std::int64_t x = static_cast<std::int64_t>(forward[0]) * v[0] + static_cast<std::int64_t>(forward[1]) * v[1];
                const std::int64_t y = static_cast<std::int64_t>(right[0]) * v[0] + static_cast<std::int64_t>(right[1]) * v[1];
                const std::int64_t z = static_cast<std::int64_t>(v[2]) * kFxOne;
                const std::int64_t ax = x < 0 ? -x : x, ay = y < 0 ? -y : y, az = z < 0 ? -z : z;
                if (ax > ay && ax > az)
                    name = x > 0 ? "forward" : "backward";
                else if (ay > az)
                    name = y > 0 ? "right" : "left";
                else
                    name = z > 0 ? "up" : "down";
            }
            int clip = model->findClip(nameHash(name));
            if (clip < 0)
                clip = model->findClip(nameHash("float"));
            if (clip >= 0 && (c.m.seqFinished || c.me.clip != clip))
                aiPlayClip(c, clip, c.me.clip != clip);
        }

        inline bool startTask(AiCtx &c, const AiTask &task)
        {
            switch (task.task)
            {
            case kTaskGetPathToEnemy:
            case kTaskGetPathToEnemyLkp:
            {
                AiTarget t;
                const bool lkp = task.task == kTaskGetPathToEnemyLkp;
                if (!lkp && (c.m.enemy == kAiNone || !aiTargetOf(c.w, c.player, c.m.enemy, t)))
                {
                    aiTaskFail(c);
                    return true;
                }
                fx at[3];
                if (lkp)
                    for (int k = 0; k < 3; ++k)
                        at[k] = c.m.enemyLkp[k];
                else
                    aiFeetOf(t, at);
                const fx viewOfs = lkp ? 0 : t.eye[2] - t.origin[2];
                if (fly::route(c, at, viewOfs, task.data, fxToInt(aiDist(at, c.me.pos)) + 1024, true))
                    aiTaskComplete(c);
                else
                    aiTaskFail(c);
                return true;
            }
            case kTaskFindCoverFromEnemy:
            {
                AiTarget t;
                if (c.m.enemy == kAiNone || !aiTargetOf(c.w, c.player, c.m.enemy, t))
                {
                    aiTaskFail(c);
                    return true;
                }
                fx at[3];
                aiFeetOf(t, at);
                if (fly::route(c, at, t.eye[2] - t.origin[2], 0, c.kind.coverRadius, false))
                    aiTaskComplete(c);
                else
                    aiTaskFail(c);
                return true;
            }
            case kTaskStopMoving:
                aiRouteClear(c);
                aiTaskComplete(c);
                return true;
            default:
                return false;
            }
        }

        /// RunTask: its small balls go out whatever the task; and while it
        /// waits, it faces its enemy and throws what it can.
        inline bool runTask(AiCtx &c, const AiTask &task)
        {
            shoot(c);
            switch (task.task)
            {
            case kTaskWaitForMovement:
            case kTaskWait:
            case kTaskWaitFaceEnemy:
            case kTaskWaitPvs:
            {
                aiMakeIdealYaw(c, c.m.enemyLkp);
                aiChangeYaw(c);
                if (c.m.seqFinished)
                    c.m.flag1 = false;
                if (!c.m.flag1)
                {
                    const std::uint8_t attack = aiHas(c, kCondCanRangeAttack1)   ? kActRangeAttack1
                                                : aiHas(c, kCondCanRangeAttack2) ? kActRangeAttack2
                                                                                 : kActReset;
                    const int clip = attack != kActReset ? aiLookupActivity(c, attack) : -1;
                    if (clip >= 0)
                    {
                        aiPlayClip(c, clip, true);
                        c.m.flag1 = true;
                    }
                    else
                    {
                        floatClip(c);
                    }
                }
                return false; // (and the wait goes on as any monster's)
            }
            default:
                return false;
            }
        }

        inline const AiSchedule *scheduleOfType(AiCtx &c, int type)
        {
            switch (type)
            {
            case kSchedChaseEnemy:         return &slControllerChaseEnemy;
            case kSchedRangeAttack1:       return &slControllerStrafe;
            case kSchedRangeAttack2:
            case kSchedMeleeAttack1:
            case kSchedMeleeAttack2:
            case kSchedTakeCoverFromEnemy: return &slControllerTakeCover;
            case kSchedFail:               return &slControllerFail;
            default:                       break;
            }
            return aiBaseScheduleOfType(c, type);
        }

        /// Killed: down it falls.
        inline bool killed(AiCtx &c)
        {
            c.m.inAir = true;
            c.me.velocity[2] = 0;
            return false;
        }

        inline const AiKind &kind()
        {
            static const AiKind k = []
            {
                AiKind k;
                k.kind = static_cast<std::uint8_t>(ActorKind::Controller);
                k.health = 60;
                k.eye = fxFromInt(40); // (the game's -2 is under its feet: its eyes are its head's)
                k.fov = fxDot(-1.0);   // VIEW_FIELD_FULL
                k.capability = kCapFly | kCapSquad;
                k.blood = kBloodYellow;
                k.squadMonster = true;
                k.sound = sound;
                k.yawSpeed = yawSpeed;
                k.checkRange1 = checkRange1;
                k.checkRange2 = checkRange2;
                k.checkMelee1 = checkMelee1;
                k.animEvent = animEvent;
                k.startTask = startTask;
                k.runTask = runTask;
                k.scheduleOfType = scheduleOfType;
                k.killed = killed;
                return k;
            }();
            return k;
        }
    } // namespace controller

    // ================================================================= leech

    namespace leech
    {
        inline constexpr int kAttack = 1;       ///< LEECH_AE_ATTACK
        inline constexpr int kCheckDist = 45;   ///< LEECH_CHECK_DIST
        inline constexpr int kSwimSpeed = 50;   ///< LEECH_SWIM_SPEED
        inline constexpr int kSwimAccel = 80;   ///< LEECH_SWIM_ACCEL
        inline constexpr int kSwimDecel = 10;   ///< LEECH_SWIM_DECEL
        inline constexpr int kTurnRate = 90;    ///< LEECH_TURN_RATE
        inline constexpr int kSizeX = 10;       ///< LEECH_SIZEX
        inline constexpr int kDmgBite = 2;      ///< sk_leech_dmg_bite
        // m_top, m_bottom, m_height: vec1; pev->speed: value1 (units/s);
        // m_flTurning: count1 (degrees/s); m_obstacle: value2 (Q12, 1.0 = clear);
        // m_stateTime, m_waterTime, m_zTime: time1..3; m_sideTime:
        // moveWaitFinished; m_attackSoundTime: moveBlockedMs (ms of its
        // clock); pev->oldorigin: lastPosition; m_fPathBlocked: flag1;
        // pitch: aimPitch (degrees Q12).

        inline bool inWater(const AiCtx &c, const fx *at)
        {
            return pointContents(*c.w.world, at) == kContentsWater;
        }

        /// RecalculateWaterlevel: its water's floor and top, a little in from
        /// each, and a height to swim at between them.
        inline void waterLevel(AiCtx &c)
        {
            const fx down[3] = {c.me.pos[0], c.me.pos[1], c.me.pos[2] - fxFromInt(400)};
            const Shot floor = traceShot(*c.w.world, c.me.pos, down);
            fx bottom = c.me.pos[2] - fxFromInt(400);
            if (floor.fraction < kFractionOne)
                bottom = c.me.pos[2] + static_cast<fx>((static_cast<std::int64_t>(down[2] - c.me.pos[2]) * floor.fraction) >> kFractionShift) + kFxOne;
            // UTIL_WaterLevel: the top, in halvings.
            fx lo = c.me.pos[2], hi = c.me.pos[2] + fxFromInt(400);
            for (int n = 0; n < 9; ++n)
            {
                const fx mid = (lo + hi) / 2;
                const fx at[3] = {c.me.pos[0], c.me.pos[1], mid};
                if (inWater(c, at))
                    lo = mid;
                else
                    hi = mid;
            }
            const fx top = lo - kFxOne;
            c.m.vec1[1] = static_cast<fx>((static_cast<std::int64_t>(bottom) * 4 + top) / 5);
            c.m.vec1[0] = static_cast<fx>((static_cast<std::int64_t>(bottom) + static_cast<std::int64_t>(top) * 4) / 5);
            c.m.vec1[2] = c.m.vec1[1] + static_cast<fx>((static_cast<std::int64_t>(c.m.vec1[0] - c.m.vec1[1]) * aiRandomLong(c.a, 0, 1000)) / 1000);
            c.m.time2 = c.ai.now + aiRandomMs(c.a, 5000, 7000);
        }

        /// SwitchLeechState: from fighting back to swimming about; or, the
        /// player seen in the water, to going for them.
        inline void switchState(AiCtx &c)
        {
            c.m.time1 = c.ai.now + aiRandomMs(c.a, 3000, 6000);
            if (c.m.state == kStateCombat)
            {
                c.m.enemy = kAiNone;
                c.m.state = c.m.idealState = kStateIdle;
                c.m.moveWaitFinished = 0;
                return;
            }
            AiTarget t;
            if (c.player.health > 0 && !unseen(c.player) && c.player.waterLevel != 0 &&
                aiDist(c.player.origin, c.me.pos) < fxFromInt(750) && aiTargetOf(c.w, c.player, kAiPlayer, t) &&
                lineClear(*c.w.world, c.me.pos, t.eye))
            {
                c.m.enemy = kAiPlayer;
                c.m.state = c.m.idealState = kStateCombat;
                c.m.time1 = c.ai.now + aiRandomMs(c.a, 18000, 25000);
                detail::aiSay(c, CommonSound::LeechAlert);
            }
        }

        /// The way it faces, as a unit vector: its yaw, and its pitch.
        inline void facing(const AiCtx &c, fx *forward)
        {
            fx sine, cosine, ps, pc;
            fxSinCos(c.me.yaw, sine, cosine);
            fxSinCos(static_cast<std::uint16_t>((static_cast<std::int64_t>(c.me.aimPitch) * 65536) / (360 * kFxOne)), ps, pc);
            forward[0] = static_cast<fx>((static_cast<std::int64_t>(cosine) * pc) >> kFxShift);
            forward[1] = static_cast<fx>((static_cast<std::int64_t>(sine) * pc) >> kFxShift);
            forward[2] = ps;
        }

        /// How much of a feeler's length is clear (Q12), three of them.
        inline fx feeler(AiCtx &c, const fx *offset, const fx *forward)
        {
            const fx to[3] = {c.me.pos[0] + offset[0] + forward[0] * kCheckDist,
                              c.me.pos[1] + offset[1] + forward[1] * kCheckDist,
                              c.me.pos[2] + offset[2] + forward[2] * kCheckDist};
            const Shot shot = traceShot(*c.w.world, c.me.pos, to);
            return static_cast<fx>(shot.fraction >> (kFractionShift - kFxShift));
        }

        /// ObstacleDistance
        inline fx obstacle(AiCtx &c, const fx *forward, bool atTarget)
        {
            const fx zero[3] = {0, 0, 0};
            const fx ahead = feeler(c, zero, forward);
            if (ahead < kFxOne)
            {
                if (!atTarget || c.m.vec1[2] - c.me.pos[2] > fxFromInt(10) || c.m.vec1[2] - c.me.pos[2] < -fxFromInt(10))
                    return ahead;
            }
            if (c.m.moveWaitFinished < c.ai.now)
            {
                fx right[3];
                aiRight(c.me.yaw, right);
                for (int side = 1; side >= -1; side -= 2)
                {
                    const fx off[3] = {right[0] * kSizeX * 2 * side, right[1] * kSizeX * 2 * side, 0};
                    const fx f = feeler(c, off, forward);
                    if (f < kFxOne)
                        return f;
                }
                c.m.moveWaitFinished = c.ai.now + aiRandomMs(c.a, 500, 1000);
            }
            return kFxOne;
        }

        inline fx approach(fx target, fx value, fx speed)
        {
            const fx delta = target - value;
            return delta > speed ? value + speed : (delta < -speed ? value - speed : target);
        }

        /// SwimThink: ten times a second while the player may see it.
        inline void think(AiCtx &c)
        {
            if (c.me.ai >= kAiDying)
                return;
            if (!aiClientInPvs(c))
            {
                c.m.thinkMs += aiRandomMs(c.a, 900, 1400);
                c.me.velocity[0] = c.me.velocity[1] = c.me.velocity[2] = 0;
                return;
            }
            if (c.m.vec1[0] == 0 && c.m.vec1[1] == 0)
                waterLevel(c);
            fx targetSpeed = fxFromInt(kSwimSpeed);
            int targetYaw = 0;
            if (c.m.time2 < c.ai.now)
                waterLevel(c);
            if (c.m.time1 < c.ai.now)
                switchState(c);
            aiClear(c, kCondCanMeleeAttack1);
            bool atTarget = false;
            if (c.m.state == kStateCombat)
            {
                AiTarget t;
                if (c.m.enemy == kAiNone || !aiTargetOf(c.w, c.player, c.m.enemy, t))
                {
                    switchState(c);
                }
                else
                {
                    atTarget = true;
                    c.m.vec1[2] = std::clamp(t.eye[2] - fxFromInt(5), c.m.vec1[1], c.m.vec1[0]);
                    const fx at[3] = {t.eye[0] - c.me.pos[0], t.eye[1] - c.me.pos[1], t.eye[2] - c.me.pos[2]};
                    if (fxLength(at) < fxFromInt(40))
                        aiSet(c, kCondCanMeleeAttack1);
                    const std::uint16_t to = fxAtan2Turns(at[1], at[0]);
                    targetYaw = static_cast<std::int16_t>(to - c.me.yaw) * 360 / 65536;
                    const int most = kTurnRate * 3 / 4;
                    if (targetYaw < -most)
                        targetYaw = -most;
                    else if (targetYaw > most)
                        targetYaw = most;
                    else
                        targetSpeed *= 2;
                }
            }
            else
            {
                if (c.m.time3 < c.ai.now)
                {
                    const fx newHeight = c.m.vec1[1] + static_cast<fx>((static_cast<std::int64_t>(c.m.vec1[0] - c.m.vec1[1]) * aiRandomLong(c.a, 0, 1000)) / 1000);
                    c.m.vec1[2] = c.m.vec1[2] / 2 + newHeight / 2;
                    c.m.time3 = c.ai.now + aiRandomMs(c.a, 1000, 4000);
                }
                if (aiRandomLong(c.a, 0, 100) < 10)
                    targetYaw = aiRandomLong(c.a, -30, 30);
                if (aiDist(c.me.pos, c.m.lastPosition) < kFxOne)
                    c.m.moveWaitFinished = 0;
            }
            fx forward[3];
            facing(c, forward);
            fx obstacleOf = obstacle(c, forward, atTarget);
            for (int k = 0; k < 3; ++k)
                c.m.lastPosition[k] = c.me.pos[k];
            if (obstacleOf < kFxOne / 10)
                obstacleOf = kFxOne / 10;
            // (Its speed and turn as a tenth of a second has them.)
            if (obstacleOf >= kFxOne)
            {
                c.m.count1 = 0;
                c.m.flag1 = false;
                c.m.value1 = approach(targetSpeed, c.m.value1, fxFromInt(kSwimAccel) / 10);
            }
            else
            {
                const fx inverse = static_cast<fx>((static_cast<std::int64_t>(kFxOne) << kFxShift) / obstacleOf);
                c.m.value2 = inverse;
                c.m.flag1 = true;
                if (c.m.count1 == 0)
                {
                    fx right[3];
                    aiRight(c.me.yaw, right);
                    const fx r[3] = {right[0] * kSizeX, right[1] * kSizeX, 0};
                    const fx l[3] = {-r[0], -r[1], 0};
                    const fx delta = feeler(c, r, forward) - feeler(c, l, forward);
                    c.m.count1 = delta > kFxOne / 10 || (delta > -kFxOne / 10 && aiRandomLong(c.a, 0, 100) < 50)
                                     ? -kTurnRate : kTurnRate;
                }
                c.m.value1 = approach(-fxFromInt(kSwimSpeed) / 2, c.m.value1,
                                      static_cast<fx>((static_cast<std::int64_t>(fxFromInt(kSwimDecel)) * inverse) >> kFxShift) / 10);
            }
            // UpdateMotion: its turn, its pitch towards its height, and the
            // bite when it is on what it is after.
            const int idealYaw = c.m.count1 + targetYaw; // degrees a second
            c.m.idealYaw = static_cast<std::uint16_t>(idealYaw * 65536 / 360);
            const fx delta = c.m.vec1[2] - c.me.pos[2];
            const fx targetPitch = delta < -fxFromInt(10) ? -fxFromInt(30) : (delta > fxFromInt(10) ? fxFromInt(30) : 0);
            c.me.aimPitch = approach(targetPitch, c.me.aimPitch, fxFromInt(6));
            std::uint8_t want = kActSwim;
            if (idealYaw * (c.m.flag1 ? fxToInt(c.m.value2) : 1) > 150)
                want = kActTurnLeft;
            else if (idealYaw * (c.m.flag1 ? fxToInt(c.m.value2) : 1) < -150)
                want = kActTurnRight;
            if (c.m.state == kStateCombat && aiHas(c, kCondCanMeleeAttack1))
                want = kActMeleeAttack1;
            if (c.m.activity != want)
                aiSetActivity(c, want);
        }

        /// Each frame: it goes as its speed, turn and pitch have it, through
        /// the water (MOVETYPE_FLY) -- or, out of it, falls.
        inline void frame(AiCtx &c, int ms)
        {
            if (c.me.ai >= kAiDying)
            {
                // DeadThink: it floats up, or falls, slowly.
                if (!c.m.inAir)
                {
                    const fx up[3] = {c.me.pos[0], c.me.pos[1], c.me.pos[2] + static_cast<fx>((static_cast<std::int64_t>(fxFromInt(8)) * ms) / 1000)};
                    if (inWater(c, up) && traceShot(*c.w.world, c.me.pos, up).fraction >= kFractionOne)
                        c.me.pos[2] = up[2];
                }
                return;
            }
            if (!inWater(c, c.me.pos))
            {
                // (Out of the water: down, twitching.)
                if (!c.m.inAir)
                {
                    c.m.inAir = true;
                    c.me.velocity[0] = c.me.velocity[1] = c.me.velocity[2] = 0;
                }
                return;
            }
            c.m.inAir = false;
            const int turn = c.m.count1 + 0; // (degrees a second, as idealYaw has it)
            (void)turn;
            const int yawRate = static_cast<std::int16_t>(c.m.idealYaw) * 360 / 65536;
            const int scaled = c.m.flag1 ? static_cast<int>((static_cast<std::int64_t>(yawRate) * c.m.value2) >> kFxShift) : yawRate;
            c.me.yaw = static_cast<std::uint16_t>(c.me.yaw + scaled * 65536 / 360 * ms / 1000);
            fx forward[3];
            facing(c, forward);
            for (int k = 0; k < 3; ++k)
                c.me.velocity[k] = static_cast<fx>((static_cast<std::int64_t>(forward[k]) * c.m.value1) >> kFxShift);
            fx to[3];
            for (int k = 0; k < 3; ++k)
                to[k] = c.me.pos[k] + static_cast<fx>((static_cast<std::int64_t>(c.me.velocity[k]) * ms) / 1000);
            const Shot shot = traceShot(*c.w.world, c.me.pos, to);
            if (shot.fraction >= kFractionOne && inWater(c, to))
            {
                for (int k = 0; k < 3; ++k)
                    c.me.pos[k] = to[k];
                actorPlaceBox(c.a, c.index);
            }
        }

        inline void animEvent(AiCtx &c, int event)
        {
            if (event != kAttack)
                return;
            if (c.ai.now > c.m.moveBlockedMs)
            {
                detail::aiSay(c, CommonSound::LeechBite);
                c.m.moveBlockedMs = c.ai.now + 500;
            }
            AiTarget t;
            if (c.m.enemy == kAiNone || !aiTargetOf(c.w, c.player, c.m.enemy, t))
                return;
            fx face[3];
            aiForward(c.me.yaw, face);
            fx d[3] = {t.origin[0] - c.me.pos[0], t.origin[1] - c.me.pos[1], 0};
            const fx far = fxLength(d);
            if (far <= 0)
                return;
            const std::int64_t dot = (static_cast<std::int64_t>(d[0]) * face[0] + static_cast<std::int64_t>(d[1]) * face[1]) / far;
            if (dot > fxDot(0.9))
                aiDamageTarget(c, c.m.enemy, kDmgBite, kDmgSlash);
            c.m.time1 -= 2000;
        }

        /// TakeDamage: knocked back from what hurt it.
        inline bool takeDamage(AiCtx &c, AiDamage &d)
        {
            c.m.value1 = 0;
            if (d.hasFrom)
            {
                const fx away[3] = {c.me.pos[0] - d.from[0], c.me.pos[1] - d.from[1], c.me.pos[2] - d.from[2]};
                const fx far = fxLength(away);
                for (int k = 0; far > 0 && k < 3; ++k)
                    c.me.velocity[k] = static_cast<fx>((static_cast<std::int64_t>(away[k]) * 25) / (far >> kFxShift > 0 ? far >> kFxShift : 1));
            }
            return true;
        }

        inline bool killed(AiCtx &c)
        {
            c.me.ai = kAiDying;
            c.m.deadflag = 1;
            c.m.state = c.m.idealState = kStateDead;
            c.me.flags &= static_cast<std::uint8_t>(~kActorSolid);
            c.me.aimPitch = 0;
            aiSetActivity(c, inWater(c, c.me.pos) ? kActDieSimple : kActDieForward);
            c.m.inAir = !inWater(c, c.me.pos);
            actorPlaceBox(c.a, c.index);
            return true;
        }

        inline const AiKind &kind()
        {
            static const AiKind k = []
            {
                AiKind k;
                k.kind = static_cast<std::uint8_t>(ActorKind::Leech);
                k.health = 2;
                k.fov = fxDot(-0.5);
                k.blood = kBloodYellow;
                k.think = think;
                k.frame = frame;
                k.animEvent = animEvent;
                k.takeDamage = takeDamage;
                k.killed = killed;
                return k;
            }();
            return k;
        }
    } // namespace leech

    inline const AiKind *aiAlienKindOf(std::uint8_t kind)
    {
        switch (static_cast<ActorKind>(kind))
        {
        case ActorKind::Cockroach:  return &roach::kind();
        case ActorKind::Snark:      return &snark::kind();
        case ActorKind::AlienGrunt: return &agrunt::kind();
        case ActorKind::Controller: return &controller::kind();
        case ActorKind::Leech:      return &leech::kind();
        default:                    return nullptr;
        }
    }
} // namespace hldata
