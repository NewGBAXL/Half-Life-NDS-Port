#pragma once

// Walking: how anything with legs gets about.
//
// A step is the game's own: allowed if, lifted a stair's height, nothing is
// in the way, and there is floor under where it ends. A walk to somewhere is
// straight there when every step of that can be made, and otherwise from
// path node to path node by the table the converter worked out (PathNext) --
// the console looks its way up; it never searches.
//
// Monsters chase with this (HlMonsters.hpp), and people sent somewhere by a
// script walk there with it (HlLogic.hpp).
//
// Also here, because seeing is what the walking is planned by: a line
// through the world as it is drawn (not the clip hulls, which are the world
// grown by the player's size).
//
// std-only and inline: the host tests run it.

#include "HlActors.hpp"
#include "HlPlayerMove.hpp"

namespace hldata
{
    /// How long a step of a walk test is.
    inline constexpr fx kWalkStep = fxFromInt(16);

    namespace detail
    {
        // --------------------------------------------------------- lines

        /// A line through the world's own tree: false, with `hit` how far
        /// along it got (Q30), if it runs into something solid.
        inline bool lineThrough(const MoveWorld &world, std::int32_t num, std::int32_t f1,
                                std::int32_t f2, const fx *from, const fx *to, std::int32_t &hit,
                                int depth)
        {
            fx p1[3] = {from[0], from[1], from[2]};
            while (true)
            {
                if (num < 0)
                {
                    const std::int8_t contents = world.leaves[-1 - num].contents;
                    if (contents == kContentsSolid || contents == kContentsSky)
                    {
                        hit = f1;
                        return false;
                    }
                    return true;
                }
                if (depth > 96)
                {
                    hit = f1;
                    return false;
                }
                const Node &node = world.nodes[num];
                const Plane &plane = world.standing.planes[node.plane];
                const std::uint8_t type = world.standing.planeTypes[node.plane];
                const fx t1 = planeDistance(plane, type, p1);
                const fx t2 = planeDistance(plane, type, to);
                if (t1 >= 0 && t2 >= 0)
                {
                    num = node.children[0];
                    continue;
                }
                if (t1 < 0 && t2 < 0)
                {
                    num = node.children[1];
                    continue;
                }
                std::int64_t part = (static_cast<std::int64_t>(t1) << kFractionShift) / (t1 - t2);
                part = part < 0 ? 0 : (part > kFractionOne ? kFractionOne : part);
                fx mid[3];
                for (int i = 0; i < 3; ++i)
                    mid[i] = p1[i] + static_cast<fx>((static_cast<std::int64_t>(to[i] - p1[i]) * part) >>
                                                     kFractionShift);
                const std::int32_t fm =
                    f1 + static_cast<std::int32_t>((static_cast<std::int64_t>(f2 - f1) * part) >>
                                                   kFractionShift);
                const int side = t1 < 0 ? 1 : 0;
                if (!lineThrough(world, node.children[side], f1, fm, p1, mid, hit, depth + 1))
                    return false;
                num = node.children[side ^ 1];
                f1 = fm;
                for (int i = 0; i < 3; ++i)
                    p1[i] = mid[i];
                ++depth;
            }
        }
    } // namespace detail

    /// How far a line gets from `start` towards `end`: against the world as
    /// it is drawn, and against the doors and boxes (as their clip hulls: a
    /// hand's breadth fatter than they look).
    struct Shot
    {
        std::int32_t fraction = kFractionOne; ///< Q30
        std::int16_t mover = -1;              ///< the brush entity that stopped it, if one did
    };

    inline Shot traceShot(const MoveWorld &world, const fx *start, const fx *end)
    {
        Shot shot;
        if (world.nodes != nullptr)
        {
            std::int32_t hit = kFractionOne;
            if (!detail::lineThrough(world, world.headNode, 0, kFractionOne, start, end, hit, 0))
                shot.fraction = hit;
        }
        else
        {
            // A world given no tree (some tests): its clip hull must do.
            const Trace t = traceHull(world.crouched, start, end);
            shot.fraction = t.startSolid ? 0 : t.fraction;
        }
        for (int m = 0; m < world.moverCount; ++m)
        {
            const MoverHull &mover = world.movers[m];
            if (!mover.solid || !detail::nearMover(mover, true, start, end))
                continue;
            fx a[3], b[3];
            detail::toMoverSpace(mover, start, a);
            detail::toMoverSpace(mover, end, b);
            const Trace t = traceHull(mover.crouched, a, b);
            if (t.startSolid || t.allSolid || t.fraction >= shot.fraction)
                continue;
            shot.fraction = t.fraction;
            shot.mover = static_cast<std::int16_t>(m);
        }
        return shot;
    }

    inline bool lineClear(const MoveWorld &world, const fx *start, const fx *end)
    {
        return traceShot(world, start, end).fraction >= kFractionOne;
    }

    inline bool pointToFloor(const MoveWorld &world, fx *pos, fx down)
    {
        const fx from[3] = {pos[0], pos[1], pos[2] + fxFromInt(1)};
        const fx to[3] = {pos[0], pos[1], pos[2] - down};
        const Shot shot = traceShot(world, from, to);
        if (shot.fraction <= 0 || shot.fraction >= kFractionOne)
            return false;
        fx z = from[2] - static_cast<fx>((static_cast<std::int64_t>(from[2] - to[2]) * shot.fraction) >>
                                         kFractionShift);
        // (A brush is met as the box a crouching person cannot enter: half
        // that person's height above where it is.)
        if (shot.mover >= 0)
            z -= kCrouchedFeet;
        if (z < pos[2])
            pos[2] = z;
        return true;
    }

    // ------------------------------------------------------------- walking

    namespace detail
    {
        /// A trace as the thing at `index` (or as nobody, -1): its own box
        /// is not in its way.
        inline Trace walkerTrace(const MoveWorld &world, const ActorWorld *a, int index, bool small,
                                 const fx *start, const fx *end)
        {
            SolidBox *own = (a != nullptr && a->boxes != nullptr && index >= 0) ? &a->boxes[index] : nullptr;
            const bool was = own != nullptr && own->active;
            if (own != nullptr)
                own->active = false;
            const Trace t = traceAll(world, small, start, end);
            if (own != nullptr)
                own->active = was;
            return t;
        }
    } // namespace detail

    /// One step of a walk: `feet` moved by (dx, dy) if something that size
    /// can make it -- lifted a stair's height, nothing in the way; then let
    /// down, floor within two stairs' height. `feet` is changed only if so.
    inline bool walkStep(const MoveWorld &world, const ActorWorld *a, int index, bool small,
                         fx *feet, fx dx, fx dy)
    {
        const fx lift = small ? kCrouchedFeet : kStandingFeet;
        const fx start[3] = {feet[0], feet[1], feet[2] + lift};
        const fx up[3] = {start[0], start[1], start[2] + kStepSize};
        const Trace raised = detail::walkerTrace(world, a, index, small, start, up);
        if (raised.startSolid)
            return false;
        const fx along[3] = {raised.endPos[0] + dx, raised.endPos[1] + dy, raised.endPos[2]};
        const Trace across = detail::walkerTrace(world, a, index, small, raised.endPos, along);
        if (across.startSolid || across.fraction < kFractionOne)
            return false;
        const fx down[3] = {along[0], along[1], start[2] - kStepSize};
        const Trace drop = detail::walkerTrace(world, a, index, small, along, down);
        if (drop.startSolid || drop.fraction >= kFractionOne)
            return false; // nothing to stand on: it does not walk off edges
        feet[0] = drop.endPos[0];
        feet[1] = drop.endPos[1];
        feet[2] = drop.endPos[2] - lift;
        return true;
    }

    /// Whether something that size can walk from `from` to `to` (feet, both)
    /// in a straight line, in at most `steps` steps of 16 units.
    inline bool walkClear(const MoveWorld &world, const ActorWorld *a, int index, bool small,
                          const fx *from, const fx *to, int steps)
    {
        fx at[3] = {from[0], from[1], from[2]};
        for (int n = 0; n <= steps; ++n)
        {
            const fx d[3] = {to[0] - at[0], to[1] - at[1], 0};
            const fx distance = fxLength(d);
            if (distance <= kWalkStep)
            {
                const fx dz = to[2] - at[2];
                return dz < fxFromInt(40) && dz > -fxFromInt(40);
            }
            const fx dx = static_cast<fx>((static_cast<std::int64_t>(d[0]) * kWalkStep) / distance);
            const fx dy = static_cast<fx>((static_cast<std::int64_t>(d[1]) * kWalkStep) / distance);
            if (!walkStep(world, a, index, small, at, dx, dy))
                return false;
        }
        return false;
    }

    /// The path node to make for from `feet`: the nearest that can be seen
    /// from there, of the three nearest; -1 if there is none.
    inline int pathNearest(const ActorWorld &a, const MoveWorld &world, const fx *feet)
    {
        int best[3] = {-1, -1, -1};
        fx bestD[3] = {0, 0, 0};
        for (int n = 0; n < a.nodeCount; ++n)
        {
            const fx d[3] = {a.nodes[n].pos[0] - feet[0], a.nodes[n].pos[1] - feet[1],
                             a.nodes[n].pos[2] - feet[2]};
            // (Height counts double: the node on the floor above is not near.)
            const fx distance = (d[0] < 0 ? -d[0] : d[0]) + (d[1] < 0 ? -d[1] : d[1]) +
                                2 * (d[2] < 0 ? -d[2] : d[2]);
            if (distance > fxFromInt(768))
                continue;
            for (int k = 0; k < 3; ++k)
            {
                if (best[k] >= 0 && bestD[k] <= distance)
                    continue;
                for (int j = 2; j > k; --j)
                {
                    best[j] = best[j - 1];
                    bestD[j] = bestD[j - 1];
                }
                best[k] = n;
                bestD[k] = distance;
                break;
            }
        }
        const fx eye[3] = {feet[0], feet[1], feet[2] + fxFromInt(20)};
        for (int k = 0; k < 3; ++k)
        {
            if (best[k] < 0)
                break;
            const fx *p = a.nodes[best[k]].pos;
            const fx to[3] = {p[0], p[1], p[2] + fxFromInt(20)};
            if (lineClear(world, eye, to))
                return best[k];
        }
        return -1;
    }

    // ------------------------------------------------------------- going somewhere

    // What a tick of walking came to.
    inline constexpr int kWalkMoved = 0;   ///< nearer, or turning a corner
    inline constexpr int kWalkBlocked = 1; ///< something is in the way, for now
    inline constexpr int kWalkNoWay = 2;   ///< no way there is known

    namespace detail
    {
        inline void faceTo(ActorState &actor, fx dx, fx dy, int ms, int turnsPerSecond)
        {
            if (dx == 0 && dy == 0)
                return;
            const std::uint16_t want = fxAtan2Turns(dy, dx);
            const auto diff = static_cast<std::int16_t>(want - actor.yaw);
            const int most = (turnsPerSecond * ms) / 1000;
            const int turn = diff > most ? most : (diff < -most ? -most : diff);
            actor.yaw = static_cast<std::uint16_t>(actor.yaw + turn);
        }

        /// Whether a box with its feet at `feet` is in the player.
        inline bool boxInPlayer(const PlayerState &player, const fx *feet, fx half, fx height)
        {
            const fx tall = kPlayerHalfHeight[player.ducked ? 1 : 0];
            return feet[0] - half <= player.origin[0] + kPlayerHalfWidth &&
                   feet[0] + half >= player.origin[0] - kPlayerHalfWidth &&
                   feet[1] - half <= player.origin[1] + kPlayerHalfWidth &&
                   feet[1] + half >= player.origin[1] - kPlayerHalfWidth &&
                   feet[2] <= player.origin[2] + tall && feet[2] + height >= player.origin[2] - tall;
        }

        /// Work out where the walker at `index` should be making for on its
        /// way to `dest` (feet): `dest` itself, or a path node.
        inline void walkPlan(ActorWorld &a, const MoveWorld &world, int index, bool small,
                             const fx *dest, int reach)
        {
            ActorState &actor = a.states[index];
            const fx d[3] = {dest[0] - actor.pos[0], dest[1] - actor.pos[1], 0};
            // Straight there, if that is a walk that can be made.
            if (fxLength(d) < fxFromInt(reach) &&
                walkClear(world, &a, index, small, actor.pos, dest, reach / 16))
            {
                for (int i = 0; i < 3; ++i)
                    actor.goal[i] = dest[i];
                actor.hasGoal = true;
                actor.goalNode = actor.fromNode = -1;
                return;
            }
            // Otherwise by the nodes: to the nearest, and from each to the
            // next on the way to the one nearest where it is going.
            actor.hasGoal = false;
            if (a.nodeCount == 0)
                return;
            const int theirs = pathNearest(a, world, dest);
            if (theirs < 0)
                return;
            if (actor.goalNode < 0)
            {
                if (actor.fromNode >= 0)
                {
                    const std::uint8_t next = a.next[actor.fromNode * a.nodeCount + theirs];
                    // (At the last node and still not there: no nearer by
                    // nodes.)
                    actor.goalNode = (next == kNoPathNode || next == actor.fromNode) ? -1 : next;
                }
                else
                {
                    actor.goalNode = static_cast<std::int16_t>(pathNearest(a, world, actor.pos));
                }
            }
            if (actor.goalNode < 0)
                return;
            for (int i = 0; i < 3; ++i)
                actor.goal[i] = a.nodes[actor.goalNode].pos[i];
            actor.hasGoal = true;
        }
    } // namespace detail

    /// One tick of the walker at `index` going to `dest` (feet) at `speed`
    /// units a second: it plans twice a second, turns to face its way, and
    /// takes the step if it can. It does not walk into `player` (if given),
    /// being `half` wide and `height` tall. Uses the actor's thinkMs,
    /// stuckMs, goal and node fields.
    ///
    /// `still` says `dest` does not move (a mark, not a player): then a
    /// straight walk is looked for from further off, since it need be found
    /// only once -- the plan is made again only on reaching a node or on
    /// being held up.
    inline int walkTick(ActorWorld &a, const MoveWorld &world, int index, bool small, const fx *dest,
                        fx speed, int ms, const PlayerState *player, fx half, fx height,
                        bool still = false)
    {
        ActorState &actor = a.states[index];
        actor.thinkMs -= ms;
        if (actor.thinkMs <= 0)
        {
            actor.thinkMs = still ? 60000 : 500;
            detail::walkPlan(a, world, index, small, dest, still ? 1024 : 480);
            // (Nothing found: asked again -- soon by something chasing,
            // whose quarry moves; not so soon by something going to a mark,
            // for which looking is dear and little will have changed.)
            if (!actor.hasGoal)
                actor.thinkMs = still ? 1400 : 500;
        }
        if (!actor.hasGoal)
            return kWalkNoWay;
        const fx d[3] = {actor.goal[0] - actor.pos[0], actor.goal[1] - actor.pos[1], 0};
        const fx distance = fxLength(d);
        // Arrived at a node: on to the next when it next plans, which is now.
        if (actor.goalNode >= 0 && distance < fxFromInt(20))
        {
            actor.fromNode = actor.goalNode;
            actor.goalNode = -1;
            actor.thinkMs = 0;
            return kWalkMoved;
        }
        if (distance < fxFromInt(4))
            return kWalkMoved;
        detail::faceTo(actor, d[0], d[1], ms, 65536); // a turn a second
        fx step = static_cast<fx>((static_cast<std::int64_t>(speed) * ms) / 1000);
        if (step > distance)
            step = distance;
        const fx dx = static_cast<fx>((static_cast<std::int64_t>(d[0]) * step) / distance);
        const fx dy = static_cast<fx>((static_cast<std::int64_t>(d[1]) * step) / distance);
        fx feet[3] = {actor.pos[0], actor.pos[1], actor.pos[2]};
        // The whole step, or what of it one way or the other can be made
        // (sliding along what is in the way).
        bool moved = walkStep(world, &a, index, small, feet, dx, dy);
        if (!moved && dx != 0)
            moved = walkStep(world, &a, index, small, feet, dx, 0);
        if (!moved && dy != 0)
            moved = walkStep(world, &a, index, small, feet, 0, dy);
        // Not into the player: it stops in front of them.
        if (moved && player != nullptr && detail::boxInPlayer(*player, feet, half, height))
            moved = false;
        if (moved)
        {
            for (int i = 0; i < 3; ++i)
                actor.pos[i] = feet[i];
            actor.stuckMs = 0;
            return kWalkMoved;
        }
        // Getting nowhere: after a second it starts again from where it is.
        actor.stuckMs += ms;
        if (actor.stuckMs > 1000)
        {
            actor.stuckMs = 0;
            actor.goalNode = actor.fromNode = -1;
            actor.thinkMs = 0;
        }
        return kWalkBlocked;
    }
} // namespace hldata
