// A guard with something to fight (shared/HlPeople.hpp), after the game's
// own: he draws before he fires, fires only when he faces it with nothing
// in the way, keeps the enemy he has, goes to where it went, and stands as
// he stood afterwards; what a scene has, or the map keeps caged, he leaves
// alone; and the player's shots turn him by the game's rule. On the real
// c1a1, with its guard and its zombie stood where the test wants them.

#include <cstdio>
#include <cstdlib>
#include <memory>
#include <algorithm>
#include <string>
#include <vector>

#include "TestGame.hpp"

using namespace hlds;
using namespace hldata;
using namespace hldstest;

namespace
{
    struct Scene
    {
        std::unique_ptr<Game> game;
        int guard = -1, monster = -1, other = -1;
        fx here[3] = {}, there[3] = {}, hidden[3] = {}, away[3] = {};
        bool ok = false;
    };

    void put(ActorState &actor, const fx *at)
    {
        for (int k = 0; k < 3; ++k)
            actor.pos[k] = at[k];
        actor.script = -1;
        actor.scriptPhase = kActorFree;
        actor.frozen = false;
        actor.flags &= static_cast<std::uint8_t>(~(kActorPrisoner | kActorDormant));
        actor.groundMover = -1;
        actor.airborne = false;
    }

    /// The map, its guard at one node and a monster at another he can see
    /// from there a few hundred units off; a third node he cannot see.
    Scene scene(const char *dir)
    {
        Scene s;
        s.game = std::make_unique<Game>();
        std::string error;
        if (!s.game->bsp.load(std::string(dir) + "/maps/c1a1.bsp", error))
            return s;
        finish(*s.game, nullptr, dir);
        if (!s.game->ok)
            return s;
        Game &g = *s.game;
        ActorWorld &a = g.actors;
        for (int i = 0; i < a.count; ++i)
        {
            const ActorState &actor = a.states[i];
            if (!actor.active)
                continue;
            if (actor.kind == static_cast<std::uint8_t>(ActorKind::Guard) && s.guard < 0)
                s.guard = i;
            else if (actor.kind == static_cast<std::uint8_t>(ActorKind::Zombie) && s.monster < 0)
                s.monster = i;
            else if (isMonster(actor.kind) && actor.kind != static_cast<std::uint8_t>(ActorKind::Barnacle) && s.other < 0 &&
                     i != s.monster)
                s.other = i;
        }
        if (s.guard < 0 || s.monster < 0)
            return s;
        // Two nodes in sight of each other, 300 to 500 units apart on one
        // floor; and one within 600 of the second that the first cannot see.
        auto eye = [&](int n, fx *out)
        {
            for (int k = 0; k < 3; ++k)
                out[k] = a.nodes[n].pos[k] + (k == 2 ? fxFromInt(50) : 0);
        };
        for (int x = 0; x < a.nodeCount && !s.ok; ++x)
        {
            for (int y = 0; y < a.nodeCount && !s.ok; ++y)
            {
                const fx d[3] = {a.nodes[y].pos[0] - a.nodes[x].pos[0], a.nodes[y].pos[1] - a.nodes[x].pos[1], 0};
                const fx gap = fxLength(d);
                const fx dz = a.nodes[y].pos[2] - a.nodes[x].pos[2];
                fx ex[3], ey[3];
                eye(x, ex);
                eye(y, ey);
                if (gap < fxFromInt(300) || gap > fxFromInt(500) || dz > fxFromInt(8) || dz < -fxFromInt(8) ||
                    !lineClear(g.move, ex, ey))
                    continue;
                for (int z = 0; z < a.nodeCount && !s.ok; ++z)
                {
                    const fx e[3] = {a.nodes[z].pos[0] - a.nodes[y].pos[0], a.nodes[z].pos[1] - a.nodes[y].pos[1], 0};
                    fx ez[3];
                    eye(z, ez);
                    if (fxLength(e) > fxFromInt(600) || lineClear(g.move, ex, ez) ||
                        pathStep(a, x, z, kPathLinkHuman, true) == kNoPathNode)
                        continue;
                    for (int k = 0; k < 3; ++k)
                    {
                        s.here[k] = a.nodes[x].pos[k];
                        s.there[k] = a.nodes[y].pos[k];
                        s.hidden[k] = a.nodes[z].pos[k];
                    }
                    s.ok = true;
                }
            }
        }
        // (The player somewhere none of this is seen from: far above.)
        s.away[0] = s.here[0];
        s.away[1] = s.here[1];
        s.away[2] = s.here[2] + fxFromInt(3000);
        return s;
    }

    /// The guard at his node, the monster at its own and standing there:
    /// everything else that fights put out of it.
    void stage(Scene &s, int turnedBy)
    {
        Game &g = *s.game;
        ActorWorld &a = g.actors;
        for (int i = 0; i < a.count; ++i)
        {
            if (i != s.guard && i != s.monster && (isMonster(a.states[i].kind) || isGrunt(a.states[i].kind)))
                a.states[i].active = false;
        }
        ActorState &guard = a.states[s.guard];
        put(guard, s.here);
        guard.ai = kPersonIdle;
        guard.foe = -1;
        guard.gun = 0;
        guard.body = 0;
        guard.struck = 0;
        guard.cooldownMs = guard.lookMs = guard.lostMs = guard.painMs = 0;
        guard.wasFollowing = false;
        guard.health = 0;
        guard.yaw = static_cast<std::uint16_t>(fxAtan2Turns(s.there[1] - s.here[1], s.there[0] - s.here[0]) + turnedBy);
        ActorState &monster = a.states[s.monster];
        monster.active = true;
        put(monster, s.there);
        monster.ai = kAiIdle;
        monster.health = 0;
        g.player.health = 100;
        for (int k = 0; k < 3; ++k)
            g.player.origin[k] = s.away[k];
        g.heard.count = 0;
    }

    /// A tick, with the player held where the test has them and the monster
    /// where it stands (this is about the guard).
    void tick(Scene &s, const fx *player, const fx *monster)
    {
        Game &g = *s.game;
        for (int k = 0; k < 3; ++k)
        {
            g.player.origin[k] = player[k];
            g.player.velocity[k] = 0;
        }
        g.player.health = 100;
        if (monster != nullptr && g.actors.states[s.monster].active && g.actors.states[s.monster].ai < kAiDying)
        {
            for (int k = 0; k < 3; ++k)
                g.actors.states[s.monster].pos[k] = monster[k];
        }
        logicTick(g.logic, g.player, 33);
        monstersTick(g.logic, g.player, 33);
        g.drain();
    }

    bool faces(const ActorState &me, const fx *at, int tenths)
    {
        fx sine = 0, cosine = 0;
        fxSinCos(me.yaw, sine, cosine);
        const fx d[3] = {at[0] - me.pos[0], at[1] - me.pos[1], 0};
        const fx far = fxLength(d);
        const std::int64_t ahead = static_cast<std::int64_t>(d[0]) * cosine + static_cast<std::int64_t>(d[1]) * sine;
        return far > 0 && ahead * 10 >= static_cast<std::int64_t>(far) * kFxOne * tenths;
    }

    void testFight(const char *dir)
    {
        Scene s = scene(dir);
        check(s.ok, "c1a1 has a guard, a zombie and somewhere to stand them");
        if (!s.ok)
            return;
        Game &g = *s.game;
        ActorState &guard = g.actors.states[s.guard];
        ActorState &monster = g.actors.states[s.monster];

        // Stood side on to it: he turns, draws, and only then fires -- at
        // what he faces.
        stage(s, 16384);
        int shots0 = g.times(CommonSound::Pistol), firstShot = -1;
        bool drew = false, facedAtShot = false, gunAtShot = false;
        for (int t = 0; t < 3000 && firstShot < 0; t += 33)
        {
            tick(s, s.away, s.there);
            drew = drew || guard.gun == 1;
            if (g.times(CommonSound::Pistol) > shots0)
            {
                firstShot = t;
                facedAtShot = faces(guard, s.there, 5);
                gunAtShot = guard.gun == 2 && guard.body == 1;
            }
        }
        std::printf("test_people: the guard's first round is %d ms after he sees it\n", firstShot);
        check(drew && firstShot >= 800, "a guard draws before he fires");
        check(facedAtShot && gunAtShot, "... and fires facing it, his pistol in his hand");
        check(guard.foe == s.monster && guard.ai == kPersonIdle, "it is his enemy; he is still the standing guard he was");

        // A round at a time, no faster than his firing clips.
        shots0 = g.times(CommonSound::Pistol);
        const int health0 = monster.health;
        for (int t = 0; t < 3000; t += 33)
            tick(s, s.away, s.there);
        const int fired = g.times(CommonSound::Pistol) - shots0;
        std::printf("test_people: %d rounds in three seconds; the zombie has taken %d\n", fired, health0 - monster.health);
        check(fired >= 4 && fired <= 8, "he fires a round to each firing clip: four to eight in three seconds");
        check(monster.health < health0, "... and they are rounds: what they hit is hurt");

        // The player in the way: he holds his fire. Out of it: he fires.
        const fx between[3] = {(s.here[0] + s.there[0]) / 2, (s.here[1] + s.there[1]) / 2, s.here[2] + fxFromInt(36)};
        for (int t = 0; t < 600; t += 33)
            tick(s, between, s.there);
        shots0 = g.times(CommonSound::Pistol);
        const int mine = g.player.health;
        for (int t = 0; t < 2000; t += 33)
            tick(s, between, s.there);
        check(g.times(CommonSound::Pistol) == shots0 && guard.foe == s.monster,
              "with the player in the line of fire he does not fire");
        (void)mine;
        for (int t = 0; t < 1500; t += 33)
            tick(s, s.away, s.there);
        check(g.times(CommonSound::Pistol) > shots0, "... and does again when they are out of it");

        // It goes where he cannot see it: he keeps it, and goes to where
        // he saw it last.
        const fx far0[3] = {s.there[0] - guard.pos[0], s.there[1] - guard.pos[1], 0};
        const int gap0 = fxToInt(fxLength(far0));
        shots0 = g.times(CommonSound::Pistol);
        int blind = 0;
        for (int t = 0; t < 2500; t += 33)
        {
            tick(s, s.away, s.hidden);
            // (While he still stands where he cannot see it from.)
            if (t == 594)
                blind = g.times(CommonSound::Pistol) - shots0;
        }
        const fx far1[3] = {s.there[0] - guard.pos[0], s.there[1] - guard.pos[1], 0};
        std::printf("test_people: hidden from him, he goes from %d to %d units of where it was\n", gap0, fxToInt(fxLength(far1)));
        check(guard.foe == s.monster && fxToInt(fxLength(far1)) < gap0 - 100, "hidden, it is still his enemy, and he goes after it");
        check(blind <= 1, "... without firing at what he cannot see");

        // Dead: he has no enemy, and stands as he stands.
        stage(s, 0);
        for (int t = 0; t < 1500; t += 33)
            tick(s, s.away, s.there);
        monsterHurt(g.logic, s.monster, 1000, true);
        for (int t = 0; t < 2000; t += 33)
            tick(s, s.away, nullptr);
        const ModelView *model = actorModel(g.actors, guard);
        const int idle = model != nullptr ? model->findClip(nameHash("idle1")) : -1;
        check(guard.foe < 0 && idle >= 0 && guard.clip == idle && !guard.once,
              "his enemy dead, a standing guard stands as he does, not as his last shot left him");
        check(guard.gun == 2, "... his pistol still out, as the game's guard keeps his");

        // What a scene has, and what the map keeps caged, he leaves alone.
        stage(s, 0);
        monster.script = 1;
        for (int t = 0; t < 2000; t += 33)
        {
            tick(s, s.away, s.there);
            monster.script = 1;
        }
        check(guard.foe < 0 && guard.gun == 0, "a monster a scene is playing is not his to shoot");
        stage(s, 0);
        monster.flags |= kActorPrisoner;
        for (int t = 0; t < 2000; t += 33)
            tick(s, s.away, s.there);
        check(guard.foe < 0 && guard.gun == 0, "nor one the map keeps caged");

        // Behind him and not near: he does not see it. (The game's wide
        // view leaves a quarter turn behind him.)
        stage(s, 32768);
        tick(s, s.away, s.there);
        check(guard.foe < 0, "what is straight behind him he does not see at once");

        // The player's rounds, by the game's rule. In a fight: a stray one.
        stage(s, 0);
        for (int t = 0; t < 1500; t += 33)
            tick(s, s.away, s.there);
        const fx from[3] = {guard.pos[0] + fxFromInt(30), guard.pos[1], guard.pos[2] + fxFromInt(40)};
        const fx at[3] = {-kFxOne, 0, 0};
        const bool stray = fireLine(g.logic, g.player, from, at, 400, 1, true);
        fireLine(g.logic, g.player, from, at, 400, 1, true);
        check(stray && guard.struck == 0, "shot by the player in the middle of a fight, he takes it for a stray round");
        // With nothing to fight: aimed at, an enemy at once -- who follows nobody.
        stage(s, 0);
        g.actors.states[s.monster].active = false;
        guard.ai = kPersonFollow;
        const bool hitHim = fireLine(g.logic, g.player, from, at, 400, 1, true);
        std::printf("test_people: aimed at: hit %d, he is at %d, following %d\n", hitHim, guard.struck, guard.ai);
        check(guard.struck == 2 && guard.ai == kPersonIdle, "aimed at with no enemy about, he turns on the player and follows no longer");
        // A round that was not aimed at him makes him wary; the next an enemy.
        stage(s, 0);
        g.actors.states[s.monster].active = false;
        // (From thirty units off, twenty-one degrees wide of his middle:
        // it catches the edge of him.)
        const fx skew[3] = {-3824, 1468, 0};
        const fx beside[3] = {guard.pos[0] + fxFromInt(30), guard.pos[1], guard.pos[2] + fxFromInt(40)};
        fireLine(g.logic, g.player, beside, skew, 400, 1, true);
        const int wary = guard.struck;
        fireLine(g.logic, g.player, beside, skew, 400, 1, true);
        std::printf("test_people: a round not aimed at him leaves him at %d, a second at %d\n", wary, guard.struck);
        check(wary == 1 && guard.struck == 2, "a round that was not aimed at him makes him wary; a second, an enemy");

        // He cries out no more than twice a second, however often he is hit.
        stage(s, 0);
        g.actors.states[s.monster].active = false;
        const int cries = g.times(CommonSound::GuardPain);
        for (int n = 0; n < 6; ++n)
            monsterHurt(g.logic, s.guard, 1, true);
        g.drain();
        check(g.times(CommonSound::GuardPain) == cries + 1, "six hits at once are one cry");
    }

    // HLDS_SWEEP=<maps, comma separated>: each map run for four seconds
    // with nobody near, and what became of its people said -- who fell, who
    // died of a place that hurts. For seeing that the rule that people fall
    // and are hurt has not cost a map someone it needs.
    void sweep(const char *dir, const char *maps)
    {
        std::string list(maps);
        for (std::size_t at = 0; at < list.size();)
        {
            const std::size_t comma = std::min(list.find(',', at), list.size());
            const std::string map = list.substr(at, comma - at);
            at = comma + 1;
            auto game = std::make_unique<Game>();
            std::string error;
            if (!game->bsp.load(std::string(dir) + "/maps/" + map + ".bsp", error))
                continue;
            finish(*game, nullptr, dir);
            if (!game->ok)
                continue;
            Game &g = *game;
            std::vector<fx> startZ;
            std::vector<bool> alive;
            int people = 0;
            for (int i = 0; i < g.actors.count; ++i)
            {
                const ActorState &actor = g.actors.states[i];
                startZ.push_back(actor.pos[2]);
                alive.push_back(actor.active && actor.ai < kAiDying);
                people += actor.active && isPerson(actor.kind) ? 1 : 0;
            }
            g.player.origin[2] += fxFromInt(6000);
            for (int n = 0; n < 120; ++n)
            {
                logicTick(g.logic, g.player, 33);
                monstersTick(g.logic, g.player, 33);
                g.heard.count = 0;
            }
            int fell = 0, died = 0, onBrush = 0;
            for (int i = 0; i < g.actors.count; ++i)
            {
                const ActorState &actor = g.actors.states[i];
                if (!isPerson(actor.kind))
                    continue;
                fell += startZ[i] - actor.pos[2] > fxFromInt(8) ? 1 : 0;
                died += alive[i] && !(actor.active && actor.ai < kAiDying) ? 1 : 0;
                onBrush += actor.groundMover >= 0 ? 1 : 0;
            }
            std::printf("sweep %-8s people %2d on a brush %2d fell %2d died %2d\n", map.c_str(), people, onBrush, fell, died);
        }
    }
} // namespace

int main()
{
    const char *dir = std::getenv("HLDS_VALVE_DIR");
    if (dir != nullptr && dir[0] != '\0' && std::getenv("HLDS_SWEEP") != nullptr)
    {
        sweep(dir, std::getenv("HLDS_SWEEP"));
        return 0;
    }
    if (dir != nullptr && dir[0] != '\0')
        testFight(dir);
    else
        std::printf("test_people: HLDS_VALVE_DIR not set, the game's maps were not read\n");
    if (g_failures == 0)
        std::printf("test_people: ok\n");
    return g_failures == 0 ? 0 : 1;
}
