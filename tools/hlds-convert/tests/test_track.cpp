// The tram: a train on a track of path_tracks (shared/HlLogic.hpp), on a
// synthetic track and then on the real thing.
//
// The synthetic track checks the rules one at a time: a train that waits to be
// started, a node that sets its speed, a node that fires something as it is
// passed, the end of the track and what is fired there, and which way the
// train faces.
//
// With HLDS_VALVE_DIR set, the opening tram ride is run from end to end with
// nobody at the controls -- six maps, five level changes, two platforms that
// lower the tram from one track to another -- and has to finish with the tram
// at rest at the station and the player still inside it. Nothing draws; a
// minute of the ride takes a fraction of a second.

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>

#include "HlLevelReader.hpp"
#include "HlTransition.hpp"
#include "TestBspBuilder.hpp"
#include "TestGame.hpp"
#include "hlds/Cooker.hpp"
#include "hlds/EntityCook.hpp"
#include "hlds/EntityParser.hpp"
#include "hlds/MdlFile.hpp"
#include "hlds/ModelCook.hpp"

using namespace hlds;
using namespace hldata;
using namespace hldstest;

namespace
{
    double toUnits(fx v) { return v / 4096.0; }

    void testSyntheticTrack()
    {
        // A track of four nodes along the room, a little above the floor, and
        // a small car on it. Node b sets the speed, node c fires a door, and
        // the track ends at d, which fires another.
        BoxRoom room;
        room.mins[0] = -400, room.maxs[0] = 400;
        BrushBox car;
        car.className = "func_tracktrain";
        car.mins[0] = -20, car.mins[1] = -10, car.mins[2] = -4;
        car.maxs[0] = 20, car.maxs[1] = 10, car.maxs[2] = 4;
        car.keys = "\"targetname\" \"car\"\n\"origin\" \"0 0 0\"\n\"target\" \"a\"\n"
                   "\"speed\" \"100\"\n\"height\" \"8\"\n\"wheels\" \"30\"\n\"spawnflags\" \"2\"\n";
        room.brushes.push_back(car);
        BrushBox door;
        door.className = "func_door";
        door.mins[0] = 300, door.mins[1] = 100, door.mins[2] = 0;
        door.maxs[0] = 310, door.maxs[1] = 120, door.maxs[2] = 64;
        door.keys = "\"targetname\" \"passing\"\n\"angle\" \"-1\"\n\"wait\" \"-1\"\n";
        room.brushes.push_back(door);
        BrushBox last = door;
        last.mins[0] = 330, last.maxs[0] = 340;
        last.keys = "\"targetname\" \"arrived\"\n\"angle\" \"-1\"\n\"wait\" \"-1\"\n";
        room.brushes.push_back(last);
        room.extraEntities =
            "{\n\"classname\" \"path_track\"\n\"targetname\" \"a\"\n\"target\" \"b\"\n"
            "\"origin\" \"-300 0 16\"\n}\n"
            "{\n\"classname\" \"path_track\"\n\"targetname\" \"b\"\n\"target\" \"c\"\n"
            "\"origin\" \"-200 0 16\"\n\"speed\" \"200\"\n}\n"
            "{\n\"classname\" \"path_track\"\n\"targetname\" \"c\"\n\"target\" \"d\"\n"
            "\"origin\" \"0 0 16\"\n\"message\" \"passing\"\n}\n"
            "{\n\"classname\" \"path_track\"\n\"targetname\" \"d\"\n"
            "\"origin\" \"200 0 16\"\n\"netname\" \"arrived\"\n}\n";
        Game g;
        std::string error;
        g.ok = g.bsp.parse(buildBoxRoom(room), error);
        if (g.ok)
            finish(g, nullptr);
        check(g.ok, "track room cooks");
        if (!g.ok)
            return;
        const int train = g.recordOfKind(LogicKind::TrackTrain);
        check(train >= 0 && g.view.cornerCount == 4, "a track train and its four nodes");
        if (train < 0)
            return;
        const LogicRecord &rec = g.view.logic[train];
        check(rec.speed == fxFromInt(100) && rec.extra[0] == static_cast<std::uint32_t>(fxFromInt(30)) &&
                  rec.extra[1] == static_cast<std::uint32_t>(fxFromInt(8)) &&
                  (rec.flags & kLogicSelfDriven) &&
                  (g.view.brushes[rec.brush].flags & kBrushRoams),
              "cooked with its speed, wheels and height, and marked as one that roams");
        // Out of the way, on the floor.
        g.player.origin[0] = fxFromInt(-380);
        g.player.origin[1] = fxFromInt(100);
        g.player.origin[2] = fxFromInt(40);

        const BrushState &car0 = g.brushStates[rec.brush];
        check(std::fabs(toUnits(car0.pos[0]) + 300) < 0.01 && std::fabs(toUnits(car0.pos[2]) - 24) < 0.01,
              "it starts on its first node, its height above the track");
        // Heading +x, and "the train points west": its yaw is half a turn.
        check(car0.angles[kYaw] == 32768, "facing along the track, back to front as the game has it");
        for (int i = 0; i < 30; ++i)
            g.frame();
        check(std::fabs(toUnits(car0.pos[0]) + 300) < 0.01, "and waits to be started");

        logicFire(g.logic, g.player, rec.name, -1, kUseToggle, 0);
        for (int i = 0; i < 30; ++i) // one second at 100
            g.frame();
        const double afterOne = toUnits(car0.pos[0]);
        check(afterOne > -210 && afterOne < -190, "started, it covers its speed in a second");
        for (int i = 0; i < 15; ++i) // half a second more: past b, at 200
            g.frame();
        const double afterMore = toUnits(car0.pos[0]);
        check(afterMore > afterOne + 85 && afterMore < afterOne + 115,
              "a node with a speed sets the speed of a train nobody drives");
        check(toUnits(g.brushStates[g.view.logic[g.recordOfKind(LogicKind::Door, 0)].brush].pos[2]) < 0.01,
              "nothing is fired before its node is reached");
        for (int i = 0; i < 20; ++i)
            g.frame();
        check(toUnits(g.brushStates[g.view.logic[g.recordOfKind(LogicKind::Door, 0)].brush].pos[2]) > 1,
              "a node's message fires as the train passes it");
        check(toUnits(g.brushStates[g.view.logic[g.recordOfKind(LogicKind::Door, 1)].brush].pos[2]) < 0.01,
              "and the end's does not, yet");
        for (int i = 0; i < 60; ++i)
            g.frame();
        check(std::fabs(toUnits(car0.pos[0]) - 200) < 0.5 && g.states[train].speed == 0,
              "it comes to rest on the last node");
        check(toUnits(g.brushStates[g.view.logic[g.recordOfKind(LogicKind::Door, 1)].brush].pos[2]) > 1,
              "which fires what the end of the track names");
        check(std::fabs(toUnits(car0.pos[1])) < 0.01 && std::fabs(toUnits(car0.pos[2]) - 24) < 0.01,
              "having kept to the track all the way");
    }

    /// How the passenger behaves on a ride.
    enum class Passenger
    {
        Idle,    ///< never touches the controls
        Restless ///< walks into every wall in turn, jumps, ducks
    };

    /// Whether the player is where a passenger should be: inside the car's
    /// own box, taken in the car's own space (it turns), and not inside what
    /// it is made of.
    bool aboard(const Game &game, int brush, double &worst)
    {
        const Brush &built = game.view.brushes[brush];
        const MoverHull &hull = game.movers[brush];
        fx local[3];
        hldata::detail::toMoverSpace(hull, game.player.origin, local);
        bool inside = true;
        for (int a = 0; a < 3; ++a)
        {
            const fx lo = fxFromInt(built.mins[a]) - built.origin[a];
            const fx hi = fxFromInt(built.maxs[a]) - built.origin[a];
            if (local[a] < lo || local[a] > hi)
            {
                inside = false;
                worst = std::max(worst, toUnits(local[a] < lo ? lo - local[a] : local[a] - hi));
            }
        }
        return inside &&
               hullPositionFree(game.player.ducked ? hull.crouched : hull.standing, local);
    }

    /// The opening ride, from the first map to the station, with `vblanks`
    /// sixtieths of a second to a frame. Returns false -- having said where --
    /// at the first frame the passenger is not aboard.
    bool ride(const char *dir, int vblanks, Passenger passenger, bool saveHalfWay)
    {
        Globals globals;
        std::string map = "c0a0";
        std::string route = map;
        auto game = std::make_unique<Game>();
        std::string error;
        if (!game->bsp.load(std::string(dir) + "/maps/" + map + ".bsp", error))
            return true;
        finish(*game, &globals);
        if (!game->ok)
            return false;

        const int ms = (vblanks * 1000 + 30) / 60;
        const std::int32_t dt = 1092 * vblanks; // sixtieths of a second, Q16
        const char *who = passenger == Passenger::Idle ? "idle" : "restless";
        std::uint16_t yaw = 0;
        long elapsed = 0;
        int restMs = 0;
        bool wasAboard = false;
        for (int frame = 0; elapsed < 12L * 60 * 1000; ++frame, elapsed += ms)
        {
            // What the passenger does: three seconds each of walking forwards,
            // left, backwards and right as they see it, a jump at the start of
            // each second and a duck through the last of every four.
            MoveCommand cmd;
            if (passenger == Passenger::Restless)
            {
                const long second = elapsed / 1000;
                const int phase = static_cast<int>((second / 3) % 4);
                cmd.forward = phase == 0 ? 1 : (phase == 2 ? -1 : 0);
                cmd.side = phase == 1 ? -1 : (phase == 3 ? 1 : 0);
                cmd.jump = elapsed % 1000 < 100;
                cmd.duck = second % 4 == 3;
            }
            fx sine, cosine;
            fxSinCos(yaw, sine, cosine);
            cmd.forwardXY[0] = cosine, cmd.forwardXY[1] = sine;
            cmd.rightXY[0] = sine, cmd.rightXY[1] = -cosine;
            cmd.viewForward[0] = cosine, cmd.viewForward[1] = sine, cmd.viewForward[2] = 0;
            cmd.viewRight[0] = sine, cmd.viewRight[1] = -cosine, cmd.viewRight[2] = 0;

            logicTick(game->logic, game->player, ms);
            yaw = static_cast<std::uint16_t>(yaw + game->logic.viewTurn);
            game->logic.viewTurn = 0;
            playerMove(game->player, cmd, game->move, dt);
            // A move can end a hair inside a corner; the next one begins by
            // edging out of it. The passenger is judged as the next move will
            // find them.
            playerSettle(game->player, game->move);

            const int train = game->tram();
            const int brush = train >= 0 ? game->view.logic[train].brush : -1;
            double out = 0;
            const bool on = brush >= 0 && aboard(*game, brush, out);
            // The restless one starts by walking about; they count as aboard
            // from the first frame they are, and must stay so.
            if (wasAboard && !on)
            {
                fx local[3];
                hldata::detail::toMoverSpace(game->movers[brush], game->player.origin, local);
                std::printf("test_track: %d ms steps, %s passenger: off the tram in %s after %ld s "
                            "(in its space %.1f %.1f %.1f, %.1f outside, yaw %.1f, speed %.0f)\n",
                            ms, who, map.c_str(), elapsed / 1000, toUnits(local[0]),
                            toUnits(local[1]), toUnits(local[2]), out,
                            game->brushStates[brush].angles[kYaw] * 360.0 / 65536.0,
                            toUnits(game->states[train].speed));
                return false;
            }
            wasAboard = wasAboard || on;

            // A save and a load in the middle of the third map: everything is
            // torn down and put back from what a save holds.
            if (saveHalfWay && map == "c0a0b" && frame % 4096 == 2048 && brush >= 0)
            {
                Arrival held;
                held.valid = true;
                // (The save keeps absolute places; an Arrival against a
                // landmark at the origin is the same thing.)
                const BrushState car = game->brushStates[brush];
                const fx carSpeed = game->states[train].speed;
                const bool driven = game->states[train].running;
                const std::uint16_t node = static_cast<std::uint16_t>(game->states[train].bits);
                const PlayerState player = game->player;
                auto again = std::make_unique<Game>();
                if (!again->bsp.load(std::string(dir) + "/maps/" + map + ".bsp", error))
                    return false;
                finish(*again, &globals);
                const int t2 = again->tram();
                logicPlaceBrush(again->logic, again->view.logic[t2].brush, car.pos,
                                car.angles[kYaw]);
                again->states[t2].speed = carSpeed;
                again->states[t2].running = driven;
                const std::uint16_t near = hldata::detail::trackNearest(
                    again->logic, car.pos, static_cast<fx>(again->view.logic[t2].extra[1]));
                again->states[t2].bits = near != kNoCorner ? near : node;
                again->player = player;
                again->player.onGround = true;
                again->player.groundMover = static_cast<std::int16_t>(again->view.logic[t2].brush);
                game = std::move(again);
                saveHalfWay = false;
                continue;
            }

            if (game->logic.pendingChange >= 0)
            {
                const int record = game->logic.pendingChange;
                const Arrival arrival = makeArrival(game->logic, game->view.landmarks,
                                                    game->view.landmarkCount, game->player, record);
                map = game->view.strings + game->view.logic[record].extra[0];
                route += " " + map;
                auto next = std::make_unique<Game>();
                if (!next->bsp.load(std::string(dir) + "/maps/" + map + ".bsp", error))
                    return false;
                finish(*next, &globals);
                if (!next->ok)
                    return false;
                applyArrival(next->logic, next->view.landmarks, next->view.landmarkCount,
                             next->view.nameHashes, next->view.nameCount, next->player, arrival);
                game = std::move(next);
                continue;
            }
            if (map == "c0a0e")
            {
                const int t = game->tram();
                restMs = (t >= 0 && game->states[t].speed == 0) ? restMs + ms : 0;
                if (restMs > 5000)
                    break;
            }
        }
        if (route != "c0a0 c0a0a c0a0b c0a0c c0a0d c0a0e")
        {
            std::printf("test_track: %d ms steps, %s passenger: the ride went %s\n", ms, who,
                        route.c_str());
            return false;
        }
        const int train = game->tram();
        const LogicRecord &rec = game->view.logic[train];
        const BrushState &car = game->brushStates[rec.brush];
        const PathCorner &station = game->view.corners[rec.firstAux];
        const double off = std::fabs(toUnits(car.pos[0] - station.pos[0])) +
                           std::fabs(toUnits(car.pos[1] - station.pos[1])) +
                           std::fabs(toUnits(car.pos[2] - station.pos[2] - static_cast<fx>(rec.extra[1])));
        if (off > 2)
        {
            std::printf("test_track: %d ms steps, %s passenger: the tram stops %.1f from the station\n",
                        ms, who, off);
            return false;
        }
        return true;
    }

    /// The end of the ride with its cast: the guard who waits at the station
    /// is in the map before it, comes along when the tram crosses, walks up
    /// and works the lock -- and the tram's door opens BECAUSE he has.
    void testTheGuard(const char *dir)
    {
        Globals globals;
        auto game = std::make_unique<Game>();
        std::string error;
        if (!game->bsp.load(std::string(dir) + "/maps/c0a0d.bsp", error))
            return;
        finish(*game, &globals, dir);
        check(game->ok, "c0a0d cooks with its models");
        if (!game->ok)
            return;
        std::string map = "c0a0d";
        int inE = 0;
        bool sawGuard = false, walked = false, doorMoved = false;
        fx guardStart[3] = {0, 0, 0}, doorStart[3] = {0, 0, 0};
        int door = -1;
        for (int frame = 0; frame < 30 * 60 * 4 && inE < 30 * 40; ++frame)
        {
            logicTick(game->logic, game->player, 33);
            game->logic.viewTurn = 0;
            playerMove(game->player, MoveCommand(), game->move, kPlayerTick);
            if (game->logic.pendingChange >= 0)
            {
                const int record = game->logic.pendingChange;
                const Arrival arrival =
                    makeArrival(game->logic, game->view.landmarks, game->view.landmarkCount,
                                game->player, record, game->view.modelRefs);
                map = game->view.strings + game->view.logic[record].extra[0];
                int coming = 0;
                for (const Arrival::Visitor &visitor : arrival.visitors)
                    coming += visitor.model != 0;
                std::printf("test_track: into %s with %d actors coming along\n", map.c_str(), coming);
                auto next = std::make_unique<Game>();
                if (!next->bsp.load(std::string(dir) + "/maps/" + map + ".bsp", error))
                    return;
                finish(*next, &globals, dir);
                applyArrival(next->logic, next->view.landmarks, next->view.landmarkCount,
                             next->view.nameHashes, next->view.nameCount, next->player, arrival,
                             next->view.modelRefs);
                game = std::move(next);
                for (std::uint32_t i = 0; i < game->view.logicCount; ++i)
                {
                    // The tram's door is the one func_train of the map.
                    if (game->view.logic[i].kind == static_cast<std::uint8_t>(LogicKind::Train))
                        door = game->view.logic[i].brush;
                }
                if (door >= 0)
                {
                    for (int a = 0; a < 3; ++a)
                        doorStart[a] = game->brushStates[door].pos[a];
                }
                continue;
            }
            if (map != "c0a0e")
                continue;
            ++inE;
            for (const ActorState &actor : game->actorStates)
            {
                if (!actor.active || actor.nameHash != nameHash("barney1"))
                    continue;
                if (!sawGuard)
                {
                    sawGuard = true;
                    for (int a = 0; a < 3; ++a)
                        guardStart[a] = actor.pos[a];
                }
                const fx d[3] = {actor.pos[0] - guardStart[0], actor.pos[1] - guardStart[1], 0};
                if (fxLength(d) > fxFromInt(100) && !doorMoved)
                    walked = true;
            }
            if (door >= 0)
            {
                const fx d[3] = {game->brushStates[door].pos[0] - doorStart[0],
                                 game->brushStates[door].pos[1] - doorStart[1],
                                 game->brushStates[door].pos[2] - doorStart[2]};
                if (fxLength(d) > fxFromInt(4))
                {
                    if (!doorMoved)
                        std::printf("test_track: the tram's door opens %d s after arriving\n", inE / 30);
                    doorMoved = true;
                }
            }
        }
        check(map == "c0a0e", "the tram gets from c0a0d to the station");
        check(sawGuard, "the guard comes along from the map before");
        check(walked, "he walks to the tram before its door moves");
        check(doorMoved, "and then the door opens");
    }

    /// The lift between c1a3d and c1a3a (a func_train known by the same
    /// name in both): ridden up from one map into the next, it carries the
    /// player on to the top of the next -- it does not start again at the
    /// bottom of the shaft, from under their feet.
    /// The lift, by its own name; and with `asOld`, every train's global
    /// name forgotten, as in a map converted before it was kept.
    int liftOf(Game &g, bool asOld)
    {
        int lift = -1;
        for (std::uint32_t i = 0; i < g.view.logicCount; ++i)
        {
            auto &record = const_cast<LogicRecord &>(g.view.logic[i]);
            if (record.kind != static_cast<std::uint8_t>(LogicKind::Train))
                continue;
            if (asOld)
                record.extra[3] = 0;
            if (record.name != 0 && g.view.nameHashes[record.name] == nameHash("c1a3_lift01"))
                lift = static_cast<int>(i);
        }
        return lift;
    }

    void testTheLift(const char *dir, bool asOld)
    {
        Globals globals;
        auto game = std::make_unique<Game>();
        std::string error;
        if (!game->bsp.load(std::string(dir) + "/maps/c1a3d.bsp", error))
            return;
        finish(*game, &globals);
        const int lift = game->ok ? liftOf(*game, asOld) : -1;
        check(lift >= 0 && (asOld || game->view.logic[lift].extra[3] != 0),
              "c1a3d's lift is there (known by a global name, in a map converted now)");
        if (lift < 0)
            return;
        // Stood on it, in the middle, and it is set going.
        {
            const MoverHull &hull = game->movers[game->view.logic[lift].brush];
            game->player.origin[0] = (hull.mins[0] + hull.maxs[0]) / 2;
            game->player.origin[1] = (hull.mins[1] + hull.maxs[1]) / 2;
            game->player.origin[2] = hull.maxs[2] + fxFromInt(37);
        }
        for (int f = 0; f < 30; ++f)
            game->frame();
        logicUse(game->logic, game->player, lift, -1, kUseToggle);
        bool crossed = false;
        for (int f = 0; f < 60 * 30 && !crossed; ++f)
        {
            game->frame();
            if (game->logic.pendingChange < 0)
                continue;
            const int record = game->logic.pendingChange;
            const Arrival arrival =
                makeArrival(game->logic, game->view.landmarks, game->view.landmarkCount, game->player, record);
            const std::string map = game->view.strings + game->view.logic[record].extra[0];
            check(map == "c1a3a", "the lift's level change is to c1a3a");
            auto next = std::make_unique<Game>();
            if (!next->bsp.load(std::string(dir) + "/maps/" + map + ".bsp", error))
                return;
            finish(*next, &globals);
            liftOf(*next, asOld);
            applyArrival(next->logic, next->view.landmarks, next->view.landmarkCount, next->view.nameHashes,
                         next->view.nameCount, next->player, arrival);
            game = std::move(next);
            crossed = true;
        }
        check(crossed, "ridden up, the lift takes the player into c1a3a");
        if (!crossed)
            return;
        const int up = liftOf(*game, asOld);
        check(up >= 0 && (asOld || (game->view.brushes[game->view.logic[up].brush].flags & kBrushRoams)),
              "c1a3a's lift is there, and (converted now) seen wherever it goes");
        // (The lift that came is that one: on its way up.)
        check(up >= 0 && game->states[up].move == MoveState::GoingUp, "the lift that came up is c1a3a's lift");
        const fx arrivedAt = game->player.origin[2];
        double worstBelow = 0;
        bool backDown = false;
        for (int f = 0; f < 20 * 30; ++f)
        {
            game->frame();
            backDown = backDown || game->logic.pendingChange >= 0;
            if (up >= 0)
            {
                const MoverHull &hull = game->movers[game->view.logic[up].brush];
                worstBelow = std::max(worstBelow, toUnits(hull.maxs[2] - (game->player.origin[2] - fxFromInt(36))));
            }
        }
        check(up >= 0 && worstBelow < 4, "in c1a3a the player stands on the lift all the way");
        check(!backDown && game->player.health > 0, "... and is not sent back down to c1a3d");
        check(game->player.origin[2] - arrivedAt > fxFromInt(250), "... which carries them on up to the top");
    }

    void testTheRide()
    {
        const char *dir = std::getenv("HLDS_VALVE_DIR");
        if (dir == nullptr || *dir == '\0')
        {
            std::printf("test_track: HLDS_VALVE_DIR not set, the real ride is skipped\n");
            return;
        }
        // Every frame rate the console runs at
        // (60 a second; 30, 20 and 15 of a frame that ran long)
        // with a passenger who stands still and one who will not.
        for (int vblanks = 1; vblanks <= 4; ++vblanks)
        {
            check(ride(dir, vblanks, Passenger::Idle, false),
                  "the tram reaches the station with a passenger who stands still");
            check(ride(dir, vblanks, Passenger::Restless, false),
                  "... and with one who walks about, jumps and ducks all the way");
        }
        check(ride(dir, 2, Passenger::Idle, true), "... and with a save and a load on the way");
        testTheGuard(dir);
        testTheLift(dir, false);
        testTheLift(dir, true); // (maps from a converter before the lift's global name was kept)
    }
} // namespace

int main()
{
    testSyntheticTrack();
    testTheRide();
    if (g_failures == 0)
        std::printf("test_track: ok\n");
    return g_failures == 0 ? 0 : 1;
}
