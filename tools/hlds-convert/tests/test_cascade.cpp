// The resonance cascade (c1a0e), played with nobody at the controls.
//
// The map's last minute is a chain: multi_managers switch on multisources,
// each of which is the master of a teleport and starts the next
// multi_manager, until the last of them changes the level. The player does
// nothing but stand where the first teleport will be. If any link of the
// chain is not understood, they are left in a room of black and green for
// good -- which is how this test came to be written.
//
// Needs HLDS_VALVE_DIR.

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <memory>

#include "TestGame.hpp"

using namespace hlds;
using namespace hldata;
using namespace hldstest;

namespace
{
    int recordNamed(const Game &g, const char *name, LogicKind kind)
    {
        const std::uint32_t hash = nameHash(name);
        for (std::uint32_t i = 0; i < g.view.logicCount; ++i)
        {
            const LogicRecord &r = g.view.logic[i];
            if (r.kind == static_cast<std::uint8_t>(kind) && r.name != 0 &&
                g.view.nameHashes[r.name] == hash)
                return static_cast<int>(i);
        }
        return -1;
    }

    void testCascade(const char *dir)
    {
        auto game = std::make_unique<Game>();
        std::string error;
        if (!game->bsp.load(std::string(dir) + "/maps/c1a0e.bsp", error))
        {
            check(false, "c1a0e is read");
            return;
        }
        finish(*game, nullptr, dir);
        check(game->ok, "c1a0e cooks");
        if (!game->ok)
            return;
        Game &g = *game;

        const int start = recordNamed(g, "probe_arm_mm", LogicKind::MultiManager);
        const int first = recordNamed(g, "killtele1", LogicKind::TriggerTeleport);
        check(start >= 0 && first >= 0, "the cascade's start and its first teleport are there");
        if (start < 0 || first < 0)
            return;
        // In the middle of the first teleport, on its floor.
        const LogicRecord &tele = g.view.logic[first];
        ActorState probe;
        probe.pos[0] = fxFromInt((tele.mins[0] + tele.maxs[0]) / 2);
        probe.pos[1] = fxFromInt((tele.mins[1] + tele.maxs[1]) / 2);
        probe.pos[2] = fxFromInt(tele.mins[2] + 40);
        actorToFloor(g.move, probe, fxFromInt(8), fxFromInt(256));
        const fx home[3] = {probe.pos[0], probe.pos[1], probe.pos[2] + kStandingFeet};
        for (int a = 0; a < 3; ++a)
            g.player.origin[a] = home[a];

        logicUse(g.logic, g.player, start, -1, kUseToggle);
        int teleports = 0;
        int frame = 0;
        fx last[3] = {home[0], home[1], home[2]};
        for (; frame < 30 * 150 && g.logic.pendingChange < 0; ++frame)
        {
            // Nothing in the chamber is allowed to kill them: this is about
            // the chain, and they would not stand in the beam.
            g.player.health = 100;
            logicTick(g.logic, g.player, 33);
            g.logic.viewTurn = 0;
            g.logic.faceSet = false;
            monstersTick(g.logic, g.player, 33);
            playerMove(g.player, MoveCommand(), g.move, kPlayerTick);
            g.drain();
            const fx d[3] = {g.player.origin[0] - last[0], g.player.origin[1] - last[1],
                             g.player.origin[2] - last[2]};
            if (fxLength(d) > fxFromInt(200))
            {
                ++teleports;
                std::printf("test_cascade: %5.1f s: to %d %d %d\n", frame / 30.0,
                            fxToInt(g.player.origin[0]), fxToInt(g.player.origin[1]),
                            fxToInt(g.player.origin[2]));
            }
            for (int a = 0; a < 3; ++a)
                last[a] = g.player.origin[a];
        }
        check(teleports >= 2, "the player is taken through the rooms");
        int sounds = 0;
        for (int n : g.timesHeard)
            sounds += n;
        std::printf("test_cascade: %d sounds and %d spoken lines were heard on the way\n", sounds,
                    g.sentencesHeard);
        check(sounds >= 10 && g.sentencesHeard >= 3, "... to the sound of it, and of the announcer");
        const bool changed = g.logic.pendingChange >= 0;
        check(changed, "... and the chain ends in the change of level");
        if (changed)
        {
            const char *next = g.view.strings + g.view.logic[g.logic.pendingChange].extra[0];
            std::printf("test_cascade: %.1f s after the start, on to %s\n", frame / 30.0, next);
            check(std::string(next) == "c1a0c", "... to c1a0c");
        }
    }

    /// The machine itself: its button is locked until the announcer has had
    /// his say, and then it starts the rotor turning.
    void testMachine(const char *dir)
    {
        auto game = std::make_unique<Game>();
        std::string error;
        if (!game->bsp.load(std::string(dir) + "/maps/c1a0e.bsp", error))
            return;
        finish(*game, nullptr, dir);
        if (!game->ok)
            return;
        Game &g = *game;
        const int begin = recordNamed(g, "portal_begin_mm", LogicKind::MultiManager);
        const int rotor = recordNamed(g, "p_1", LogicKind::Rotating);
        int button = -1;
        for (std::uint32_t i = 0; i < g.view.logicCount; ++i)
        {
            const LogicRecord &r = g.view.logic[i];
            if (r.kind == static_cast<std::uint8_t>(LogicKind::Button) && r.target != 0 &&
                g.view.nameHashes[r.target] == nameHash("motorstartmm"))
                button = static_cast<int>(i);
        }
        check(begin >= 0 && rotor >= 0 && button >= 0, "the machine, its rotor and its button are there");
        if (begin < 0 || rotor < 0 || button < 0)
            return;
        g.player.origin[2] += fxFromInt(4000);
        auto run = [&](int frames)
        {
            for (int n = 0; n < frames; ++n)
            {
                logicTick(g.logic, g.player, 33);
                g.drain();
            }
        };
        run(30);
        logicUse(g.logic, g.player, button, -1, kUseSet);
        run(90);
        check(g.logic.states[rotor].speed == 0, "its button does nothing before the test is under way");
        logicUse(g.logic, g.player, begin, -1, kUseToggle);
        run(30 * 37);
        logicUse(g.logic, g.player, button, -1, kUseSet);
        run(30 * 6);
        std::printf("test_cascade: the rotor turns at %d (of %d) six seconds after its button\n",
                    static_cast<int>(g.logic.states[rotor].speed), static_cast<int>(g.view.logic[rotor].speed));
        check(g.logic.states[rotor].speed != 0, "... and once it is, starts the rotor");
    }

    /// The sample cart: it comes up on a lift, and is pushed into the beam,
    /// which is what starts it all.
    void testCart(const char *dir)
    {
        auto game = std::make_unique<Game>();
        std::string error;
        if (!game->bsp.load(std::string(dir) + "/maps/c1a0e.bsp", error))
            return;
        finish(*game, nullptr, dir);
        if (!game->ok)
            return;
        Game &g = *game;
        const int cart = g.recordOfKind(LogicKind::Pushable);
        const int lift = recordNamed(g, "sample_cart2_lift", LogicKind::Door);
        const int cover = recordNamed(g, "sample_delivery_door1", LogicKind::Door);
        const int start = recordNamed(g, "probe_arm_mm", LogicKind::MultiManager);
        check(cart >= 0 && lift >= 0 && cover >= 0 && start >= 0, "the cart, its lift and its hatch are there");
        if (cart < 0 || lift < 0 || cover < 0 || start < 0)
            return;
        const int cartBrush = g.view.logic[cart].brush, liftBrush = g.view.logic[lift].brush;
        auto run = [&](int frames)
        {
            for (int n = 0; n < frames; ++n)
            {
                logicTick(g.logic, g.player, 33);
                g.drain();
            }
        };
        // Out of the way: this is the cart's business.
        g.player.origin[2] += fxFromInt(4000);
        auto bottom = [&]() { return g.movers[cartBrush].mins[2]; };
        auto liftTop = [&]() { return g.movers[liftBrush].maxs[2]; };
        run(60);
        const fx down = bottom();
        check(down - liftTop() < fxFromInt(2) && down - liftTop() > -fxFromInt(2),
              "the cart waits on its lift, under the floor");
        // The hatch opens and the lift comes up, as the map has them do.
        logicUse(g.logic, g.player, cover, -1, kUseToggle);
        run(90);
        logicUse(g.logic, g.player, lift, -1, kUseToggle);
        run(30 * 12);
        std::printf("test_cascade: the cart comes up %d units on its lift\n", fxToInt(bottom() - down));
        check(bottom() - down > fxFromInt(100) && bottom() - liftTop() < fxFromInt(2) &&
                  bottom() - liftTop() > -fxFromInt(2),
              "... and comes up with it");
        // The fence comes down (every piece of it has the one name), and the
        // cart is shoved towards the machine.
        for (std::uint32_t i = 0; i < g.view.logicCount; ++i)
        {
            const LogicRecord &r = g.view.logic[i];
            if (r.name != 0 && g.view.nameHashes[r.name] == nameHash("sample_fence") &&
                r.kind == static_cast<std::uint8_t>(LogicKind::Door))
            {
                logicUse(g.logic, g.player, static_cast<int>(i), -1, kUseToggle);
                break; // (one use reaches them all only through a name; this is one of them)
            }
        }
        const std::uint16_t fence = g.view.logic[recordNamed(g, "sample_fence", LogicKind::Door)].name;
        logicFire(g.logic, g.player, fence, -1, kUseToggle, 0);
        run(30 * 4);
        bool started = false;
        for (int frame = 0; frame < 30 * 20 && !started; ++frame)
        {
            g.logic.states[cart].travel[0] = fxFromInt(60); // someone pushing, steadily
            logicTick(g.logic, g.player, 33);
            g.drain();
            started = g.logic.states[start].running;
        }
        std::printf("test_cascade: pushed, the cart is at %d %d when the machine takes it\n",
                    fxToInt((g.movers[cartBrush].mins[0] + g.movers[cartBrush].maxs[0]) / 2),
                    fxToInt((g.movers[cartBrush].mins[1] + g.movers[cartBrush].maxs[1]) / 2));
        check(started, "pushed into the beam, the cart starts the cascade");
    }
} // namespace

int main()
{
    const char *dir = std::getenv("HLDS_VALVE_DIR");
    if (dir != nullptr && dir[0] != '\0')
    {
        testCascade(dir);
        testMachine(dir);
        testCart(dir);
    }
    else
        std::printf("test_cascade: HLDS_VALVE_DIR not set, the map was not played\n");
    if (g_failures == 0)
        std::printf("test_cascade: ok\n");
    return g_failures == 0 ? 0 : 1;
}
