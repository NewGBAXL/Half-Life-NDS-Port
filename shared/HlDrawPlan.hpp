#pragma once

// Who is drawn, of the map's actors, when not everything fits a frame.
//
// The hardware takes so many polygons a frame and no more, and the models
// have a share of that. The rule here: an enemy that is alive and in view
// is ALWAYS drawn, and so are the people of the place; what gives way for
// them is, in order, what matters least -- bodies and furniture, things to
// pick up, the far enemies' detail, the nearest's, the detail of the weapon
// in hand, and at the last a little of the world's far end. (It used to be
// "nearest first, whatever it is": a medkit at the player's feet could cost
// them the sight of the headcrab behind it.)
//
// And what "in view" means follows the shape of the thing: a barnacle is a
// mouth on the ceiling and a tongue that hangs to the floor, and is in view
// if any of that is.
//
// std-only and inline: the host tests run it (test_drawplan).

#include <cstdint>

#include "HlActors.hpp"
#include "HlMonsters.hpp"

namespace hldata
{
    // ------------------------------------------------------------ what it is

    inline constexpr int kDrawEnemy = 0;  ///< alive, and fights: never left out
    inline constexpr int kDrawPerson = 1; ///< alive, one of the people; or in a scene
    inline constexpr int kDrawPickup = 2; ///< something to take
    inline constexpr int kDrawProp = 3;   ///< bodies, furniture, the rest: first to go
    inline constexpr int kDrawClasses = 4;

    inline int drawClass(const ActorState &actor)
    {
        const bool dead = actor.ai == kAiDead;
        if (fights(actor.kind))
            return dead ? kDrawProp : kDrawEnemy;
        if (isPerson(actor.kind) || actor.kind == static_cast<std::uint8_t>(ActorKind::Gman))
            return dead ? kDrawProp : kDrawPerson;
        // (Whatever a scene has taken is what the scene is about.)
        if (actor.script >= 0 && actor.scriptPhase != kActorFree)
            return kDrawPerson;
        if (actor.kind == static_cast<std::uint8_t>(ActorKind::Item))
            return kDrawPickup;
        return kDrawProp;
    }

    /// Whether `a` (a class, and how far off) is drawn before `b`.
    inline constexpr bool drawBefore(int classA, std::int32_t farA, int classB, std::int32_t farB)
    {
        return classA != classB ? classA < classB : farA < farB;
    }

    // ---------------------------------------------------------- where it is

    /// The middles of the balls (of the model's radius) an actor is in view
    /// by: in view if any is. In whole units.
    struct DrawPoints
    {
        static constexpr int kMost = 8;
        std::int16_t at[kMost][3] = {};
        int count = 0;
    };

    /// `top` is the model's height (its bounds' top), `radius` its radius.
    inline DrawPoints actorViewPoints(const ActorState &actor, int top, int radius)
    {
        DrawPoints points;
        const std::int16_t x = static_cast<std::int16_t>(fxToInt(actor.pos[0]));
        const std::int16_t y = static_cast<std::int16_t>(fxToInt(actor.pos[1]));
        const int z = fxToInt(actor.pos[2]);
        auto add = [&](int height)
        {
            if (points.count >= DrawPoints::kMost)
                return;
            std::int16_t *p = points.at[points.count++];
            p[0] = x;
            p[1] = y;
            p[2] = static_cast<std::int16_t>(height);
        };
        if (actor.kind == static_cast<std::uint8_t>(ActorKind::Barnacle))
        {
            // Its mouth is where it is, on the ceiling; the rest of it is
            // its tongue, as far down as that hangs. A ball at the mouth
            // and one every radius down to the tongue's end.
            const int hang = fxToInt(actor.reach) > 0 ? fxToInt(actor.reach) : 0;
            const int most = DrawPoints::kMost - 1;
            int step = radius > 16 ? radius : 16;
            if (hang > step * most)
                step = (hang + most - 1) / most;
            add(z);
            for (int down = step; down < hang + step && points.count < DrawPoints::kMost; down += step)
                add(z - (down < hang ? down : hang));
            return points;
        }
        // Something standing: a ball round its middle, which is about half
        // its height up, and one round its head -- someone in a doorway has
        // their middle in the frame and their head in the room. (Hung from
        // a ceiling, it is all below where it is fixed.)
        const int up = isTurret(actor.kind) && (actor.flags & kActorCeiling) ? -top : top;
        add(z + up / 2);
        add(z + up);
        return points;
    }

    // ------------------------------------------------------- who gets drawn

    inline constexpr int kDrawLevels = 4;  ///< the most levels of detail a model has
    inline constexpr int kDrawMost = 12;   ///< the most actors drawn at once
    /// How far the models' share may grow for an enemy's sake, out of the
    /// world's far end.
    inline constexpr int kDrawGrowPolygons = 300;
    inline constexpr int kDrawGrowVertices = 850;

    /// One actor in view. They are given in the order they would be drawn
    /// (drawBefore).
    struct DrawCandidate
    {
        std::uint8_t kind = kDrawProp; ///< kDraw*
        std::uint8_t levels = 1;
        std::uint8_t want = 0; ///< the level its distance asks for
        std::uint16_t polygons[kDrawLevels] = {};
        std::uint16_t vertices[kDrawLevels] = {};
    };

    struct DrawHeld
    {
        bool active = false;
        std::uint8_t levels = 1;
        std::uint8_t want = 0;
        std::uint16_t polygons[kDrawLevels] = {};
        std::uint16_t vertices[kDrawLevels] = {};
    };

    struct DrawPlan
    {
        bool drawn[kDrawMost * 2] = {};
        std::uint8_t level[kDrawMost * 2] = {};
        std::uint8_t heldLevel = 0;
        int polygons = 0, vertices = 0; ///< of everything drawn, the weapon with it
        int cut[kDrawClasses] = {};     ///< left out, by what they are
        int grown = 0;                  ///< polygons over the share
        /// An enemy or a person had to be made room for: the frame's
        /// effects give way next (stains, pieces).
        bool strained = false;
    };

    /// `count` candidates (at most kDrawMost * 2), in drawing order; no more
    /// than `limit` drawn -- but an enemy is, up to kDrawMost. `pressure`
    /// (0..3) is how full the frame's passes have been lately; `whole` is
    /// the size over which a model's finest level is "the game's own mesh",
    /// kept for the nearest in a frame with room.
    inline DrawPlan planModels(const DrawCandidate *all, int count, int limit, const DrawHeld &held, int pressure,
                               int sharePolygons, int shareVertices, int whole)
    {
        DrawPlan plan;
        if (count > kDrawMost * 2)
            count = kDrawMost * 2;
        limit = limit < 1 ? 1 : (limit > kDrawMost ? kDrawMost : limit);
        pressure = pressure < 0 ? 0 : (pressure > 3 ? 3 : pressure);

        // The weapon in hand is always in view.
        plan.heldLevel = held.active ? (held.want < held.levels ? held.want : held.levels - 1) : 0;
        auto heldCost = [&](bool vertices) -> int
        {
            if (!held.active)
                return 0;
            return vertices ? held.vertices[plan.heldLevel] : held.polygons[plan.heldLevel];
        };

        // Who has a place at all: the first `limit`; and every enemy.
        int kept = 0;
        for (int i = 0; i < count; ++i)
        {
            const DrawCandidate &one = all[i];
            const bool place = kept < limit || (one.kind == kDrawEnemy && kept < kDrawMost);
            if (!place)
            {
                ++plan.cut[one.kind];
                continue;
            }
            plan.drawn[i] = true;
            int level = one.want < one.levels ? one.want : one.levels - 1;
            // The game's own mesh is for whoever is nearest, in a frame
            // with room.
            if (level == 0 && one.levels > 1 && one.polygons[0] > whole && (kept > 0 || pressure > 0))
                level = 1;
            // (Coarser again for each step of pressure, for all but the
            // nearest.)
            if (kept > 0)
                level = level + pressure < one.levels ? level + pressure : one.levels - 1;
            plan.level[i] = static_cast<std::uint8_t>(level);
            ++kept;
        }

        // The enemies and the people first: all of them fit, whatever it
        // takes.
        auto sum = [&](bool vertices, int upTo) -> int
        {
            int total = heldCost(vertices);
            for (int i = 0; i < count; ++i)
            {
                if (plan.drawn[i] && all[i].kind <= upTo)
                    total += vertices ? all[i].vertices[plan.level[i]] : all[i].polygons[plan.level[i]];
            }
            return total;
        };
        int capPolygons = sharePolygons, capVertices = shareVertices;
        auto over = [&]() { return sum(false, kDrawPerson) > capPolygons || sum(true, kDrawPerson) > capVertices; };
        // 1. The far ones coarser, a step at a time, the farthest first;
        //    the nearest last of them.
        while (over())
        {
            int who = -1, first = -1;
            for (int i = 0; i < count; ++i)
            {
                if (!plan.drawn[i] || all[i].kind > kDrawPerson)
                    continue;
                if (first < 0)
                    first = i;
                if (plan.level[i] + 1 < all[i].levels && i != first)
                    who = i; // (the last found is the farthest, and a person before an enemy)
            }
            if (who < 0 && first >= 0 && plan.level[first] + 1 < all[first].levels)
                who = first;
            if (who < 0)
                break;
            ++plan.level[who];
            plan.strained = true;
        }
        // 2. The weapon in hand at its coarser level.
        bool tight = false; // nothing is left over for what matters less
        while (over() && held.active && plan.heldLevel + 1 < held.levels)
        {
            ++plan.heldLevel;
            plan.strained = true;
            tight = true;
        }
        // 3. A little of the world's far end.
        if (over())
        {
            const int needPolygons = sum(false, kDrawPerson) - capPolygons;
            const int needVertices = sum(true, kDrawPerson) - capVertices;
            const int morePolygons = needPolygons < 0 ? 0 : (needPolygons > kDrawGrowPolygons ? kDrawGrowPolygons : needPolygons);
            const int moreVertices = needVertices < 0 ? 0 : (needVertices > kDrawGrowVertices ? kDrawGrowVertices : needVertices);
            capPolygons += morePolygons;
            capVertices += moreVertices;
            plan.grown = morePolygons;
            plan.strained = true;
            tight = true;
        }
        // 4. And if that is still not enough (a dozen soldiers at once):
        //    the farthest person, then the farthest enemy, until it is.
        while (over())
        {
            int who = -1;
            for (int i = 0; i < count; ++i)
            {
                if (plan.drawn[i] && all[i].kind <= kDrawPerson)
                    who = i;
            }
            if (who < 0)
                break;
            plan.drawn[who] = false;
            ++plan.cut[all[who].kind];
        }

        // Then what there is to pick up, and the rest, for as long as they
        // fit what is left of the share -- which under pressure is less.
        const int share = 4 - (pressure > 2 ? 2 : pressure);
        int polygons = sum(false, kDrawPerson), vertices = sum(true, kDrawPerson);
        bool any = held.active;
        for (int i = 0; i < count; ++i)
            any = any || (plan.drawn[i] && all[i].kind <= kDrawPerson);
        for (int i = 0; i < count; ++i)
        {
            if (!plan.drawn[i] || all[i].kind <= kDrawPerson)
                continue;
            const DrawCandidate &one = all[i];
            bool fits = false;
            int level = plan.level[i];
            // (Nothing of theirs when the enemies have had to take more
            // than the share.)
            for (; level < one.levels && !tight; ++level)
            {
                const int p = polygons + one.polygons[level], v = vertices + one.vertices[level];
                fits = p <= sharePolygons && v <= shareVertices &&
                       (!any || (p <= sharePolygons * share / 4 && v <= shareVertices * share / 4));
                if (fits)
                    break;
            }
            if (!fits)
            {
                plan.drawn[i] = false;
                ++plan.cut[one.kind];
                continue;
            }
            plan.level[i] = static_cast<std::uint8_t>(level);
            polygons += one.polygons[level];
            vertices += one.vertices[level];
            any = true;
        }
        plan.polygons = polygons;
        plan.vertices = vertices;
        return plan;
    }
} // namespace hldata
