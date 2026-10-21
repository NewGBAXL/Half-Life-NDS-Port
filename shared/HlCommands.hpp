#pragma once

// The COMMAND line: what is typed on the pause menu's keyboard, read as the
// game's own console reads it -- a word, then what it is given, with the
// game's names and aliases ("impulse 101" is everything; "changelevel" is
// "map"). This only reads the line; source/main.cpp does what it says.
//
// std-only and inline: the host tests run it (test_commands).

#include <cstdint>
#include <cstring>

#include "HlLevel.hpp"

namespace hldata
{
    enum class Command : std::uint8_t
    {
        Unknown,
        Help,
        Map,         ///< map / changelevel <name>
        GiveAll,     ///< give (alone), impulse 101
        Give,        ///< give <item>
        Heal,
        Health,      ///< health <n>
        Kill,        ///< the player dies
        God,
        Notarget,
        Noclip,
        Save,        ///< quick save
        Load,        ///< quick load
        Restart,     ///< the map again from its start
        Flashlight,  ///< impulse 100
        Fov,         ///< fov <n>
        Crosshair,   ///< crosshair 0|1
        Gravity,     ///< sv_gravity <n>
        Timescale,   ///< host_timescale <x>
        KillAll,     ///< killall <class>
        BarnacleKill, ///< barnaclekill: a barnacle that grabs the player dies of it
        Count
    };

    struct CommandInfo
    {
        const char *name;
        Command command;
        const char *usage; ///< for help: how it is written
    };
    /// Every name the line knows (aliases too), in the order help shows them.
    inline constexpr CommandInfo kCommands[] = {
        {"help", Command::Help, "help [page]"},
        {"god", Command::God, "god"},
        {"notarget", Command::Notarget, "notarget"},
        {"noclip", Command::Noclip, "noclip"},
        {"give", Command::GiveAll, "give [item]"},
        {"impulse", Command::Unknown, "impulse 101 / 100"},
        {"heal", Command::Heal, "heal"},
        {"health", Command::Health, "health <n>"},
        {"kill", Command::Kill, "kill"},
        {"killall", Command::KillAll, "killall <monster>"},
        {"barnaclekill", Command::BarnacleKill, "barnaclekill"},
        {"map", Command::Map, "map <name>"},
        {"changelevel", Command::Map, "changelevel <name>"},
        {"restart", Command::Restart, "restart"},
        {"save", Command::Save, "save"},
        {"load", Command::Load, "load"},
        {"fov", Command::Fov, "fov <60..110>"},
        {"crosshair", Command::Crosshair, "crosshair 0/1"},
        {"sv_gravity", Command::Gravity, "sv_gravity <n>"},
        {"host_timescale", Command::Timescale, "host_timescale <x>"},
    };
    inline constexpr int kCommandNames = static_cast<int>(sizeof(kCommands) / sizeof(kCommands[0]));

    /// A line, read: what it asks for, and up to three words after it.
    struct CommandLine
    {
        Command command = Command::Unknown;
        char verb[24] = {};
        char args[3][24] = {};
        int argc = 0;
    };

    /// Read `line`: lower case, split on spaces (a word too long is cut).
    inline CommandLine parseCommand(const char *line)
    {
        CommandLine out;
        int word = -1, length = 0;
        bool inWord = false;
        for (const char *p = line; p != nullptr && *p != '\0'; ++p)
        {
            char ch = *p;
            if (ch == ' ' || ch == '\t')
            {
                inWord = false;
                continue;
            }
            if (!inWord)
            {
                inWord = true;
                ++word;
                length = 0;
                if (word > 3)
                    break;
                out.argc = word;
            }
            if (ch >= 'A' && ch <= 'Z')
                ch = static_cast<char>(ch - 'A' + 'a');
            char *into = word == 0 ? out.verb : out.args[word - 1];
            if (length < 23)
            {
                into[length++] = ch;
                into[length] = '\0';
            }
        }
        for (const CommandInfo &info : kCommands)
        {
            if (std::strcmp(info.name, out.verb) == 0)
                out.command = info.command;
        }
        // The game's aliases and numbered impulses.
        if (std::strcmp(out.verb, "impulse") == 0)
        {
            if (std::strcmp(out.args[0], "101") == 0)
                out.command = Command::GiveAll;
            else if (std::strcmp(out.args[0], "100") == 0)
                out.command = Command::Flashlight;
        }
        if (out.command == Command::GiveAll && out.argc > 0 && std::strcmp(out.verb, "give") == 0)
            out.command = Command::Give;
        return out;
    }

    /// A number as the console writes it ("800", "-3", "0.5"), times 256.
    /// False if it is not one.
    inline bool parseNumber(const char *text, int &q8)
    {
        bool negative = false, digits = false, point = false;
        std::int64_t whole = 0, frac = 0, scale = 1;
        const char *p = text;
        if (*p == '-' || *p == '+')
            negative = *p++ == '-';
        for (; *p != '\0'; ++p)
        {
            if (*p == '.' && !point)
            {
                point = true;
                continue;
            }
            if (*p < '0' || *p > '9')
                return false;
            digits = true;
            if (point)
            {
                if (scale < 100000)
                {
                    frac = frac * 10 + (*p - '0');
                    scale *= 10;
                }
            }
            else if (whole < 1000000)
            {
                whole = whole * 10 + (*p - '0');
            }
        }
        if (!digits)
            return false;
        const std::int64_t value = whole * 256 + (frac * 256 + scale / 2) / scale;
        q8 = static_cast<int>(negative ? -value : value);
        return true;
    }

    /// The things that can be given, by the names the maps give them.
    struct ItemName
    {
        const char *name;
        ItemKind item;
    };
    inline constexpr ItemName kItemNames[] = {
        {"weapon_crowbar", ItemKind::Crowbar},       {"weapon_glock", ItemKind::Glock},
        {"weapon_9mmhandgun", ItemKind::Glock},      {"weapon_shotgun", ItemKind::Shotgun},
        {"ammo_glockclip", ItemKind::GlockClip},     {"ammo_9mmclip", ItemKind::GlockClip},
        {"ammo_buckshot", ItemKind::Buckshot},       {"item_healthkit", ItemKind::HealthKit},
        {"item_battery", ItemKind::Battery},         {"item_suit", ItemKind::Suit},
        {"weapon_handgrenade", ItemKind::Grenades},  {"weapon_9mmar", ItemKind::Mp5},
        {"weapon_mp5", ItemKind::Mp5},               {"weapon_357", ItemKind::Python},
        {"weapon_python", ItemKind::Python},         {"ammo_9mmar", ItemKind::Mp5Clip},
        {"ammo_mp5clip", ItemKind::Mp5Clip},         {"ammo_9mmbox", ItemKind::AmmoBox},
        {"ammo_argrenades", ItemKind::ArGrenades},   {"ammo_mp5grenades", ItemKind::ArGrenades},
        {"ammo_357", ItemKind::Ammo357},             {"weapon_crossbow", ItemKind::Crossbow},
        {"ammo_crossbow", ItemKind::Bolts},          {"weapon_satchel", ItemKind::Satchel},
        {"weapon_tripmine", ItemKind::Tripmine},     {"weapon_snark", ItemKind::Snark},
        {"weapon_rpg", ItemKind::Rpg},               {"ammo_rpgclip", ItemKind::RpgClip},
        {"weapon_gauss", ItemKind::Gauss},           {"ammo_gaussclip", ItemKind::GaussClip},
        {"weapon_egon", ItemKind::Egon},             {"weapon_hornetgun", ItemKind::Hornetgun},
    };
    /// ItemKind::None if there is no such thing.
    inline ItemKind itemOfName(const char *name)
    {
        for (const ItemName &item : kItemNames)
        {
            if (std::strcmp(item.name, name) == 0)
                return item.item;
        }
        return ItemKind::None;
    }

    /// The monsters killall knows: the maps' class names, and short ones.
    struct KindName
    {
        const char *name;
        ActorKind kind;
    };
    inline constexpr KindName kKindNames[] = {
        {"monster_headcrab", ActorKind::Headcrab},      {"headcrab", ActorKind::Headcrab},
        {"monster_zombie", ActorKind::Zombie},          {"zombie", ActorKind::Zombie},
        {"monster_barnacle", ActorKind::Barnacle},      {"barnacle", ActorKind::Barnacle},
        {"monster_alien_slave", ActorKind::Slave},      {"slave", ActorKind::Slave},
        {"vortigaunt", ActorKind::Slave},               {"monster_bullchicken", ActorKind::Bullsquid},
        {"bullsquid", ActorKind::Bullsquid},            {"monster_houndeye", ActorKind::Houndeye},
        {"houndeye", ActorKind::Houndeye},              {"monster_human_grunt", ActorKind::Grunt},
        {"grunt", ActorKind::Grunt},                    {"monster_miniturret", ActorKind::Miniturret},
        {"miniturret", ActorKind::Miniturret},          {"monster_sentry", ActorKind::Sentry},
        {"sentry", ActorKind::Sentry},                  {"monster_turret", ActorKind::Turret},
        {"turret", ActorKind::Turret},                  {"monster_tripmine", ActorKind::Tripmine},
        {"tripmine", ActorKind::Tripmine},              {"monster_scientist", ActorKind::Scientist},
        {"scientist", ActorKind::Scientist},            {"monster_barney", ActorKind::Guard},
        {"barney", ActorKind::Guard},                   {"guard", ActorKind::Guard},
        {"monster_cockroach", ActorKind::Cockroach},    {"roach", ActorKind::Cockroach},
        {"monster_leech", ActorKind::Leech},            {"leech", ActorKind::Leech},
        {"monster_alien_grunt", ActorKind::AlienGrunt}, {"agrunt", ActorKind::AlienGrunt},
        {"monster_alien_controller", ActorKind::Controller}, {"controller", ActorKind::Controller},
        {"monster_snark", ActorKind::Snark},            {"snark", ActorKind::Snark},
    };
    /// ActorKind::Count if there is no such monster.
    inline ActorKind kindOfName(const char *name)
    {
        for (const KindName &kind : kKindNames)
        {
            if (std::strcmp(kind.name, name) == 0)
                return kind.kind;
        }
        return ActorKind::Count;
    }
} // namespace hldata
