// What fights, and what it is fought with (shared/HlMonsters.hpp).
//
// The sums that need no map first: armour, what is picked up, the weapons'
// clips and reloads. Then, with HLDS_VALVE_DIR set, the game's own maps: the
// path nodes are cooked and joined up; a headcrab finds the player in the
// open and round corners, leaps and bites, and is never once inside a wall
// whatever the frame rate; a zombie walks up and hits; both die of being
// shot; a monstermaker makes.

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
    double toUnits(fx v) { return v / 4096.0; }

    void testSums()
    {
        PlayerState player;
        playerHurt(player, 20);
        check(player.health == 80, "with no armour a blow is taken whole");
        player.health = 100;
        player.inv.armor = 100;
        playerHurt(player, 20);
        check(player.health == 96 && player.inv.armor == 92,
              "armour takes four fifths, and loses half of what it takes");
        player.inv.armor = 3;
        player.health = 100;
        playerHurt(player, 20);
        check(player.inv.armor == 0 && player.health == 86, "armour that runs out takes what it can");

        // Picking up.
        player = PlayerState();
        std::uint32_t got = 0;
        check(hldata::detail::itemTake(player, static_cast<std::uint8_t>(ItemKind::Glock), got) &&
                  player.inv.current == kWeaponGlock && player.inv.clipGlock == 17 &&
                  (got & kGotWeapon),
              "a pistol picked up is loaded and in hand");
        check(hldata::detail::itemTake(player, static_cast<std::uint8_t>(ItemKind::Glock), got) &&
                  player.inv.ammo9mm == 17,
              "a second pistol is its rounds");
        check(!hldata::detail::itemTake(player, static_cast<std::uint8_t>(ItemKind::HealthKit), got),
              "a health kit is left lying by someone in full health");
        player.health = 90;
        check(hldata::detail::itemTake(player, static_cast<std::uint8_t>(ItemKind::HealthKit), got) &&
                  player.health == 100,
              "... and taken by someone who is not, no further than full");
        check(hldata::detail::itemTake(player, static_cast<std::uint8_t>(ItemKind::Crowbar), got) &&
                  weaponAfter(player.inv) == kWeaponGlock,
              "weapons come round in turn");

        // Firing, with nothing to hit: a world of nothing at all.
        LogicWorld logic;
        MoveWorld world;
        logic.world = &world;
        WeaponState weapon;
        player.inv.current = kWeaponGlock;
        const fx eye[3] = {0, 0, 0};
        const fx forward[3] = {kFxOne, 0, 0}, right[3] = {0, -kFxOne, 0}, up[3] = {0, 0, kFxOne};
        int shots = 0, reloads = 0;
        for (int ms = 0; ms < 16000; ms += 33)
        {
            const std::uint32_t did =
                weaponsTick(logic, player, weapon, eye, forward, right, up, true, false, 33);
            shots += (did & kWeaponFired) ? 1 : 0;
            reloads += (did & kWeaponReload) ? 1 : 0;
        }
        check(shots == 34 && reloads == 1 && player.inv.clipGlock == 0 && player.inv.ammo9mm == 0,
              "a pistol fires its clip, reloads once from what is carried, and is empty");
    }

    std::unique_ptr<Game> load(const char *dir, const char *map)
    {
        auto game = std::make_unique<Game>();
        std::string error;
        if (!game->bsp.load(std::string(dir) + "/maps/" + map + ".bsp", error))
            return nullptr;
        finish(*game, nullptr, dir);
        if (!game->ok)
            return nullptr;
        return game;
    }

    int findKind(const Game &g, ActorKind kind, int nth = 0)
    {
        for (int i = 0; i < g.actors.count; ++i)
        {
            const ActorState &actor = g.actors.states[i];
            if (actor.active && actor.kind == static_cast<std::uint8_t>(kind) && actor.script < 0 &&
                nth-- == 0)
                return i;
        }
        return -1;
    }

    bool inSolid(const Game &g, const ActorState &actor)
    {
        const bool small = monsterKind(actor.kind).small;
        const fx at[3] = {actor.pos[0], actor.pos[1],
                          actor.pos[2] + (small ? kCrouchedFeet : kStandingFeet)};
        const Hull &hull = small ? g.move.crouched : g.move.standing;
        return hullPointContents(hull, hull.root, at) == kContentsSolid;
    }

    /// Stand the player somewhere `actor` can walk to in a straight line,
    /// about `distance` units off. False if there is no such place.
    bool standNear(Game &g, int actor, int distance)
    {
        const ActorState &a = g.actors.states[actor];
        const bool small = monsterKind(a.kind).small;
        for (int turn = 0; turn < 16; ++turn)
        {
            fx sine, cosine;
            fxSinCos(static_cast<std::uint16_t>(turn * 4096), sine, cosine);
            ActorState probe;
            probe.pos[0] = a.pos[0] + cosine * distance;
            probe.pos[1] = a.pos[1] + sine * distance;
            probe.pos[2] = a.pos[2] + fxFromInt(8);
            if (!actorToFloor(g.move, probe, fxFromInt(8), fxFromInt(64)) ||
                !walkClear(g.move, &g.actors, actor, small, a.pos, probe.pos, distance / 16 + 4))
                continue;
            g.player.origin[0] = probe.pos[0];
            g.player.origin[1] = probe.pos[1];
            g.player.origin[2] = probe.pos[2] + kStandingFeet;
            g.player.velocity[0] = g.player.velocity[1] = g.player.velocity[2] = 0;
            return true;
        }
        return false;
    }

    void tick(Game &g, int vblanks)
    {
        const int ms = (vblanks * 1000 + 30) / 60;
        logicTick(g.logic, g.player, ms);
        g.logic.viewTurn = 0;
        monstersTick(g.logic, g.player, ms);
        for (int n = 0; n < vblanks; n += 2)
            playerMove(g.player, MoveCommand(), g.move, kPlayerTick);
        g.drain();
    }

    void testNodes(const char *dir)
    {
        auto game = load(dir, "c1a1");
        check(game != nullptr, "c1a1 cooks");
        if (!game)
            return;
        const int n = static_cast<int>(game->view.pathNodeCount);
        int joined = 0, ways = 0;
        for (int a = 0; a < n; ++a)
        {
            for (int b = 0; b < n; ++b)
            {
                if (a == b)
                    continue;
                const std::uint8_t next = game->view.pathNext[a * n + b];
                ways += next != kNoPathNode;
                joined += next == b;
            }
        }
        std::printf("test_monsters: c1a1 has %d nodes, %d links, %d of %d pairs joined (%d bytes)\n",
                    n, joined / 2, ways, n * (n - 1), n * n);
        check(n >= 15, "c1a1's path nodes are cooked");
        check(joined >= n, "... and joined to one another");
        // Following the table from anywhere to anywhere it says there is a
        // way gets there, by links only.
        bool arrives = true;
        for (int a = 0; a < n && arrives; ++a)
        {
            for (int b = 0; b < n && arrives; ++b)
            {
                if (game->view.pathNext[a * n + b] == kNoPathNode)
                    continue;
                int at = a, hops = 0;
                while (at != b && hops++ <= n)
                {
                    const int next = game->view.pathNext[at * n + b];
                    if (next == kNoPathNode || game->view.pathNext[at * n + next] != next)
                        break;
                    at = next;
                }
                arrives = at == b;
            }
        }
        check(arrives, "... and the table's ways all arrive");
    }

    void testHeadcrab(const char *dir, int vblanks)
    {
        auto game = load(dir, "c1a1f");
        if (!game)
        {
            check(false, "c1a1f cooks");
            return;
        }
        Game &g = *game;
        // Whichever of the map's headcrabs has room about it.
        int crab = -1;
        for (int nth = 0; nth < 8 && crab < 0; ++nth)
        {
            const int candidate = findKind(g, ActorKind::Headcrab, nth);
            if (candidate >= 0 && standNear(g, candidate, 160))
                crab = candidate;
        }
        check(crab >= 0, "a headcrab with room to be met in");
        if (crab < 0)
            return;
        ActorState &actor = g.actors.states[crab];
        // The others are not part of this.
        for (int i = 0; i < g.actors.count; ++i)
        {
            if (i != crab && isMonster(g.actors.states[i].kind))
                g.actors.states[i].active = false;
        }
        bool leapt = false, solid = false;
        int frames = 0;
        for (; frames < 600 && g.player.health == 100; ++frames)
        {
            tick(g, vblanks);
            leapt = leapt || actor.ai == kAiLeap;
            solid = solid || inSolid(g, actor);
        }
        char what[96];
        std::snprintf(what, sizeof(what), "at %d blanks a frame the headcrab sees, leaps and bites", vblanks);
        check(leapt && g.player.health == 90, what);
        check(g.times(CommonSound::HeadcrabAlert) == 1 && g.times(CommonSound::HeadcrabAttack) >= 1 &&
                  g.times(CommonSound::HeadcrabBite) == 1,
              "... and is heard: seeing them, leaping, biting");
        // It keeps at it; it is never in a wall.
        for (int n = 0; n < 1200 / vblanks; ++n)
        {
            tick(g, vblanks);
            solid = solid || inSolid(g, actor);
        }
        check(!solid, "... and is never inside the world");
        check(g.player.health < 90, "... and bites again");

        // Shot: one round of the pistol is not enough (8 of its 10), two are.
        g.player.health = 100;
        g.player.inv.have = 1u << kWeaponGlock;
        g.player.inv.current = kWeaponGlock;
        g.player.inv.clipGlock = 17;
        WeaponState weapon;
        int rounds = 0;
        for (int n = 0; n < 900 && actor.ai < kAiDying; ++n)
        {
            tick(g, vblanks);
            // Aimed at its middle, as someone good would.
            const fx eye[3] = {g.player.origin[0], g.player.origin[1],
                               g.player.origin[2] + g.player.viewHeight};
            fx to[3] = {actor.pos[0] - eye[0], actor.pos[1] - eye[1],
                        actor.pos[2] + fxFromInt(10) - eye[2]};
            const fx length = fxLength(to);
            if (length == 0)
                continue;
            for (fx &c : to)
                c = static_cast<fx>((static_cast<std::int64_t>(c) << kFxShift) / length);
            const fx right[3] = {to[1], -to[0], 0}, up[3] = {0, 0, kFxOne};
            const std::uint32_t did = weaponsTick(g.logic, g.player, weapon, eye, to, right, up, true,
                                                  false, (vblanks * 1000 + 30) / 60);
            rounds += (did & kWeaponHit) ? 1 : 0;
        }
        g.drain();
        check(actor.ai >= kAiDying && rounds == 2, "two rounds of the pistol kill it");
        check(g.times(CommonSound::Pistol) >= 2 && g.times(CommonSound::HeadcrabDie) == 1,
              "... the shots are heard, and its death");
        for (int n = 0; n < 200; ++n)
            tick(g, vblanks);
        check(actor.ai == kAiDead && !(actor.flags & kActorSolid),
              "... and it lies where it fell, in nobody's way");
    }

    void testRoundTheCorner(const char *dir)
    {
        auto game = load(dir, "c1a1f");
        if (!game)
            return;
        Game &g = *game;
        const int n = g.actors.nodeCount;
        // A headcrab and a node it cannot see but can get to, a few links off.
        int crab = -1, node = -1;
        for (int nth = 0; nth < 8 && node < 0; ++nth)
        {
            crab = findKind(g, ActorKind::Headcrab, nth);
            if (crab < 0)
                break;
            const ActorState &actor = g.actors.states[crab];
            const int mine = pathNearest(g.actors, g.move, actor.pos);
            if (mine < 0 || !walkClear(g.move, &g.actors, crab, true, actor.pos,
                                       g.actors.nodes[mine].pos, 48))
                continue;
            for (int b = 0; b < n && node < 0; ++b)
            {
                int hops = 0, at = mine;
                while (at != b && g.actors.next[at * n + b] != kNoPathNode && hops < n)
                {
                    at = g.actors.next[at * n + b];
                    ++hops;
                }
                const fx eye[3] = {actor.pos[0], actor.pos[1], actor.pos[2] + fxFromInt(20)};
                const fx there[3] = {g.actors.nodes[b].pos[0], g.actors.nodes[b].pos[1],
                                     g.actors.nodes[b].pos[2] + fxFromInt(64)};
                if (at == b && hops >= 2 && !lineClear(g.move, eye, there))
                    node = b;
            }
        }
        check(node >= 0, "a headcrab with a way to somewhere out of its sight");
        if (node < 0)
            return;
        ActorState &actor = g.actors.states[crab];
        for (int i = 0; i < g.actors.count; ++i)
        {
            if (i != crab && isMonster(g.actors.states[i].kind))
                g.actors.states[i].active = false;
        }
        for (int a = 0; a < 3; ++a)
            g.player.origin[a] = g.actors.nodes[node].pos[a];
        g.player.origin[2] += kStandingFeet;
        const fx d0[3] = {g.player.origin[0] - actor.pos[0], g.player.origin[1] - actor.pos[1], 0};
        const double start = toUnits(fxLength(d0));
        // It has not seen them. Stung, it comes looking.
        for (int f = 0; f < 60; ++f)
            tick(g, 2);
        check(actor.ai == kAiIdle, "what it cannot see it does not chase");
        tick(g, 2); // (its first tick has given it its health)
        monsterHurt(g.logic, crab, 1, false);
        bool solid = false;
        int frames = 0;
        for (; frames < 30 * 40 && g.player.health == 100; ++frames)
        {
            tick(g, 2);
            solid = solid || inSolid(g, actor);
        }
        const fx d1[3] = {g.player.origin[0] - actor.pos[0], g.player.origin[1] - actor.pos[1], 0};
        std::printf("test_monsters: round the corner from %.0f units: bitten after %.1f s (now %.0f off)\n",
                    start, frames / 30.0, toUnits(fxLength(d1)));
        check(g.player.health < 100 && !solid, "stung, it finds them by the nodes and bites");
    }

    void testZombie(const char *dir)
    {
        auto game = load(dir, "c1a1a");
        if (!game)
        {
            check(false, "c1a1a cooks");
            return;
        }
        Game &g = *game;
        int zombie = -1;
        for (int nth = 0; nth < 4 && zombie < 0; ++nth)
        {
            const int candidate = findKind(g, ActorKind::Zombie, nth);
            if (candidate >= 0 && standNear(g, candidate, 128))
                zombie = candidate;
        }
        check(zombie >= 0, "a zombie with room to be met in");
        if (zombie < 0)
            return;
        ActorState &actor = g.actors.states[zombie];
        for (int i = 0; i < g.actors.count; ++i)
        {
            if (i != zombie && isMonster(g.actors.states[i].kind))
                g.actors.states[i].active = false;
        }
        // It faces them, for the test's sake (a zombie with its back turned
        // hears only what is close).
        actor.yaw = fxAtan2Turns(g.player.origin[1] - actor.pos[1], g.player.origin[0] - actor.pos[0]);
        bool swung = false, solid = false;
        int frames = 0;
        for (; frames < 30 * 30 && g.player.health == 100; ++frames)
        {
            tick(g, 2);
            swung = swung || actor.ai == kAiAttack;
            solid = solid || inSolid(g, actor);
        }
        std::printf("test_monsters: the zombie hits after %.1f s, for %d\n", frames / 30.0,
                    100 - g.player.health);
        check(swung && (g.player.health == 80 || g.player.health == 60) && !solid,
              "the zombie walks up and hits, for what a zombie's blow is worth");
        check(g.times(CommonSound::ZombieAttack) >= 1 && g.times(CommonSound::ClawStrike) >= 1,
              "... and its swing and its blow are heard");
        // The crowbar: five blows of ten for its fifty.
        g.player.health = 100;
        g.player.inv.have = 1u << kWeaponCrowbar;
        g.player.inv.current = kWeaponCrowbar;
        WeaponState weapon;
        int blows = 0;
        for (int n = 0; n < 900 && actor.ai < kAiDying; ++n)
        {
            tick(g, 2);
            g.player.health = 100;
            const fx eye[3] = {g.player.origin[0], g.player.origin[1],
                               g.player.origin[2] + g.player.viewHeight};
            fx to[3] = {actor.pos[0] - eye[0], actor.pos[1] - eye[1], 0};
            const fx length = fxLength(to);
            if (length == 0)
                continue;
            for (fx &c : to)
                c = static_cast<fx>((static_cast<std::int64_t>(c) << kFxShift) / length);
            const fx right[3] = {to[1], -to[0], 0}, up[3] = {0, 0, kFxOne};
            const std::uint32_t did =
                weaponsTick(g.logic, g.player, weapon, eye, to, right, up, true, false, 33);
            blows += (did & kWeaponHit) ? 1 : 0;
        }
        check(actor.ai >= kAiDying && blows == 5, "five blows of the crowbar kill it");
    }

    void testMaker(const char *dir)
    {
        auto game = load(dir, "c1a0c");
        if (!game)
        {
            check(false, "c1a0c cooks");
            return;
        }
        Game &g = *game;
        const int maker = g.recordOfKind(LogicKind::Maker);
        check(maker >= 0, "c1a0c has a monstermaker");
        if (maker < 0)
            return;
        const LogicRecord &record = g.view.logic[maker];
        const int first = static_cast<int>(record.extra[0]);
        check(record.extra[1] >= 1 && !g.actors.states[first].active,
              "its children's places are kept, and empty");
        // Somewhere else, so as not to be stood where they are born.
        g.player.origin[0] += fxFromInt(4000);
        logicUse(g.logic, g.player, maker, -1, kUseToggle);
        for (int n = 0; n < 90; ++n)
        {
            logicTick(g.logic, g.player, 33);
            monstersTick(g.logic, g.player, 33);
        }
        check(g.actors.states[first].active && isMonster(g.actors.states[first].kind),
              "used, it makes one");
    }

    void testItems(const char *dir)
    {
        auto game = load(dir, "c1a1");
        if (!game)
            return;
        Game &g = *game;
        int crowbar = -1;
        for (int i = 0; i < g.actors.count; ++i)
        {
            if (g.actors.states[i].active &&
                g.actors.states[i].item == static_cast<std::uint8_t>(ItemKind::Crowbar))
                crowbar = i;
        }
        check(crowbar >= 0, "the crowbar lies in c1a1");
        if (crowbar < 0)
            return;
        check(g.player.inv.have == 0, "the player starts with nothing");
        for (int a = 0; a < 3; ++a)
            g.player.origin[a] = g.actors.states[crowbar].pos[a];
        g.player.origin[2] += kStandingFeet;
        monstersTick(g.logic, g.player, 33);
        check(!g.actors.states[crowbar].active && g.player.inv.current == kWeaponCrowbar &&
                  (g.actors.got & kGotWeapon),
              "walked over, it is theirs");
    }

    /// One of the newer kinds, met in the open in its own map: it must be
    /// roused, make the attack that is its own (heard as `attackSound`),
    /// hurt the player, never be inside the world, and die of being shot.
    void testKind(const char *dir, const char *map, ActorKind kind, const char *what,
                  CommonSound attackSound, int distance, bool wantShot, bool wantBolt)
    {
        auto game = load(dir, map);
        char text[128];
        std::snprintf(text, sizeof(text), "%s cooks", map);
        check(game != nullptr, text);
        if (!game)
            return;
        Game &g = *game;
        int who = -1;
        for (int nth = 0; nth < 12 && who < 0; ++nth)
        {
            const int candidate = findKind(g, kind, nth);
            if (candidate >= 0 && standNear(g, candidate, distance))
                who = candidate;
        }
        std::snprintf(text, sizeof(text), "%s: one with room to be met in", what);
        check(who >= 0, text);
        if (who < 0)
            return;
        ActorState &actor = g.actors.states[who];
        for (int i = 0; i < g.actors.count; ++i)
        {
            if (i != who && isMonster(g.actors.states[i].kind))
                g.actors.states[i].active = false;
        }
        actor.yaw = fxAtan2Turns(g.player.origin[1] - actor.pos[1], g.player.origin[0] - actor.pos[0]);
        bool attacked = false, solid = false, shot = false, bolt = false;
        int frames = 0;
        for (; frames < 30 * 40 && g.player.health == 100; ++frames)
        {
            tick(g, 2);
            attacked = attacked || actor.ai == kAiAttack;
            solid = solid || inSolid(g, actor);
            for (const Projectile &p : g.actors.shots)
                shot = shot || p.active;
            for (const Effect &e : g.actors.effects)
                bolt = bolt || e.kind == kEffectBolt;
        }
        std::printf("test_monsters: %s: hurts after %.1f s, for %d\n", what, frames / 30.0,
                    100 - g.player.health);
        std::snprintf(text, sizeof(text), "%s: is roused, attacks and hurts", what);
        check(attacked && g.player.health < 100 && !solid, text);
        std::snprintf(text, sizeof(text), "%s: its attack is heard", what);
        check(g.times(attackSound) >= 1, text);
        if (wantShot)
            check(shot, "... and what it spat was in the air on the way");
        if (wantBolt)
            check(bolt, "... and its bolt was there to be seen");

        // Shot until it drops: its health in pistol rounds, no more.
        const int needs = (monsterKind(actor.kind).health + 7) / 8;
        g.player.inv.have = 1u << kWeaponGlock;
        g.player.inv.current = kWeaponGlock;
        g.player.inv.clipGlock = 17;
        WeaponState weapon;
        int rounds = 0;
        for (int n = 0; n < 900 && actor.ai < kAiDying; ++n)
        {
            tick(g, 2);
            g.player.health = 100;
            const fx eye[3] = {g.player.origin[0], g.player.origin[1],
                               g.player.origin[2] + g.player.viewHeight};
            fx to[3] = {actor.pos[0] - eye[0], actor.pos[1] - eye[1],
                        actor.pos[2] + monsterKind(actor.kind).height / 2 - eye[2]};
            const fx length = fxLength(to);
            if (length == 0)
                continue;
            for (fx &c : to)
                c = static_cast<fx>((static_cast<std::int64_t>(c) << kFxShift) / length);
            const fx right[3] = {to[1], -to[0], 0}, up[3] = {0, 0, kFxOne};
            const std::uint32_t did =
                weaponsTick(g.logic, g.player, weapon, eye, to, right, up, true, false, 33);
            rounds += (did & kWeaponHit) ? 1 : 0;
        }
        g.drain();
        std::snprintf(text, sizeof(text), "%s: %d rounds of the pistol kill it", what, needs);
        check(actor.ai >= kAiDying && rounds == needs, text);
    }

    void testGrenade(const char *dir)
    {
        auto game = load(dir, "c1a1a");
        if (!game)
            return;
        Game &g = *game;
        int zombie = -1;
        for (int nth = 0; nth < 4 && zombie < 0; ++nth)
        {
            const int candidate = findKind(g, ActorKind::Zombie, nth);
            if (candidate >= 0 && standNear(g, candidate, 200))
                zombie = candidate;
        }
        check(zombie >= 0, "a zombie to throw a grenade at");
        if (zombie < 0)
            return;
        ActorState &actor = g.actors.states[zombie];
        for (int i = 0; i < g.actors.count; ++i)
        {
            if (i != zombie && isMonster(g.actors.states[i].kind))
                g.actors.states[i].active = false;
        }
        std::uint32_t got = 0;
        check(hldata::detail::itemTake(g.player, static_cast<std::uint8_t>(ItemKind::Grenades), got) &&
                  g.player.inv.grenades == 5 && (g.player.inv.have & (1u << kWeaponGrenade)),
              "grenades are picked up five at a time");
        g.player.inv.current = kWeaponGrenade;
        tick(g, 2); // (its first tick gives the zombie its health)
        // Thrown at its feet from where they stand; then they stand there.
        const fx eye[3] = {g.player.origin[0], g.player.origin[1], g.player.origin[2] + g.player.viewHeight};
        fx to[3] = {actor.pos[0] - eye[0], actor.pos[1] - eye[1], 0};
        const fx length = fxLength(to);
        for (fx &c : to)
            c = static_cast<fx>((static_cast<std::int64_t>(c) << kFxShift) / length);
        const fx right[3] = {to[1], -to[0], 0}, up[3] = {0, 0, kFxOne};
        WeaponState weapon;
        const std::uint32_t did = weaponsTick(g.logic, g.player, weapon, eye, to, right, up, true, false, 33);
        check((did & kWeaponFired) && g.player.inv.grenades == 4, "one is thrown");
        // The zombie is held where it is -- and, once the grenade has come
        // to rest, stood sixty units from it: this is about the grenade, and
        // where one lands after its bounces is not for a test to say.
        fx there[3] = {actor.pos[0], actor.pos[1], actor.pos[2]};
        bool flew = false, burst = false, solid = false;
        int burstFrame = -1;
        for (int frame = 0; frame < 30 * 5; ++frame)
        {
            g.player.health = 100;
            if (frame == 80)
            {
                for (const Projectile &p : g.actors.shots)
                {
                    if (!p.active)
                        continue;
                    there[0] = p.pos[0] + fxFromInt(60);
                    there[1] = p.pos[1];
                    there[2] = p.pos[2] - kCrouchedFeet;
                }
            }
            for (int a = 0; a < 3; ++a)
                actor.pos[a] = there[a];
            const int healthBefore = actor.health;
            tick(g, 2);
            for (const Projectile &p : g.actors.shots)
            {
                if (!p.active)
                    continue;
                flew = true;
                solid = solid || pointContents(g.move, p.pos) == kContentsSolid;
            }
            for (const Effect &e : g.actors.effects)
            {
                if (e.kind == kEffectBurst && burstFrame < 0)
                    burstFrame = frame;
            }
            burst = burst || (burstFrame >= 0 && actor.health < healthBefore) || actor.ai >= kAiDying;
        }
        fx burstAt[3] = {0, 0, 0};
        for (const Effect &e : g.actors.effects)
        {
            if (e.wholeMs == 350)
                for (int k = 0; k < 3; ++k)
                    burstAt[k] = e.a[k];
        }
        const fx off[3] = {burstAt[0] - there[0], burstAt[1] - there[1], burstAt[2] - there[2]};
        std::printf("test_monsters: the grenade bursts %.1f s after it is thrown, %d units from the zombie "
                    "(health %d, heard %d)\n",
                    burstFrame / 30.0, fxToInt(fxLength(off)), actor.health, g.times(CommonSound::Explode));
        check(flew && !solid, "it flies and bounces, and is never inside the world");
        check(burstFrame >= 85 && burstFrame <= 95, "... and bursts three seconds on");
        check(burst && g.times(CommonSound::Explode) == 1, "... which is heard, and hurts what is near");
    }

    void testSuit(const char *dir)
    {
        auto game = load(dir, "c1a0d");
        check(game != nullptr, "c1a0d cooks");
        if (!game)
            return;
        Game &g = *game;
        int suit = -1;
        for (int i = 0; i < g.actors.count; ++i)
        {
            if (g.actors.states[i].active &&
                g.actors.states[i].item == static_cast<std::uint8_t>(ItemKind::Suit))
                suit = i;
        }
        check(suit >= 0, "the suit stands in c1a0d");
        if (suit < 0)
            return;
        // What it is the key to: a multisource, off until it is taken.
        int master = -1;
        for (std::uint32_t i = 0; i < g.view.logicCount; ++i)
        {
            const LogicRecord &r = g.view.logic[i];
            if (r.kind == static_cast<std::uint8_t>(LogicKind::MultiSource) && r.name != 0 &&
                g.view.nameHashes[r.name] == nameHash("hevmaster1"))
                master = static_cast<int>(i);
        }
        check(master >= 0 && g.view.logic[master].auxCount >= 1, "... and something waits on it");
        if (master < 0)
            return;
        const std::uint32_t all = (1u << g.view.logic[master].auxCount) - 1u;
        check(g.logic.states[master].bits != all, "... which is off");
        // A battery does nothing for someone with no suit.
        std::uint32_t got = 0;
        check(!hldata::detail::itemTake(g.player, static_cast<std::uint8_t>(ItemKind::Battery), got),
              "a battery is no use without the suit");
        for (int a = 0; a < 3; ++a)
            g.player.origin[a] = g.actors.states[suit].pos[a];
        g.player.origin[2] += kStandingFeet;
        for (int n = 0; n < 5; ++n)
        {
            logicTick(g.logic, g.player, 33);
            monstersTick(g.logic, g.player, 33);
        }
        check(!g.actors.states[suit].active && (g.player.inv.have & kHaveSuit), "walked into, it is worn");
        check(g.logic.states[master].bits == all, "... and what waited on it is on");
        check(hldata::detail::itemTake(g.player, static_cast<std::uint8_t>(ItemKind::Battery), got) &&
                  g.player.inv.armor == 15,
              "... and a battery charges it");
    }
} // namespace

int main()
{
    testSums();
    const char *dir = std::getenv("HLDS_VALVE_DIR");
    if (dir != nullptr && dir[0] != '\0')
    {
        testNodes(dir);
        for (int vblanks = 1; vblanks <= 4; ++vblanks)
            testHeadcrab(dir, vblanks);
        testRoundTheCorner(dir);
        testZombie(dir);
        testMaker(dir);
        testItems(dir);
        testSuit(dir);
        testKind(dir, "c1a1b", ActorKind::Houndeye, "houndeye", CommonSound::HoundBlast, 160, false, false);
        testKind(dir, "c1a2a", ActorKind::Slave, "alien slave", CommonSound::SlaveZap, 300, false, true);
        testKind(dir, "c1a1c", ActorKind::Bullsquid, "bullsquid", CommonSound::SquidSpit, 320, true, false);
        testGrenade(dir);
    }
    else
    {
        std::printf("test_monsters: HLDS_VALVE_DIR not set, the maps were not played\n");
    }
    if (g_failures == 0)
        std::printf("test_monsters: ok\n");
    return g_failures == 0 ? 0 : 1;
}
