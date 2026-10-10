#pragma once

// The people of the place, when no scene has them: scientists and guards.
//
// The game lets the player take them along. USE on one and he follows; USE
// again and he stays. A scientist who sees something that would kill him
// runs for somewhere it cannot see him; a guard draws and shoots it -- and
// shoots a player who has shot him twice. That is all of it: they do not
// talk here beyond what the map's scenes have them say, and the monsters
// still have eyes only for the player.
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
    inline constexpr int kPersonSight = 512; ///< how far off a guard sees what he shoots
    inline constexpr int kPersonFright = 384; ///< ... and a scientist what he runs from
    inline constexpr int kGuardRound = 8;    ///< what one of a guard's shots does

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
        inline bool personHide(LogicWorld &w, int index, const fx *threat, fx *out)
        {
            ActorWorld &a = *w.actors;
            const ActorState &me = a.states[index];
            const int mine = pathNearest(a, *w.world, me.pos, kWalkStanding, index);
            int best = -1;
            fx bestD = fxFromInt(900);
            const fx from[3] = {threat[0], threat[1], threat[2] + fxFromInt(24)};
            for (int n = 0; n < a.nodeCount; ++n)
            {
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
                if (mine >= 0 && n != mine && pathStep(a, mine, n, kPathLinkHuman, true) == kNoPathNode)
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

        inline void personTick(LogicWorld &w, PlayerState &player, int index, int ms)
        {
            ActorWorld &a = *w.actors;
            ActorState &me = a.states[index];
            if (me.script >= 0 || me.ai >= kAiDying || me.kind == static_cast<std::uint8_t>(ActorKind::Sitter))
                return;
            const bool guard = me.kind == static_cast<std::uint8_t>(ActorKind::Guard);
            if (me.cooldownMs > 0)
                me.cooldownMs -= ms;

            // A look about, twice a second.
            me.lookMs -= ms;
            if (me.lookMs <= 0)
            {
                me.lookMs = 500;
                me.foe = static_cast<std::int16_t>(personFoe(w, me, guard ? kPersonSight : kPersonFright));
                me.lostMs = me.foe >= 0 ? 0 : me.lostMs + 500;
            }
            const bool foeThere = me.foe >= 0 && me.foe < a.count && a.states[me.foe].active &&
                                  a.states[me.foe].ai < kAiDying;

            if (guard)
            {
                // What he shoots: a monster in sight; or the player, who
                // has shot him twice.
                const bool turned = me.struck >= 2 && player.health > 0;
                const fx eye[3] = {me.pos[0], me.pos[1], me.pos[2] + fxFromInt(56)};
                const fx *at = nullptr;
                fx body[3];
                if (foeThere)
                {
                    body[0] = a.states[me.foe].pos[0];
                    body[1] = a.states[me.foe].pos[1];
                    body[2] = a.states[me.foe].pos[2] + fxFromInt(24);
                    at = body;
                }
                else if (turned)
                {
                    const fx d[3] = {player.origin[0] - me.pos[0], player.origin[1] - me.pos[1], 0};
                    if (fxLength(d) < fxFromInt(kPersonSight) && lineClear(*w.world, eye, player.origin))
                        at = player.origin;
                }
                if (at != nullptr)
                {
                    faceTo(me, at[0] - me.pos[0], at[1] - me.pos[1], ms, 65536 * 2);
                    if (me.cooldownMs <= 0)
                    {
                        me.cooldownMs = 600;
                        monsterClip(a, me, "shootgun2", true);
                        if (w.heard != nullptr)
                            w.heard->add(CommonSound::Pistol, me.pos, 110, kRadiusLarge);
                        if (foeThere)
                        {
                            monsterHurt(w, me.foe, kGuardRound, true);
                            bleed(w, me.foe, at, nullptr, a.states[me.foe].ai >= kAiDying || !a.states[me.foe].active);
                        }
                        else
                        {
                            playerHurt(player, 5, me.pos);
                            a.got |= kGotHurt;
                        }
                    }
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
