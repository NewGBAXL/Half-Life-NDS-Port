// Who is drawn when not everything fits (shared/HlDrawPlan.hpp): an enemy
// alive and in view always is; what gives way is what matters least; and a
// barnacle is in view by its tongue as much as by its mouth.

#include <cstdio>

#include "HlDrawPlan.hpp"
#include "TestBspBuilder.hpp"

using namespace hldata;
using namespace hldstest;

namespace
{
    std::uint8_t kind(ActorKind k)
    {
        return static_cast<std::uint8_t>(k);
    }

    /// A model of `levels` levels, the finest of `finest` polygons, each
    /// level about two thirds of the one before (three vertices to two
    /// polygons, near enough).
    DrawCandidate model(int what, int finest, int levels = 3, int want = 0)
    {
        DrawCandidate one;
        one.kind = static_cast<std::uint8_t>(what);
        one.levels = static_cast<std::uint8_t>(levels);
        one.want = static_cast<std::uint8_t>(want);
        int p = finest;
        for (int l = 0; l < levels; ++l, p = p * 2 / 3)
        {
            one.polygons[l] = static_cast<std::uint16_t>(p);
            one.vertices[l] = static_cast<std::uint16_t>(p * 3 / 2);
        }
        return one;
    }
    DrawHeld weapon(int finest, int coarse)
    {
        DrawHeld held;
        held.active = true;
        held.levels = 2;
        held.polygons[0] = static_cast<std::uint16_t>(finest);
        held.vertices[0] = static_cast<std::uint16_t>(finest * 3 / 2);
        held.polygons[1] = static_cast<std::uint16_t>(coarse);
        held.vertices[1] = static_cast<std::uint16_t>(coarse * 3 / 2);
        return held;
    }
    int drawnOf(const DrawPlan &plan, const DrawCandidate *all, int count, int what)
    {
        int n = 0;
        for (int i = 0; i < count; ++i)
            n += plan.drawn[i] && all[i].kind == what ? 1 : 0;
        return n;
    }

    constexpr int kShare = 1200, kShareVertices = 3400, kWhole = 420;

    void testClasses()
    {
        ActorState a;
        a.kind = kind(ActorKind::Headcrab);
        check(drawClass(a) == kDrawEnemy, "a headcrab is an enemy");
        a.kind = kind(ActorKind::Barnacle);
        check(drawClass(a) == kDrawEnemy, "so is a barnacle");
        a.ai = kAiDying;
        check(drawClass(a) == kDrawEnemy, "... still, while it dies");
        a.ai = kAiDead;
        check(drawClass(a) == kDrawProp, "... and a body once it is dead");
        a = ActorState();
        a.kind = kind(ActorKind::Tripmine);
        check(drawClass(a) == kDrawEnemy, "a mine on a wall is to be seen");
        a.kind = kind(ActorKind::Sentry);
        check(drawClass(a) == kDrawEnemy, "and a gun on its legs");
        a.kind = kind(ActorKind::Scientist);
        check(drawClass(a) == kDrawPerson, "a scientist is one of the people");
        a.kind = kind(ActorKind::Gman);
        check(drawClass(a) == kDrawPerson, "and the man in the suit");
        a.kind = kind(ActorKind::Item);
        check(drawClass(a) == kDrawPickup, "a medkit is something to pick up");
        a.kind = kind(ActorKind::Furniture);
        check(drawClass(a) == kDrawProp, "a chair is a prop");
        a.script = 3;
        a.scriptPhase = kActorPlaying;
        check(drawClass(a) == kDrawPerson, "... unless a scene is playing it");
        check(drawBefore(kDrawEnemy, 900, kDrawProp, 10) && drawBefore(kDrawPerson, 500, kDrawPickup, 20) &&
                  drawBefore(kDrawEnemy, 100, kDrawEnemy, 200) && !drawBefore(kDrawProp, 10, kDrawEnemy, 900),
              "what it is comes before how far off it is");
    }

    void testPoints()
    {
        // A barnacle on a ceiling 400 units up, its tongue hung to 8 off
        // the floor: balls of its radius from the mouth to the tongue's end.
        ActorState b;
        b.kind = kind(ActorKind::Barnacle);
        b.pos[2] = fxFromInt(400);
        b.reach = fxFromInt(392);
        const DrawPoints hung = actorViewPoints(b, 2, 137);
        check(hung.count >= 3 && hung.at[0][2] == 400 && hung.at[hung.count - 1][2] == 8,
              "a barnacle is in view from its mouth to the end of its tongue");
        bool spaced = true;
        for (int i = 1; i < hung.count; ++i)
            spaced = spaced && hung.at[i - 1][2] - hung.at[i][2] <= 137 && hung.at[i][2] < hung.at[i - 1][2];
        check(spaced, "... with no gap between the balls");
        // Looking level from a head 64 up, 64 units off: the mouth's ball
        // is over the top of the view (39 degrees up), a ball lower down is
        // not. (The view's top plane: seen if h * cos39 - d * sin39 < r.)
        auto above = [](int height, int away, int radius)
        {
            return (height - 64) * 777 - away * 629 > radius * 1000;
        };
        check(above(hung.at[0][2], 64, 137), "its mouth alone is out of a level view under a high ceiling");
        bool some = false;
        for (int i = 0; i < hung.count; ++i)
            some = some || !above(hung.at[i][2], 64, 137);
        check(some, "... and its tongue is in it");
        // The longest tongue there is still ends where it ends.
        b.pos[2] = fxFromInt(1100);
        b.reach = fxFromInt(1024);
        const DrawPoints longest = actorViewPoints(b, 2, 137);
        check(longest.count == DrawPoints::kMost && longest.at[longest.count - 1][2] == 76,
              "a tongue of a thousand units is followed to its end");
        b.reach = 0;
        check(actorViewPoints(b, 2, 137).count == 1, "pulled right up, it is its mouth");
        // Someone standing: their middle and their head. A turret under a
        // ceiling: below where it is fixed.
        ActorState s;
        s.kind = kind(ActorKind::Scientist);
        s.pos[2] = fxFromInt(100);
        const DrawPoints stood = actorViewPoints(s, 72, 40);
        check(stood.count == 2 && stood.at[0][2] == 136 && stood.at[1][2] == 172, "someone standing: middle and head");
        s.kind = kind(ActorKind::Miniturret);
        s.flags = kActorCeiling;
        const DrawPoints fixed = actorViewPoints(s, 40, 30);
        check(fixed.count == 2 && fixed.at[0][2] == 80 && fixed.at[1][2] == 60, "a gun under a ceiling hangs below it");
    }

    void testWho()
    {
        // Eight props near, two headcrabs behind them, room for eight: the
        // headcrabs are drawn, and it is props that are not.
        {
            DrawCandidate all[10];
            all[0] = model(kDrawEnemy, 150);
            all[1] = model(kDrawEnemy, 150);
            for (int i = 2; i < 10; ++i)
                all[i] = model(kDrawProp, 60);
            const DrawPlan plan = planModels(all, 10, 8, DrawHeld(), 0, kShare, kShareVertices, kWhole);
            check(plan.drawn[0] && plan.drawn[1] && plan.cut[kDrawEnemy] == 0 && plan.cut[kDrawProp] == 2 &&
                      drawnOf(plan, all, 10, kDrawProp) == 6,
                  "with room for eight, two enemies and six of eight props");
        }
        // More enemies than the limit: all of them, up to the most there
        // can be; nothing else.
        {
            DrawCandidate all[14];
            for (int i = 0; i < 10; ++i)
                all[i] = model(kDrawEnemy, 60);
            for (int i = 10; i < 14; ++i)
                all[i] = model(kDrawPickup, 20);
            const DrawPlan plan = planModels(all, 14, 8, DrawHeld(), 0, kShare, kShareVertices, kWhole);
            check(drawnOf(plan, all, 14, kDrawEnemy) == 10 && plan.cut[kDrawEnemy] == 0 && plan.cut[kDrawPickup] == 4,
                  "ten enemies are ten drawn, whatever the limit says");
        }
        // The big revolver in hand (a DSi's: 1064 polygons) and three
        // soldiers: all three drawn. It used to be one.
        {
            DrawCandidate all[3] = {model(kDrawEnemy, 731, 4), model(kDrawEnemy, 731, 4, 1), model(kDrawEnemy, 731, 4, 2)};
            const DrawHeld held = weapon(1064, 340);
            const DrawPlan plan = planModels(all, 3, 12, held, 0, kShare, kShareVertices, kWhole);
            check(plan.drawn[0] && plan.drawn[1] && plan.drawn[2] && plan.cut[kDrawEnemy] == 0,
                  "three soldiers and the big revolver: three soldiers drawn");
            check(plan.heldLevel == 1 && plan.strained, "... the weapon in hand coarser for it, and the frame said to be full");
            check(plan.polygons <= kShare + kDrawGrowPolygons, "... within what the models may take at the very most");
        }
        // The same with the frame's passes full for a while: still three.
        for (int pressure = 0; pressure <= 3; ++pressure)
        {
            DrawCandidate all[5] = {model(kDrawEnemy, 349), model(kDrawEnemy, 349), model(kDrawPerson, 500),
                                    model(kDrawPickup, 40), model(kDrawProp, 300)};
            const DrawHeld held = weapon(345, 200);
            const DrawPlan plan = planModels(all, 5, 8, held, pressure, kShare, kShareVertices, kWhole);
            check(plan.drawn[0] && plan.drawn[1] && plan.drawn[2] && plan.cut[kDrawEnemy] == 0 && plan.cut[kDrawPerson] == 0,
                  "under pressure the enemies and the people are all still drawn");
            check(pressure == 0 || plan.level[1] > 0, "... the ones behind the nearest coarser for it");
        }
        // Room to spare: nobody is made coarser than their distance asks,
        // nothing is cut, nothing is said to be full.
        {
            DrawCandidate all[3] = {model(kDrawEnemy, 150), model(kDrawPickup, 30), model(kDrawProp, 80, 3, 1)};
            const DrawPlan plan = planModels(all, 3, 8, weapon(300, 200), 0, kShare, kShareVertices, kWhole);
            check(plan.drawn[0] && plan.drawn[1] && plan.drawn[2] && plan.level[0] == 0 && plan.level[2] == 1 &&
                      plan.heldLevel == 0 && !plan.strained && plan.grown == 0,
                  "with room to spare everything is as its distance asks");
            check(plan.polygons == 300 + 150 + 30 + 80 * 2 / 3, "... and the count is what was drawn");
        }
        // The game's own mesh (a DSi's finest) only for the nearest.
        {
            DrawCandidate all[2] = {model(kDrawEnemy, 700, 4), model(kDrawEnemy, 700, 4)};
            const DrawPlan plan = planModels(all, 2, 8, DrawHeld(), 0, 2000, 6000, kWhole);
            check(plan.level[0] == 0 && plan.level[1] == 1, "the game's own mesh is the nearest's alone");
        }
        // Enemies that take all there is: the props wait, and are counted.
        {
            DrawCandidate all[6];
            for (int i = 0; i < 4; ++i)
                all[i] = model(kDrawEnemy, 700, 3);
            all[4] = model(kDrawPickup, 30);
            all[5] = model(kDrawProp, 100);
            const DrawPlan plan = planModels(all, 6, 8, weapon(345, 200), 0, kShare, kShareVertices, kWhole);
            check(drawnOf(plan, all, 6, kDrawEnemy) == 4 && plan.cut[kDrawEnemy] == 0, "four big enemies: four drawn");
            check(plan.grown > 0 && plan.grown <= kDrawGrowPolygons && !plan.drawn[4] && !plan.drawn[5] &&
                      plan.cut[kDrawPickup] == 1 && plan.cut[kDrawProp] == 1,
                  "... out of the world's far end, with nothing spent on what matters less");
        }
        // More than can be had at all: it is a person who goes before an
        // enemy does, and the farthest of them.
        {
            DrawCandidate all[12];
            for (int i = 0; i < 9; ++i)
                all[i] = model(kDrawEnemy, 400, 2);
            for (int i = 9; i < 12; ++i)
                all[i] = model(kDrawPerson, 400, 2);
            const DrawPlan plan = planModels(all, 12, 12, DrawHeld(), 0, kShare, kShareVertices, kWhole);
            check(plan.cut[kDrawPerson] == 3 && plan.polygons <= kShare + kDrawGrowPolygons,
                  "when even that is not enough, the people go first");
            check(plan.drawn[0] && plan.drawn[1], "... and the nearest enemies never");
        }
    }
} // namespace

int main()
{
    testClasses();
    testPoints();
    testWho();
    if (g_failures == 0)
        std::printf("test_drawplan: ok\n");
    return g_failures == 0 ? 0 : 1;
}
