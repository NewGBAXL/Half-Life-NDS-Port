#include "HlSound.hpp"
#include "hlds/EntityCook.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <cstring>

namespace hlds
{
    using namespace hldata;

    namespace
    {
        double numberKey(const Entity &entity, std::string_view key, double fallback = 0)
        {
            const std::string text(entity.value(key));
            return text.empty() ? fallback : std::atof(text.c_str());
        }

        /// Seconds to milliseconds; a negative time is "never" and stays -1.
        std::int32_t millis(double seconds)
        {
            return seconds < 0 ? -1 : static_cast<std::int32_t>(std::lround(seconds * 1000));
        }

        // The map's own spawnflags, per class (SDK dlls/doors.h, buttons.cpp,
        // triggers.cpp).
        constexpr int kDoorStartOpen = 1;
        constexpr int kDoorNoAutoReturn = 32;
        constexpr int kDoorUseOnly = 256;
        constexpr int kButtonDontMove = 1;
        constexpr int kButtonToggle = 32;
        constexpr int kButtonTouchOnly = 256;
        constexpr int kTriggerNoClients = 2;
        constexpr int kRelayFireOnce = 1;
        constexpr int kManagerThread = 1;

        /// Classes that have no logic record and are not "unsupported" either:
        /// they are handled elsewhere, or are data the cooker has already used.
        bool isAccountedFor(const std::string &cls)
        {
            static const char *const kClasses[] = {
                "worldspawn", "info_player_start", "light", "light_spot", "light_environment",
                "func_wall", "func_illusionary", "func_ladder", "infodecal", "info_null",
                "info_target", "path_corner", "info_landmark", "path_track",
                "info_teleport_destination", "monster_scientist", "monster_barney", "monster_gman",
                "monster_sitting_scientist", "monster_scientist_dead", "monster_barney_dead",
                "monster_generic", "monster_furniture", "cycler", "monster_headcrab",
                "monster_zombie", "monster_barnacle", "info_node", "weapon_crowbar", "weapon_glock",
                "weapon_9mmhandgun", "weapon_shotgun", "ammo_glockclip", "ammo_9mmclip",
                "ammo_buckshot", "item_healthkit", "item_battery", "item_suit", "world_items",
                "ambient_generic", "trigger_cdaudio", "target_cdaudio", "env_sound",
                "monster_alien_slave", "monster_bullchicken", "monster_houndeye",
                "weapon_handgrenade"};
            for (const char *name : kClasses)
            {
                if (cls == name)
                    return true;
            }
            return false;
        }
    } // namespace

    namespace
    {
        std::string lowered(std::string_view text)
        {
            std::string out(text);
            for (char &c : out)
                c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            return out;
        }

        /// What an entity is as an actor, if it is one.
        struct ActorInfo
        {
            bool is = false;
            ActorKind kind = ActorKind::Generic;
            std::string file;              ///< the model, as the game names it
            std::vector<std::string> clips; ///< what it needs without being told
            std::string startClip;         ///< the clip it starts in; empty: the model's first
            bool firstSequence = false;
            bool solid = true;
            ItemKind item = ItemKind::None;
            /// For what a monstermaker makes: how many places are kept for
            /// its children (they are not there when the map starts).
            int spare = 0;
        };

        /// What lies about to be picked up, by class.
        struct ItemClass
        {
            const char *cls;
            ItemKind kind;
            const char *file;
        };
        const ItemClass kItemClasses[] = {
            {"weapon_crowbar", ItemKind::Crowbar, "models/w_crowbar.mdl"},
            {"weapon_glock", ItemKind::Glock, "models/w_9mmhandgun.mdl"},
            {"weapon_9mmhandgun", ItemKind::Glock, "models/w_9mmhandgun.mdl"},
            {"weapon_shotgun", ItemKind::Shotgun, "models/w_shotgun.mdl"},
            {"ammo_glockclip", ItemKind::GlockClip, "models/w_9mmclip.mdl"},
            {"ammo_9mmclip", ItemKind::GlockClip, "models/w_9mmclip.mdl"},
            {"ammo_buckshot", ItemKind::Buckshot, "models/w_shotbox.mdl"},
            {"item_healthkit", ItemKind::HealthKit, "models/w_medkit.mdl"},
            {"item_battery", ItemKind::Battery, "models/w_battery.mdl"},
            {"item_suit", ItemKind::Suit, "models/w_suit.mdl"},
            {"weapon_handgrenade", ItemKind::Grenades, "models/w_grenade.mdl"},
        };

        ActorInfo actorOf(const Entity &entity)
        {
            ActorInfo info;
            std::string cls(entity.className());
            const int pose = static_cast<int>(numberKey(entity, "pose"));
            int spare = 0;
            if (cls == "monstermaker")
            {
                // It is what it makes, a few times over: as many as may be
                // alive at once, and never more than three.
                cls = lowered(entity.value("monstertype"));
                const int count = static_cast<int>(numberKey(entity, "monstercount"));
                const int alive = static_cast<int>(numberKey(entity, "m_imaxlivechildren"));
                spare = std::clamp(alive > 0 ? alive : (count > 0 ? count : 2), 1, 3);
                if (count > 0)
                    spare = std::min(spare, count);
            }
            // The campaign's suit is not an item_suit: it is a "world_items"
            // of type 45 (44 is a battery; the antidote and the security
            // card, 42 and 43, were never finished in the game either).
            if (cls == "world_items")
            {
                const int type = static_cast<int>(numberKey(entity, "type"));
                cls = type == 45 ? "item_suit" : (type == 44 ? "item_battery" : "");
            }
            for (const ItemClass &item : kItemClasses)
            {
                if (cls != item.cls)
                    continue;
                info = {true, ActorKind::Item, item.file, {"idle"}, "idle"};
                info.solid = false;
                info.item = item.kind;
                return info;
            }
            if (cls == "monster_headcrab")
            {
                info = {true, ActorKind::Headcrab, "models/headcrab.mdl",
                        {"idle1", "run", "jump", "flinch", "dieback"}, "idle1"};
            }
            else if (cls == "monster_zombie")
            {
                info = {true, ActorKind::Zombie, "models/zombie.mdl",
                        {"idle1", "walk", "attack1", "attack2", "flinchsmall", "diesimple"}, "idle1"};
            }
            else if (cls == "monster_barnacle")
            {
                info = {true, ActorKind::Barnacle, "models/barnacle.mdl",
                        {"idle1", "attack1", "chewcycle", "flinch_sm", "death"}, "idle1"};
            }
            else if (cls == "monster_alien_slave")
            {
                info = {true, ActorKind::Slave, "models/islave.mdl",
                        {"idle1", "walk1", "attack1", "zapattack1", "flinch2", "diesimple"}, "idle1"};
            }
            else if (cls == "monster_bullchicken")
            {
                info = {true, ActorKind::Bullsquid, "models/bullsquid.mdl",
                        {"idle", "run", "range", "bite", "whip", "flinchs", "die1"}, "idle"};
            }
            else if (cls == "monster_houndeye")
            {
                info = {true, ActorKind::Houndeye, "models/houndeye.mdl",
                        {"idle1", "run", "attack", "flinch_small", "die"}, "idle1"};
            }
            else if (spare != 0)
            {
                return info; // a maker of something there is not
            }
            else if (cls == "monster_scientist")
            {
                info = {true, ActorKind::Scientist, "models/scientist.mdl", {"idle1", "walk", "run"}, "idle1"};
            }
            else if (cls == "monster_barney")
            {
                info = {true, ActorKind::Guard, "models/barney.mdl", {"idle1", "walk", "run"}, "idle1"};
            }
            else if (cls == "monster_gman")
            {
                info = {true, ActorKind::Gman, "models/gman.mdl", {"idle01", "walk"}, "idle01"};
            }
            else if (cls == "monster_sitting_scientist")
            {
                // (The five ways he sits, which he goes between: HlActors.hpp.)
                info = {true, ActorKind::Sitter, "models/scientist.mdl",
                        {"sitlookleft", "sitlookright", "sitscared", "sitting2", "sitting3"}, "sitlookleft"};
            }
            else if (cls == "monster_scientist_dead")
            {
                static const char *const kPoses[] = {"lying_on_back", "lying_on_stomach", "dead_sitting",
                                                     "dead_hang",     "dead_table1",      "dead_table2",
                                                     "dead_table3"};
                const char *clip = kPoses[std::clamp(pose, 0, 6)];
                info = {true, ActorKind::Pose, "models/scientist.mdl", {clip}, clip};
                info.solid = false;
            }
            else if (cls == "monster_barney_dead")
            {
                static const char *const kPoses[] = {"lying_on_back", "lying_on_side", "lying_on_stomach"};
                const char *clip = kPoses[std::clamp(pose, 0, 2)];
                info = {true, ActorKind::Pose, "models/barney.mdl", {clip}, clip};
                info.solid = false;
            }
            else if ((cls == "monster_generic" || cls == "monster_furniture" || cls == "cycler") &&
                     !entity.value("model").empty())
            {
                info.is = true;
                info.kind = cls == "cycler" ? ActorKind::Cycler
                                            : (cls == "monster_furniture" ? ActorKind::Furniture
                                                                          : ActorKind::Generic);
                info.file = lowered(entity.value("model"));
                info.firstSequence = true;
                // A monster_generic with flag 4 is scenery one walks through.
                info.solid = !(cls == "monster_generic" &&
                               (static_cast<int>(numberKey(entity, "spawnflags")) & 4));
            }
            // Only models, not sprites or brush models given by name.
            if (info.is && (info.file.size() < 5 || info.file.substr(info.file.size() - 4) != ".mdl"))
                info.is = false;
            info.spare = spare;
            return info;
        }

        std::string modelNameOf(const std::string &file)
        {
            const std::size_t slash = file.find_last_of("/\\");
            const std::string base = slash == std::string::npos ? file : file.substr(slash + 1);
            return base.substr(0, base.size() - 4);
        }

        /// The class name a scripted sequence may give in place of an
        /// actor's name, as the kind of actor it means; -1 if it is not one.
        int kindOfClass(const std::string &cls)
        {
            if (cls == "monster_scientist")
                return static_cast<int>(ActorKind::Scientist);
            if (cls == "monster_barney")
                return static_cast<int>(ActorKind::Guard);
            if (cls == "monster_gman")
                return static_cast<int>(ActorKind::Gman);
            if (cls == "monster_generic")
                return static_cast<int>(ActorKind::Generic);
            if (cls == "monster_furniture")
                return static_cast<int>(ActorKind::Furniture);
            if (cls == "monster_headcrab")
                return static_cast<int>(ActorKind::Headcrab);
            if (cls == "monster_zombie")
                return static_cast<int>(ActorKind::Zombie);
            if (cls == "monster_barnacle")
                return static_cast<int>(ActorKind::Barnacle);
            if (cls == "monster_alien_slave")
                return static_cast<int>(ActorKind::Slave);
            if (cls == "monster_bullchicken")
                return static_cast<int>(ActorKind::Bullsquid);
            if (cls == "monster_houndeye")
                return static_cast<int>(ActorKind::Houndeye);
            return -1;
        }

        bool isScript(const std::string &cls)
        {
            return cls == "scripted_sequence" || cls == "aiscripted_sequence";
        }
    } // namespace

    std::vector<ModelNeed> collectModelNeeds(const std::vector<Entity> &entities)
    {
        std::vector<ModelNeed> needs;
        auto needOf = [&](const std::string &file) -> ModelNeed &
        {
            for (ModelNeed &need : needs)
            {
                if (need.file == file)
                    return need;
            }
            needs.emplace_back();
            needs.back().file = file;
            needs.back().name = modelNameOf(file);
            return needs.back();
        };
        auto add = [](ModelNeed &need, const std::string &clip)
        {
            if (!clip.empty() && std::find(need.clips.begin(), need.clips.end(), clip) == need.clips.end())
                need.clips.push_back(clip);
        };
        for (const Entity &entity : entities)
        {
            const ActorInfo info = actorOf(entity);
            if (!info.is)
                continue;
            ModelNeed &need = needOf(info.file);
            need.firstSequence = need.firstSequence || info.firstSequence;
            for (const std::string &clip : info.clips)
                add(need, clip);
        }
        // What the scripts have their actors do: an actor by name, or every
        // actor of a class.
        for (const Entity &script : entities)
        {
            if (!isScript(std::string(script.className())))
                continue;
            const std::string who = lowered(script.value("m_iszEntity"));
            const std::string play = lowered(script.value("m_iszPlay"));
            const std::string idle = lowered(script.value("m_iszIdle"));
            const int moveTo = static_cast<int>(numberKey(script, "m_fMoveTo"));
            for (const Entity &entity : entities)
            {
                const ActorInfo info = actorOf(entity);
                if (!info.is)
                    continue;
                if (lowered(entity.value("targetname")) != who && std::string(entity.className()) != who)
                    continue;
                ModelNeed &need = needOf(info.file);
                add(need, play);
                add(need, idle);
                // Anything may be told to walk or run somewhere.
                if (moveTo == 1 || moveTo == 2)
                {
                    add(need, "walk");
                    add(need, "run");
                }
            }
        }
        return needs;
    }

    void addVisitorNeeds(const std::vector<Entity> &entities, const std::vector<Entity> &neighbour,
                         const std::string &landmark, std::vector<ModelNeed> &needs)
    {
        double mark[3] = {0, 0, 0};
        bool found = false;
        for (const Entity &entity : neighbour)
        {
            if (entity.className() == "info_landmark" &&
                lowered(entity.value("targetname")) == lowered(landmark))
                found = entity.vec3("origin", mark) || true;
        }
        if (!found)
            return;
        for (const Entity &entity : neighbour)
        {
            const ActorInfo info = actorOf(entity);
            const std::string name = lowered(entity.value("targetname"));
            double origin[3] = {0, 0, 0};
            entity.vec3("origin", origin);
            // The same test the DS makes when the player crosses
            // (kVisitorRange).
            if (!info.is || name.empty() || std::fabs(origin[0] - mark[0]) > 2048 ||
                std::fabs(origin[1] - mark[1]) > 2048 || std::fabs(origin[2] - mark[2]) > 2048)
                continue;
            ModelNeed *need = nullptr;
            for (ModelNeed &have : needs)
            {
                if (have.file == info.file)
                    need = &have;
            }
            if (need == nullptr)
            {
                needs.emplace_back();
                need = &needs.back();
                need->file = info.file;
                need->name = modelNameOf(info.file);
            }
            auto add = [&](const std::string &clip)
            {
                if (!clip.empty() &&
                    std::find(need->clips.begin(), need->clips.end(), clip) == need->clips.end())
                    need->clips.push_back(clip);
            };
            need->firstSequence = need->firstSequence || info.firstSequence;
            for (const std::string &clip : info.clips)
                add(clip);
            add("walk");
            add("run");
            for (const Entity &script : entities)
            {
                if (!isScript(std::string(script.className())) ||
                    lowered(script.value("m_iszEntity")) != name)
                    continue;
                add(lowered(script.value("m_iszPlay")));
                add(lowered(script.value("m_iszIdle")));
            }
        }
    }

    void moverDirection(const Entity &entity, double *out)
    {
        double angles[3] = {0, 0, 0};
        if (!entity.vec3("angles", angles))
            angles[1] = numberKey(entity, "angle");
        out[0] = out[1] = out[2] = 0;
        if (angles[0] == 0 && angles[2] == 0 && angles[1] == -1)
        {
            out[2] = 1;
            return;
        }
        if (angles[0] == 0 && angles[2] == 0 && angles[1] == -2)
        {
            out[2] = -1;
            return;
        }
        const double pitch = angles[0] * M_PI / 180.0;
        const double yaw = angles[1] * M_PI / 180.0;
        out[0] = std::cos(pitch) * std::cos(yaw);
        out[1] = std::cos(pitch) * std::sin(yaw);
        out[2] = -std::sin(pitch);
        // A heading of 90 degrees is exactly +Y, not +Y and a rounding error
        // of X: the travel below multiplies it by hundreds of units.
        for (int a = 0; a < 3; ++a)
        {
            if (std::fabs(out[a]) < 1e-9)
                out[a] = 0;
        }
    }

    void moverTravel(const double *direction, const double *size, double lip, double *out)
    {
        // The whole brush slides clear of where it stood, less two units and
        // less the lip that is meant to stay showing.
        const double distance = std::fabs(direction[0] * (size[0] - 2)) +
                                std::fabs(direction[1] * (size[1] - 2)) +
                                std::fabs(direction[2] * (size[2] - 2)) - lip;
        for (int a = 0; a < 3; ++a)
            out[a] = direction[a] * distance;
    }

    void cookLogic(const BspFile &bsp, const std::vector<Entity> &entities,
                   const std::vector<int> &brushRecordOfEntity,
                   const std::vector<std::string> &models, LogicCook &out, const SentenceBook *book)
    {
        out = LogicCook();
        out.recordOfEntity.assign(entities.size(), -1);

        // The sounds: the common ones first, at their fixed places, then
        // whatever the map names. A sound that loops and the same one played
        // once are two sounds (they are cooked differently).
        const int commonCount = static_cast<int>(CommonSound::Count);
        std::vector<bool> soundUsed(commonCount, false);
        for (int c = 0; c < commonCount; ++c)
        {
            out.sounds.push_back(commonSoundPath(static_cast<CommonSound>(c)));
            out.soundLoops.push_back(false);
        }
        auto soundIndex = [&](std::string path, bool loops) -> std::uint16_t
        {
            for (char &c : path)
                c = c == '\\' ? '/' : static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            // (The game lets a name start with a character that says how it
            // is to be streamed: not part of the name.)
            while (!path.empty() && (path[0] == '*' || path[0] == '/' || path[0] == '#' || path[0] == '@'))
                path.erase(0, 1);
            if (path.empty() || path == "common/null.wav")
                return kNoSound;
            for (std::size_t i = commonCount; i < out.sounds.size(); ++i)
            {
                if (out.sounds[i] == path && out.soundLoops[i] == loops)
                    return static_cast<std::uint16_t>(i);
            }
            out.sounds.push_back(path);
            out.soundLoops.push_back(loops);
            return static_cast<std::uint16_t>(out.sounds.size() - 1);
        };
        auto numbered = [&](const char *stem, int number, int most) -> std::uint16_t
        {
            if (number < 1 || number > most)
                return kNoSound;
            return soundIndex(std::string(stem) + std::to_string(number) + ".wav", false);
        };
        // A spoken line, as an index into out.sentences; -1 if the book has
        // none of that name.
        std::map<std::string, int> sentenceIds;
        auto sentenceIndex = [&](const std::string &name) -> int
        {
            const Sentence *sentence = book != nullptr ? book->find(name) : nullptr;
            if (sentence == nullptr || sentence->words.size() > 255)
                return -1;
            const auto known = sentenceIds.find(name);
            if (known != sentenceIds.end())
                return known->second;
            SentenceRecord record{};
            record.firstWord = static_cast<std::uint16_t>(out.sentenceWords.size());
            record.volume = static_cast<std::uint8_t>(std::clamp(sentence->volume, 0, 127));
            for (const std::string &word : sentence->words)
            {
                const std::uint16_t index = soundIndex(word, false);
                if (index != kNoSound)
                    out.sentenceWords.push_back(index);
            }
            record.wordCount = static_cast<std::uint8_t>(out.sentenceWords.size() - record.firstWord);
            out.sentences.push_back(record);
            sentenceIds[name] = static_cast<int>(out.sentences.size() - 1);
            return sentenceIds[name];
        };
        std::vector<LogicSound> heard; // parallel to out.records

        std::map<std::string, std::uint16_t> nameIds;
        auto nameId = [&](std::string_view text) -> std::uint16_t
        {
            if (text.empty())
                return 0;
            // The game compares names without regard to case.
            const std::string key = lowered(text);
            const auto found = nameIds.find(key);
            if (found != nameIds.end())
                return found->second;
            const auto id = static_cast<std::uint16_t>(nameIds.size() + 1);
            nameIds[key] = id;
            return id;
        };

        // Strings records point into; offset 0 is the empty string.
        out.strings.assign(1, '\0');
        auto stringOffset = [&](std::string_view text) -> std::uint32_t
        {
            const auto offset = static_cast<std::uint32_t>(out.strings.size());
            for (char c : text)
                out.strings.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
            out.strings.push_back('\0');
            return offset;
        };
        auto hashOf = [](std::string_view text) -> std::uint32_t
        {
            return text.empty() ? 0 : nameHash(std::string(text).c_str());
        };

        for (const Entity &entity : entities)
        {
            if (entity.className() != "info_landmark" || entity.value("targetname").empty())
                continue;
            Landmark landmark{};
            landmark.hash = hashOf(entity.value("targetname"));
            double origin[3] = {0, 0, 0};
            entity.vec3("origin", origin);
            for (int a = 0; a < 3; ++a)
                landmark.origin[a] = fxFromDouble(origin[a]);
            out.landmarks.push_back(landmark);
        }

        // Path corners first: a train names the one it starts at. Each is
        // found by its own name; "target" is the name of the next.
        constexpr int kCornerFlagWait = 1;
        constexpr int kCornerFlagTeleport = 2;
        std::map<std::string, std::uint16_t> cornerOfName;
        std::vector<std::string> cornerNext;
        for (const Entity &entity : entities)
        {
            if (entity.className() != "path_corner")
                continue;
            PathCorner corner{};
            double origin[3] = {0, 0, 0};
            entity.vec3("origin", origin);
            for (int a = 0; a < 3; ++a)
                corner.pos[a] = fxFromDouble(origin[a]);
            corner.speed = fxFromDouble(std::max(0.0, numberKey(entity, "speed")));
            corner.waitMs = std::max(0, millis(numberKey(entity, "wait")));
            corner.fireTarget = nameId(entity.value("message"));
            const int flags = static_cast<int>(numberKey(entity, "spawnflags"));
            corner.flags = static_cast<std::uint16_t>((flags & kCornerFlagWait ? kCornerWaitForUse : 0) |
                                                      (flags & kCornerFlagTeleport ? kCornerTeleport : 0));
            corner.next = kNoCorner;
            const std::string name(entity.value("targetname"));
            // Two corners with one name: the first is the one found, as in
            // the game.
            if (!name.empty() && cornerOfName.find(name) == cornerOfName.end())
                cornerOfName[name] = static_cast<std::uint16_t>(out.corners.size());
            cornerNext.emplace_back(entity.value("target"));
            out.corners.push_back(corner);
        }
        for (std::size_t c = 0; c < out.corners.size(); ++c)
        {
            const auto found = cornerOfName.find(cornerNext[c]);
            if (found != cornerOfName.end())
                out.corners[c].next = found->second;
        }

        // A track is the same list again, in a namespace of its own: a
        // path_track's "target" is another path_track.
        std::map<std::string, std::uint16_t> trackOfName;
        {
            const std::size_t firstTrack = out.corners.size();
            std::vector<std::string> trackNext;
            for (const Entity &entity : entities)
            {
                if (entity.className() != "path_track")
                    continue;
                PathCorner node{};
                double origin[3] = {0, 0, 0};
                entity.vec3("origin", origin);
                for (int a = 0; a < 3; ++a)
                    node.pos[a] = fxFromDouble(origin[a]);
                node.speed = fxFromDouble(std::max(0.0, numberKey(entity, "speed")));
                node.fireTarget = nameId(entity.value("message"));
                node.netName = nameId(entity.value("netname"));
                const int flags = static_cast<int>(numberKey(entity, "spawnflags"));
                node.flags = static_cast<std::uint16_t>((flags & 1 ? kCornerDisabled : 0) |
                                                        (flags & 2 ? kCornerFireOnce : 0) |
                                                        (flags & 8 ? kCornerDisableTrain : 0));
                node.next = kNoCorner;
                const std::string name(entity.value("targetname"));
                if (!name.empty() && trackOfName.find(name) == trackOfName.end())
                    trackOfName[name] = static_cast<std::uint16_t>(out.corners.size());
                trackNext.emplace_back(entity.value("target"));
                out.corners.push_back(node);
            }
            for (std::size_t c = firstTrack; c < out.corners.size(); ++c)
            {
                const auto found = trackOfName.find(trackNext[c - firstTrack]);
                if (found != trackOfName.end())
                    out.corners[c].next = found->second;
            }
        }
        auto trackNode = [&](std::string_view name) -> std::uint16_t
        {
            const auto found = trackOfName.find(std::string(name));
            return found != trackOfName.end() ? found->second : kNoCorner;
        };

        struct ManagerOutput
        {
            std::uint16_t target;
            std::uint32_t delayMs;
        };
        std::vector<std::vector<ManagerOutput>> managerOutputs; // by record
        std::vector<std::uint16_t> trainCorners;                // by record

        for (std::size_t e = 0; e < entities.size(); ++e)
        {
            const Entity &entity = entities[e];
            const std::string cls(entity.className());
            const int spawnflags = static_cast<int>(numberKey(entity, "spawnflags"));
            const bool named = !entity.value("targetname").empty();

            // The entity's brush model, if it has one, for sizes and volumes.
            const BspModel *model = nullptr;
            {
                const std::string text(entity.value("model"));
                if (text.size() >= 2 && text[0] == '*')
                {
                    const int index = std::atoi(text.c_str() + 1);
                    if (index > 0 && index < static_cast<int>(bsp.models.size()))
                        model = &bsp.models[index];
                }
            }
            double origin[3] = {0, 0, 0};
            entity.vec3("origin", origin);

            if (cls == "env_sound")
            {
                // Not something that acts: a place, and how it sounds.
                RoomRecord room{};
                for (int a = 0; a < 3; ++a)
                    room.origin[a] = fxFromDouble(origin[a]);
                room.radius = static_cast<std::uint16_t>(
                    std::clamp(numberKey(entity, "radius", 128), 0.0, 65535.0));
                room.type = static_cast<std::uint8_t>(
                    std::clamp(static_cast<int>(numberKey(entity, "roomtype")), 0, 28));
                out.rooms.push_back(room);
                continue;
            }

            LogicRecord record{};
            record.brush = brushRecordOfEntity[e] >= 0
                               ? static_cast<std::uint16_t>(brushRecordOfEntity[e])
                               : kNoBrush;
            record.useType = kUseToggle;
            record.waitMs = -1;
            bool make = false;
            std::uint16_t trainCorner = kNoCorner;
            std::vector<ManagerOutput> outputs;

            auto travel = [&](double lip)
            {
                if (model == nullptr)
                    return;
                double direction[3], move[3];
                const double size[3] = {double(model->maxs[0]) - model->mins[0],
                                        double(model->maxs[1]) - model->mins[1],
                                        double(model->maxs[2]) - model->mins[2]};
                moverDirection(entity, direction);
                moverTravel(direction, size, lip, move);
                for (int a = 0; a < 3; ++a)
                    record.move[a] = fxFromDouble(move[a]);
            };

            if (cls == "func_door" || cls == "func_water")
            {
                make = true;
                record.kind = static_cast<std::uint8_t>(LogicKind::Door);
                record.speed = fxFromDouble(numberKey(entity, "speed", 100));
                if (record.speed <= 0)
                    record.speed = fxFromInt(100);
                record.waitMs = millis(numberKey(entity, "wait"));
                record.damage = static_cast<std::uint16_t>(
                    std::clamp(numberKey(entity, "dmg"), 0.0, 65535.0));
                travel(numberKey(entity, "lip"));
                if (spawnflags & kDoorStartOpen)
                    record.flags |= kLogicStartOpen;
                if (spawnflags & kDoorNoAutoReturn)
                    record.flags |= kLogicToggle;
                if (spawnflags & kDoorUseOnly)
                    record.flags |= kLogicUseOnly;
                // Walking into a door opens it only if nothing else is meant
                // to: a door with a name waits to be told.
                else if (!named)
                    record.flags |= kLogicTouch;
                record.netName = nameId(entity.value("netname"));
            }
            else if (cls == "func_door_rotating")
            {
                make = true;
                record.kind = static_cast<std::uint8_t>(LogicKind::DoorRotating);
                // The hinge is vertical unless the map says otherwise. The
                // flags are named for the AXIS in the editor ("Z", "X") but
                // choose an ANGLE: 64 is roll, 128 is pitch.
                record.extra[0] = (spawnflags & 64) ? kRoll : ((spawnflags & 128) ? kPitch : kYaw);
                // Degrees, and degrees a second, as turns * 65536.
                double distance = numberKey(entity, "distance", 90);
                if (spawnflags & 2)
                    distance = -distance; // "reverse direction"
                record.move[0] = static_cast<fx>(std::lround(distance * 65536.0 / 360.0));
                double speed = numberKey(entity, "speed", 100);
                if (speed <= 0)
                    speed = 100;
                record.speed = static_cast<fx>(std::lround(speed * 65536.0 / 360.0));
                record.waitMs = millis(numberKey(entity, "wait"));
                record.damage = static_cast<std::uint16_t>(
                    std::clamp(numberKey(entity, "dmg"), 0.0, 65535.0));
                if (spawnflags & kDoorStartOpen)
                    record.flags |= kLogicStartOpen;
                if (spawnflags & kDoorNoAutoReturn)
                    record.flags |= kLogicToggle;
                if (spawnflags & 16)
                    record.flags |= kLogicOneWay;
                if (spawnflags & kDoorUseOnly)
                    record.flags |= kLogicUseOnly;
                else if (!named)
                    record.flags |= kLogicTouch;
                record.netName = nameId(entity.value("netname"));
            }
            else if (cls == "func_button")
            {
                make = true;
                record.kind = static_cast<std::uint8_t>(LogicKind::Button);
                record.speed = fxFromDouble(numberKey(entity, "speed", 40));
                if (record.speed <= 0)
                    record.speed = fxFromInt(40);
                // The game's own defaults when the map leaves these at 0.
                double wait = numberKey(entity, "wait");
                if (wait == 0)
                    wait = 1;
                record.waitMs = millis(wait);
                double lip = numberKey(entity, "lip");
                if (lip == 0)
                    lip = 4;
                if (!(spawnflags & kButtonDontMove))
                {
                    travel(lip);
                    // A button whose travel comes to nothing does not move.
                    const fx length = fxLength(record.move);
                    if (length < kFxOne)
                        record.move[0] = record.move[1] = record.move[2] = 0;
                }
                if (spawnflags & kButtonToggle)
                    record.flags |= kLogicToggle;
                if (spawnflags & kButtonTouchOnly)
                    record.flags |= kLogicTouch;
            }
            else if (cls == "func_train" && model != nullptr)
            {
                make = true;
                record.kind = static_cast<std::uint8_t>(LogicKind::Train);
                record.speed = fxFromDouble(numberKey(entity, "speed", 100));
                if (record.speed <= 0)
                    record.speed = fxFromInt(100);
                record.damage = static_cast<std::uint16_t>(
                    std::clamp(numberKey(entity, "dmg", 2), 0.0, 65535.0));
                // The point of the brush that visits each corner: its middle.
                for (int a = 0; a < 3; ++a)
                    record.move[a] =
                        fxFromDouble((double(model->mins[a]) + model->maxs[a]) / 2 + origin[a]);
                const auto first = cornerOfName.find(std::string(entity.value("target")));
                trainCorner = first != cornerOfName.end() ? first->second : kNoCorner;
            }
            else if (cls == "func_tracktrain" && brushRecordOfEntity[e] >= 0)
            {
                make = true;
                record.kind = static_cast<std::uint8_t>(LogicKind::TrackTrain);
                double speed = numberKey(entity, "speed");
                if (speed == 0)
                    speed = 100;
                record.speed = fxFromDouble(speed);
                record.extra[0] = static_cast<std::uint32_t>(fxFromDouble(numberKey(entity, "wheels")));
                record.extra[1] = static_cast<std::uint32_t>(fxFromDouble(numberKey(entity, "height")));
                record.extra[2] = hashOf(entity.value("globalname"));
                record.extra[3] =
                    static_cast<std::uint32_t>(fxFromDouble(numberKey(entity, "startspeed")));
                if (spawnflags & 2)
                    record.flags |= kLogicSelfDriven;
                trainCorner = trackNode(entity.value("target"));
            }
            else if ((cls == "func_trackautochange" || cls == "func_trackchange") &&
                     brushRecordOfEntity[e] >= 0)
            {
                make = true;
                record.kind = static_cast<std::uint8_t>(LogicKind::TrackChange);
                double speed = numberKey(entity, "speed");
                if (speed <= 0)
                    speed = 150;
                record.speed = fxFromDouble(speed);
                record.move[2] = -fxFromDouble(numberKey(entity, "height"));
                record.extra[0] = static_cast<std::uint32_t>(static_cast<std::int32_t>(
                    std::lround(numberKey(entity, "rotation") * 65536.0 / 360.0)));
                record.extra[1] = trackNode(entity.value("toptrack"));
                record.extra[2] = trackNode(entity.value("bottomtrack"));
                record.extra[3] = nameId(entity.value("train"));
            }
            else if (cls == "func_rotating" && brushRecordOfEntity[e] >= 0)
            {
                make = true;
                record.kind = static_cast<std::uint8_t>(LogicKind::Rotating);
                // As for a door: flag 4 ("Z axis") is roll, 8 ("X axis") is
                // pitch, neither is a turn about the vertical.
                record.extra[0] = (spawnflags & 4) ? kRoll : ((spawnflags & 8) ? kPitch : kYaw);
                // Degrees a second, as turns * 65536. The game's comment says
                // "default 100" and its code does not: no speed is no spin.
                double speed = std::max(0.0, numberKey(entity, "speed"));
                if (spawnflags & 2)
                    speed = -speed;
                record.speed = static_cast<fx>(std::lround(speed * 65536.0 / 360.0));
                // "fanfriction" is the share of full speed gained or lost
                // each tenth of a second, in percent; 0 is all of it.
                double friction = numberKey(entity, "fanfriction") / 100.0;
                if (friction <= 0)
                    friction = 1;
                record.extra[1] = static_cast<std::uint32_t>(
                    std::max(1L, std::lround(std::fabs(speed) * friction * 65536.0 / 360.0)));
                if (spawnflags & 1)
                    record.flags |= kLogicStartOpen;
                if (spawnflags & 16)
                    record.flags |= kLogicToggle;
                record.damage = static_cast<std::uint16_t>(
                    std::clamp(numberKey(entity, "dmg"), 0.0, 65535.0));
            }
            else if ((cls == "func_breakable" || cls == "func_pushable") &&
                     brushRecordOfEntity[e] >= 0)
            {
                make = true;
                const bool box = cls == "func_pushable";
                record.kind =
                    static_cast<std::uint8_t>(box ? LogicKind::Pushable : LogicKind::Breakable);
                // "health" has no default: with none, the first blow does it.
                record.extra[0] = static_cast<std::uint32_t>(
                    std::clamp(std::lround(numberKey(entity, "health")), 0L, 1000000L));
                record.extra[1] = static_cast<std::uint32_t>(
                    std::clamp(std::lround(numberKey(entity, "material")), 0L, 9L));
                if (box)
                {
                    // "friction" is how HARD it is to push: the fastest it
                    // goes is 400 less that.
                    const double friction = std::min(numberKey(entity, "friction"), 399.0);
                    record.extra[2] =
                        static_cast<std::uint32_t>(fxFromDouble(400.0 - std::max(0.0, friction)));
                    // It moves through the world as the player's standing
                    // hull or the crouched one, whichever is nearer its
                    // height: those are the hulls a map has.
                    const double height = model != nullptr ? double(model->maxs[2]) - model->mins[2] : 64;
                    record.extra[3] = height < 54 ? 1 : 0;
                    if (spawnflags & 128)
                        record.flags |= kLogicBreaks;
                }
                else
                {
                    if (spawnflags & 1)
                        record.flags |= kLogicTriggerOnly;
                    if (spawnflags & 2)
                        record.flags |= kLogicTouch;
                    if (spawnflags & 4)
                        record.flags |= kLogicPressure;
                }
                if (spawnflags & 256)
                    record.flags |= kLogicInstant;
            }
            else if ((cls == "trigger_hurt" || cls == "trigger_push" ||
                      cls == "trigger_teleport") &&
                     model != nullptr && !(spawnflags & (cls == "trigger_hurt" ? 8 : 2)))
            {
                // (Flag 8 on a trigger_hurt and 2 on the others: not for the
                // player.)
                for (int a = 0; a < 3; ++a)
                {
                    record.mins[a] = static_cast<std::int16_t>(
                        std::clamp(std::floor(model->mins[a] + origin[a]), -32768.0, 32767.0));
                    record.maxs[a] = static_cast<std::int16_t>(
                        std::clamp(std::ceil(model->maxs[a] + origin[a]), -32768.0, 32767.0));
                }
                if (cls == "trigger_hurt")
                {
                    make = true;
                    record.kind = static_cast<std::uint8_t>(LogicKind::TriggerHurt);
                    record.extra[0] = static_cast<std::uint32_t>(
                        static_cast<std::int32_t>(std::lround(numberKey(entity, "dmg"))));
                    if (spawnflags & 2)
                        record.flags |= kLogicStartOpen; // starts off
                }
                else if (cls == "trigger_push")
                {
                    make = true;
                    record.kind = static_cast<std::uint8_t>(LogicKind::TriggerPush);
                    double direction[3];
                    moverDirection(entity, direction);
                    double speed = numberKey(entity, "speed");
                    if (speed == 0)
                        speed = 100;
                    for (int a = 0; a < 3; ++a)
                        record.move[a] = fxFromDouble(direction[a] * speed);
                }
                else
                {
                    // Where it sends them is another entity, found by name.
                    const std::string to(entity.value("target"));
                    for (const Entity &other : entities)
                    {
                        if (to.empty() || other.value("targetname") != to)
                            continue;
                        double at[3] = {0, 0, 0}, angles[3] = {0, 0, 0};
                        other.vec3("origin", at);
                        if (!other.vec3("angles", angles))
                            angles[1] = numberKey(other, "angle");
                        make = true;
                        record.kind = static_cast<std::uint8_t>(LogicKind::TriggerTeleport);
                        // The destination is where their FEET go: the middle
                        // of a standing player is 36 units above that.
                        record.move[0] = fxFromDouble(at[0]);
                        record.move[1] = fxFromDouble(at[1]);
                        record.move[2] = fxFromDouble(at[2] + 36);
                        record.extra[0] = static_cast<std::uint16_t>(
                            std::lround(angles[1] * 65536.0 / 360.0));
                        break;
                    }
                }
            }
            else if (cls == "func_wall_toggle")
            {
                make = true;
                record.kind = static_cast<std::uint8_t>(LogicKind::WallToggle);
            }
            else if (cls == "trigger_once" || cls == "trigger_multiple")
            {
                // A trigger that ignores the player is for a box that is
                // pushed into it (flag 4), or for monsters, which do not set
                // triggers off yet.
                if (model != nullptr && (!(spawnflags & kTriggerNoClients) || (spawnflags & 4)))
                {
                    make = true;
                    if (spawnflags & 4)
                        record.flags |= kLogicPushables;
                    if (spawnflags & kTriggerNoClients)
                        record.flags |= kLogicNoPlayer;
                    const bool once = cls == "trigger_once";
                    record.kind = static_cast<std::uint8_t>(once ? LogicKind::TriggerOnce
                                                                 : LogicKind::TriggerMultiple);
                    double wait = numberKey(entity, "wait");
                    if (wait == 0)
                        wait = 0.2;
                    record.waitMs = once ? -1 : millis(wait);
                    for (int a = 0; a < 3; ++a)
                    {
                        record.mins[a] = static_cast<std::int16_t>(
                            std::clamp(std::floor(model->mins[a] + origin[a]), -32768.0, 32767.0));
                        record.maxs[a] = static_cast<std::int16_t>(
                            std::clamp(std::ceil(model->maxs[a] + origin[a]), -32768.0, 32767.0));
                    }
                }
            }
            else if (cls == "func_ladder" && model != nullptr)
            {
                make = true;
                record.kind = static_cast<std::uint8_t>(LogicKind::Ladder);
                for (int a = 0; a < 3; ++a)
                {
                    record.mins[a] = static_cast<std::int16_t>(
                        std::clamp(std::floor(model->mins[a] + origin[a]), -32768.0, 32767.0));
                    record.maxs[a] = static_cast<std::int16_t>(
                        std::clamp(std::ceil(model->maxs[a] + origin[a]), -32768.0, 32767.0));
                }
            }
            else if (cls == "trigger_changelevel" && !entity.value("map").empty())
            {
                make = true;
                record.kind = static_cast<std::uint8_t>(LogicKind::ChangeLevel);
                record.extra[0] = stringOffset(entity.value("map"));
                record.extra[1] = hashOf(entity.value("landmark"));
                record.extra[2] = hashOf(entity.value("changetarget"));
                record.extra[3] =
                    static_cast<std::uint32_t>(std::max(0, millis(numberKey(entity, "changedelay"))));
                // Flag 2: only when fired by name. Otherwise walking in does
                // it, name or no name.
                if (!(spawnflags & 2) && model != nullptr)
                    record.flags |= kLogicTouch;
                if (model != nullptr)
                {
                    for (int a = 0; a < 3; ++a)
                    {
                        record.mins[a] = static_cast<std::int16_t>(
                            std::clamp(std::floor(model->mins[a] + origin[a]), -32768.0, 32767.0));
                        record.maxs[a] = static_cast<std::int16_t>(
                            std::clamp(std::ceil(model->maxs[a] + origin[a]), -32768.0, 32767.0));
                    }
                }
            }
            else if (cls == "trigger_autosave" && model != nullptr)
            {
                make = true;
                record.kind = static_cast<std::uint8_t>(LogicKind::AutoSave);
                for (int a = 0; a < 3; ++a)
                {
                    record.mins[a] = static_cast<std::int16_t>(
                        std::clamp(std::floor(model->mins[a] + origin[a]), -32768.0, 32767.0));
                    record.maxs[a] = static_cast<std::int16_t>(
                        std::clamp(std::ceil(model->maxs[a] + origin[a]), -32768.0, 32767.0));
                }
            }
            else if (cls == "env_global" && !entity.value("globalstate").empty())
            {
                make = true;
                record.kind = static_cast<std::uint8_t>(LogicKind::EnvGlobal);
                record.extra[0] = hashOf(entity.value("globalstate"));
                record.extra[1] = static_cast<std::uint32_t>(
                    std::clamp(numberKey(entity, "triggermode"), 0.0, 3.0));
                record.extra[2] = static_cast<std::uint32_t>(
                    std::clamp(numberKey(entity, "initialstate"), 0.0, 2.0));
                if (spawnflags & 1)
                    record.flags |= kLogicStartOpen;
            }
            else if (cls == "trigger_auto" || cls == "trigger_relay")
            {
                make = true;
                const bool automatic = cls == "trigger_auto";
                record.kind = static_cast<std::uint8_t>(automatic ? LogicKind::TriggerAuto
                                                                  : LogicKind::TriggerRelay);
                // "triggerstate": 0 off, 1 on, 2 toggle.
                const int state = static_cast<int>(numberKey(entity, "triggerstate"));
                record.useType = state == 0 ? kUseOff : (state == 1 ? kUseOn : kUseToggle);
                if (!automatic && (spawnflags & kRelayFireOnce))
                    record.flags |= kLogicFireOnce;
            }
            else if (isScript(cls))
            {
                make = true;
                record.kind = static_cast<std::uint8_t>(LogicKind::Script);
                const std::string who = lowered(entity.value("m_iszEntity"));
                // An actor by name -- or, if nothing is called that, the
                // nearest actor of that class.
                bool byName = false;
                for (const Entity &other : entities)
                    byName = byName || (!who.empty() && lowered(other.value("targetname")) == who &&
                                        actorOf(other).is);
                // (A name nobody in this map has is still a name: its owner
                // may be someone who walks in from the map next door.)
                if (byName || kindOfClass(who) < 0)
                    record.extra[0] = nameId(who);
                else
                    record.damage = static_cast<std::uint16_t>(kindOfClass(who) + 1);
                double radius = numberKey(entity, "m_flRadius");
                if (radius <= 0)
                    radius = 512;
                record.waitMs = static_cast<std::int32_t>(std::min(radius, 32000.0));
                const std::string idle = lowered(entity.value("m_iszIdle"));
                const std::string play = lowered(entity.value("m_iszPlay"));
                record.extra[1] = idle.empty() ? 0 : nameHash(idle.c_str());
                record.extra[2] = play.empty() ? 0 : nameHash(play.c_str());
                record.extra[3] = static_cast<std::uint32_t>(
                    std::clamp(static_cast<int>(numberKey(entity, "m_fMoveTo")), 0, 5));
                for (int a = 0; a < 3; ++a)
                    record.move[a] = fxFromDouble(origin[a]);
                double angles[3] = {0, 0, 0};
                if (!entity.vec3("angles", angles))
                    angles[1] = numberKey(entity, "angle");
                record.speed = static_cast<fx>(
                    static_cast<std::uint16_t>(std::lround(angles[1] * 65536.0 / 360.0)));
                // With no name it has nothing to wait for and plays as soon
                // as it has its actor; with an idle clip it takes its actor
                // at once and holds it there until used.
                if (!named || !idle.empty())
                    record.flags |= kLogicStartOpen;
                if (named)
                    record.flags |= kLogicUseOnly;
                if (spawnflags & 4)
                    record.flags |= kLogicToggle; // can be played again
            }
            else if (cls == "monstermaker" && actorOf(entity).is)
            {
                // Its children's places are filled in below, with the actors
                // (extra[0] and [1]).
                make = true;
                record.kind = static_cast<std::uint8_t>(LogicKind::Maker);
                const double delay = numberKey(entity, "delay");
                record.waitMs = millis(delay > 0.1 ? delay : 1.0);
                const int count = static_cast<int>(numberKey(entity, "monstercount"));
                record.extra[2] = count > 0 ? static_cast<std::uint32_t>(count) : 0;
                if (spawnflags & 4)
                    record.flags |= kLogicToggle; // one each time it is used
                else if (spawnflags & 1)
                    record.flags |= kLogicStartOpen;
            }
            else if ((cls == "func_healthcharger" || cls == "func_recharge") &&
                     brushRecordOfEntity[e] >= 0)
            {
                make = true;
                const bool suit = cls == "func_recharge";
                record.kind = static_cast<std::uint8_t>(suit ? LogicKind::SuitCharger
                                                             : LogicKind::HealthCharger);
                record.extra[0] = suit ? 75 : 50; // what it holds, at "normal"
            }
            else if (cls == "ambient_generic")
            {
                // Flags: 1 heard everywhere, 2 / 4 / 8 a small, medium or
                // large radius; 16 starts silent; 32 is played once each
                // time it is used rather than switched on and off.
                const std::string message(entity.value("message"));
                const bool spoken = !message.empty() && message[0] == '!';
                const bool looped = !spoken && !(spawnflags & 32);
                const int index = spoken ? sentenceIndex(message)
                                         : static_cast<int>(soundIndex(message, looped));
                if (index >= 0 && index != kNoSound)
                {
                    make = true;
                    record.kind = static_cast<std::uint8_t>(LogicKind::Ambient);
                    for (int a = 0; a < 3; ++a)
                        record.move[a] = fxFromDouble(origin[a]);
                    record.extra[0] = static_cast<std::uint32_t>(index);
                    record.extra[1] = (spawnflags & 1)   ? kRadiusEverywhere
                                      : (spawnflags & 2) ? kRadiusSmall
                                      : (spawnflags & 4) ? kRadiusMedium
                                                         : kRadiusLarge;
                    const double volume = numberKey(entity, "health", 10);
                    record.extra[2] = static_cast<std::uint32_t>(
                        std::clamp(std::lround(volume * 12.7), 0L, 127L));
                    record.extra[3] = spoken ? 1 : 0;
                    if (looped)
                    {
                        record.flags |= kLogicToggle;
                        if (!(spawnflags & 16))
                            record.flags |= kLogicStartOpen;
                    }
                }
            }
            else if (cls == "trigger_cdaudio" && model != nullptr)
            {
                make = true;
                record.kind = static_cast<std::uint8_t>(LogicKind::Music);
                record.flags |= kLogicTouch;
                record.extra[0] = static_cast<std::uint32_t>(std::max(0.0, numberKey(entity, "health")));
                for (int a = 0; a < 3; ++a)
                {
                    record.mins[a] = static_cast<std::int16_t>(
                        std::clamp(std::floor(model->mins[a] + origin[a]), -32768.0, 32767.0));
                    record.maxs[a] = static_cast<std::int16_t>(
                        std::clamp(std::ceil(model->maxs[a] + origin[a]), -32768.0, 32767.0));
                }
            }
            else if (cls == "target_cdaudio")
            {
                // A point with a radius: coming within it starts the track.
                make = true;
                record.kind = static_cast<std::uint8_t>(LogicKind::Music);
                record.flags |= kLogicTouch;
                record.extra[0] = static_cast<std::uint32_t>(std::max(0.0, numberKey(entity, "health")));
                const double radius = std::max(16.0, numberKey(entity, "radius", 128));
                for (int a = 0; a < 3; ++a)
                {
                    record.mins[a] = static_cast<std::int16_t>(
                        std::clamp(std::floor(origin[a] - radius), -32768.0, 32767.0));
                    record.maxs[a] = static_cast<std::int16_t>(
                        std::clamp(std::ceil(origin[a] + radius), -32768.0, 32767.0));
                }
            }
            else if (cls == "scripted_sentence" && named)
            {
                // Somebody says something, and the target fires as they
                // start.
                make = true;
                const int index = sentenceIndex(std::string(entity.value("sentence")));
                if (index >= 0)
                {
                    record.kind = static_cast<std::uint8_t>(LogicKind::Ambient);
                    for (int a = 0; a < 3; ++a)
                        record.move[a] = fxFromDouble(origin[a]);
                    record.extra[0] = static_cast<std::uint32_t>(index);
                    record.extra[1] = kRadiusEverywhere;
                    record.extra[2] = static_cast<std::uint32_t>(std::clamp(
                        std::lround(numberKey(entity, "volume", 10) * 12.7), 0L, 127L));
                    record.extra[3] = 1;
                }
                else
                {
                    // A line the book does not have: only the target fires.
                    record.kind = static_cast<std::uint8_t>(LogicKind::TriggerRelay);
                }
            }
            else if (cls == "multi_manager")
            {
                make = true;
                record.kind = static_cast<std::uint8_t>(LogicKind::MultiManager);
                if (spawnflags & kManagerThread)
                    record.flags |= kLogicThread;
                // Every key that is not one of the entity's own is a target,
                // and its value is how long after the manager is used it
                // fires. The editor writes a repeated name as "name#1".
                static const char *const kOwn[] = {"classname", "targetname", "origin",
                                                   "spawnflags", "angles", "angle", "wait",
                                                   "delay",     "master", "killtarget"};
                for (const auto &pair : entity.pairs)
                {
                    bool own = false;
                    for (const char *key : kOwn)
                        own = own || pair.first == key;
                    if (own)
                        continue;
                    std::string target = pair.first;
                    const std::size_t hash = target.find('#');
                    if (hash != std::string::npos)
                        target.resize(hash);
                    const double delay = std::atof(pair.second.c_str());
                    outputs.push_back(ManagerOutput{
                        nameId(target), static_cast<std::uint32_t>(std::max(0, millis(delay)))});
                }
                std::stable_sort(outputs.begin(), outputs.end(),
                                 [](const ManagerOutput &a, const ManagerOutput &b)
                                 { return a.delayMs < b.delayMs; });
            }
            else if (cls == "multisource")
            {
                make = true;
                record.kind = static_cast<std::uint8_t>(LogicKind::MultiSource);
            }
            else if (brushRecordOfEntity[e] >= 0 && named)
            {
                // A brush that does nothing but can be named: so that a
                // killtarget can take it away.
                make = true;
                record.kind = static_cast<std::uint8_t>(LogicKind::Static);
                // It stands there and can be taken away, but whatever its
                // class does is still missing.
                if (!isAccountedFor(cls))
                    ++out.unsupported[cls];
            }

            if (!make)
            {
                if (!isAccountedFor(cls) && brushRecordOfEntity[e] < 0)
                    ++out.unsupported[cls];
                else if (!isAccountedFor(cls) && cls.rfind("func_", 0) == 0)
                    ++out.unsupported[cls]; // drawn and solid, but its behaviour is missing
                continue;
            }

            // An env_global's "globalstate" is the state it SETS; on a
            // trigger_auto or a multisource it is one that has to be on.
            if (record.kind != static_cast<std::uint8_t>(LogicKind::EnvGlobal))
                record.globalState = hashOf(entity.value("globalstate"));
            record.name = nameId(entity.value("targetname"));
            // A multi_manager's "targets" are its keys, not a target key; and
            // a train's "target" is the corner it starts at, not something it
            // fires.
            if (record.kind != static_cast<std::uint8_t>(LogicKind::MultiManager) &&
                record.kind != static_cast<std::uint8_t>(LogicKind::Train) &&
                record.kind != static_cast<std::uint8_t>(LogicKind::TrackTrain) &&
                record.kind != static_cast<std::uint8_t>(LogicKind::TriggerTeleport))
                record.target = nameId(entity.value("target"));
            record.killTarget = nameId(entity.value("killtarget"));
            record.master = nameId(entity.value("master"));
            record.delayMs =
                static_cast<std::uint32_t>(std::max(0, millis(numberKey(entity, "delay"))));

            // What it sounds like: a door's or a button's sounds are numbers
            // into the game's fixed lists; what breaks sounds of what it is
            // made of.
            LogicSound sound{kNoSound, kNoSound};
            if (cls == "func_door" || cls == "func_door_rotating")
            {
                sound.move = numbered("doors/doormove", static_cast<int>(numberKey(entity, "movesnd")), 10);
                sound.stop = numbered("doors/doorstop", static_cast<int>(numberKey(entity, "stopsnd")), 8);
            }
            else if (cls == "func_button")
            {
                sound.move = numbered("buttons/button", static_cast<int>(numberKey(entity, "sounds")), 11);
            }
            else if (cls == "func_breakable" || cls == "func_pushable")
            {
                static const char *const kBreaks[] = {
                    "debris/bustglass1.wav",  "debris/bustcrate1.wav",    "debris/bustmetal1.wav",
                    "debris/bustflesh1.wav",  "debris/bustconcrete1.wav", "debris/bustceiling.wav",
                    "debris/bustmetal2.wav",  "debris/bustglass2.wav",    "debris/bustconcrete2.wav"};
                const int material = std::clamp(static_cast<int>(numberKey(entity, "material")), 0, 8);
                sound.move = soundIndex(kBreaks[material], false);
            }
            heard.push_back(sound);

            out.recordOfEntity[e] = static_cast<int>(out.records.size());
            out.records.push_back(record);
            managerOutputs.push_back(std::move(outputs));
            trainCorners.push_back(trainCorner);
        }

        // The actors: every entity that is a model this map had cooked.
        for (const std::string &model : models)
        {
            ModelRef ref{};
            std::strncpy(ref.name, model.c_str(), sizeof(ref.name) - 1);
            out.modelRefs.push_back(ref);
        }
        for (std::size_t e = 0; e < entities.size(); ++e)
        {
            const Entity &entity = entities[e];
            const ActorInfo info = actorOf(entity);
            if (!info.is)
                continue;
            const auto found = std::find(models.begin(), models.end(), modelNameOf(info.file));
            if (found == models.end())
            {
                ++out.unsupported[std::string(entity.className())];
                continue;
            }
            // (A maker whose model is missing keeps a record that makes
            // nothing: it has no places.)
            ActorRecord actor{};
            actor.model = static_cast<std::uint16_t>(found - models.begin());
            actor.name = nameId(entity.value(info.spare != 0 ? "netname" : "targetname"));
            actor.item = static_cast<std::uint32_t>(info.item);
            if (info.item != ItemKind::None && !entity.value("target").empty())
            {
                // What it fires when taken goes through a relay of its own:
                // then a multisource it targets counts it among its inputs
                // like anything else that does.
                LogicRecord relay{};
                relay.kind = static_cast<std::uint8_t>(LogicKind::TriggerRelay);
                relay.useType = kUseToggle;
                relay.waitMs = -1;
                relay.brush = kNoBrush;
                relay.name = nameId("\x01item" + std::to_string(e));
                relay.target = nameId(entity.value("target"));
                out.records.push_back(relay);
                heard.push_back(LogicSound{kNoSound, kNoSound});
                managerOutputs.emplace_back();
                trainCorners.push_back(kNoCorner);
                actor.target = relay.name;
            }
            double origin[3] = {0, 0, 0}, angles[3] = {0, 0, 0};
            entity.vec3("origin", origin);
            if (!entity.vec3("angles", angles))
                angles[1] = numberKey(entity, "angle");
            for (int a = 0; a < 3; ++a)
                actor.origin[a] = fxFromDouble(origin[a]);
            actor.yaw = static_cast<std::uint16_t>(std::lround(angles[1] * 65536.0 / 360.0));
            // "body" -1 on a scientist is "any head": one is picked, the
            // same one every time the map is cooked.
            int body = static_cast<int>(numberKey(entity, "body"));
            if (body < 0)
                body = static_cast<int>(e % 4);
            actor.body = static_cast<std::uint8_t>(std::clamp(body, 0, 255));
            actor.kind = static_cast<std::uint8_t>(info.kind);
            actor.clip = info.startClip.empty() ? 0 : nameHash(info.startClip.c_str());
            actor.flags = info.solid ? kActorSolid : 0;
            actor.light = 24;
            if (info.spare != 0)
            {
                // A maker's children: places kept, nobody in them yet.
                actor.flags |= kActorDormant;
                const int record = out.recordOfEntity[e];
                if (record >= 0)
                {
                    out.records[record].extra[0] = static_cast<std::uint32_t>(out.actors.size());
                    out.records[record].extra[1] = static_cast<std::uint32_t>(info.spare);
                }
                for (int n = 1; n < info.spare; ++n)
                    out.actors.push_back(actor);
            }
            out.actors.push_back(actor);
        }

        // Lists. A multi_manager's outputs are known; a multisource's inputs
        // are every record that targets it, which needs all the records first.
        for (std::size_t r = 0; r < out.records.size(); ++r)
        {
            LogicRecord &record = out.records[r];
            if (record.kind == static_cast<std::uint8_t>(LogicKind::Train) ||
                record.kind == static_cast<std::uint8_t>(LogicKind::TrackTrain))
            {
                record.firstAux = trainCorners[r];
                record.auxCount = 0;
                continue;
            }
            record.firstAux = static_cast<std::uint16_t>(out.aux.size());
            if (record.kind == static_cast<std::uint8_t>(LogicKind::MultiManager))
            {
                for (const ManagerOutput &output : managerOutputs[r])
                    out.aux.push_back(LogicAux{output.target, 0, output.delayMs});
            }
            else if (record.kind == static_cast<std::uint8_t>(LogicKind::MultiSource) &&
                     record.name != 0)
            {
                for (std::size_t other = 0; other < out.records.size(); ++other)
                {
                    // 32 inputs is the width of the state word; the game's own
                    // limit is the same.
                    // What targets it -- and a multi_manager targets what
                    // its keys name, which is how the game's own maps switch
                    // a multisource on from a sequence.
                    bool targets = out.records[other].target == record.name;
                    for (const ManagerOutput &output : managerOutputs[other])
                        targets = targets || output.target == record.name;
                    if (targets && out.aux.size() - record.firstAux < 32)
                        out.aux.push_back(LogicAux{static_cast<std::uint16_t>(other), 0, 0});
                }
            }
            record.auxCount = static_cast<std::uint16_t>(out.aux.size() - record.firstAux);
        }
        // Which of the common sounds this map has a use for: the player's
        // own always, a monster's only where there is one.
        auto use = [&](CommonSound from, CommonSound to)
        {
            for (int c = static_cast<int>(from); c <= static_cast<int>(to); ++c)
                soundUsed[c] = true;
        };
        use(CommonSound::Step1, CommonSound::Explode);
        for (const ActorRecord &actor : out.actors)
        {
            if (actor.kind == static_cast<std::uint8_t>(ActorKind::Headcrab))
                use(CommonSound::HeadcrabAlert, CommonSound::HeadcrabBite);
            else if (actor.kind == static_cast<std::uint8_t>(ActorKind::Zombie))
                use(CommonSound::ZombieAlert, CommonSound::ClawMiss);
            else if (actor.kind == static_cast<std::uint8_t>(ActorKind::Barnacle))
                use(CommonSound::BarnacleAlert, CommonSound::BarnacleDie);
            else if (actor.kind == static_cast<std::uint8_t>(ActorKind::Slave))
            {
                use(CommonSound::SlaveAlert, CommonSound::SlaveDie);
                use(CommonSound::ClawStrike, CommonSound::ClawMiss);
            }
            else if (actor.kind == static_cast<std::uint8_t>(ActorKind::Bullsquid))
                use(CommonSound::SquidAlert, CommonSound::SpitHit);
            else if (actor.kind == static_cast<std::uint8_t>(ActorKind::Houndeye))
                use(CommonSound::HoundAlert, CommonSound::HoundDie);
        }
        for (int c = 0; c < commonCount; ++c)
        {
            if (!soundUsed[c])
                out.sounds[c].clear();
        }
        out.logicSounds = std::move(heard);

        out.nameHashes.assign(nameIds.size() + 1, 0);
        for (const auto &entry : nameIds)
            out.nameHashes[entry.second] = nameHash(entry.first.c_str());
        out.names = static_cast<int>(nameIds.size());
    }
} // namespace hlds
