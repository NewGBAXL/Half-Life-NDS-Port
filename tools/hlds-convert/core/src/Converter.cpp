#include "HlSound.hpp"
#include "hlds/SoundCook.hpp"
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
            RgbaImage atlas;
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
                {kViewModelName, !before, {"idle1", "attack1", "attack1miss", "draw"}},
                {"v_9mmhandgun", !before && map != "c1a1" && map != "c1a1a",
                 {"idle1", "shoot", "reload", "draw"}},
                {"v_shotgun", !before && !early, {"sm_idle", "shoot", "reload", "draw"}},
                // (Grenades are first found in c1a1b.)
                {"v_grenade", !before && map != "c1a1" && map != "c1a1a" && map != "c1a1f",
                 {"idle", "throw1", "draw"}},
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
            if (!cookModel(mdl, clips, cooked, modelReport, why, skinTexels) ||
                !writeModel(cooked, out.bytes, why))
            {
                if (!quiet)
                    result.warnings.push_back("model '" + need.file + "' cannot be cooked: " + why);
                return false;
            }
            out.atlas = std::move(cooked.atlas);
            return true;
        };
        // What lies on the floor is a few pixels on the screen; what is held
        // fills a corner of it; people and monsters are in between.
        std::vector<int> skinOf;
        std::vector<std::size_t> needOf;
        for (std::size_t n = 0; n < needs.size(); ++n)
        {
            const ModelNeed &need = needs[n];
            const int skin = need.name.rfind("w_", 0) == 0 ? 64 * 64 : kModelSkinTexels;
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
            result.modelKb += static_cast<int>(out.bytes.size() / 1024);
        result.models = static_cast<int>(models.size());
        result.modelTexelKb = modelTexels / 1024;

        const int skyTexels = (skyName.empty() ? 0 : 6 * kSkySize * kSkySize) + modelTexels;
        const TexturePlan plan =
            planTextures(bsp, textureNeeds(bsp, brushes, findHiddenTextures(bsp, wads)),
                         kWorldTexelBudget - skyTexels,
                         kPaletteColourBudget - (skyName.empty() ? 0 : 6 * 256) -
                             static_cast<int>(models.size()) * kModelSkinColours);
        if (!plan.fits)
            result.warnings.push_back("its textures do not fit the DS even at their smallest");
        result.texturesUsed = plan.usedTextures;
        result.texelKb = plan.texels / 1024;
        result.alphaTexelKb = plan.alphaTexels / 1024;

        CookOptions cookOptions;
        cookOptions.lightGamma = options_.lightGamma;
        cookOptions.guardedMerges = options_.guardedMerges;
        cookOptions.skyName = skyName;
        for (const auto &model : models)
            cookOptions.models.push_back(model.name);
        cookOptions.viewModel = kViewModelName;
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
                               reinterpret_cast<bool *>(kept.data()));
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
            while (total > kMapSoundBytes)
            {
                // The longest that is still at the full rate and long enough
                // for the loss to be worth it.
                One *longest = nullptr;
                for (One &one : made)
                {
                    if (kept[one.index] && one.cooked.rate == hldata::kSoundRate &&
                        one.cooked.samples > 8000 &&
                        (longest == nullptr || one.cooked.samples > longest->cooked.samples))
                        longest = &one;
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
            for (const One &one : made)
            {
                const bool loops = level.soundLoops[one.index];
                const std::string name =
                    soundCardName(level.soundPaths[one.index], one.cooked.rate, loops);
                hldata::SoundRef ref{};
                std::strncpy(ref.name, name.c_str(), sizeof(ref.name) - 1);
                level.sounds[one.index] = ref;
                // (The hash is of the source and of how it was cooked: the
                // file is made again only if either changed.)
                const std::uint32_t settings[] = {static_cast<std::uint32_t>(one.cooked.rate),
                                                  loops ? 1u : 0u, hldata::kSoundVersion};
                const std::uint32_t hash =
                    one.sourceHash ^
                    hldata::crc32(reinterpret_cast<const std::uint8_t *>(settings), sizeof(settings)) * 131u;
                const fs::path file = dataDir(options_.cardRoot) / "snd" / (name + ".hsn");
                std::vector<std::uint8_t> existing;
                std::string why;
                CookedSound old;
                hldata::SoundHeader oldHeader;
                if (readFile(file, existing, why) && unpackSound(existing, old, oldHeader) &&
                    oldHeader.sourceHash == hash)
                    continue;
                if (!writeFileAtomic(file, packSound(one.cooked, hash), result.error))
                    return result;
            }
            result.sounds = static_cast<int>(made.size());
            result.soundKb = static_cast<int>(total / 1024);
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
                                         dataDir(options_.cardRoot) / "tex", texReport,
                                         result.error) &&
                            (skyName.empty() ||
                             cookSky(options_.gameDir, skyName, encoder,
                                     dataDir(options_.cardRoot) / "tex", result.error));
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
                const fs::path skin = dataDir(options_.cardRoot) / "mdl" /
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
                dataDir(options_.cardRoot) / "mdl" / result.name / (model.name + ".hmd");
            if (!writeFileAtomic(file, model.bytes, result.error))
                return result;
        }
        result.actors = static_cast<int>(level.actors.size());

        // The level last: it is what makes the map loadable, so it lands only
        // once everything it names is on the card.
        const fs::path outPath = dataDir(options_.cardRoot) / "maps" / (result.name + ".hlm");
        if (!writeFileAtomic(outPath, bytes, result.error))
            return result;

        result.ok = true;
        result.levelBytes = bytes.size();
        result.surfaces = report.surfaces;
        result.brushes = report.brushes;
        result.logicRecords = report.logicRecords;
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

    int Converter::cookMaps(const std::vector<std::string> &names,
                            const std::function<void(std::size_t, const std::string &)> &progress,
                            const std::function<void(const MapResult &)> &done,
                            const std::atomic<bool> *cancel, std::string &error)
    {
        int failed = 0;
        for (std::size_t i = 0; i < names.size(); ++i)
        {
            if (cancel != nullptr && cancel->load())
                break;
            if (progress)
                progress(i, names[i]);
            const MapResult result = cookMap(names[i]);
            if (!result.ok)
                ++failed;
            if (done)
                done(result);
        }

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
            if (it->path().extension() != ".hlm")
                continue;
            std::vector<std::uint8_t> bytes;
            std::string ignored;
            hldata::LevelView view;
            // Checksum included: a level that does not open is not listed, so
            // the DS never offers a map it would then refuse.
            if (!readFile(it->path(), bytes, ignored) ||
                view.open(bytes.data(), static_cast<std::uint32_t>(bytes.size())) !=
                    hldata::LevelError::Ok)
                continue;
            hldata::ManifestMap map;
            map.name = it->path().stem().string();
            map.levelBytes = static_cast<std::uint32_t>(bytes.size());
            map.worstPolygons = view.header->worstPolygons;
            map.worstVertices = view.header->worstVertices;
            map.textureCount = static_cast<std::uint16_t>(view.textureCount);
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
            return failed + 1;
        return failed;
    }
} // namespace hlds
