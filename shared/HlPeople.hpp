#pragma once

// The people of the place, when no scene has them: scientists and guards.
//
// The game lets the player take them along. USE on one and he follows; USE
// again and he stays. A scientist who sees something that would kill him
// runs for somewhere it cannot see him. A guard fights it, after the game's
// own guard (dlls/barney.cpp): he keeps the enemy he has until it is dead
// or long out of sight, draws before he fires, fires only when he faces it
// with nothing in the way -- a real round, which can miss -- and goes to
// where it went when it is hidden; he follows only when he has nothing to
// fight. He turns on a player who shoots him, by the game's rule. They do
// not talk here beyond what the map's scenes have them say, and the
// monsters still have eyes only for the player.
//
// They find their way as everything that walks does (HlWalk.hpp): by the
// map's nodes, through the doors on the way, which they open.
//
// Included at the end of HlMonsters.hpp. std-only and inline: the host
// tests run it.

namespace hldata
{
    // ActorState::ai, for a person (5 and up is dying and dead, as for
    // everything else).
    inline constexpr std::uint8_t kPersonIdle = 0;
    inline constexpr std::uint8_t kPersonFollow = 1;
    inline constexpr std::uint8_t kPersonFlee = 2;

    inline constexpr int kPersonNear = 112;  ///< as close as one who follows comes
    inline constexpr int kPersonSight = 1024; ///< how far off a guard sees, and shoots: the game's
    inline constexpr int kPersonFright = 384; ///< ... and a scientist what he runs from
    inline constexpr int kGuardRound = 5;    ///< what one of a guard's rounds does (the game's, at its middle difficulty)
    inline constexpr int kGuardCone = 143;   ///< two degrees, in 4096ths: how far a round of his may go off line
    inline constexpr int kGuardTurn = 65536 * 140 / 360; ///< a second, as the game turns him
    inline constexpr int kGuardForget = 6000; ///< ms out of sight before an enemy is given up

    /// USE, from `eye` along `forward` (Q12, unit): the scientist or guard
    /// in front of the player within arm's reach follows, or stays. True if
    /// there was one.
    inline bool personUse(LogicWorld &w, const fx *eye, const fx *forward)
    {
        if (w.actors == nullptr)
            return false;
        ActorWorld &a = *w.actors;
        int best = -1;
        fx bestD = fxFromInt(88);
        for (int i = 0; i < a.count; ++i)
        {
            const ActorState &actor = a.states[i];
            if (!actor.active || actor.script >= 0 || actor.ai >= kAiDying ||
                (actor.kind != static_cast<std::uint8_t>(ActorKind::Scientist) &&
                 actor.kind != static_cast<std::uint8_t>(ActorKind::Guard)))
                continue;
            // (Across the floor: the player looks level, and a person's
            // middle is well below their eyes. Within arm's reach, more or
            // less faced, and on the same floor.)
            const fx to[3] = {actor.pos[0] - eye[0], actor.pos[1] - eye[1], 0};
            const fx up = actor.pos[2] + fxFromInt(36) - eye[2];
            const fx distance = fxLength(to);
            const fx flat[3] = {forward[0], forward[1], 0};
            const fx facing = fxLength(flat);
            const std::int64_t ahead = static_cast<std::int64_t>(to[0]) * forward[0] +
                                       static_cast<std::int64_t>(to[1]) * forward[1];
            if (up > fxFromInt(72) || up < -fxFromInt(72) || facing <= 0)
                continue;
            if (distance < bestD && ahead * 10 > (static_cast<std::int64_t>(distance) * facing * 7))
            {
                best = i;
                bestD = distance;
            }
        }
        if (best < 0)
            return false;
        ActorState &actor = a.states[best];
        // (One who was following, or had run off while following, stays.)
        const bool follows = actor.ai != kPersonFollow && !(actor.ai == kPersonFlee && actor.wasFollowing);
        actor.ai = actor.ai == kPersonFlee ? kPersonFlee : (follows ? kPersonFollow : kPersonIdle);
        actor.wasFollowing = follows;
        actor.thinkMs = 0;
        actor.hasGoal = false;
        actor.goalNode = actor.fromNode = -1;
        if (actor.ai == kPersonIdle)
            detail::monsterClip(a, actor, "idle1", false);
        return true;
    }

    namespace detail
    {
        /// What there is to be afraid of, or to shoot: the nearest monster
        /// or soldier alive within `reach` units that can be seen from
        /// where the actor's eyes are. -1 for none.
        inline int personFoe(LogicWorld &w, const ActorState &me, int reach)
        {
            const ActorWorld &a = *w.actors;
            const fx eye[3] = {me.pos[0], me.pos[1], me.pos[2] + fxFromInt(60)};
            int best = -1;
            fx bestD = fxFromInt(reach);
            for (int i = 0; i < a.count; ++i)
            {
                const ActorState &other = a.states[i];
                if (!other.active || other.ai >= kAiDying || !(isMonster(other.kind) || isGrunt(other.kind)) ||
                    other.kind == static_cast<std::uint8_t>(ActorKind::Barnacle))
                    continue;
                const fx d[3] = {other.pos[0] - me.pos[0], other.pos[1] - me.pos[1], other.pos[2] - me.pos[2]};
                const fx distance = fxLength(d);
                if (distance >= bestD)
                    continue;
                const fx body[3] = {other.pos[0], other.pos[1], other.pos[2] + fxFromInt(24)};
                if (!lineClear(*w.world, eye, body))
                    continue;
                best = i;
                bestD = distance;
            }
            return best;
        }

        /// Somewhere to run to from `threat`: the nearest node that is
        /// further from it than he is, that it cannot see, and that there
        /// is a way to. False if there is none.
        inline bool personHideSearch(LogicWorld &w, int index, const fx *threat, fx *out)
        {
            ActorWorld &a = *w.actors;
            const ActorState &me = a.states[index];
            int pause = 0;
            const int mine = pathNearest(a, *w.world, me.pos, kWalkStanding, index);
            std::uint8_t reached[kMaxPathNodes];
            if (mine >= 0)
                pathReach(a, mine, kPathLinkHuman, true, reached);
            int best = -1;
            fx bestD = fxFromInt(900);
            const fx from[3] = {threat[0], threat[1], threat[2] + fxFromInt(24)};
            for (int n = 0; n < a.nodeCount; ++n)
            {
                thinkPoint(pause, 8);
                const fx *p = a.nodes[n].pos;
                const fx own[3] = {p[0] - me.pos[0], p[1] - me.pos[1], p[2] - me.pos[2]};
                const fx far = fxLength(own);
                if (far < fxFromInt(96) || far >= bestD)
                    continue;
                const fx theirs[3] = {p[0] - threat[0], p[1] - threat[1], p[2] - threat[2]};
                if (fxLength(theirs) < far)
                    continue;
                const fx eye[3] = {p[0], p[1], p[2] + fxFromInt(50)};
                if (lineClear(*w.world, from, eye))
                    continue;
                if (mine >= 0 && n != mine && !reached[n])
                    continue;
                best = n;
                bestD = far;
            }
            if (best < 0)
                return false;
            for (int k = 0; k < 3; ++k)
                out[k] = a.nodes[best].pos[k];
            return true;
        }

        /// Somewhere for the scientist at `index` to hide from `threat` (in
        /// his tick: as a soldier's cover, it is part of deciding).
        inline bool personHide(LogicWorld &w, int index, const fx *threat, fx *out)
        {
            return personHideSearch(w, index, threat, out);
        }

        /// Whether a guard can see `other` from where his eyes are: within
        /// his sight, with nothing of the world between.
        inline bool guardSees(LogicWorld &w, const ActorState &me, const fx *body)
        {
            const fx d[3] = {body[0] - me.pos[0], body[1] - me.pos[1], body[2] - me.pos[2]};
            if (fxLength(d) >= fxFromInt(kPersonSight))
                return false;
            const fx eye[3] = {me.pos[0], me.pos[1], me.pos[2] + fxFromInt(55)};
            return lineClear(*w.world, eye, body);
        }

        /// Something for a guard to fight that he has not got already: the
        /// nearest monster or soldier alive that he can see -- in front of
        /// him (the game's wide view: all but the 45 degrees behind each
        /// shoulder), or near enough to be heard. Not what a scene is
        /// playing, nor what the map keeps caged; not a barnacle. -1: none.
        inline int guardFoe(LogicWorld &w, const ActorState &me)
        {
            const ActorWorld &a = *w.actors;
            fx sine = 0, cosine = 0;
            fxSinCos(me.yaw, sine, cosine);
            int best = -1;
            fx bestD = fxFromInt(kPersonSight);
            for (int i = 0; i < a.count; ++i)
            {
                const ActorState &other = a.states[i];
                if (!other.active || other.ai >= kAiDying || !(isMonster(other.kind) || isGrunt(other.kind)) ||
                    other.kind == static_cast<std::uint8_t>(ActorKind::Barnacle) || other.script >= 0 ||
                    (other.flags & (kActorPrisoner | kActorDormant)))
                    continue;
                const fx d[3] = {other.pos[0] - me.pos[0], other.pos[1] - me.pos[1], other.pos[2] - me.pos[2]};
                const fx distance = fxLength(d);
                if (distance >= bestD)
                    continue;
                const std::int64_t ahead = static_cast<std::int64_t>(d[0]) * cosine + static_cast<std::int64_t>(d[1]) * sine;
                if (distance > fxFromInt(128) && ahead * 10 < -static_cast<std::int64_t>(distance) * kFxOne * 7)
                    continue;
                const fx body[3] = {other.pos[0], other.pos[1], other.pos[2] + fxFromInt(24)};
                if (!guardSees(w, me, body))
                    continue;
                best = i;
                bestD = distance;
            }
            return best;
        }

        /// Whether the player stands in a line from `from` to `to`.
        inline bool playerInLine(const PlayerState &player, const fx *from, const fx *to)
        {
            const fx half = fxFromInt(20);
            const fx mins[3] = {player.origin[0] - half, player.origin[1] - half, playerFeet(player)};
            const fx maxs[3] = {player.origin[0] + half, player.origin[1] + half, playerFeet(player) + fxFromInt(72)};
            std::int64_t enter = 0;
            return lineEntersBox(from, to, mins, maxs, enter) && enter < kFractionOne;
        }

        /// A guard with something to fight, or his pistol on its way out.
        /// True if that is what he did this tick (he follows nobody then).
        inline bool guardTick(LogicWorld &w, PlayerState &player, int index, int ms)
        {
            ActorWorld &a = *w.actors;
            ActorState &me = a.states[index];
            auto alive = [&](int i) { return i >= 0 && i < a.count && a.states[i].active && a.states[i].ai < kAiDying; };
            const bool enemy = me.struck >= 2 && player.health > 0 && !unseen(player); // the player is one
            if (!alive(me.foe))
                me.foe = -1;

            // A look, four times a second: at the one he has, and for a
            // better one.
            if (me.lookMs <= 0)
            {
                me.lookMs = 250;
                if (me.foe >= 0)
                {
                    const ActorState &foe = a.states[me.foe];
                    const fx body[3] = {foe.pos[0], foe.pos[1], foe.pos[2] + fxFromInt(24)};
                    me.foeSeen = guardSees(w, me, body);
                    if (me.foeSeen)
                    {
                        for (int k = 0; k < 3; ++k)
                            me.foeAt[k] = foe.pos[k];
                        me.lostMs = 0;
                    }
                    else if ((me.lostMs += 250) > kGuardForget)
                    {
                        me.foe = -1;
                    }
                }
                // Another only in place of none, of one he cannot see, or
                // of one twice as far off: he does not turn from one to
                // the other and back.
                const int other = guardFoe(w, me);
                if (other >= 0 && other != me.foe)
                {
                    bool take = me.foe < 0 || !me.foeSeen;
                    if (!take)
                    {
                        const fx mine[3] = {a.states[me.foe].pos[0] - me.pos[0], a.states[me.foe].pos[1] - me.pos[1],
                                            a.states[me.foe].pos[2] - me.pos[2]};
                        const fx theirs[3] = {a.states[other].pos[0] - me.pos[0], a.states[other].pos[1] - me.pos[1],
                                              a.states[other].pos[2] - me.pos[2]};
                        take = fxLength(theirs) * 2 < fxLength(mine);
                    }
                    if (take)
                    {
                        me.foe = static_cast<std::int16_t>(other);
                        me.foeSeen = true;
                        me.lostMs = 0;
                        for (int k = 0; k < 3; ++k)
                            me.foeAt[k] = a.states[other].pos[k];
                    }
                }
                // The player, for one who has turned on them: seen or not.
                if (me.foe < 0 && enemy)
                {
                    const fx body[3] = {player.origin[0], player.origin[1], player.origin[2]};
                    me.foeSeen = guardSees(w, me, body);
                    if (me.foeSeen)
                    {
                        for (int k = 0; k < 3; ++k)
                            me.foeAt[k] = k == 2 ? playerFeet(player) : player.origin[k];
                    }
                }
                // Nothing in the way of a round: not the world (seen), and
                // not the player.
                if (me.foe >= 0 && me.foeSeen)
                {
                    const fx muzzle[3] = {me.pos[0], me.pos[1], me.pos[2] + fxFromInt(55)};
                    const fx body[3] = {a.states[me.foe].pos[0], a.states[me.foe].pos[1],
                                        a.states[me.foe].pos[2] + fxFromInt(24)};
                    me.clearShot = !playerInLine(player, muzzle, body);
                }
                else
                {
                    me.clearShot = me.foe < 0 && enemy && me.foeSeen;
                }
            }
            const bool atPlayer = me.foe < 0 && enemy;
            if (me.foe < 0 && !(atPlayer && me.foeSeen))
            {
                // Nothing to fight. (His pistol stays out, as the game's
                // guard keeps his.)
                if (me.gun == 1)
                {
                    me.gun = 2;
                    me.body = 1;
                }
                return false;
            }

            fx body[3];
            if (atPlayer)
            {
                body[0] = player.origin[0];
                body[1] = player.origin[1];
                body[2] = player.origin[2];
            }
            else
            {
                body[0] = a.states[me.foe].pos[0];
                body[1] = a.states[me.foe].pos[1];
                body[2] = a.states[me.foe].pos[2] + fxFromInt(24);
            }

            // Hidden: to where it was last seen, and wait there for it.
            if (!me.foeSeen)
            {
                const fx d[3] = {me.foeAt[0] - me.pos[0], me.foeAt[1] - me.pos[1], 0};
                if (me.gun < 2 || fxLength(d) < fxFromInt(48))
                {
                    monsterClip(a, me, "idle1", false);
                    return true;
                }
                const fx speed = actorClipSpeed(a, me, nameHash("run"), fxFromInt(200));
                const int went = walkTick(a, *w.world, index, kWalkStanding, me.foeAt, speed, ms, &player, fxFromInt(16),
                                          fxFromInt(72));
                monsterClip(a, me, went == kWalkNoWay ? "idle1" : "run", false);
                return true;
            }

            // In sight: he stops, turns to it, has his pistol out, and
            // fires when he faces it with nothing in the way.
            faceTo(me, body[0] - me.pos[0], body[1] - me.pos[1], ms, kGuardTurn);
            if (me.gun == 0)
            {
                me.gun = 1;
                me.gunMs = 800; // (the length of his draw)
                monsterClip(a, me, "draw", true);
                return true;
            }
            if (me.gun == 1)
            {
                if ((me.gunMs -= ms) > 0)
                    return true;
                me.gun = 2;
                me.body = 1; // (the pistol in his hand, not at his hip)
            }
            fx sine = 0, cosine = 0;
            fxSinCos(me.yaw, sine, cosine);
            const fx to[3] = {body[0] - me.pos[0], body[1] - me.pos[1], 0};
            const fx flat = fxLength(to);
            const std::int64_t ahead = static_cast<std::int64_t>(to[0]) * cosine + static_cast<std::int64_t>(to[1]) * sine;
            // (Within sixty degrees of the way he faces: the game's.)
            const bool faced = flat <= 0 || ahead * 2 >= static_cast<std::int64_t>(flat) * kFxOne;
            if (me.cooldownMs > 0 || !faced || !me.clearShot)
            {
                if (me.once && me.finished)
                    monsterClip(a, me, "idle1", false);
                else if (!me.once)
                    monsterClip(a, me, "idle1", false);
                return true;
            }
            // One round to each firing clip, which is how long he takes
            // over it.
            a.seed = a.seed * 1664525u + 1013904223u;
            const bool quick = (a.seed >> 12) & 1;
            me.cooldownMs = quick ? 400 : 640;
            monsterClip(a, me, quick ? "shootgun2" : "shootgun", true);
            if (w.heard != nullptr)
            {
                a.seed = a.seed * 1664525u + 1013904223u;
                const int pitch = (a.seed >> 12) & 1 ? 100 : 95 + static_cast<int>((a.seed >> 16) % 11);
                w.heard->add(static_cast<std::uint16_t>(CommonSound::Pistol), me.pos, 110, kRadiusLarge, 0, pitch);
            }
            const fx muzzle[3] = {me.pos[0], me.pos[1], me.pos[2] + fxFromInt(55)};
            fx aim[3] = {body[0] - muzzle[0], body[1] - muzzle[1], body[2] - muzzle[2]};
            const fx length = fxLength(aim);
            if (length <= 0)
                return true;
            for (fx &c : aim)
                c = static_cast<fx>((static_cast<std::int64_t>(c) << kFxShift) / length);
            // Off line by the game's two degrees: across, and up or down.
            auto draw = [&]() -> int
            {
                a.seed = a.seed * 1664525u + 1013904223u;
                const int one = static_cast<int>((a.seed >> 8) % 4097);
                a.seed = a.seed * 1664525u + 1013904223u;
                return (one + static_cast<int>((a.seed >> 8) % 4097) - 4096) * kGuardCone / 4096;
            };
            const int across = draw(), upward = draw();
            const fx side[3] = {-aim[1], aim[0], 0};
            fx dir[3] = {aim[0] + ((side[0] * across) >> kFxShift), aim[1] + ((side[1] * across) >> kFxShift),
                         aim[2] + upward};
            effectAdd(a, kEffectMuzzle, muzzle, nullptr, 0, 50);
            if (atPlayer)
            {
                const fx end[3] = {muzzle[0] + dir[0] * kPersonSight, muzzle[1] + dir[1] * kPersonSight,
                                   muzzle[2] + dir[2] * kPersonSight};
                if (playerInLine(player, muzzle, end))
                {
                    playerHurt(player, kGuardRound, me.pos);
                    a.got |= kGotHurt;
                }
            }
            else
            {
                fireLine(w, player, muzzle, dir, kPersonSight, kGuardRound, true, true, nullptr, index);
            }
            return true;
        }

        inline void personTick(LogicWorld &w, PlayerState &player, int index, int ms)
        {
            ActorWorld &a = *w.actors;
            ActorState &me = a.states[index];
            if (me.script >= 0 || me.ai >= kAiDying || me.kind == static_cast<std::uint8_t>(ActorKind::Sitter))
                return;
            const bool guard = me.kind == static_cast<std::uint8_t>(ActorKind::Guard);
            if (me.cooldownMs > 0)
                me.cooldownMs -= ms;

            if (me.painMs > 0)
                me.painMs -= ms;
            // A look about, twice a second. (A guard's is his own: guardTick.)
            me.lookMs -= ms;
            if (me.lookMs <= 0 && !guard)
            {
                me.lookMs = 500;
                me.foe = static_cast<std::int16_t>(personFoe(w, me, kPersonFright));
                me.lostMs = me.foe >= 0 ? 0 : me.lostMs + 500;
            }
            const bool foeThere = me.foe >= 0 && me.foe < a.count && a.states[me.foe].active &&
                                  a.states[me.foe].ai < kAiDying;

            if (guard)
            {
                if (guardTick(w, player, index, ms))
                {
                    actorPlaceBox(a, index);
                    return;
                }
            }
            else if (foeThere && me.ai != kPersonFlee)
            {
                // A scientist, and something that would kill him: away.
                fx hide[3];
                if (personHide(w, index, a.states[me.foe].pos, hide))
                {
                    me.wasFollowing = me.ai == kPersonFollow;
                    me.ai = kPersonFlee;
                    for (int k = 0; k < 3; ++k)
                        me.target[k] = hide[k];
                    me.thinkMs = 0;
                    me.hasGoal = false;
                    me.goalNode = me.fromNode = -1;
                }
            }

            if (me.ai == kPersonFlee)
            {
                const fx d[3] = {me.target[0] - me.pos[0], me.target[1] - me.pos[1], 0};
                const fx speed = actorClipSpeed(a, me, nameHash("run"), fxFromInt(200));
                if (fxLength(d) < fxFromInt(28) ||
                    walkTick(a, *w.world, index, kWalkStanding, me.target, speed, ms, &player, fxFromInt(16),
                             fxFromInt(72), true) == kWalkNoWay)
                {
                    // There (or as near as there is a way): he stays put
                    // until it has been quiet for a while.
                    monsterClip(a, me, "idle1", false);
                    if (!foeThere && me.lostMs > 4000)
                    {
                        me.ai = me.wasFollowing ? kPersonFollow : kPersonIdle;
                        me.thinkMs = 0;
                        me.hasGoal = false;
                        me.goalNode = me.fromNode = -1;
                    }
                }
                else
                {
                    monsterClip(a, me, "run", false);
                }
                actorPlaceBox(a, index);
                return;
            }

            // (One who stands where he is stands as he does: not as his last
            // shot left him.)
            if (me.ai == kPersonIdle && guard && me.once && me.finished)
                monsterClip(a, me, "idle1", false);
            if (me.ai == kPersonFollow)
            {
                // After the player: at a run if they are far ahead, a walk
                // if they are near, and no nearer than a couple of strides.
                const fx feet[3] = {player.origin[0], player.origin[1], playerFeet(player)};
                const fx d[3] = {feet[0] - me.pos[0], feet[1] - me.pos[1], 0};
                const fx gap = fxLength(d);
                if (player.health <= 0 || gap < fxFromInt(kPersonNear))
                {
                    faceTo(me, d[0], d[1], ms, 32768);
                    monsterClip(a, me, "idle1", false);
                }
                else
                {
                    const bool far = gap > fxFromInt(280);
                    const fx speed = actorClipSpeed(a, me, nameHash(far ? "run" : "walk"), fxFromInt(far ? 200 : 80));
                    const int went = walkTick(a, *w.world, index, kWalkStanding, feet, speed, ms, &player,
                                              fxFromInt(16), fxFromInt(72));
                    monsterClip(a, me, went == kWalkNoWay ? "idle1" : (far ? "run" : "walk"), false);
                }
                actorPlaceBox(a, index);
            }
        }
    } // namespace detail
} // namespace hldata
