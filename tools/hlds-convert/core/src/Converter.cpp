#include "HlSound.hpp"
#include "HlSprite.hpp"
#include "hlds/UiCook.hpp"
#include "hlds/SoundCook.hpp"
#include "hlds/SpriteCook.hpp"
#include "hlds/Converter.hpp"
#include "hlds/EntityCook.hpp"
#include "hlds/MdlFile.hpp"
#include "hlds/ModelCook.hpp"

#include <cstdio>
#include <cstring>
#include <algorithm>
#include <cctype>

#include "HlLevelReader.hpp"
#include "hlds/AtomicWrite.hpp"
#include "hlds/BrushInfo.hpp"
#include "hlds/BspFile.hpp"
#include "hlds/Cooker.hpp"
#include "hlds/EntityParser.hpp"
#include "hlds/Materials.hpp"
#include "hlds/Pack.hpp"
#include "hlds/TextureCook.hpp"

namespace fs = std::filesystem;

namespace hlds
{
    namespace
    {
        std::string lower(std::string text)
        {
            for (char &c : text)
                c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            return text;
        }
    } // namespace

    Converter::Converter(ConvertOptions options) : options_(std::move(options)) {}

    fs::path Converter::dataDir(const fs::path &cardRoot)
    {
        return cardRoot / "_nds" / "hl_nds" / "data";
    }

    fs::path Converter::stageDir() const
    {
        if (!options_.stageRoot.empty())
            return dataDir(options_.stageRoot);
        // (A card root given with a slash at its end has no last name.)
        fs::path card = options_.cardRoot;
        if (!card.has_filename())
            card = card.parent_path();
        fs::path beside = card;
        beside += "-stage";
        return dataDir(beside);
    }

    void Converter::packInto(MapResult &result) const
    {
        PackReport report;
        std::string why;
        if (!packMap(stageDir(), dataDir(options_.cardRoot), result.name, options_.compress, report, why))
        {
            result.ok = false;
            result.error = "the map's archive was not made: " + why;
            return;
        }
        result.archiveKb = static_cast<int>(report.cardBytes / 1024);
        result.archiveRawKb = static_cast<int>(report.rawBytes / 1024);
        result.archiveMembers = report.members;
    }

    std::string Converter::preflight() const
    {
        std::error_code ec;
        if (options_.gameDir.empty() || !fs::is_directory(options_.gameDir / "maps", ec))
            return "The game folder has no 'maps' folder. Choose Half-Life's 'valve' folder.";
        if (listMaps().empty())
            return "The game folder's 'maps' folder holds no .bsp files.";
        if (options_.cardRoot.empty() || !fs::is_directory(options_.cardRoot, ec))
            return "The card folder does not exist.";
        if (options_.textures && findPtexconv(options_.ptexconv).empty())
            return "ptexconv was not found. It comes with BlocksDS; point the converter at it, "
                   "or convert without textures.";
        return {};
    }

    std::vector<std::string> Converter::listMaps() const
    {
        std::vector<std::string> names;
        std::error_code ec;
        for (auto it = fs::directory_iterator(options_.gameDir / "maps", ec);
             !ec && it != fs::directory_iterator(); it.increment(ec))
        {
            if (it->is_regular_file(ec) && lower(it->path().extension().string()) == ".bsp")
                names.push_back(lower(it->path().stem().string()));
        }
        std::sort(names.begin(), names.end());
        names.erase(std::unique(names.begin(), names.end()), names.end());
        return names;
    }

    bool Converter::findMap(const std::string &name, fs::path &out) const
    {
        // Without regard to case: the game's own files are lower-case, but
        // mods and old discs are not consistent.
        std::string wanted = lower(name);
        if (wanted.size() > 4 && wanted.substr(wanted.size() - 4) == ".bsp")
            wanted.resize(wanted.size() - 4);
        std::error_code ec;
        for (auto it = fs::directory_iterator(options_.gameDir / "maps", ec);
             !ec && it != fs::directory_iterator(); it.increment(ec))
        {
            if (lower(it->path().extension().string()) == ".bsp" &&
                lower(it->path().stem().string()) == wanted)
            {
                out = it->path();
                return true;
            }
        }
        return false;
    }

    namespace
    {
        /// What all of a map's sounds may take in the console's memory.
        constexpr std::size_t kMapSoundBytes = 350 * 1024;
        /// ... and a DSi, which has four times the memory: its sounds at
        /// the rate the game has them, up to this.
        constexpr std::size_t kMapSoundBytesDsi = 2 * 1024 * 1024;
        constexpr int kSoundRateDsi = 22050;

        /// `relative` under `root`, whatever case each part of it is in on
        /// disk (the game's files are named in whatever case their authors
        /// typed, and its maps name them in another). Empty if it is not
        /// there.
        fs::path findNoCase(const fs::path &root, const std::string &relative)
        {
            fs::path at = root;
            std::string rest = relative;
            for (char &c : rest)
                c = c == '\\' ? '/' : static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            for (std::size_t from = 0; from <= rest.size();)
            {
                const std::size_t slash = std::min(rest.find('/', from), rest.size());
                const std::string part = rest.substr(from, slash - from);
                from = slash + 1;
                if (part.empty())
                    continue;
                bool found = false;
                std::error_code ec;
                for (const auto &entry : fs::directory_iterator(at, ec))
                {
                    std::string name = entry.path().filename().string();
                    for (char &c : name)
                        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
                    if (name == part)
                    {
                        at = entry.path();
                        found = true;
                        break;
                    }
                }
                if (!found)
                    return fs::path();
            }
            return at;
        }
    } // namespace

    MapResult Converter::cookMap(const std::string &name)
    {
        MapResult result;
        result.name = lower(name);

        fs::path bspPath;
        if (!findMap(name, bspPath))
        {
            result.error = "no such map in " + (options_.gameDir / "maps").string();
            return result;
        }
        result.name = lower(bspPath.stem().string());

        BspFile bsp;
        if (!bsp.load(bspPath, result.error))
            return result;
        std::vector<Entity> entities;
        if (!parseEntities(bsp.entities, entities, result.error))
            return result;
        // (A multiplayer map: what it has of weapons the port has not got
        // yet is given as ones it has.)
        result.stoodIn = standInEntities(entities);
        std::vector<std::string> missingWads;
        WadSet wads;
        wads.openForMap(options_.gameDir, entities.empty() ? "" : entities[0].value("wad"),
                        missingWads);

        // The texture plan comes first: it decides which faces are drawn at
        // all, and geometry is built in cooked texels.
        const std::vector<BrushInfo> brushes = analyseBrushes(bsp, entities);
        // A map with a sky gives up room for its six sides.
        std::string skyName;
        if (!entities.empty())
        {
            skyName = std::string(entities[0].value("skyname"));
            for (char &c : skyName)
                c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            if (skyName.size() > 12 || !skyExists(options_.gameDir, skyName))
                skyName.clear();
        }
        // The models its actors are made of, with the clips this map plays.
        // Before the textures are planned: their skins come out of the same
        // video memory.
        struct ModelOut
        {
            std::string name;
            std::vector<std::uint8_t> bytes;
            /// The same with the model as the game has it for its finest
            /// level, for a DSi; empty where that is no more than `bytes`.
            std::vector<std::uint8_t> bytesDsi;
            std::vector<std::string> eventSounds; ///< CookedModel::eventSounds
            RgbaImage atlas;
            int triangles = 0, fullTriangles = 0; ///< body 0 at its finest, in each
            int far = 0;                          ///< ... and at its coarsest
        };
        std::vector<ModelOut> models;
        int modelTexels = 0;
        std::vector<ModelNeed> needs = collectModelNeeds(entities);
        // And of those who may walk in from next door: each neighbouring map
        // is looked at for who stands by the door.
        for (const Entity &entity : entities)
        {
            if (entity.className() != "trigger_changelevel")
                continue;
            const fs::path next =
                options_.gameDir / "maps" / (lower(std::string(entity.value("map"))) + ".bsp");
            BspFile other;
            std::vector<Entity> otherEntities;
            std::string ignored;
            if (other.load(next, ignored) && parseEntities(other.entities, otherEntities, ignored))
                addVisitorNeeds(entities, otherEntities, std::string(entity.value("landmark")), needs);
        }
        // What the player may be holding by the time they are in this map.
        // The game's own maps are known: nothing before the crowbar is found
        // (c1a1), the pistol from c1a1f, the shotgun from c1a2. Any other
        // map gets them all.
        {
            const std::string map = lower(name);
            const bool before = map.rfind("c0", 0) == 0 || map.rfind("c1a0", 0) == 0;
            const bool early = map.rfind("c1a1", 0) == 0;
            struct Held
            {
                const char *name;
                bool wanted;
                std::vector<std::string> clips;
            };
            const Held kHeld[] = {
                // (The crowbar's clips are those the game plays: its three
                // swings at the air, the two that land, out and away.)
                {kViewModelName, !before,
                 {"draw", "holster", "attack1miss", "attack2", "attack3", "attack2hit", "attack3hit"}},
                // (The others' likewise, as the game's own weapon code asks
                // for them -- HlMonsters.hpp's weaponsTick: every way each
                // is held still, its shot, its last round, its reloads.
                // The first of each is what it is held as between them.)
                {"v_9mmhandgun", !before && map != "c1a1" && map != "c1a1a",
                 {"idle1", "idle2", "idle3", "shoot", "shoot_empty", "reload", "reload_noshot", "draw"}},
                {"v_shotgun", !before && !early,
                 {"sm_idle", "idle4", "deepidle", "shoot", "shoot_big", "start_reload", "reload", "pump", "draw"}},
                // (Grenades are first found in c1a1b.)
                {"v_grenade", !before && map != "c1a1" && map != "c1a1a" && map != "c1a1f",
                 {"idle", "fidget", "pinpull", "throw1", "throw2", "throw3", "draw"}},
                // (The MP5 is first found in c1a3, the .357 in c1a4g.)
                {"v_9mmar", !before && !early && map.rfind("c1a2", 0) != 0,
                 {"longidle", "idle1", "shoot", "grenade", "reload", "deploy"}},
                {"v_357",
                 !before && !early && map.rfind("c1a2", 0) != 0 && map.rfind("c1a3", 0) != 0 && map != "c1a4" &&
                     map != "c1a4b" && map != "c1a4d" && map != "c1a4e" && map != "c1a4f" && map != "c1a4k" &&
                     map != "c1a4i",
                 {"idle1", "idle2", "idle3", "fidget1", "fire1", "reload", "draw"}},
            };
            for (const Held &one : kHeld)
            {
                if (!one.wanted)
                    continue;
                ModelNeed held;
                held.name = one.name;
                held.file = std::string("models/") + one.name + ".mdl";
                held.clips = one.clips;
                needs.push_back(held);
            }
        }
        // A multiplayer map: the others in the game are seen. One model for
        // all of them, the game's own player in the suit, with what a body
        // does whole -- standing, running, crouching, jumping, dying.
        addMultiplayerNeeds(entities, needs);
        // One model, with a skin of at most `skinTexels`. False if it cannot
        // be read or cooked (said once, in the warnings).
        auto cookOne = [&](const ModelNeed &need, int skinTexels, ModelOut &out, bool quiet)
        {
            // The map names the file in whatever case its author typed.
            fs::path file = options_.gameDir;
            bool found = true;
            std::string rest = need.file;
            for (char &c : rest)
                c = c == '\\' ? '/' : c;
            for (std::size_t at = 0; at <= rest.size() && found;)
            {
                const std::size_t slash = std::min(rest.find('/', at), rest.size());
                const std::string part = rest.substr(at, slash - at);
                at = slash + 1;
                if (part.empty())
                    continue;
                found = false;
                std::error_code ec;
                for (const auto &entry : fs::directory_iterator(file, ec))
                {
                    if (lower(entry.path().filename().string()) == part)
                    {
                        file = entry.path();
                        found = true;
                        break;
                    }
                }
            }
            MdlFile mdl;
            std::string why;
            if (!found || need.name.size() >= sizeof(hldata::ModelRef::name) || !mdl.load(file, why))
            {
                if (!quiet)
                    result.warnings.push_back("model '" + need.file + "' cannot be read" +
                                              (why.empty() ? "" : ": " + why));
                return false;
            }
            std::vector<std::string> clips;
            if (need.firstSequence && !mdl.sequences.empty())
                clips.push_back(mdl.sequences[0].label);
            clips.insert(clips.end(), need.clips.begin(), need.clips.end());
            CookedModel cooked;
            ModelCookReport modelReport;
            out = ModelOut();
            out.name = need.name;
            const bool cookedOk = cookModel(mdl, clips, cooked, modelReport, why, skinTexels);
            out.eventSounds = cooked.eventSounds;
            if (!cookedOk ||
                !writeModel(cooked, out.bytes, why))
            {
                if (!quiet)
                    result.warnings.push_back("model '" + need.file + "' cannot be cooked: " + why);
                return false;
            }
            // A clip the hand is told to play and its model has not got is
            // a weapon that stands still when it should move: said.
            if (!quiet && need.name.rfind("v_", 0) == 0)
            {
                for (const std::string &missing : modelReport.missingClips)
                    result.warnings.push_back("model '" + need.file + "' has no clip '" + missing + "'");
            }
            out.atlas = std::move(cooked.atlas);
            out.triangles = modelReport.triangles;
            out.fullTriangles = modelReport.sourceTriangles;
            out.far = modelReport.levels.empty() ? modelReport.triangles : modelReport.levels.back().triangles;
            // A DSi's file, where the game's own mesh is more than that.
            if (modelReport.sourceTriangles > modelReport.triangles)
            {
                CookedModel whole;
                ModelCookReport wholeReport;
                std::string ignored;
                if (!cookModel(mdl, clips, whole, wholeReport, ignored, skinTexels, true) ||
                    !writeModel(whole, out.bytesDsi, ignored))
                    out.bytesDsi.clear();
            }
            return true;
        };
        // What lies on the floor is a few pixels on the screen; what is held
        // fills a corner of it; people and monsters are in between.
        std::vector<int> skinOf;
        std::vector<std::size_t> needOf;
        for (std::size_t n = 0; n < needs.size(); ++n)
        {
            const ModelNeed &need = needs[n];
            // (What lies about to be picked up, and the pieces things come
            // apart into, are small on screen: a small skin does.)
            const int skin = need.name.rfind("w_", 0) == 0 || need.name.find("gib") != std::string::npos ||
                                     need.name == "v_tripmine"
                                 ? 64 * 64
                                 : kModelSkinTexels;
            ModelOut out;
            if (!cookOne(need, skin, out, false))
                continue;
            modelTexels += out.atlas.width * out.atlas.height;
            models.push_back(std::move(out));
            skinOf.push_back(skin);
            needOf.push_back(n);
        }
        // Too much between them for the map's own textures to have room: the
        // largest skin is halved, again and again, until it is not.
        while (modelTexels > kMapSkinTexels)
        {
            std::size_t largest = models.size();
            int most = 64 * 64;
            for (std::size_t m = 0; m < models.size(); ++m)
            {
                const int texels = models[m].atlas.width * models[m].atlas.height;
                if (texels > most)
                {
                    most = texels;
                    largest = m;
                }
            }
            if (largest == models.size())
                break; // nothing left to take from
            ModelOut smaller;
            if (!cookOne(needs[needOf[largest]], most / 2, smaller, true) ||
                smaller.atlas.width * smaller.atlas.height >= most)
            {
                skinOf[largest] = 0;
                break;
            }
            modelTexels += smaller.atlas.width * smaller.atlas.height - most;
            models[largest] = std::move(smaller);
        }
        for (const ModelOut &out : models)
        {
            result.modelKb += static_cast<int>(out.bytes.size() / 1024);
            result.modelKbDsi += static_cast<int>((out.bytesDsi.empty() ? out.bytes : out.bytesDsi).size() / 1024);
            result.modelTriangles += out.triangles;
            result.modelTrianglesFar += out.far;
            result.modelTrianglesFull += out.fullTriangles;
        }
        result.models = static_cast<int>(models.size());
        result.modelTexelKb = modelTexels / 1024;

        const int skyTexels = (skyName.empty() ? 0 : 6 * kSkySize * kSkySize) + modelTexels;
        const TexturePlan plan =
            planTextures(bsp, textureNeeds(bsp, brushes, findHiddenTextures(bsp, wads)),
                         kWorldTexelBudget - skyTexels,
                         kPaletteColourBudget - (skyName.empty() ? 0 : 6 * 256) -
                             static_cast<int>(models.size()) * kModelSkinColours - kSpriteColourBudget,
                         kAlphaTexelBudget - kSpriteTexelBudget);
        if (!plan.fits)
            result.warnings.push_back("its textures do not fit the DS even at their smallest");
        result.texturesUsed = plan.usedTextures;
        result.texelKb = plan.texels / 1024;
        result.alphaTexelKb = plan.alphaTexels / 1024;

        CookOptions cookOptions;
        cookOptions.lightScale = options_.lightScale;
        cookOptions.lightPower = options_.lightPower;
        cookOptions.guardedMerges = options_.guardedMerges;
        cookOptions.skyName = skyName;
        for (const auto &model : models)
            cookOptions.models.push_back(model.name);
        cookOptions.viewModel = kViewModelName;
        // (What the models sound like as they move: the level has to know
        // where each of those sounds is.)
        for (const ModelOut &model : models)
        {
            for (const std::string &sound : model.eventSounds)
            {
                if (std::find(cookOptions.modelSounds.begin(), cookOptions.modelSounds.end(), sound) ==
                    cookOptions.modelSounds.end())
                    cookOptions.modelSounds.push_back(sound);
            }
        }
        // What the game's textures are made of, for the sound of a step.
        MaterialBook materials;
        {
            std::vector<std::uint8_t> text;
            std::string ignored;
            const fs::path file = findNoCase(options_.gameDir / "sound", "materials.txt");
            if (!file.empty() && readFile(file, text, ignored))
                materials = parseMaterials(std::string(text.begin(), text.end()));
        }
        cookOptions.materials = &materials;
        // The game's own graph of the map's nodes, where it came with one.
        NodGraph nodGraph;
        {
            std::vector<std::uint8_t> bytes;
            std::string ignored;
            const fs::path file = findNoCase(options_.gameDir / "maps" / "graphs", result.name + ".nod");
            if (!file.empty() && readFile(file, bytes, ignored) && parseNodGraph(bytes, nodGraph, ignored))
                cookOptions.nodGraph = &nodGraph;
        }
        // What the map's people say is looked up in the game's own list.
        SentenceBook book;
        {
            std::vector<std::uint8_t> text;
            std::string ignored;
            const fs::path file = findNoCase(options_.gameDir / "sound", "sentences.txt");
            if (!file.empty() && readFile(file, text, ignored))
                book = parseSentences(std::string(text.begin(), text.end()));
        }
        cookOptions.sentences = &book;
        // ... and what it writes on the screen, in its titles.txt.
        TitlesBook titles;
        {
            std::vector<std::uint8_t> text;
            std::string ignored;
            const fs::path file = findNoCase(options_.gameDir, "titles.txt");
            if (!file.empty() && readFile(file, text, ignored))
                titles = parseTitles(std::string(text.begin(), text.end()));
        }
        cookOptions.titles = &titles;
        // The chapter this map is part of (asked of all the game's maps,
        // once: each chapter is named by the map that opens it).
        {
            static std::filesystem::path scanned;
            static std::vector<std::pair<std::string, std::string>> titled;
            if (scanned != options_.gameDir)
            {
                titled = chapterTitles(options_.gameDir, &titles);
                scanned = options_.gameDir;
            }
            cookOptions.chapter = chapterOf(lower(name), titled);
        }
        CookedLevel level;
        CookReport report;
        std::vector<std::uint8_t> bytes;
        if (!cookLevel(bsp, cookOptions, plan, level, report, result.error))
            return result;

        // The map's sounds: each read, brought to the console's rate and
        // packed. All of a map's sounds are in memory while it is played, so
        // they have an allowance; over it, the longest are done again at
        // half the rate (their names say so) until they fit.
        {
            struct One
            {
                std::size_t index = 0;
                WavData wav;
                std::uint32_t sourceHash = 0;
                CookedSound cooked;
                CookedSound twin; ///< a DSi's copy, if it is to have a better one
            };
            std::vector<One> made;
            std::size_t total = 0;
            // (Not the words of spoken lines, which the console reads off
            // the card as they are said: those are not in the allowance,
            // and stay at the full rate.)
            std::vector<char> kept(level.soundPaths.size() + 1, 0);
            hldata::soundsKept(level.logic.data(), static_cast<std::uint32_t>(level.logic.size()),
                               level.logicSounds.data(),
                               static_cast<std::uint32_t>(level.logicSounds.size()),
                               static_cast<std::uint32_t>(level.soundPaths.size()),
                               reinterpret_cast<bool *>(kept.data()), level.modelSounds.data(),
                               static_cast<std::uint32_t>(level.modelSounds.size()));
            for (std::size_t n = 0; n < level.soundPaths.size(); ++n)
            {
                if (level.soundPaths[n].empty())
                    continue;
                One one;
                one.index = n;
                std::vector<std::uint8_t> source;
                std::string why;
                const fs::path file = findNoCase(options_.gameDir / "sound", level.soundPaths[n]);
                if (file.empty() || !readFile(file, source, why) || !parseWav(source, one.wav, why))
                {
                    // Not in the game folder: the map does without it.
                    level.sounds[n] = hldata::SoundRef{};
                    ++result.soundsMissing;
                    continue;
                }
                one.sourceHash = hldata::crc32(source.data(), static_cast<std::uint32_t>(source.size()));
                one.cooked = encodeSound(one.wav, hldata::kSoundRate, level.soundLoops[n]);
                if (kept[n])
                    total += one.cooked.data.size();
                made.push_back(std::move(one));
            }
            std::uint32_t shortest = 8000;
            while (total > kMapSoundBytes)
            {
                // The longest that is still at the full rate and long enough
                // for the loss to be worth it (and, if that is not enough,
                // the shorter ones after all).
                One *longest = nullptr;
                for (One &one : made)
                {
                    if (kept[one.index] && one.cooked.rate == hldata::kSoundRate &&
                        one.cooked.samples > shortest &&
                        (longest == nullptr || one.cooked.samples > longest->cooked.samples))
                        longest = &one;
                }
                if (longest == nullptr && shortest > 2000)
                {
                    shortest = 2000;
                    continue;
                }
                if (longest == nullptr)
                    break;
                total -= longest->cooked.data.size();
                longest->cooked =
                    encodeSound(longest->wav, hldata::kSoundRate / 2, level.soundLoops[longest->index]);
                total += longest->cooked.data.size();
                ++result.soundsHalved;
            }
            if (total > kMapSoundBytes)
                result.warnings.push_back("its sounds are " + std::to_string(total / 1024) +
                                          " KB, over what a DS has room for");
            // A DSi's copies: whatever the game has at a better rate than a
            // DS is given, at that rate (22 kHz at most) -- while they fit
            // what a DSi is allowed, the longest going without first.
            std::size_t totalDsi = total;
            for (One &one : made)
            {
                if (!kept[one.index] || one.wav.rate <= one.cooked.rate)
                    continue;
                one.twin = encodeSound(one.wav, std::min(one.wav.rate, kSoundRateDsi), level.soundLoops[one.index]);
                totalDsi += one.twin.data.size() - one.cooked.data.size();
            }
            while (totalDsi > kMapSoundBytesDsi)
            {
                One *longest = nullptr;
                for (One &one : made)
                {
                    if (!one.twin.data.empty() &&
                        (longest == nullptr || one.twin.data.size() > longest->twin.data.size()))
                        longest = &one;
                }
                if (longest == nullptr)
                    break;
                totalDsi -= longest->twin.data.size() - longest->cooked.data.size();
                longest->twin = CookedSound();
            }
            for (const One &one : made)
            {
                const bool loops = level.soundLoops[one.index];
                const std::string name =
                    soundCardName(level.soundPaths[one.index], one.cooked.rate, loops);
                hldata::SoundRef ref{};
                std::strncpy(ref.name, name.c_str(), sizeof(ref.name) - 1);
                // (Its better copy is written below, the same way.)
                const bool twinned = !one.twin.data.empty();
                ref.twin = twinned ? static_cast<std::uint8_t>(one.twin.rate / 1000) : std::uint8_t(0);
                level.sounds[one.index] = ref;
                result.soundsTwinned += twinned ? 1 : 0;
                for (int which = 0; which < (twinned ? 2 : 1); ++which)
                {
                    const CookedSound &cooked = which == 0 ? one.cooked : one.twin;
                    const std::string cardName = soundCardName(level.soundPaths[one.index], cooked.rate, loops);
                    // (The hash is of the source and of how it was cooked:
                    // the file is made again only if either changed.)
                    // (The last: which way of deciding what goes round the
                    // file was cooked by -- 2: only what has a cue point.)
                    const std::uint32_t settings[] = {static_cast<std::uint32_t>(cooked.rate), loops ? 1u : 0u,
                                                      hldata::kSoundVersion, 2u};
                    const std::uint32_t hash =
                        one.sourceHash ^
                        hldata::crc32(reinterpret_cast<const std::uint8_t *>(settings), sizeof(settings)) * 131u;
                    const fs::path file = stageDir() / "snd" / (cardName + ".hsn");
                    std::vector<std::uint8_t> existing;
                    std::string why;
                    CookedSound old;
                    hldata::SoundHeader oldHeader;
                    if (readFile(file, existing, why) && unpackSound(existing, old, oldHeader) &&
                        oldHeader.sourceHash == hash)
                        continue;
                    if (!writeFileAtomic(file, packSound(cooked, hash), result.error))
                        return result;
                }
            }
            result.sounds = static_cast<int>(made.size());
            result.soundKb = static_cast<int>(total / 1024);
            result.soundKbDsi = static_cast<int>(totalDsi / 1024);
        }
        // The music the map starts: each track decoded once, to something
        // that can be read off the card as it plays.
        for (const hldata::LogicRecord &record : level.logic)
        {
            if (record.kind != static_cast<std::uint8_t>(hldata::LogicKind::Music))
                continue;
            const int track = static_cast<int>(record.extra[0]);
            if (track < 2 || track > 99)
                continue;
            ++result.musicTracks;
            char cardName[16];
            hldata::musicFileName(track, cardName, sizeof(cardName));
            const fs::path file = dataDir(options_.cardRoot) / "music" / cardName;
            // ... and the same in stereo for a DSi, whose DSP plays it.
            char stereoName[16];
            hldata::musicFileName(track, stereoName, sizeof(stereoName), true);
            const fs::path stereoFile = dataDir(options_.cardRoot) / "music" / stereoName;
            std::error_code ec;
            const bool haveMono = fs::exists(file, ec) && fs::file_size(file, ec) > sizeof(hldata::MusicHeader);
            const bool haveStereo =
                fs::exists(stereoFile, ec) && fs::file_size(stereoFile, ec) > sizeof(hldata::StereoHeader);
            if (haveMono && haveStereo)
                continue;
            // The game's track N is the file numbered N - 1: the first
            // track of its disc was the program.
            char source[32];
            std::snprintf(source, sizeof(source), "half-life%02d.mp3", track - 1);
            const fs::path mp3 = findNoCase(options_.gameDir / "media", source);
            std::vector<std::uint8_t> packed;
            WavData music;
            std::vector<std::int16_t> left, right;
            std::string why;
            if (mp3.empty() || !readFile(mp3, packed, why) || !decodeMp3(packed, music, why, &left, &right))
                continue; // a game folder without its music has none
            if (!haveMono)
            {
                const std::vector<std::int16_t> samples =
                    resample(music.samples, music.rate, hldata::kMusicRate);
                hldata::MusicHeader header{};
                std::memcpy(header.magic, hldata::kMusicMagic, 4);
                header.version = hldata::kSoundVersion;
                header.rate = hldata::kMusicRate;
                header.bytes = static_cast<std::uint32_t>(samples.size());
                std::vector<std::uint8_t> out(sizeof(header) + samples.size());
                std::memcpy(out.data(), &header, sizeof(header));
                for (std::size_t i = 0; i < samples.size(); ++i)
                    out[sizeof(header) + i] = static_cast<std::uint8_t>(samples[i] >> 8);
                if (!writeFileAtomic(file, out, result.error))
                    return result;
            }
            if (!haveStereo &&
                !writeFileAtomic(stereoFile, encodeMusicStereo(left, right, music.rate), result.error))
                return result;
        }

        // The sprites: each cooked to its atlas, the common ones as the
        // port's own effects want them, a map's own at a middling size; and
        // while they are more than a map may have, the largest is made
        // smaller. (A sprite the game folder has not got is left with no
        // frames: nothing is drawn for it.)
        struct SpriteOut
        {
            std::size_t index = 0;
            SprFile spr;
            int cell = 32, frames = 8;
            CookedSprite cooked;
            std::uint32_t sourceHash = 0;
        };
        std::vector<SpriteOut> sprites;
        {
            static const int kCommon[][2] = {{32, 8}, {32, 2}, {16, 6}, {32, 1}, {32, 1}, {32, 3}, {16, 4}, {32, 1}};
            int texels = 0;
            for (std::size_t n = 0; n < level.spritePaths.size(); ++n)
            {
                SpriteOut one;
                one.index = n;
                std::vector<std::uint8_t> source;
                std::string why;
                const fs::path file = findNoCase(options_.gameDir, level.spritePaths[n]);
                if (file.empty() || !readFile(file, source, why) || !one.spr.parse(source, why))
                {
                    ++result.spritesMissing;
                    continue;
                }
                if (n < static_cast<std::size_t>(hldata::CommonSprite::Count))
                {
                    one.cell = kCommon[n][0];
                    one.frames = kCommon[n][1];
                }
                one.sourceHash = hldata::crc32(source.data(), static_cast<std::uint32_t>(source.size()));
                if (!cookSprite(one.spr, one.cell, one.frames, level.spriteLights[n], one.cooked, why))
                    continue;
                texels += one.cooked.atlas.width * one.cooked.atlas.height;
                sprites.push_back(std::move(one));
            }
            while (texels > kSpriteTexelBudget)
            {
                SpriteOut *largest = nullptr;
                for (SpriteOut &one : sprites)
                {
                    const int size = one.cooked.atlas.width * one.cooked.atlas.height;
                    if (size > 256 && (one.cell > 8 || one.frames > 1) &&
                        (largest == nullptr ||
                         size > largest->cooked.atlas.width * largest->cooked.atlas.height))
                        largest = &one;
                }
                if (largest == nullptr)
                    break;
                texels -= largest->cooked.atlas.width * largest->cooked.atlas.height;
                // (Fewer frames first, while it has many; then smaller ones.)
                if (largest->frames > 4)
                    largest->frames /= 2;
                else if (largest->cell > 8)
                    largest->cell /= 2;
                else
                    largest->frames = 1;
                std::string why;
                cookSprite(largest->spr, largest->cell, largest->frames, level.spriteLights[largest->index],
                           largest->cooked, why);
                texels += largest->cooked.atlas.width * largest->cooked.atlas.height;
            }
            if (texels > kSpriteTexelBudget)
                result.warnings.push_back("its sprites are " + std::to_string(texels / 1024) +
                                          "K texels, over what is kept for them");
            for (const SpriteOut &one : sprites)
            {
                hldata::SpriteRef ref = one.cooked.ref;
                // (Its name says whether it was cooked as light: the same
                // file may be wanted both ways.)
                const std::string name =
                    spriteName(level.spritePaths[one.index]) + (level.spriteLights[one.index] ? "-l" : "");
                std::strncpy(ref.name, name.c_str(), sizeof(ref.name) - 1);
                level.sprites[one.index] = ref;
            }
            result.sprites = static_cast<int>(sprites.size());
            result.spriteTexelKb = texels / 1024;
        }

        if (!writeLevel(level, bytes, result.error))
            return result;
        result.warnings.insert(result.warnings.end(), report.warnings.begin(),
                               report.warnings.end());

        if (options_.textures)
        {
            const fs::path exe = findPtexconv(options_.ptexconv);
            if (exe.empty())
            {
                result.error = "ptexconv not found";
                return result;
            }
            const fs::path temp = fs::temp_directory_path() / ("hlds-convert-" + result.name);
            PtexconvEncoder encoder(exe, temp);
            TextureCookReport texReport;
            const bool ok = cookTextures(bsp, wads, plan, encoder,
                                         stageDir() / "tex", texReport,
                                         result.error) &&
                            (skyName.empty() ||
                             cookSky(options_.gameDir, skyName, encoder,
                                     stageDir() / "tex", result.error));
            // The models' skins: one for each size a model's skin is made
            // in (a map crowded with models has them smaller), shared by
            // every map that uses the model at that size, and encoded again
            // only if the picture has changed.
            for (const auto &model : models)
            {
                if (!ok || model.atlas.width == 0)
                    continue;
                const std::uint32_t settings[] = {kModelSkinColours, encoder.version()};
                const std::uint32_t hash =
                    hldata::crc32(model.atlas.rgba.data(),
                                  static_cast<std::uint32_t>(model.atlas.rgba.size())) ^
                    hldata::crc32(reinterpret_cast<const std::uint8_t *>(settings), sizeof(settings)) * 131u;
                const fs::path skin = stageDir() / "mdl" /
                                      hldata::modelSkinFile(model.name.c_str(), model.atlas.width,
                                                            model.atlas.height);
                std::vector<std::uint8_t> existing;
                std::string ignored;
                CookedTexture old;
                hldata::TextureHeader oldHeader;
                if (readFile(skin, existing, ignored) && unpackTexture(existing, old, oldHeader) &&
                    oldHeader.sourceHash == hash)
                    continue;
                CookedTexture encoded;
                std::string why;
                if (!encoder.encode(model.atlas, kModelSkinColours, TextureFormat::Tex4x4, encoded, why) ||
                    !writeFileAtomic(skin, packTexture(encoded, 0, hash), why))
                    result.warnings.push_back("the skin of model '" + model.name + "' was not made: " + why);
            }
            // The sprites' atlases, likewise.
            for (const SpriteOut &one : sprites)
            {
                if (!ok)
                    break;
                const hldata::SpriteRef &ref = level.sprites[one.index];
                const std::uint32_t settings[] = {static_cast<std::uint32_t>(one.cell),
                                                  static_cast<std::uint32_t>(one.frames),
                                                  level.spriteLights[one.index] ? 1u : 0u, encoder.version(), 1u};
                const std::uint32_t hash =
                    one.sourceHash ^
                    hldata::crc32(reinterpret_cast<const std::uint8_t *>(settings), sizeof(settings)) * 131u;
                const fs::path file = stageDir() / "spr" /
                                      hldata::spriteFile(ref.name, ref.width, ref.height);
                std::vector<std::uint8_t> existing;
                std::string ignored;
                CookedTexture old;
                hldata::TextureHeader oldHeader;
                if (readFile(file, existing, ignored) && unpackTexture(existing, old, oldHeader) &&
                    oldHeader.sourceHash == hash)
                    continue;
                CookedTexture encoded;
                std::string why;
                if (!encoder.encode(one.cooked.atlas, hldata::kA3I5Colours, TextureFormat::A3I5, encoded, why) ||
                    !writeFileAtomic(file, packTexture(encoded, hldata::kTextureA3I5, hash), why))
                    result.warnings.push_back("the sprite '" + std::string(ref.name) + "' was not made: " + why);
            }
            std::error_code ec;
            fs::remove_all(temp, ec);
            if (!ok)
                return result;
            result.texturesEncoded = texReport.encoded;
            for (const std::string &missing : texReport.missing)
                result.warnings.push_back("texture '" + missing + "' was found in no WAD");
        }

        // The models themselves, under the map's name: each holds the clips
        // THIS map plays.
        for (const auto &model : models)
        {
            const fs::path file =
                stageDir() / "mdl" / result.name / (model.name + ".hmd");
            if (!writeFileAtomic(file, model.bytes, result.error))
                return result;
            // (A DSi's, beside it -- and none left over from a cook that
            // had one where this has not.)
            const fs::path dsi = stageDir() / "mdl" / result.name / (model.name + ".dsi.hmd");
            std::error_code gone;
            if (model.bytesDsi.empty())
                fs::remove(dsi, gone);
            else if (!writeFileAtomic(dsi, model.bytesDsi, result.error))
                return result;
        }
        result.actors = static_cast<int>(level.actors.size());

        // The level last: it is what makes the map loadable, so it lands only
        // once everything it names is on the card.
        const fs::path outPath = stageDir() / "maps" / (result.name + ".hlm");
        // (Beside it, the map's entity text as the game had it: for whoever
        // reads the stage, not for the console.)
        const std::vector<std::uint8_t> text(level.entityText.begin(), level.entityText.end());
        if (!writeFileAtomic(stageDir() / "maps" / (result.name + ".ent"), text, result.error) ||
            !writeFileAtomic(outPath, bytes, result.error))
            return result;

        result.ok = true;
        result.levelBytes = bytes.size();
        result.surfaces = report.surfaces;
        result.brushes = report.brushes;
        result.logicRecords = report.logicRecords;
        result.pathNodes = report.paths.nodes;
        result.pathLinks = report.paths.links;
        result.pathDoorLinks = report.paths.doorLinks;
        result.pathRefused = report.paths.refused;
        result.pathDrops = report.paths.dropLinks;
        result.pathFromGraph = report.paths.fromGraph;
        result.unsupportedClasses = static_cast<int>(report.unsupported.size());
        for (const auto &entry : report.unsupported)
            result.unsupportedEntities += entry.second;
        result.faces = report.faces;
        result.faceVertices = report.faceVertices;
        result.vertices = report.vertices;
        result.streamKb = static_cast<int>(level.gxStream.size() * 4 / 1024);
        result.worstPolygons = level.worstPolygons;
        result.worstVertices = level.worstVertices;
        return result;
    }

    CommonResult Converter::cookCommon()
    {
        CommonResult result;
        const fs::path ui = stageDir();
        std::string why;
        // A file is written only if it is not already what it would be.
        auto put = [&](const fs::path &file, const std::vector<std::uint8_t> &bytes) -> bool
        {
            std::vector<std::uint8_t> existing;
            std::string ignored;
            if (readFile(file, existing, ignored) && existing == bytes)
                return true;
            return writeFileAtomic(file, bytes, why);
        };

        // The fonts: the game's two smaller menu faces, and the one its
        // credits are in for titles.
        struct Face
        {
            const char *wad, *lump;
            bool outlined;
        };
        const Face kFaces[hldata::kFontCount] = {
            {"fonts.wad", "FONT0", true}, {"fonts.wad", "FONT1", true}, {"gfx.wad", "CREDITSFONT", false}};
        for (int n = 0; n < hldata::kFontCount; ++n)
        {
            std::vector<std::uint8_t> lump;
            QFont font;
            const fs::path wad = findNoCase(options_.gameDir, kFaces[n].wad);
            if (wad.empty() || !readWadLump(wad, kFaces[n].lump, lump, why) || !parseQFont(lump, font, why) ||
                !put(ui / hldata::kFontFiles[n], cookFont(font, kFaces[n].outlined)))
            {
                result.warnings.push_back(std::string("the font ") + kFaces[n].lump + " was not made" +
                                          (why.empty() ? "" : ": " + why));
                continue;
            }
            ++result.fonts;
        }

        // The menu's sounds.
        for (int n = 0; n < hldata::kUiSoundCount; ++n)
        {
            std::vector<std::uint8_t> source;
            WavData wav;
            const fs::path file = findNoCase(options_.gameDir / "sound", hldata::kUiSoundSources[n]);
            if (file.empty() || !readFile(file, source, why) || !parseWav(source, wav, why))
            {
                result.warnings.push_back(std::string("the sound ") + hldata::kUiSoundSources[n] + " was not made");
                continue;
            }
            const std::uint32_t hash = hldata::crc32(source.data(), static_cast<std::uint32_t>(source.size())) ^
                                       hldata::kSoundVersion;
            const CookedSound cooked = encodeSound(wav, hldata::kSoundRate, false);
            if (put(ui / "snd" / (std::string(hldata::kUiSoundFiles[n]) + ".hsn"), packSound(cooked, hash)))
                ++result.sounds;
        }

        // The pictures: these go through the texture encoder.
        if (!options_.textures)
            return result;
        const fs::path exe = findPtexconv(options_.ptexconv);
        if (exe.empty())
            return result;
        const fs::path temp = fs::temp_directory_path() / "hlds-convert-ui";
        PtexconvEncoder encoder(exe, temp);
        auto texture = [&](const RgbaImage &image, TextureFormat format, int colours, std::uint32_t flags,
                           const char *name) -> bool
        {
            const std::uint32_t settings[] = {static_cast<std::uint32_t>(colours), flags, encoder.version()};
            const std::uint32_t hash =
                hldata::crc32(image.rgba.data(), static_cast<std::uint32_t>(image.rgba.size())) ^
                hldata::crc32(reinterpret_cast<const std::uint8_t *>(settings), sizeof(settings)) * 131u;
            std::vector<std::uint8_t> existing;
            std::string ignored;
            CookedTexture old;
            hldata::TextureHeader oldHeader;
            if (readFile(ui / name, existing, ignored) && unpackTexture(existing, old, oldHeader) &&
                oldHeader.sourceHash == hash)
                return true;
            CookedTexture encoded;
            return encoder.encode(image, colours, format, encoded, why) &&
                   writeFileAtomic(ui / name, packTexture(encoded, flags, hash), why);
        };
        {
            SprFile hud, crosshairs;
            const fs::path hudFile = findNoCase(options_.gameDir, "sprites/320hud2.spr");
            const fs::path crossFile = findNoCase(options_.gameDir, "sprites/crosshairs.spr");
            if (!hudFile.empty() && hud.load(hudFile, why) && !crossFile.empty() && crosshairs.load(crossFile, why) &&
                texture(hudAtlas(hud, crosshairs), TextureFormat::A3I5, hldata::kA3I5Colours, hldata::kTextureA3I5,
                        hldata::kHudAtlasFile))
                result.hud = true;
            else
                result.warnings.push_back("the head-up display's pictures were not made" +
                                          (why.empty() ? std::string() : ": " + why));
        }
        {
            // The weapons' pictures for the touch screen's slots.
            SprFile sheet;
            const fs::path file = findNoCase(options_.gameDir, "sprites/320hud1.spr");
            if (!file.empty() && sheet.load(file, why) && put(ui / hldata::kWeaponIconsFile, weaponIcons(sheet)))
                result.icons = true;
            else
                result.warnings.push_back("the weapons' pictures were not made" +
                                          (why.empty() ? std::string() : ": " + why));
        }
        {
            // The marks shots and blood leave.
            WadFile decals;
            int missing = 0;
            const fs::path file = findNoCase(options_.gameDir, "decals.wad");
            if (!file.empty() && decals.open(file, why) &&
                texture(decalAtlas(decals, missing), TextureFormat::A3I5, hldata::kA3I5Colours, hldata::kTextureA3I5,
                        hldata::kDecalAtlasFile) &&
                missing == 0)
                result.decals = true;
            else
                result.warnings.push_back("the marks' pictures were not all made" +
                                          (why.empty() ? std::string() : ": " + why));
        }
        {
            // The game's title, for the first map.
            SprFile sheet;
            const fs::path file = findNoCase(options_.gameDir, "sprites/320hud3.spr");
            if (!file.empty() && sheet.load(file, why) && put(ui / hldata::kTitleFile, titlePicture(sheet)))
                result.title = true;
            else
                result.warnings.push_back("the game's title was not made" + (why.empty() ? std::string() : ": " + why));
        }
        {
            std::vector<std::uint8_t> lump;
            IndexedImage picture;
            const fs::path wad = findNoCase(options_.gameDir, "cached.wad");
            if (!wad.empty() && readWadLump(wad, "LOADING", lump, why) && parseQPic(lump, picture, why) &&
                texture(loadingTexture(picture), TextureFormat::Tex4x4, 1024, 0, hldata::kLoadingFile))
                result.loading = true;
            else
                result.warnings.push_back("the loading picture was not made" +
                                          (why.empty() ? std::string() : ": " + why));
        }
        std::error_code ec;
        fs::remove_all(temp, ec);
        return result;
    }

    int Converter::cookMaps(const std::vector<std::string> &names,
                            const std::function<void(std::size_t, const std::string &)> &progress,
                            const std::function<void(const MapResult &)> &done,
                            const std::atomic<bool> *cancel, std::string &error)
    {
        int failed = 0;
        common_ = cookCommon();
        {
            PackReport report;
            std::string why;
            if (!packCommon(stageDir(), dataDir(options_.cardRoot), options_.compress, report, why))
                common_.warnings.push_back("the common archive was not made: " + why);
        }
        for (std::size_t i = 0; i < names.size(); ++i)
        {
            if (cancel != nullptr && cancel->load())
                break;
            if (progress)
                progress(i, names[i]);
            MapResult result = cookMap(names[i]);
            if (result.ok)
                packInto(result);
            if (!result.ok)
                ++failed;
            if (done)
                done(result);
        }

        if (!writeManifest(error))
            return failed + 1;
        return failed;
    }

    int Converter::packAll(const std::function<void(const MapResult &)> &done, std::string &error)
    {
        int failed = 0;
        PackReport commonReport;
        std::string why;
        if (!packCommon(stageDir(), dataDir(options_.cardRoot), options_.compress, commonReport, why))
            common_.warnings.push_back("the common archive was not made: " + why);
        std::vector<std::string> names;
        std::error_code ec;
        for (auto it = fs::directory_iterator(stageDir() / "maps", ec);
             !ec && it != fs::directory_iterator(); it.increment(ec))
        {
            if (it->path().extension() == ".hlm")
                names.push_back(it->path().stem().string());
        }
        std::sort(names.begin(), names.end());
        for (const std::string &name : names)
        {
            MapResult result;
            result.name = name;
            result.ok = true;
            packInto(result);
            if (!result.ok)
                ++failed;
            if (done)
                done(result);
        }
        if (!writeManifest(error))
            return failed + 1;
        return failed;
    }

    bool Converter::writeManifest(std::string &error) const
    {
        // The manifest is rebuilt from what is ACTUALLY on the card, not from
        // what this run did: a run of one map must not forget the hundred an
        // earlier run cooked, and a cancelled run must not list maps it never
        // reached. Each level's own header is the source.
        hldata::Manifest manifest;
        manifest.converterVersion = kConverterVersion;
        const fs::path dir = dataDir(options_.cardRoot);
        std::string previousStart;
        {
            std::vector<std::uint8_t> old;
            std::string ignored;
            hldata::Manifest previous;
            if (readFile(dir / "manifest.bin", old, ignored) &&
                hldata::readManifest(old.data(), old.size(), previous) ==
                    hldata::ManifestError::Ok)
                previousStart = previous.startMap;
        }

        std::error_code ec;
        for (auto it = fs::directory_iterator(dir / "maps", ec);
             !ec && it != fs::directory_iterator(); it.increment(ec))
        {
            if (it->path().extension() != ".npac")
                continue;
            std::vector<std::uint8_t> bytes;
            std::string ignored;
            std::size_t archiveBytes = 0;
            hldata::LevelView view;
            // Every member inflated, the level put together and its
            // checksum taken: an archive that does not come back whole is
            // not listed, so the DS never offers a map it would then refuse.
            if (!unpackLevel(it->path(), bytes, archiveBytes, ignored) ||
                view.open(bytes.data(), static_cast<std::uint32_t>(bytes.size())) !=
                    hldata::LevelError::Ok)
                continue;
            hldata::ManifestMap map;
            map.name = it->path().stem().string();
            map.levelBytes = static_cast<std::uint32_t>(bytes.size());
            map.archiveBytes = static_cast<std::uint32_t>(archiveBytes);
            map.worstPolygons = view.header->worstPolygons;
            map.worstVertices = view.header->worstVertices;
            map.textureCount = static_cast<std::uint16_t>(view.textureCount);
            for (std::uint32_t e = 0; e < view.entityCount; ++e)
            {
                if (view.entities[e].classId == static_cast<std::uint16_t>(hldata::EntityClass::InfoPlayerDeathmatch) &&
                    map.spawns < 255)
                    ++map.spawns;
            }
            manifest.maps.push_back(std::move(map));
        }
        std::sort(manifest.maps.begin(), manifest.maps.end(),
                  [](const hldata::ManifestMap &a, const hldata::ManifestMap &b)
                  { return a.name < b.name; });

        // The start map: what was asked for, else what it was, else the
        // game's first map, else whatever there is.
        for (const std::string &candidate :
             {lower(options_.startMap), previousStart, std::string(kDefaultStartMap)})
        {
            if (!candidate.empty() && manifest.find(candidate) != nullptr)
            {
                manifest.startMap = candidate;
                break;
            }
        }
        if (manifest.startMap.empty() && !manifest.maps.empty())
            manifest.startMap = manifest.maps.front().name;

        if (!manifest.maps.empty() &&
            !writeFileAtomic(dir / "manifest.bin", hldata::writeManifest(manifest), error))
            return false;
        return true;
    }
    std::string chapterOf(const std::string &map, const std::vector<std::pair<std::string, std::string>> &titled)
    {
        const std::string head = map.substr(0, 4);
        std::string like, likeFirst, before;
        for (const auto &[name, title] : titled)
        {
            if (name.substr(0, 4) == head)
            {
                if (likeFirst.empty())
                    likeFirst = title;
                if (name <= map)
                    like = title;
            }
            if (name <= map)
                before = title;
        }
        if (!like.empty())
            return like;
        if (!likeFirst.empty())
            return likeFirst;
        if (!before.empty())
            return before;
        // (The game's first maps name none: their chapter's title is shown
        // by the ride itself.)
        return map.rfind("c0a0", 0) == 0 ? "C0A0TITLE" : std::string();
    }

    std::vector<std::pair<std::string, std::string>> chapterTitles(const std::filesystem::path &gameDir,
                                                                   const TitlesBook *titles)
    {
        std::vector<std::pair<std::string, std::string>> out;
        std::error_code ignored;
        const fs::path maps = findNoCase(gameDir, "maps");
        if (maps.empty())
            return out;
        for (const auto &entry : fs::directory_iterator(maps, ignored))
        {
            if (entry.path().extension() != ".bsp")
                continue;
            // The map's entities are a text at the place its first lump
            // says; the world is the first of them.
            std::vector<std::uint8_t> bytes;
            std::string why;
            if (!readFile(entry.path(), bytes, why) || bytes.size() < 12)
                continue;
            std::uint32_t at = 0, length = 0;
            std::memcpy(&at, bytes.data() + 4, 4);
            std::memcpy(&length, bytes.data() + 8, 4);
            if (at > bytes.size() || length > bytes.size() - at)
                continue;
            const std::string text(bytes.begin() + at, bytes.begin() + at + length);
            const std::size_t end = text.find("}");
            const std::string world = text.substr(0, end);
            const std::size_t key = world.find("\"chaptertitle\"");
            if (key == std::string::npos)
                continue;
            const std::size_t open = world.find('"', key + 14);
            const std::size_t close = open == std::string::npos ? open : world.find('"', open + 1);
            if (close == std::string::npos)
                continue;
            std::string title = world.substr(open + 1, close - open - 1);
            // (One that says nothing opens no chapter.)
            const Title *said = titles != nullptr ? titles->find(title) : nullptr;
            bool blank = said == nullptr;
            if (said != nullptr)
            {
                blank = true;
                for (char c : said->text)
                    blank = blank && (c == ' ' || c == '\n' || c == '\r' || c == '\t');
            }
            if (title.empty() || blank)
                continue;
            std::string name = entry.path().stem().string();
            for (char &c : name)
                c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            out.emplace_back(name, title);
        }
        std::sort(out.begin(), out.end());
        return out;
    }
} // namespace hlds
