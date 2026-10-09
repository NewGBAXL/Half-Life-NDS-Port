// The early game's own machinery, on its own maps: a train that is turned
// (the tram's door), the turning lift, lights that are switched and that
// flicker (the level's LightPatch records), and the microwave's chain of
// buttons with everything it sets off.

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <set>
#include <string>

#include "HlLightStyle.hpp"
#include "TestGame.hpp"

using namespace hlds;
using namespace hldata;
using namespace hldstest;

namespace
{
    std::unique_ptr<Game> load(const char *dir, const char *map)
    {
        auto game = std::make_unique<Game>();
        std::string error;
        if (!game->bsp.load(std::string(dir) + "/maps/" + map + ".bsp", error))
            return nullptr;
        finish(*game, nullptr, dir);
        return game->ok ? std::move(game) : nullptr;
    }

    int nameOf(const Game &g, const char *name)
    {
        const std::uint32_t hash = nameHash(name);
        for (std::uint32_t i = 1; i < g.view.nameCount; ++i)
        {
            if (g.view.nameHashes[i] == hash)
                return static_cast<int>(i);
        }
        return 0;
    }

    void testStyles()
    {
        check(lightStyleValue(0, 5, 0) == kStyleWhole && lightStyleValue(40, 5, 0) == kStyleWhole &&
                  lightStyleValue(40, 5, std::uint64_t(1) << 40) == 0 && lightStyleValue(41, 5, std::uint64_t(1) << 40) == kStyleWhole,
              "plain light is whole; a switched light is all or nothing, each by its own switch");
        // (Style 4 is "mamamama...": on and off by turns. Style 2 swells
        // from dark to twice as bright and back.)
        check(lightStyleValue(4, 0, 0) == kStyleWhole && lightStyleValue(4, 1, 0) == 0 && lightStyleValue(4, 12, 0) == kStyleWhole &&
                  lightStyleValue(2, 0, 0) == 0 && lightStyleValue(2, 25, 0) > 500 && lightStyleValue(2, 51, 0) == 0,
              "the game's patterns play a letter each tenth of a second, round and round");
        std::uint16_t values[kLightStyles] = {};
        const bool first = lightStyleValues(0, 0, values);
        const bool same = lightStyleValues(0, 0, values);
        const bool flicker = lightStyleValues(1, 0, values);
        check(first && !same && flicker, "a change of any style is told, and no change is not");
        // A vertex: dim by itself, with a switched light adding to it.
        const LightPatch patches[] = {{2, 4 | 4 << 5 | 4 << 10, kPatchBase, 0},
                                      {2, 20 | 10 << 5 | 0 << 10, 40, 0},
                                      {5, 31 | 31 << 5 | 31 << 10, kPatchBase, 0},
                                      {5, 8 | 8 << 5 | 8 << 10, 4, 0}};
        std::uint32_t stream[8] = {};
        lightStyleValues(0, 0, values);
        lightPatchApply(patches, 4, values, stream);
        const bool lit = stream[2] == (24u | 14u << 5 | 4u << 10) && stream[5] == (31u | 31u << 5 | 31u << 10);
        lightStyleValues(1, std::uint64_t(1) << 40, values);
        lightPatchApply(patches, 4, values, stream);
        check(lit && stream[2] == (4u | 4u << 5 | 4u << 10) && stream[5] == (31u | 31u << 5 | 31u << 10) && stream[0] == 0,
              "a vertex is what it is without the changing light, plus what each adds at its strength; never past white");
    }

    void testTramDoor(const char *dir)
    {
        const std::unique_ptr<Game> game = load(dir, "c0a0e");
        check(game != nullptr, "the tram's last map cooks");
        if (game == nullptr)
            return;
        const int name = nameOf(*game, "traindoor");
        int door = -1;
        for (std::uint32_t i = 0; i < game->view.logicCount; ++i)
        {
            if (game->view.logic[i].name == name && game->view.logic[i].kind == static_cast<std::uint8_t>(LogicKind::Train))
                door = static_cast<int>(i);
        }
        check(door >= 0, "the tram's door is a train of its own");
        if (door < 0)
            return;
        const LogicRecord &record = game->view.logic[door];
        const BrushState &brush = game->brushStates[record.brush];
        check(record.extra[1] == 16384 && brush.angles[1] == 16384 && brush.angles[0] == 0 && brush.angles[2] == 0 &&
                  game->movers[record.brush].turned,
              "... and is a quarter turn round from how it was built, as the map says: drawn so, and solid so");
    }

    void testLift(const char *dir)
    {
        const std::unique_ptr<Game> game = load(dir, "c1a0b");
        check(game != nullptr, "the lift's map cooks");
        if (game == nullptr)
            return;
        Game &g = *game;
        const int lift = g.recordOfKind(LogicKind::PlatRot);
        check(lift >= 0, "the round lift is in the level");
        if (lift < 0)
            return;
        const LogicRecord &record = g.view.logic[lift];
        BrushState &brush = g.brushStates[record.brush];
        const fx z0 = brush.pos[2];
        const std::uint16_t yaw0 = brush.angles[1];
        std::printf("test_incident: the lift travels %d units and turns %d degrees; it starts turned %d\n",
                    fxToInt(record.move[2]), static_cast<int>(static_cast<std::int32_t>(record.extra[0])) * 360 / 65536,
                    yaw0 * 360 / 65536);
        check(record.move[2] == fxFromInt(216) && static_cast<std::int32_t>(record.extra[0]) == 16384 && yaw0 == 16384,
              "it is built turned a quarter, with 216 units to go");
        // Someone on it goes with it, and is turned as far.
        const MoverHull &mover = g.movers[record.brush];
        for (int a = 0; a < 2; ++a)
            g.player.origin[a] = (mover.mins[a] + mover.maxs[a]) / 2 + fxFromInt(a == 0 ? 20 : 0);
        g.player.origin[2] = mover.maxs[2] + fxFromInt(37);
        g.player.onGround = true;
        g.player.groundMover = static_cast<std::int16_t>(record.brush);
        const fx rider0[3] = {g.player.origin[0] - brush.pos[0], g.player.origin[1] - brush.pos[1], g.player.origin[2] - brush.pos[2]};
        logicUse(g.logic, g.player, lift, -1, kUseToggle);
        int turned = 0, ticks = 0;
        for (; ticks < 400 && g.states[lift].move != MoveState::AtTop; ++ticks)
        {
            g.player.onGround = true;
            g.player.groundMover = static_cast<std::int16_t>(record.brush);
            logicTick(g.logic, g.player, 33);
            turned += static_cast<std::int16_t>(g.logic.viewTurn);
            g.logic.viewTurn = 0;
        }
        const fx rider1[3] = {g.player.origin[0] - brush.pos[0], g.player.origin[1] - brush.pos[1], g.player.origin[2] - brush.pos[2]};
        std::printf("test_incident: after %d ticks it is %d units from where it began, turned %d; the rider's view turned %d\n",
                    ticks, fxToInt(brush.pos[2] - z0), brush.angles[1] * 360 / 65536, turned * 360 / 65536);
        check(g.states[lift].move == MoveState::AtTop && brush.pos[2] - z0 == fxFromInt(216) && brush.angles[1] == 0,
              "used, it travels its 216 units and turns its quarter");
        // (Where they stood on it, turned a quarter the same way; and no
        // nearer or farther from its floor.)
        const fx dz = rider1[2] - rider0[2];
        check(turned < -16000 && turned > -16800 && dz > -fxFromInt(2) && dz < fxFromInt(2) &&
                  std::abs(fxToInt(rider1[0])) + std::abs(fxToInt(rider1[1])) > 15,
              "... and whoever stands on it goes with it, turned as far");
        logicUse(g.logic, g.player, lift, -1, kUseToggle);
        for (int n = 0; n < 400 && g.states[lift].move != MoveState::AtBottom; ++n)
            logicTick(g.logic, g.player, 33);
        check(brush.pos[2] == z0 && brush.angles[1] == yaw0, "used again, it goes back");
    }

    void testLights(const char *dir)
    {
        const std::unique_ptr<Game> game = load(dir, "c1a0b");
        if (game == nullptr)
            return;
        Game &g = *game;
        const LevelView &v = g.view;
        int lights = 0, first = -1;
        std::set<int> styles;
        for (std::uint32_t i = 0; i < v.logicCount; ++i)
        {
            if (v.logic[i].kind != static_cast<std::uint8_t>(LogicKind::Light))
                continue;
            ++lights;
            styles.insert(static_cast<int>(v.logic[i].extra[0]));
            first = first < 0 ? static_cast<int>(i) : first;
        }
        std::set<int> patched;
        std::uint32_t bases = 0;
        bool ordered = true;
        for (std::uint32_t i = 0; i < v.lightPatchCount; ++i)
        {
            const LightPatch &p = v.lightPatches[i];
            if (p.style == kPatchBase)
            {
                ++bases;
                // (Each base is followed by at least one thing added.)
                ordered = ordered && i + 1 < v.lightPatchCount && v.lightPatches[i + 1].style != kPatchBase &&
                          v.lightPatches[i + 1].word == p.word;
            }
            else
            {
                patched.insert(p.style);
            }
        }
        std::printf("test_incident: c1a0b has %d switched lights of %zu styles; %u vertices in changing light (%u records)\n",
                    lights, styles.size(), bases, v.lightPatchCount);
        bool covered = true;
        for (int style : styles)
            covered = covered && patched.count(style) != 0;
        check(lights >= 5 && bases > 50 && ordered && covered,
              "the lights a switch works are in the level, and the vertices each reaches are known");
        if (first < 0)
            return;
        // Switch one: its vertices change, and change back.
        const int style = static_cast<int>(v.logic[first].extra[0]);
        std::vector<std::uint32_t> stream(v.gxStream, v.gxStream + v.gxWordCount);
        std::uint16_t values[kLightStyles] = {};
        logicTick(g.logic, g.player, 33);
        lightStyleValues(0, g.logic.stylesOff, values);
        lightPatchApply(v.lightPatches, v.lightPatchCount, values, stream.data());
        // (As cooked: what the stream has is what the same sum gives.)
        int differ = 0;
        for (std::uint32_t i = 0; i < v.gxWordCount; ++i)
            differ += stream[i] != v.gxStream[i];
        check(differ == 0, "what is cooked into the vertices is what the console's own sum gives as the map starts");
        // (By its name, as a switch does: every light of that name with it.)
        auto switchIt = [&]()
        {
            for (std::uint32_t i = 0; i < v.logicCount; ++i)
            {
                if (v.logic[i].kind == static_cast<std::uint8_t>(LogicKind::Light) && v.logic[i].name == v.logic[first].name)
                    logicUse(g.logic, g.player, static_cast<int>(i), -1, kUseToggle);
            }
        };
        const bool wasOff = (g.logic.stylesOff >> style) & 1;
        switchIt();
        logicTick(g.logic, g.player, 33);
        const bool nowOff = (g.logic.stylesOff >> style) & 1;
        lightStyleValues(0, g.logic.stylesOff, values);
        lightPatchApply(v.lightPatches, v.lightPatchCount, values, stream.data());
        int changed = 0, darker = 0;
        for (std::uint32_t i = 0; i < v.gxWordCount; ++i)
        {
            if (stream[i] == v.gxStream[i])
                continue;
            ++changed;
            darker += (stream[i] & 31) <= (v.gxStream[i] & 31);
        }
        std::printf("test_incident: switching style %d (%s) changes %d vertices\n", style, nowOff ? "off" : "on", changed);
        check(wasOff != nowOff && changed > 5 && (nowOff ? darker == changed : darker < changed),
              "a light switched changes the vertices it reaches: darker off, brighter on");
        switchIt();
        logicTick(g.logic, g.player, 33);
        lightStyleValues(0, g.logic.stylesOff, values);
        lightPatchApply(v.lightPatches, v.lightPatchCount, values, stream.data());
        differ = 0;
        for (std::uint32_t i = 0; i < v.gxWordCount; ++i)
            differ += stream[i] != v.gxStream[i];
        check(differ == 0, "... and switched back, they are as they were");
    }

    /// The microwave of c1a0d: five presses, and it goes.
    void testMicrowave(const char *dir)
    {
        const std::unique_ptr<Game> game = load(dir, "c1a0d");
        check(game != nullptr, "the microwave's map cooks");
        if (game == nullptr)
            return;
        Game &g = *game;
        const LevelView &v = g.view;
        for (int n = 0; n < 30; ++n)
            g.frame();
        // Its buttons: each fires the multisource that is the next one's
        // master (beep1ms .. beep5ms); the first has none.
        int marks = 0;
        for (std::uint32_t i = 0; i < v.logicCount; ++i)
            marks += v.logic[i].kind == static_cast<std::uint8_t>(LogicKind::Mark);
        check(marks >= 9, "the marks it leaves are in the level, waiting");
        auto live = [&]()
        {
            int count = 0;
            for (const Decal &decal : g.actors.decals)
                count += decal.active;
            return count;
        };
        const int before = live();
        int presses = 0;
        bool broke = false;
        const int sentences = g.sentencesHeard;
        for (int step = 1; step <= 5; ++step)
        {
            char name[16];
            std::snprintf(name, sizeof(name), "beep%dms", step);
            const int target = nameOf(g, name);
            int button = -1;
            for (std::uint32_t i = 0; i < v.logicCount; ++i)
            {
                if (v.logic[i].kind == static_cast<std::uint8_t>(LogicKind::Button) && v.logic[i].target == target && target != 0)
                    button = static_cast<int>(i);
            }
            if (button < 0)
            {
                std::printf("test_incident: no button fires %s\n", name);
                continue;
            }
            ++presses;
            logicUse(g.logic, g.player, button, -1, kUseSet);
            for (int n = 0; n < 60; ++n)
            {
                logicTick(g.logic, g.player, 33);
                broke = broke || g.logic.brokeCount > 0;
                g.logic.brokeCount = 0;
                g.drain();
            }
        }
        std::printf("test_incident: the microwave: %d of 5 buttons pressed, %d marks left, %s, %d sentences\n", presses,
                    live() - before, broke ? "the stew burst" : "nothing burst", g.sentencesHeard - sentences);
        check(presses == 5, "its five buttons are there to press");
        check(live() - before >= 3, "after the fifth press its marks are on the wall");
        check(broke, "... and what was in it has burst");
    }
} // namespace

int main()
{
    testStyles();
    const char *dir = std::getenv("HLDS_VALVE_DIR");
    if (dir != nullptr && dir[0] != '\0')
    {
        testTramDoor(dir);
        testLift(dir);
        testLights(dir);
        testMicrowave(dir);
    }
    else
    {
        std::printf("test_incident: HLDS_VALVE_DIR not set, the game's maps were not read\n");
    }
    if (g_failures == 0)
        std::printf("test_incident: ok\n");
    return g_failures == 0 ? 0 : 1;
}
