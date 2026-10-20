// The COMMAND line (shared/HlCommands.hpp) and the cheats it switches on:
// what a typed line is read as, by the game's own names; and, with
// HLDS_VALVE_DIR set, on the game's maps, that god keeps the player whole,
// that notarget is let alone, that a barnacle grabbing someone with
// barnaclekill on dies of it, that killall kills (each in its own death),
// and that sv_gravity pulls as hard as it says.

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>

#include "HlCommands.hpp"
#include "TestGame.hpp"

using namespace hldata;
using namespace hldstest;

namespace
{
    void testParse()
    {
        CommandLine line = parseCommand("  god ");
        check(line.command == Command::God && line.argc == 0, "a word alone, spaces round it");
        line = parseCommand("map c1a0");
        check(line.command == Command::Map && line.argc == 1 && std::strcmp(line.args[0], "c1a0") == 0,
              "a word and what it is given");
        check(parseCommand("changelevel c1a1").command == Command::Map, "changelevel is map");
        check(parseCommand("impulse 101").command == Command::GiveAll, "impulse 101 is everything");
        check(parseCommand("impulse 100").command == Command::Flashlight, "impulse 100 the flashlight");
        check(parseCommand("impulse 7").command == Command::Unknown, "... and no other impulse is anything");
        check(parseCommand("give").command == Command::GiveAll, "give alone gives everything");
        line = parseCommand("give weapon_crossbow");
        check(line.command == Command::Give && itemOfName(line.args[0]) == ItemKind::Crossbow, "give one thing");
        check(parseCommand("GOD").command == Command::God, "capitals are as small letters");
        line = parseCommand("killall   monster_barnacle");
        check(line.command == Command::KillAll && kindOfName(line.args[0]) == ActorKind::Barnacle,
              "killall, by the maps' class name");
        check(kindOfName("barnacle") == ActorKind::Barnacle && kindOfName("zombie") == ActorKind::Zombie &&
                  kindOfName("monster_human_grunt") == ActorKind::Grunt && kindOfName("nothing") == ActorKind::Count,
              "... or a short one; and nothing else");
        check(parseCommand("frobnicate").command == Command::Unknown && parseCommand("").command == Command::Unknown,
              "what is not a command is not one");
        line = parseCommand("a b c d e");
        check(line.argc == 3, "no more than three words after the first");

        int q8 = 0;
        check(parseNumber("800", q8) && q8 == 800 * 256, "a number");
        check(parseNumber("0.5", q8) && q8 == 128, "a fraction");
        check(parseNumber("-3", q8) && q8 == -3 * 256, "below nothing");
        check(parseNumber("1.25", q8) && q8 == 320, "... and both");
        check(!parseNumber("", q8) && !parseNumber("abc", q8) && !parseNumber("1.2.3", q8) && !parseNumber("-", q8),
              "and nothing that is not one");

        // Every thing the maps hold can be given, by its name there.
        const char *const kNames[] = {"weapon_crowbar",  "weapon_9mmhandgun", "weapon_shotgun",  "weapon_357",
                                      "weapon_9mmar",    "weapon_handgrenade", "weapon_crossbow", "weapon_satchel",
                                      "weapon_tripmine", "weapon_snark",      "ammo_9mmclip",    "ammo_buckshot",
                                      "ammo_357",        "ammo_9mmar",        "ammo_argrenades", "ammo_crossbow",
                                      "item_battery",    "item_healthkit",    "item_suit"};
        bool all = true;
        for (const char *name : kNames)
            all = all && itemOfName(name) != ItemKind::None;
        check(all && itemOfName("weapon_bfg") == ItemKind::None, "the things there are to give, by their names");
        // Each name help shows is one the line reads.
        bool known = true;
        for (const CommandInfo &info : kCommands)
            known = known && (parseCommand(info.name).command != Command::Unknown || std::strcmp(info.name, "impulse") == 0);
        check(known, "every command help shows is read");
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

    void tick(Game &g, int frames)
    {
        for (int f = 0; f < frames; ++f)
        {
            logicTick(g.logic, g.player, 33);
            monstersTick(g.logic, g.player, 33);
            g.drain();
        }
    }

    int findKind(const Game &g, ActorKind kind, int nth = 0)
    {
        for (int i = 0; i < g.actors.count; ++i)
        {
            const ActorState &actor = g.actors.states[i];
            if (actor.active && actor.kind == static_cast<std::uint8_t>(kind) && actor.script < 0 && nth-- == 0)
                return i;
        }
        return -1;
    }

    /// The player held next to `actor`, `distance` units off along x.
    void beside(Game &g, const ActorState &actor, int distance)
    {
        g.player.origin[0] = actor.pos[0] + fxFromInt(distance);
        g.player.origin[1] = actor.pos[1];
        g.player.origin[2] = actor.pos[2] + kStandingFeet;
        g.player.velocity[0] = g.player.velocity[1] = g.player.velocity[2] = 0;
    }

    void testCheats(const char *dir)
    {
        Cheats cheats;
        // God and notarget, against a zombie in c1a1a.
        {
            auto game = load(dir, "c1a1a");
            if (!game)
                return;
            Game &g = *game;
            g.player.cheats = &cheats;
            const int zombie = findKind(g, ActorKind::Zombie);
            check(zombie >= 0, "a zombie to stand by");
            if (zombie < 0)
                return;
            for (int i = 0; i < g.actors.count; ++i)
            {
                if (i != zombie && isMonster(g.actors.states[i].kind))
                    g.actors.states[i].active = false;
            }
            ActorState &actor = g.actors.states[zombie];
            const fx home[3] = {actor.pos[0], actor.pos[1], actor.pos[2]};
            auto stay = [&]()
            {
                for (int k = 0; k < 3; ++k)
                    actor.pos[k] = home[k];
                beside(g, actor, 40);
            };
            // Notarget: stood beside it for five seconds, it never turns on them.
            cheats.notarget = true;
            bool after = false;
            for (int f = 0; f < 150; ++f)
            {
                stay();
                tick(g, 1);
                after = after || actor.ai == kAiChase || actor.ai == kAiAttack;
            }
            check(!after && g.player.health == 100, "notarget: a zombie at arm's length lets them be");
            cheats.notarget = false;
            // God: it goes for them, and nothing comes of it.
            cheats.god = true;
            bool struck = false;
            for (int f = 0; f < 300; ++f)
            {
                stay();
                tick(g, 1);
                struck = struck || actor.ai == kAiAttack;
            }
            check(struck && g.player.health == 100 && g.player.inv.armor == 0, "god: it attacks, and does no harm");
            logicHurt(g.player, 40);
            check(g.player.health == 100, "... nor does anything else");
            logicHurt(g.player, -10);
            check(g.player.health == 100, "(... and healing still heals up to the full)");
            cheats.god = false;
            for (int f = 0; f < 300 && g.player.health == 100; ++f)
            {
                stay();
                tick(g, 1);
            }
            check(g.player.health < 100, "without it, it does");
            g.player.health = 100;

            // Killall: dead, each of its own death.
            cheats = Cheats();
            const int killed = killAllOfKind(g.logic, ActorKind::Zombie);
            check(killed == 1 && actor.ai >= kAiDying && actor.active, "killall zombie: it dies, in its own way");
            check(killAllOfKind(g.logic, ActorKind::Zombie) == 0, "... and the dead are not killed again");

            // sv_gravity: the player falls as fast as it says.
            // (From the same place each time, in the open over the zombie's
            // floor, its body moved out of the way.)
            actor.active = false;
            PlayerState start = g.player;
            start.origin[0] = home[0];
            start.origin[1] = home[1];
            start.origin[2] = home[2] + kStandingFeet + fxFromInt(80);
            start.velocity[0] = start.velocity[1] = start.velocity[2] = 0;
            start.onGround = false;
            start.stuck = false;
            auto fall = [&](int gravity)
            {
                cheats.gravity = fxFromInt(gravity);
                g.player = start;
                const fx from = g.player.origin[2];
                for (int n = 0; n < 8; ++n)
                    playerMove(g.player, MoveCommand(), g.move, kPlayerTick);
                return fxToInt(from - g.player.origin[2]);
            };
            const int normal = fall(800), light = fall(200);
            std::printf("test_commands: fallen in 8 ticks: %d units at 800, %d at 200\n", normal, light);
            check(normal > 0 && light > 0 && light * 3 < normal, "sv_gravity: a quarter of the pull, a quarter of the fall");
            cheats = Cheats();
        }
        // Barnacles, in c1a1c: one grabbing with barnaclekill on dies of it.
        {
            auto game = load(dir, "c1a1c");
            if (!game)
                return;
            Game &g = *game;
            g.player.cheats = &cheats;
            const int barnacle = findKind(g, ActorKind::Barnacle);
            check(barnacle >= 0, "a barnacle in c1a1c");
            if (barnacle < 0)
                return;
            ActorState &actor = g.actors.states[barnacle];
            tick(g, 2); // (its first look: its health, how far its tongue hangs)
            auto under = [&]()
            {
                g.player.origin[0] = actor.pos[0];
                g.player.origin[1] = actor.pos[1];
                g.player.origin[2] = actor.pos[2] - fxFromInt(40) - kStandingFeet;
                g.player.velocity[2] = 0;
            };
            int frames = 0;
            for (; frames < 60 && actor.ai != kAiAttack; ++frames)
            {
                under();
                tick(g, 1);
            }
            check(actor.ai == kAiAttack, "without it, a barnacle grabs whoever is under it");
            // Another, with it on.
            const int other = findKind(g, ActorKind::Barnacle, 1);
            check(other >= 0, "a second barnacle");
            if (other < 0)
                return;
            ActorState &next = g.actors.states[other];
            cheats.barnacleKill = true;
            const fx held = next.pos[2] - fxFromInt(40) - kStandingFeet;
            bool lifted = false;
            for (frames = 0; frames < 60 && next.ai < kAiDying; ++frames)
            {
                g.player.origin[0] = next.pos[0];
                g.player.origin[1] = next.pos[1];
                g.player.origin[2] = held;
                tick(g, 1);
                lifted = lifted || g.player.origin[2] > held;
            }
            check(next.ai >= kAiDying && !lifted && g.player.health == 100,
                  "barnaclekill: the one that grabs dies of it, and lifts nobody");
            // And every one there is, at once.
            const int alive = [&]()
            {
                int n = 0;
                for (int i = 0; i < g.actors.count; ++i)
                    n += g.actors.states[i].active && g.actors.states[i].kind == static_cast<std::uint8_t>(ActorKind::Barnacle) &&
                         g.actors.states[i].ai < kAiDying;
                return n;
            }();
            const int killed = killAllOfKind(g.logic, ActorKind::Barnacle);
            int left = 0;
            for (int i = 0; i < g.actors.count; ++i)
                left += g.actors.states[i].active && g.actors.states[i].kind == static_cast<std::uint8_t>(ActorKind::Barnacle) &&
                        g.actors.states[i].ai < kAiDying;
            std::printf("test_commands: killall barnacle: %d of %d\n", killed, alive);
            check(killed == alive && alive >= 5 && left == 0, "killall barnacle: every one of them");
            cheats = Cheats();
        }
    }
} // namespace

int main()
{
    testParse();
    const char *dir = std::getenv("HLDS_VALVE_DIR");
    if (dir != nullptr && dir[0] != '\0')
        testCheats(dir);
    else
        std::printf("test_commands: HLDS_VALVE_DIR not set, the game's maps were not played\n");
    if (g_failures == 0)
        std::printf("test_commands: ok\n");
    return g_failures == 0 ? 0 : 1;
}
