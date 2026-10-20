#pragma once

// Soldiers: the game's hgrunt.cpp, by its rules and its numbers.
//
// The game's soldier is run by "schedules" -- lists of tasks, each list
// broken off when one of the conditions it names comes true -- chosen by one
// long function (GetSchedule) every time a list ends or is broken off. This
// is that function, and those lists, over what the port has: path nodes, the
// walker, lines through the world. A soldier is always doing one of five
// things (ActorState::ai), and `attack` says which of its kind:
//
//   kAiIdle     standing where the map put him, looking and listening
//   kAiChase    going somewhere: to cover, to cover to reload, at the
//               player for a line of fire, away from a grenade
//   kAiAttack   acting something out, start to finish: a burst, a reload, a
//               grenade thrown, launched or left at his feet, a kick, a wave
//               of the hand
//   kGruntWait  holding where he is, facing where the player is or was
//   kAiFlinch   hit
//
// and gruntChoose() is asked what next. Squads are the game's: by name,
// where the map names one and flags its leader, or whoever of his own kind a
// soldier can see within 1024 units when the map starts (four at most). A
// squad has two places for those firing at once and two for those with a
// grenade in hand; the rest find cover.
//
// What the game's soldier does that this one does not: fight anything but
// the player, drop his gun when he dies, slide down a rope, be eaten by a
// barnacle.
//
// Included at the end of HlMonsters.hpp. std-only and inline.

namespace hldata
{
    inline constexpr std::uint8_t kGruntWait = kAiLeap;

    // Why he is going where he is going (ai == kAiChase).
    inline constexpr std::uint8_t kGoCover = 1;  ///< out of the player's sight
    inline constexpr std::uint8_t kGoReload = 2; ///< ... to reload there
    inline constexpr std::uint8_t kGoCharge = 3; ///< at the player, until there is a line of fire
    inline constexpr std::uint8_t kGoFlee = 4;   ///< away from a grenade

    // What he is acting out (ai == kAiAttack).
    inline constexpr std::uint8_t kActBurst = 1;
    inline constexpr std::uint8_t kActReload = 2;
    inline constexpr std::uint8_t kActToss = 3;
    inline constexpr std::uint8_t kActLaunch = 4;
    inline constexpr std::uint8_t kActDrop = 5;   ///< a grenade left at his feet before he runs
    inline constexpr std::uint8_t kActKick = 6;
    inline constexpr std::uint8_t kActSignal = 7; ///< the leader's wave, before the first volley
    inline constexpr std::uint8_t kActFound = 8;  ///< "over here"
    inline constexpr std::uint8_t kActCower = 9;
    inline constexpr std::uint8_t kActVictory = 10;

    // ActorState::mood.
    inline constexpr std::uint8_t kMoodStanding = 1; ///< fires standing, not crouched
    inline constexpr std::uint8_t kMoodMet = 2;      ///< a leader who has given his first signal
    inline constexpr std::uint8_t kMoodHurt = 4;     ///< hurt since he last chose what to do
    inline constexpr std::uint8_t kMoodEluded = 8;   ///< (the leader's) the squad has lost sight of the player
    inline constexpr std::uint8_t kMoodSeen = 16;    ///< the player has seen him
    inline constexpr std::uint8_t kMoodScripted = 32; ///< a script has had him
    inline constexpr std::uint8_t kMoodNew = 64;     ///< has only just learnt of the player
    inline constexpr std::uint8_t kMoodReady = 128;  ///< his first tick is behind him

    // The game's numbers, at "normal".
    inline constexpr int kGruntHealth = 50;
    inline constexpr int kGruntLimp = 20;        ///< health at which he limps
    inline constexpr int kGruntClip = 36;
    inline constexpr int kGruntShells = 8;
    inline constexpr int kGruntRange = 2048;
    inline constexpr int kGruntRound = 4;        ///< a round of his MP5
    inline constexpr int kGruntPellet = 5;       ///< ... and each of his shotgun's five pellets
    inline constexpr int kGruntPellets = 5;
    inline constexpr int kGruntConeMp5 = 357;    ///< ten degrees across, in 4096ths either way
    inline constexpr int kGruntConeShotgun = 535; ///< fifteen
    inline constexpr int kGruntKick = 10;
    inline constexpr int kGruntKickRange = 64;
    inline constexpr int kGruntLaunchSpeed = 600;
    inline constexpr int kGruntCoverRadius = 784;
    inline constexpr std::uint8_t kHitStomach = 3;
    inline constexpr std::uint8_t kHitHelmet = 11;

    namespace detail
    {
        inline bool gruntAlive(const ActorState &actor)
        {
            return actor.active && isGrunt(actor.kind) && actor.ai < kAiDying;
        }

        /// What the map has him set off when `condition` comes about (the
        /// game's numbers; 1 stands for all those of noticing the player):
        /// set off, the once.
        inline void gruntTrigger(LogicWorld &w, ActorState &me, int condition)
        {
            int his = me.item >> kGruntTriggerShift;
            his = his >= 7 && his <= 11 ? 1 : his;
            if (me.fires == 0 || his != condition)
                return;
            logicEnqueue(w, me.fires, 0, kUseToggle, -1, 1);
            me.fires = 0;
        }

        inline fx gruntGun(const ActorState &actor)
        {
            return fxFromInt((actor.mood & kMoodStanding) ? 60 : 48);
        }

        /// One of the lines of `group` (kSpeak*), if nobody has spoken for a
        /// moment; and then nobody speaks for a moment more.
        inline void gruntSay(LogicWorld &w, const ActorState &actor, int group)
        {
            ActorWorld &a = *w.actors;
            if (a.speakMs > 0)
                return;
            a.speakMs = 1500 + static_cast<int>(chance(a) % 500);
            if (w.speech == nullptr || w.heard == nullptr)
                return;
            const std::uint16_t *lines = w.speech + group * kSpeechLines;
            int count = 0;
            while (count < kSpeechLines && lines[count] != 0xFFFF)
                ++count;
            if (count != 0)
                w.heard->add(lines[chance(a) % static_cast<std::uint32_t>(count)], actor.pos, 127, kRadiusLarge,
                             kSoundSentence);
        }

        /// How many others of his squad are acting out `one` or `other`.
        inline int squadActing(const ActorWorld &a, int index, std::uint8_t one, std::uint8_t other)
        {
            const ActorState &me = a.states[index];
            int count = 0;
            for (int i = 0; me.squad >= 0 && i < a.count; ++i)
            {
                const ActorState &mate = a.states[i];
                if (i != index && gruntAlive(mate) && mate.squad == me.squad && mate.ai == kAiAttack &&
                    (mate.attack == one || mate.attack == other))
                    ++count;
            }
            return count;
        }

        /// He is out of his squad (dead); and if he led it, it is no squad.
        inline void gruntLeaves(ActorWorld &a, int index)
        {
            const int squad = a.states[index].squad;
            a.states[index].squad = -1;
            if (squad != index)
                return;
            for (int i = 0; i < a.count; ++i)
            {
                if (a.states[i].squad == squad && isGrunt(a.states[i].kind))
                    a.states[i].squad = -1;
            }
        }

        /// The squad he is to lead, got together: those of the name the map
        /// gave him (if it flagged him their leader), or, with no name,
        /// those without one he can see within 1024 units.
        inline void gruntRecruit(LogicWorld &w, int index)
        {
            ActorWorld &a = *w.actors;
            ActorState &me = a.states[index];
            if (me.squad >= 0)
                return;
            auto nameOf = [&](int i) -> std::uint32_t
            {
                return i < a.recordCount ? a.records[i].item >> kActorSquadShift : 0u;
            };
            const std::uint32_t name = nameOf(index);
            if (name != 0 && !(me.flags & kActorLeader))
                return;
            const int most = name != 0 ? 5 : 4;
            const fx eye[3] = {me.pos[0], me.pos[1], me.pos[2] + fxFromInt(50)};
            int members = 1;
            for (int i = 0; i < a.count && members < most; ++i)
            {
                ActorState &other = a.states[i];
                if (i == index || !gruntAlive(other) || other.squad >= 0 || nameOf(i) != name ||
                    (other.flags & kActorPrisoner))
                    continue;
                if (name == 0)
                {
                    const fx d[3] = {other.pos[0] - me.pos[0], other.pos[1] - me.pos[1], other.pos[2] - me.pos[2]};
                    const fx theirs[3] = {other.pos[0], other.pos[1], other.pos[2] + fxFromInt(50)};
                    if (fxLength(d) > fxFromInt(1024) || !lineClear(*w.world, eye, theirs))
                        continue;
                }
                other.squad = static_cast<std::int16_t>(index);
                ++members;
            }
            if (members == 1)
                return;
            // Whoever leads wears the beret (and, in the game, a pale face).
            me.squad = static_cast<std::int16_t>(index);
            me.body = static_cast<std::uint8_t>(me.body - me.body % 4 + 1);
            me.skin = 0;
        }

        /// Whether there is a clear line between him and the player, within
        /// range; `cone`: and the player is in front of him (as wide as the
        /// game has him see).
        inline bool gruntSees(const LogicWorld &w, const PlayerState &player, const ActorState &actor, bool cone)
        {
            if ((player.health <= 0 || unseen(player)))
                return false;
            const fx eye[3] = {actor.pos[0], actor.pos[1], actor.pos[2] + gruntGun(actor)};
            const fx d[3] = {player.origin[0] - eye[0], player.origin[1] - eye[1], player.origin[2] - eye[2]};
            if (fxLength(d) > fxFromInt(kGruntRange))
                return false;
            if (cone)
            {
                fx sine, cosine;
                fxSinCos(actor.yaw, sine, cosine);
                const fx flat[3] = {d[0], d[1], 0};
                const std::int64_t ahead = static_cast<std::int64_t>(d[0]) * cosine +
                                           static_cast<std::int64_t>(d[1]) * sine;
                if (ahead * 5 < static_cast<std::int64_t>(fxLength(flat)) * kFxOne)
                    return false;
            }
            const fx head[3] = {player.origin[0], player.origin[1], player.origin[2] + player.viewHeight};
            return lineClear(*w.world, eye, player.origin) || lineClear(*w.world, eye, head);
        }

        /// How squarely he faces `to` (feet to feet): the cosine, Q12.
        inline fx gruntFacing(const ActorState &actor, const fx *to)
        {
            const fx d[3] = {to[0] - actor.pos[0], to[1] - actor.pos[1], 0};
            const fx flat = fxLength(d);
            if (flat <= 0)
                return kFxOne;
            fx sine, cosine;
            fxSinCos(actor.yaw, sine, cosine);
            return static_cast<fx>((static_cast<std::int64_t>(d[0]) * cosine + static_cast<std::int64_t>(d[1]) * sine) /
                                   flat);
        }

        /// Whether one of his squad stands where his rounds would go: ahead
        /// of him on the way to `to`, within a body and a half of the line.
        inline bool gruntFriendInWay(const ActorWorld &a, int index, const fx *to)
        {
            const ActorState &me = a.states[index];
            if (me.squad < 0)
                return false;
            const fx d[3] = {to[0] - me.pos[0], to[1] - me.pos[1], 0};
            const fx flat = fxLength(d);
            if (flat <= 0)
                return false;
            for (int i = 0; i < a.count; ++i)
            {
                const ActorState &mate = a.states[i];
                if (i == index || !gruntAlive(mate) || mate.squad != me.squad)
                    continue;
                const std::int64_t mx = mate.pos[0] - me.pos[0], my = mate.pos[1] - me.pos[1];
                const std::int64_t along = (mx * d[0] + my * d[1]) / flat;
                std::int64_t aside = (mx * d[1] - my * d[0]) / flat;
                aside = aside < 0 ? -aside : aside;
                if (along > 0 && aside < fxFromInt(48))
                    return true;
            }
            return false;
        }

        /// A live grenade near enough to run from: where it is, if so.
        inline bool gruntDanger(const ActorWorld &a, const ActorState &me, fx *where)
        {
            for (const Projectile &shot : a.shots)
            {
                if (!shot.active || shot.kind != kShotGrenade)
                    continue;
                const fx d[3] = {shot.pos[0] - me.pos[0], shot.pos[1] - me.pos[1], shot.pos[2] - me.pos[2]};
                if (fxLength(d) < fxFromInt(300))
                {
                    for (int k = 0; k < 3; ++k)
                        where[k] = shot.pos[k];
                    return true;
                }
            }
            return false;
        }

        /// Somewhere to stand that `threat` (an eye, or a grenade) has no
        /// line to: a few steps to one side or the other, if that does it;
        /// else the nearest path node, at least `least` and at most 784
        /// units off, that is no nearer the threat than it is to him. False
        /// if there is nowhere (and then he does not look again for a
        /// while: looking is a good many lines through the world).
        inline bool gruntFindCover(LogicWorld &w, int index, const fx *threat, int least, fx *out)
        {
            ActorWorld &a = *w.actors;
            ActorState &me = a.states[index];
            if (me.coverMs > 0)
                return false;
            const MoveWorld &world = *w.world;
            fx sine, cosine;
            fxSinCos(me.yaw, sine, cosine);
            for (int n = 1; least == 0 && n <= 5; ++n)
            {
                for (int side = -1; side <= 1; side += 2)
                {
                    const fx there[3] = {me.pos[0] + sine * (48 * n * side), me.pos[1] - cosine * (48 * n * side),
                                         me.pos[2]};
                    const fx eye[3] = {there[0], there[1], there[2] + fxFromInt(50)};
                    if (lineClear(world, threat, eye) ||
                        !walkClear(world, &a, index, kWalkStanding, me.pos, there, n * 3 + 1))
                        continue;
                    for (int k = 0; k < 3; ++k)
                        out[k] = there[k];
                    return true;
                }
            }
            int best = -1;
            fx bestD = fxFromInt(kGruntCoverRadius);
            // (Somewhere he can GET to: the node he is at, and a way from
            // it. A wall away is hidden, and no use.)
            const int mineNode = pathNearest(a, world, me.pos, kWalkStanding, index);
            for (int n = 0; n < a.nodeCount; ++n)
            {
                const fx *p = a.nodes[n].pos;
                const fx mine[3] = {p[0] - me.pos[0], p[1] - me.pos[1], p[2] - me.pos[2]};
                const fx far = fxLength(mine);
                if (far < fxFromInt(least) || far >= bestD)
                    continue;
                const fx theirs[3] = {p[0] - threat[0], p[1] - threat[1], p[2] - threat[2]};
                if (fxLength(theirs) < far)
                    continue;
                const fx eye[3] = {p[0], p[1], p[2] + fxFromInt(50)};
                if (lineClear(world, threat, eye))
                    continue;
                if (mineNode >= 0 && n != mineNode &&
                    pathStep(a, mineNode, n, kPathLinkHuman, true) == kNoPathNode)
                    continue;
                best = n;
                bestD = far;
            }
            if (best < 0)
            {
                me.coverMs = 2000;
                return false;
            }
            for (int k = 0; k < 3; ++k)
                out[k] = a.nodes[best].pos[k];
            return true;
        }

        // --------------------------------------------------- what he does

        inline void gruntGo(ActorWorld &a, ActorState &me, const fx *dest, std::uint8_t why)
        {
            me.ai = kAiChase;
            me.attack = why;
            for (int k = 0; k < 3; ++k)
                me.target[k] = dest[k];
            me.thinkMs = 0;
            me.lookMs = 200;
            me.stuckMs = 0;
            me.moveMs = 6000; // (longer than that and he is getting nowhere)
            me.hasGoal = false;
            me.goalNode = me.fromNode = -1;
            // Hurt badly enough, he limps.
            const char *clip = me.health <= kGruntLimp ? "limpingrun" : "run";
            const ModelView *model = actorModel(a, me);
            if (model == nullptr || model->findClip(nameHash(clip)) < 0)
                clip = "run";
            me.speed = actorClipSpeed(a, me, nameHash(clip), fxFromInt(170));
            monsterClip(a, me, clip, false);
        }

        inline void gruntAct(ActorWorld &a, ActorState &me, std::uint8_t what, const char *clip)
        {
            me.ai = kAiAttack;
            me.attack = what;
            me.struck = 0;
            me.lookMs = 0;
            monsterClip(a, me, clip, true);
        }

        inline void gruntWait(ActorWorld &a, ActorState &me, int ms)
        {
            me.ai = kGruntWait;
            me.attack = 0;
            me.cooldownMs = ms;
            me.lookMs = 300;
            monsterClip(a, me, (me.mood & kMoodStanding) ? "combatidle" : "crouching_idle", false);
        }

        /// A burst begun: one of `bursts` (0: the next of those he has left).
        inline void gruntBurst(ActorWorld &a, ActorState &me, int bursts)
        {
            if (bursts != 0)
            {
                me.bursts = static_cast<std::uint8_t>(bursts);
                // Standing or crouched: chosen again one time in ten.
                if (chance(a) % 10 == 0)
                    me.mood = static_cast<std::uint8_t>((me.mood & ~kMoodStanding) | ((chance(a) & 1) ? kMoodStanding : 0));
            }
            const bool shotgun = (me.item & kGruntShotgun) != 0;
            const bool standing = (me.mood & kMoodStanding) != 0;
            gruntAct(a, me, kActBurst,
                     shotgun ? (standing ? "standing_shotgun" : "crouching_shotgun")
                             : (standing ? "standing_mp5" : "crouching_mp5"));
        }

        /// The game's toss: up to the top of an arc (as high as the ceiling
        /// midway lets it be) and down onto `to`, under half gravity. False
        /// if the place is too far above him, there is no headroom, he is
        /// too near, or something is in the way of either half.
        inline bool gruntToss(LogicWorld &w, const ActorState &me, const fx *from, fx *to, fx *velocity)
        {
            ActorWorld &a = *w.actors;
            if (to[2] - from[2] > fxFromInt(500))
                return false;
            // A little to one side: not down on their head.
            fx sine, cosine;
            fxSinCos(me.yaw, sine, cosine);
            const int right = static_cast<int>(chance(a) % 49) - 24, ahead = static_cast<int>(chance(a) % 49) - 24;
            to[0] += sine * right + cosine * ahead;
            to[1] += -cosine * right + sine * ahead;
            fx mid[3] = {(from[0] + to[0]) / 2, (from[1] + to[1]) / 2, (from[2] + to[2]) / 2};
            const fx above[3] = {mid[0], mid[1], mid[2] + fxFromInt(500)};
            const Shot up = traceShot(*w.world, mid, above);
            mid[2] += static_cast<fx>((static_cast<std::int64_t>(fxFromInt(500)) * up.fraction) >> kFractionShift) -
                      fxFromInt(15);
            if (mid[2] < from[2] || mid[2] < to[2])
                return false;
            // How long up, and how long down: t = sqrt(d / (g / 2)), g = 400.
            const std::int64_t t1 = isqrt64(static_cast<std::uint64_t>(fxToInt(mid[2] - from[2])) * 5000u);
            const std::int64_t t2 = isqrt64(static_cast<std::uint64_t>(fxToInt(mid[2] - to[2])) * 5000u);
            if (t1 < 100)
                return false;
            velocity[0] = static_cast<fx>((static_cast<std::int64_t>(to[0] - from[0]) * 1000) / (t1 + t2));
            velocity[1] = static_cast<fx>((static_cast<std::int64_t>(to[1] - from[1]) * 1000) / (t1 + t2));
            velocity[2] = static_cast<fx>((static_cast<std::int64_t>(fxFromInt(400)) * t1) / 1000);
            const fx apex[3] = {from[0] + static_cast<fx>((static_cast<std::int64_t>(velocity[0]) * t1) / 1000),
                                from[1] + static_cast<fx>((static_cast<std::int64_t>(velocity[1]) * t1) / 1000),
                                mid[2]};
            const fx land[3] = {to[0], to[1], to[2] + fxFromInt(8)};
            return lineClear(*w.world, from, apex) && lineClear(*w.world, land, apex);
        }

        /// The game's throw: straight at `to` at `speed`, lifted for what
        /// half gravity will take off it on the way.
        inline bool gruntThrow(LogicWorld &w, const fx *from, const fx *to, int speed, fx *velocity)
        {
            const fx d[3] = {to[0] - from[0], to[1] - from[1], to[2] - from[2]};
            const std::int64_t ms = static_cast<std::int64_t>(fxToInt(fxLength(d))) * 1000 / speed;
            if (ms < 1)
                return false;
            for (int k = 0; k < 3; ++k)
                velocity[k] = static_cast<fx>((static_cast<std::int64_t>(d[k]) * 1000) / ms);
            velocity[2] += static_cast<fx>((static_cast<std::int64_t>(fxFromInt(200)) * ms) / 1000);
            const fx apex[3] = {from[0] + d[0] / 2, from[1] + d[1] / 2,
                                from[2] + d[2] / 2 +
                                    static_cast<fx>((static_cast<std::int64_t>(fxFromInt(200)) * ms * ms) / 4000000)};
            const fx land[3] = {to[0], to[1], to[2] + fxFromInt(8)};
            return lineClear(*w.world, from, apex) && lineClear(*w.world, land, apex);
        }

        /// Whether he has a grenade for the player and a way to get it to
        /// them: then ActorState::velocity is how it is to leave him.
        inline bool gruntGrenade(LogicWorld &w, const PlayerState &player, int index, bool sees)
        {
            ActorWorld &a = *w.actors;
            ActorState &me = a.states[index];
            if (!(me.item & (kGruntGrenades | kGruntLauncher)) || me.grenadeMs > 0)
                return false;
            // Not at someone in the air above him.
            if (!player.onGround && player.waterLevel == 0 && me.seen[2] > me.pos[2] + fxFromInt(72))
                return false;
            const fx gun[3] = {me.pos[0], me.pos[1], me.pos[2] + fxFromInt(60)};
            fx to[3];
            if (me.item & kGruntGrenades)
            {
                // At their feet (which he knows, seen or not), or where he
                // last saw them: one or the other.
                const bool feet = (chance(a) & 1) != 0;
                for (int k = 0; k < 3; ++k)
                    to[k] = feet ? player.origin[k] : me.seen[k];
                to[2] -= player.ducked ? kCrouchedFeet : kStandingFeet;
            }
            else
            {
                // Where they will be when it gets there, if he can see them.
                const fx d[3] = {player.origin[0] - gun[0], player.origin[1] - gun[1], player.origin[2] - gun[2]};
                const std::int64_t ms = static_cast<std::int64_t>(fxToInt(fxLength(d))) * 1000 / kGruntLaunchSpeed;
                for (int k = 0; k < 3; ++k)
                    to[k] = player.origin[k] +
                            (sees ? static_cast<fx>((static_cast<std::int64_t>(player.velocity[k]) * ms) / 1000) : 0);
            }
            // Not onto his own squad, and not at his own feet.
            me.grenadeMs = 1000;
            for (int i = 0; i < a.count; ++i)
            {
                const ActorState &mate = a.states[i];
                if (i == index || me.squad < 0 || !gruntAlive(mate) || mate.squad != me.squad)
                    continue;
                const fx d[3] = {mate.pos[0] - to[0], mate.pos[1] - to[1], mate.pos[2] - to[2]};
                if (fxLength(d) <= fxFromInt(256))
                    return false;
            }
            const fx flat[3] = {to[0] - me.pos[0], to[1] - me.pos[1], 0};
            if (fxLength(flat) <= fxFromInt(256))
                return false;
            const bool can = (me.item & kGruntGrenades) ? gruntToss(w, me, gun, to, me.velocity)
                                                        : gruntThrow(w, gun, to, kGruntLaunchSpeed, me.velocity);
            if (can)
                me.grenadeMs = 0;
            return can;
        }

        /// A round (or a shell): from his gun at the player, or where he
        /// last saw them, as loose as the gun is. It hurts whatever it meets
        /// first -- the player, or anything else that can be shot.
        inline void gruntShoot(LogicWorld &w, PlayerState &player, int index, bool sees)
        {
            ActorWorld &a = *w.actors;
            ActorState &me = a.states[index];
            const bool shotgun = (me.item & kGruntShotgun) != 0;
            const fx gun[3] = {me.pos[0], me.pos[1], me.pos[2] + gruntGun(me)};
            // (Somewhere between their belt and the top of their head.)
            const fx *at = sees ? player.origin : me.seen;
            const fx aim[3] = {at[0], at[1],
                               at[2] + static_cast<fx>((static_cast<std::int64_t>(player.viewHeight) *
                                                        (50 + chance(a) % 61)) / 100)};
            const fx d[3] = {aim[0] - gun[0], aim[1] - gun[1], aim[2] - gun[2]};
            const fx length = fxLength(d);
            if (length <= 0)
                return;
            fx dir[3], right[3], up[3];
            for (int k = 0; k < 3; ++k)
                dir[k] = static_cast<fx>((static_cast<std::int64_t>(d[k]) << kFxShift) / length);
            const fx flat[3] = {dir[0], dir[1], 0};
            const fx across = fxLength(flat);
            right[0] = across > 0 ? static_cast<fx>((static_cast<std::int64_t>(dir[1]) << kFxShift) / across) : kFxOne;
            right[1] = across > 0 ? static_cast<fx>((static_cast<std::int64_t>(-dir[0]) << kFxShift) / across) : 0;
            right[2] = 0;
            up[0] = fxMul(right[1], dir[2]);
            up[1] = -fxMul(right[0], dir[2]);
            up[2] = fxMul(right[0], dir[1]) - fxMul(right[1], dir[0]);
            const int cone = shotgun ? kGruntConeShotgun : kGruntConeMp5;
            const fx tall = player.ducked ? kCrouchedFeet : kStandingFeet;
            const fx mins[3] = {player.origin[0] - fxFromInt(16), player.origin[1] - fxFromInt(16),
                                player.origin[2] - tall};
            const fx maxs[3] = {player.origin[0] + fxFromInt(16), player.origin[1] + fxFromInt(16),
                                player.origin[2] + tall};
            for (int n = 0; n < (shotgun ? kGruntPellets : 1); ++n)
            {
                // (Two throws added: more of them near the middle.)
                const int x = static_cast<int>(chance(a) % 513) + static_cast<int>(chance(a) % 513) - 512;
                const int y = static_cast<int>(chance(a) % 513) + static_cast<int>(chance(a) % 513) - 512;
                fx end[3];
                for (int k = 0; k < 3; ++k)
                {
                    const fx off = dir[k] + static_cast<fx>((static_cast<std::int64_t>(right[k]) * x * cone +
                                                             static_cast<std::int64_t>(up[k]) * y * cone) >> 21);
                    end[k] = gun[k] + off * kGruntRange;
                }
                std::int64_t reach = traceShot(*w.world, gun, end).fraction;
                const std::int64_t wall = reach;
                int struck = -1;
                for (int i = 0; i < a.count; ++i)
                {
                    const ActorState &other = a.states[i];
                    if (i == index || !other.active || !fights(other.kind) || other.ai >= kAiDying)
                        continue;
                    fx lo[3], hi[3];
                    targetBox(other, lo, hi);
                    std::int64_t enter;
                    if (lineEntersBox(gun, end, lo, hi, enter) && enter < reach)
                    {
                        reach = enter;
                        struck = i;
                    }
                }
                std::int64_t enter;
                if (player.health > 0 && lineEntersBox(gun, end, mins, maxs, enter) && enter < reach)
                {
                    reach = enter;
                    struck = -1;
                    playerHurt(player, shotgun ? kGruntPellet : kGruntRound, gun);
                    a.got |= kGotHurt;
                }
                if (struck >= 0)
                {
                    monsterHurt(w, struck, shotgun ? kGruntPellet : kGruntRound, true);
                    fx mins[3], maxs[3];
                    targetBox(a.states[struck], mins, maxs);
                    const fx at[3] = {a.states[struck].pos[0], a.states[struck].pos[1], (mins[2] + maxs[2]) / 2};
                    bleed(w, struck, at, nullptr, a.states[struck].ai >= kAiDying || !a.states[struck].active);
                }
                if (n == 0)
                {
                    fx land[3];
                    for (int k = 0; k < 3; ++k)
                        land[k] = gun[k] + static_cast<fx>((static_cast<std::int64_t>(end[k] - gun[k]) * reach) >>
                                                           kFractionShift);
                    effectAdd(a, kEffectTracer, gun, land, 0, 60);
                    // (What misses marks the wall behind.)
                    if (reach == wall && wall < kFractionOne)
                        markShot(a, *w.world, gun, end, false);
                }
            }
            const fx flash[3] = {gun[0] + dir[0] * 24, gun[1] + dir[1] * 24, gun[2] + dir[2] * 24};
            effectAdd(a, kEffectMuzzle, flash, nullptr, 0, 50);
            if (me.ammo > 0)
                --me.ammo;
        }

        inline void gruntChoose(LogicWorld &w, PlayerState &player, int index);

        /// Out of the player's sight, if there is anywhere: a soldier on
        /// his own leaves a grenade behind half the time. Otherwise he
        /// fires if he may, and failing that stays where he is.
        inline void gruntTakeCover(LogicWorld &w, PlayerState &player, int index, bool canShoot, bool slot)
        {
            ActorWorld &a = *w.actors;
            ActorState &me = a.states[index];
            const fx threat[3] = {player.origin[0], player.origin[1], player.origin[2] + player.viewHeight};
            fx dest[3];
            if (gruntFindCover(w, index, threat, 0, dest))
            {
                if (me.squad < 0 && (chance(a) & 1))
                {
                    // (Where he is going is kept for when it is dropped.)
                    for (int k = 0; k < 3; ++k)
                        me.target[k] = dest[k];
                    gruntAct(a, me, kActDrop, "drop_grenade");
                    return;
                }
                gruntGo(a, me, dest, kGoCover);
                return;
            }
            if (canShoot && slot)
                gruntBurst(a, me, 4);
            else
                gruntWait(a, me, 2000);
        }

        /// What next: the game's GetSchedule, in its order.
        inline void gruntChoose(LogicWorld &w, PlayerState &player, int index)
        {
            ActorWorld &a = *w.actors;
            ActorState &me = a.states[index];
            me.attack = 0;
            if ((player.health <= 0 || unseen(player)))
            {
                // Nobody left to fight: whoever leads (or is alone) crows.
                me.ai = kAiIdle;
                me.lookMs = 1000;
                if (me.squad < 0 || me.squad == index)
                    gruntAct(a, me, kActVictory, "victorydance");
                else
                    monsterClip(a, me, "idle1", false);
                return;
            }
            const bool inSquad = me.squad >= 0;
            const bool isNew = (me.mood & kMoodNew) != 0;
            const bool hurt = (me.mood & kMoodHurt) != 0;
            me.mood &= static_cast<std::uint8_t>(~(kMoodNew | kMoodHurt));

            // A grenade by him comes before everything.
            fx danger[3];
            if (gruntDanger(a, me, danger))
            {
                gruntSay(w, me, kSpeakGrenade);
                fx dest[3];
                me.coverMs = 0;
                if (gruntFindCover(w, index, danger, 256, dest))
                    gruntGo(a, me, dest, kGoFlee);
                else
                    gruntAct(a, me, kActCower, "cower");
                return;
            }

            const bool sees = gruntSees(w, player, me, false);
            if (sees)
            {
                for (int k = 0; k < 3; ++k)
                    me.seen[k] = player.origin[k];
            }
            const fx feet[3] = {player.origin[0], player.origin[1], playerFeet(player)};
            const fx d[3] = {feet[0] - me.pos[0], feet[1] - me.pos[1], feet[2] - me.pos[2]};
            const fx far = fxLength(d);
            const fx facing = gruntFacing(me, feet);
            const bool clearOfFriends = !gruntFriendInWay(a, index, feet);
            const bool canShoot = sees && facing >= kFxOne / 2 && clearOfFriends;
            const bool canKick = far <= fxFromInt(kGruntKickRange) && facing >= kFxOne * 7 / 10;
            const bool engage = !inSquad || squadActing(a, index, kActBurst, kActSignal) < 2;
            const bool grenadeSlot = !inSquad || squadActing(a, index, kActToss, kActLaunch) < 2;
            ActorState *leader = inSquad ? &a.states[me.squad] : nullptr;

            auto grenade = [&]()
            {
                if (me.item & kGruntGrenades)
                    gruntAct(a, me, kActToss, "throwgrenade");
                else
                    gruntAct(a, me, kActLaunch, "launchgrenade");
            };
            auto charge = [&]()
            {
                gruntGo(a, me, feet, kGoCharge);
            };

            if (isNew && inSquad)
            {
                leader->mood &= static_cast<std::uint8_t>(~kMoodEluded);
                if (me.squad != index)
                {
                    gruntTakeCover(w, player, index, canShoot, engage);
                    return;
                }
                // The leader calls it, and keeps their head down (the first
                // time, with a wave to the others first).
                gruntSay(w, me, kSpeakAlert);
                if (!canShoot)
                {
                    if (sees && facing < kFxOne / 2)
                    {
                        me.mood |= kMoodNew;
                        gruntWait(a, me, 300);
                    }
                    else
                    {
                        charge();
                    }
                    return;
                }
                if (!(me.mood & kMoodMet))
                {
                    me.mood |= kMoodMet;
                    me.bursts = 5;
                    gruntAct(a, me, kActSignal, "flank_signal");
                }
                else
                {
                    gruntBurst(a, me, 5);
                }
                return;
            }
            if (me.ammo == 0)
            {
                const fx threat[3] = {player.origin[0], player.origin[1], player.origin[2] + player.viewHeight};
                fx dest[3];
                if (gruntFindCover(w, index, threat, 0, dest))
                    gruntGo(a, me, dest, kGoReload);
                else
                    gruntAct(a, me, kActReload, (me.item & kGruntShotgun) ? "reload_shotgun" : "reload_mp5");
                return;
            }
            if (hurt)
            {
                // Nine times in ten he runs for cover; else he flinches.
                if (chance(a) % 100 <= 90)
                {
                    gruntSay(w, me, kSpeakCover);
                    gruntTakeCover(w, player, index, canShoot, engage);
                }
                else
                {
                    me.ai = kAiFlinch;
                    monsterClip(a, me, "smflinch", true);
                }
                return;
            }
            if (canKick)
            {
                gruntAct(a, me, kActKick, "frontkick");
                return;
            }
            const bool canGrenade = grenadeSlot && gruntGrenade(w, player, index, sees);
            if ((me.item & kGruntLauncher) && canGrenade)
            {
                grenade();
                return;
            }
            if (canShoot)
            {
                // The squad had lost them, and here they are with their
                // back turned: a wave to the others first.
                const fx back[3] = {me.pos[0] - player.origin[0], me.pos[1] - player.origin[1], 0};
                const fx span = fxLength(back);
                const bool facingMe =
                    span <= 0 || static_cast<std::int64_t>(back[0]) * player.facingXY[0] +
                                         static_cast<std::int64_t>(back[1]) * player.facingXY[1] >
                                     static_cast<std::int64_t>(span) * (kFxOne / 2);
                if (inSquad && (leader->mood & kMoodEluded) && !facingMe)
                {
                    leader->mood &= static_cast<std::uint8_t>(~kMoodEluded);
                    gruntAct(a, me, kActFound, "advance_signal");
                    return;
                }
                if (engage)
                    gruntBurst(a, me, 4);
                else if (canGrenade)
                    grenade();
                else
                    gruntTakeCover(w, player, index, false, false);
                return;
            }
            if (!sees)
            {
                if (canGrenade)
                {
                    gruntSay(w, me, kSpeakThrow);
                    grenade();
                }
                else if (engage)
                {
                    gruntSay(w, me, kSpeakCharge);
                    charge();
                }
                else
                {
                    // Nothing for it but to wait and see if they come out.
                    if (chance(a) & 1)
                        gruntSay(w, me, kSpeakTaunt);
                    if (leader != nullptr)
                        leader->mood |= kMoodEluded;
                    gruntWait(a, me, 2000);
                }
                return;
            }
            // In sight, and no shot to be had: turned the wrong way, or one
            // of his own between.
            if (facing < kFxOne / 2)
                gruntWait(a, me, 300);
            else
                charge();
        }

        /// He learns of the player, and so do those of his squad who had
        /// not: `where` is where the player is taken to be.
        inline void gruntAlert(LogicWorld &w, int index, const fx *where)
        {
            ActorWorld &a = *w.actors;
            const int squad = a.states[index].squad;
            for (int i = 0; i < a.count; ++i)
            {
                ActorState &one = a.states[i];
                if (i != index && (squad < 0 || one.squad != squad))
                    continue;
                if (!gruntAlive(one) || one.ai != kAiIdle || one.script >= 0 || (one.flags & kActorPrisoner))
                    continue;
                one.mood |= kMoodNew | kMoodSeen | kMoodScripted;
                if (i == index)
                    gruntTrigger(w, one, 1);
                for (int k = 0; k < 3; ++k)
                    one.seen[k] = where[k];
                gruntWait(a, one, 0);
            }
        }

        inline void gruntTick(LogicWorld &w, PlayerState &player, int index, int ms)
        {
            ActorWorld &a = *w.actors;
            ActorState &me = a.states[index];
            const MoveWorld &world = *w.world;
            if (!(me.mood & kMoodReady))
            {
                // His first tick: what he has to lose, what is in his gun,
                // and whom he leads.
                me.mood |= kMoodReady | kMoodStanding;
                me.health = kGruntHealth;
                if ((me.item & (kGruntMp5 | kGruntShotgun)) == 0)
                    me.item |= kGruntMp5 | ((me.item & 15) == 0 ? kGruntGrenades : 0);
                me.ammo = static_cast<std::uint8_t>((me.item & kGruntShotgun) ? kGruntShells : kGruntClip);
                me.speed = actorClipSpeed(a, me, nameHash("run"), fxFromInt(170));
                me.lookMs = (index * 37) % 300;
                me.restClip = me.clip;
                me.squad = me.squad >= 0 ? me.squad : std::int16_t(-1);
                gruntRecruit(w, index);
            }
            if (me.ai == kAiDead)
                return;
            if (me.ai == kAiDying)
            {
                if (me.finished)
                    me.ai = kAiDead;
                return;
            }
            if (me.script >= 0)
            {
                me.mood |= kMoodScripted;
                return;
            }
            if (me.cooldownMs > 0)
                me.cooldownMs -= ms;
            if (me.grenadeMs > 0)
                me.grenadeMs -= ms;
            if (me.painMs > 0)
                me.painMs -= ms;
            if (me.coverMs > 0)
                me.coverMs -= ms;

            const fx feet[3] = {player.origin[0], player.origin[1], playerFeet(player)};
            const fx d[3] = {feet[0] - me.pos[0], feet[1] - me.pos[1], feet[2] - me.pos[2]};

            switch (me.ai)
            {
            case kAiIdle:
            {
                // (Acting out his crowing over the player: let it finish.)
                if (me.attack == kActVictory && me.finished)
                {
                    me.attack = 0;
                    monsterClip(a, me, "idle1", false);
                }
                if ((me.lookMs -= ms) > 0)
                    break;
                me.lookMs = 300;
                if ((player.health <= 0 || unseen(player)) || (me.flags & kActorPrisoner))
                    break;
                if ((me.flags & kActorWaitScript) && !(me.mood & kMoodScripted))
                    break;
                if ((me.flags & kActorWaitSeen) && !(me.mood & kMoodSeen))
                {
                    // Nothing, until the player has looked his way.
                    const fx flat[3] = {-d[0], -d[1], 0};
                    const fx span = fxLength(flat);
                    if (span > 0 &&
                        static_cast<std::int64_t>(flat[0]) * player.facingXY[0] +
                                static_cast<std::int64_t>(flat[1]) * player.facingXY[1] >
                            static_cast<std::int64_t>(span) * (kFxOne / 2) &&
                        gruntSees(w, player, me, false))
                        me.mood |= kMoodSeen;
                    break;
                }
                // Something loud not far off: he turns to it.
                if (a.noiseMs > 0)
                {
                    const fx n[3] = {a.noise[0] - me.pos[0], a.noise[1] - me.pos[1], a.noise[2] - me.pos[2]};
                    if (fxLength(n) < fxFromInt(1024))
                        me.yaw = fxAtan2Turns(n[1], n[0]);
                }
                if (gruntSees(w, player, me, true))
                {
                    gruntAlert(w, index, player.origin);
                    break;
                }
                // Now and then, something said to pass the time: a question
                // (which the next to speak answers), or nothing much.
                if (!(me.flags & kActorGag) && chance(a) % 33 == 0 && a.speakMs <= 0 && (a.asked != 0 || (chance(a) & 1)))
                {
                    if (a.asked != 0)
                    {
                        gruntSay(w, me, a.asked == 1 ? kSpeakClear : kSpeakAnswer);
                        a.asked = 0;
                    }
                    else
                    {
                        const int what = static_cast<int>(chance(a) % 3);
                        gruntSay(w, me, what == 0 ? kSpeakCheck : (what == 1 ? kSpeakQuestion : kSpeakIdle));
                        a.asked = static_cast<std::uint8_t>(what == 0 ? 1 : (what == 1 ? 2 : 0));
                    }
                }
                break;
            }
            case kAiChase:
            {
                const bool charging = me.attack == kGoCharge;
                const fx *dest = charging ? feet : me.target;
                const int went = walkTick(a, world, index, kWalkStanding, dest, me.speed, ms, &player, fxFromInt(16),
                                          fxFromInt(72), !charging);
                me.moveMs -= ms;
                const fx left[3] = {dest[0] - me.pos[0], dest[1] - me.pos[1], 0};
                const bool there = !charging && fxLength(left) < fxFromInt(24);
                if (charging && (me.lookMs -= ms) <= 0)
                {
                    // Broken off for a shot, a kick, or a grenade at his feet.
                    me.lookMs = 200;
                    fx danger[3];
                    if ((player.health <= 0 || unseen(player)) || gruntDanger(a, me, danger) ||
                        (fxLength(d) <= fxFromInt(kGruntKickRange)) || gruntSees(w, player, me, false))
                    {
                        gruntChoose(w, player, index);
                        break;
                    }
                }
                if (!there && went != kWalkNoWay && me.moveMs > 0)
                    break;
                if (me.attack == kGoReload)
                {
                    gruntAct(a, me, kActReload, (me.item & kGruntShotgun) ? "reload_shotgun" : "reload_mp5");
                }
                else if (charging)
                {
                    // No way to them: back out of sight, if there is that.
                    const bool sees = gruntSees(w, player, me, false);
                    gruntTakeCover(w, player, index, sees, me.squad < 0 ||
                                                               squadActing(a, index, kActBurst, kActSignal) < 2);
                }
                else
                {
                    gruntWait(a, me, 1000);
                }
                break;
            }
            case kGruntWait:
            {
                faceTo(me, me.seen[0] - me.pos[0], me.seen[1] - me.pos[1], ms, 27307); // 150 degrees a second
                if (me.cooldownMs <= 0)
                {
                    gruntChoose(w, player, index);
                    break;
                }
                if ((me.lookMs -= ms) > 0)
                    break;
                me.lookMs = 200;
                // (A look is a line through the world: five a second, not
                // one a frame.)
                const bool sees = gruntSees(w, player, me, false);
                if (sees)
                {
                    for (int k = 0; k < 3; ++k)
                        me.seen[k] = player.origin[k];
                }
                // Broken off as soon as there is something to do about them.
                fx danger[3];
                if (gruntDanger(a, me, danger) || (me.mood & kMoodHurt) ||
                    fxLength(d) <= fxFromInt(kGruntKickRange) || (sees && gruntFacing(me, feet) >= kFxOne / 2))
                    gruntChoose(w, player, index);
                break;
            }
            case kAiAttack:
            {
                // Whether he still sees them, looked at ten times a second
                // (kept in the top bit of `struck`, which the frames below
                // do not use).
                if (me.attack == kActBurst && (me.lookMs -= ms) <= 0)
                {
                    me.lookMs = 100;
                    me.struck &= 0x7F;
                    if (gruntSees(w, player, me, false))
                    {
                        me.struck |= 0x80;
                        for (int k = 0; k < 3; ++k)
                            me.seen[k] = player.origin[k];
                    }
                }
                const bool sees = (me.struck & 0x80) != 0;
                if (me.attack != kActCower && me.attack != kActDrop)
                    faceTo(me, me.seen[0] - me.pos[0], me.seen[1] - me.pos[1], ms, 21845); // 120 degrees a second
                const fx gun[3] = {me.pos[0], me.pos[1], me.pos[2] + fxFromInt(60)};
                // What the clip's own frames bring (the model's events).
                auto at = [&](int bit, int frame)
                {
                    if ((me.struck & (1 << bit)) || (frame > 0 && !passedFrame(me, frame)))
                        return false;
                    me.struck |= static_cast<std::uint8_t>(1 << bit);
                    return true;
                };
                switch (me.attack)
                {
                case kActBurst:
                {
                    // Three rounds a tenth of a second apart (a shotgun:
                    // one shell), the first with the gun's noise.
                    const bool shotgun = (me.item & kGruntShotgun) != 0;
                    for (int n = 0; n < (shotgun ? 1 : 3); ++n)
                    {
                        if (!at(n, n * 2) || me.ammo == 0)
                            continue;
                        if (n == 0)
                            monsterSound(w, me, shotgun ? CommonSound::Shotgun
                                                        : ((chance(a) & 1) ? CommonSound::GruntFire1
                                                                           : CommonSound::GruntFire2));
                        gruntShoot(w, player, index, sees);
                    }
                    if (!me.finished)
                        break;
                    if (me.bursts > 0)
                        --me.bursts;
                    // Another, if there are more to come, rounds for it,
                    // and still a shot to be had.
                    if (me.bursts > 0 && me.ammo > 0 && sees && player.health > 0 &&
                        !gruntFriendInWay(a, index, feet))
                        gruntBurst(a, me, 0);
                    else
                        gruntChoose(w, player, index);
                    break;
                }
                case kActReload:
                    if (at(0, 47))
                    {
                        me.ammo = static_cast<std::uint8_t>((me.item & kGruntShotgun) ? kGruntShells : kGruntClip);
                        monsterSound(w, me, CommonSound::GruntReload);
                    }
                    if (me.finished)
                    {
                        // (A model without the clip: reloaded all the same.)
                        me.ammo = static_cast<std::uint8_t>((me.item & kGruntShotgun) ? kGruntShells : kGruntClip);
                        gruntChoose(w, player, index);
                    }
                    break;
                case kActToss:
                    if (at(0, 36))
                    {
                        projectileAdd(a, kShotGrenade, gun, me.velocity, 3500);
                        me.grenadeMs = 6000;
                    }
                    if (me.finished)
                        gruntChoose(w, player, index);
                    break;
                case kActLaunch:
                    if (at(0, 16))
                    {
                        monsterSound(w, me, CommonSound::Launcher);
                        if (Projectile *shot = projectileAdd(a, kShotContact, gun, me.velocity, 10000))
                            shot->sprite = 1; // (a soldier's: it goes off at the player too)
                        me.grenadeMs = 6000;
                    }
                    if (me.finished)
                        gruntChoose(w, player, index);
                    break;
                case kActDrop:
                    if (at(0, 22))
                    {
                        fx sine, cosine;
                        fxSinCos(me.yaw, sine, cosine);
                        const fx from[3] = {me.pos[0] + cosine * 17 - sine * 27, me.pos[1] + sine * 17 + cosine * 27,
                                            me.pos[2] + fxFromInt(6)};
                        const fx still[3] = {0, 0, 0};
                        projectileAdd(a, kShotGrenade, from, still, 3000);
                    }
                    if (me.finished)
                    {
                        const fx dest[3] = {me.target[0], me.target[1], me.target[2]};
                        gruntGo(a, me, dest, kGoCover);
                    }
                    break;
                case kActKick:
                    if (at(0, 14) && player.health > 0 && fxLength(d) <= fxFromInt(70) &&
                        gruntFacing(me, feet) >= kFxOne / 2)
                    {
                        // Ten, and back they go.
                        playerHurt(player, kGruntKick, me.pos);
                        a.got |= kGotHurt;
                        fx sine, cosine;
                        fxSinCos(me.yaw, sine, cosine);
                        player.velocity[0] += cosine * 100;
                        player.velocity[1] += sine * 100;
                        player.velocity[2] += fxFromInt(50);
                        player.onGround = false;
                    }
                    if (me.finished)
                        gruntChoose(w, player, index);
                    break;
                case kActSignal:
                    if (me.finished)
                        gruntBurst(a, me, me.bursts > 0 ? me.bursts : 5);
                    break;
                default:
                    if (me.finished)
                        gruntChoose(w, player, index);
                    break;
                }
                break;
            }
            case kAiFlinch:
                if (me.finished)
                    gruntChoose(w, player, index);
                break;
            default:
                break;
            }
        }
    } // namespace detail

    /// Hurt the soldier at `index`, with `damage` to a hit box of `group`.
    /// The game's rules: his helmet takes twenty off what lands on it (and
    /// all of it, if that is all there was); a hit to the head counts three
    /// times; he dies by where he was hit, or by which way he was facing
    /// whoever hit him; far enough past dead, he comes apart.
    inline void gruntHurt(LogicWorld &w, int index, int damage, bool bullet, int group)
    {
        ActorWorld &a = *w.actors;
        ActorState &me = a.states[index];
        if (!me.active || me.ai >= kAiDying)
            return;
        (void)bullet;
        if (group == kHitHelmet)
        {
            // (Only a plain soldier wears one: the first of the heads.)
            if (me.body % 4 == 0)
            {
                damage -= 20;
                if (damage <= 0)
                    return;
            }
            group = kHitHead;
        }
        if (group == kHitHead)
            damage *= 3;
        me.health = static_cast<std::int16_t>(me.health - damage);
        detail::gruntTrigger(w, me, 2);
        if (me.health <= kGruntHealth / 2)
            detail::gruntTrigger(w, me, 3);
        if (me.health <= 0)
            detail::gruntTrigger(w, me, 4);
        if (me.health < -30)
        {
            const int squad = me.squad;
            if (gibBurst(w, index))
            {
                me.squad = static_cast<std::int16_t>(squad);
                detail::gruntLeaves(a, index);
                return;
            }
        }
        if (me.health <= 0)
        {
            if (me.script >= 0 && me.script < w.recordCount)
            {
                w.states[me.script].dead = true;
                me.script = -1;
                me.scriptPhase = kActorFree;
            }
            detail::gruntLeaves(a, index);
            // Which way he goes down: shot in the head or the gut, by that;
            // else away from whoever did it.
            const char *clip = "diesimple";
            if (group == kHitHead)
                clip = "dieheadshot";
            else if (group == kHitStomach)
                clip = "diegutshot";
            else if (me.ai != kAiIdle)
            {
                const fx facing = detail::gruntFacing(me, a.foe);
                clip = facing > kFxOne * 3 / 10 ? ((detail::chance(a) & 1) ? "dieback1" : "diebackwards")
                                                : (facing < -kFxOne * 3 / 10 ? "dieforward" : "diesimple");
            }
            const ModelView *model = actorModel(a, me);
            if (model == nullptr || model->findClip(nameHash(clip)) < 0)
                clip = "diesimple";
            me.ai = kAiDying;
            me.attack = 0;
            detail::monsterSound(w, me, (detail::chance(a) & 1) ? CommonSound::GruntDie1 : CommonSound::GruntDie2);
            me.flags &= static_cast<std::uint8_t>(~kActorSolid);
            me.velocity[0] = me.velocity[1] = me.velocity[2] = 0;
            detail::monsterClip(a, me, clip, true);
            actorPlaceBox(a, index);
            return;
        }
        if (me.painMs <= 0)
        {
            me.painMs = 1000;
            detail::monsterSound(w, me, (detail::chance(a) & 1) ? CommonSound::GruntPain1 : CommonSound::GruntPain2);
        }
        me.mood |= kMoodHurt;
        if (me.script >= 0 || (me.flags & kActorPrisoner))
            return;
        // Whoever was minding his own business is not any more, nor his
        // squad.
        if (me.ai == kAiIdle)
            detail::gruntAlert(w, index, a.foe);
    }
} // namespace hldata
