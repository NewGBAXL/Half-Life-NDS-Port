#pragma once

// A cooked map set up the way the DS sets it up, for the tests that play one:
// its collision, its logic, its models and its actors.

#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include "HlLevelReader.hpp"
#include "HlMonsters.hpp"
#include "HlTransition.hpp"
#include "TestBspBuilder.hpp"
#include "hlds/Cooker.hpp"
#include "hlds/EntityCook.hpp"
#include "hlds/EntityParser.hpp"
#include "hlds/MdlFile.hpp"
#include "hlds/ModelCook.hpp"

namespace hldstest
{
    using namespace hlds;
    using namespace hldata;

    /// A cooked map with the logic set up the way the DS sets it up.
    struct Game
    {
        BspFile bsp;
        std::vector<std::uint8_t> bytes;
        LevelView view;
        MoveWorld move;
        std::vector<MoverHull> movers;
        std::vector<BrushState> brushStates;
        std::vector<LogicState> states;
        std::vector<LadderVolume> ladders;
        std::vector<std::uint8_t> cornerState;
        // The map's models, cooked as the converter cooks them, and its actors.
        std::vector<std::vector<std::uint32_t>> modelBlocks;
        std::vector<ModelView> modelViews;
        std::vector<ActorState> actorStates;
        std::vector<SolidBox> actorBoxes;
        ActorWorld actors;
        LogicWorld logic;
        PlayerState player;
        bool ok = false;
        /// What the map has made heard: the queue the logic fills, and how
        /// often each sound (by its index in the level's Sounds) has been in
        /// it since the game began. drain() moves one to the other.
        SoundQueue heard;
        std::vector<int> timesHeard;
        int sentencesHeard = 0;
        void drain()
        {
            for (int i = 0; i < heard.count; ++i)
            {
                const SoundEvent &event = heard.events[i];
                if (event.flags & kSoundSentence)
                    ++sentencesHeard;
                else if (event.sound < timesHeard.size())
                    ++timesHeard[event.sound];
            }
            heard.count = 0;
        }
        int times(CommonSound sound) const
        {
            return static_cast<std::size_t>(sound) < timesHeard.size()
                       ? timesHeard[static_cast<std::size_t>(sound)]
                       : 0;
        }

        int recordOfKind(LogicKind kind, int nth = 0) const
        {
            for (std::uint32_t i = 0; i < view.logicCount; ++i)
            {
                if (view.logic[i].kind == static_cast<std::uint8_t>(kind) && nth-- == 0)
                    return static_cast<int>(i);
            }
            return -1;
        }
        /// The tram: the track train that is known by name across maps (the
        /// forklifts and the flatbed of the ride are track trains too).
        int tram() const
        {
            for (std::uint32_t i = 0; i < view.logicCount; ++i)
            {
                if (view.logic[i].kind == static_cast<std::uint8_t>(LogicKind::TrackTrain) &&
                    view.logic[i].extra[2] != 0)
                    return static_cast<int>(i);
            }
            return -1;
        }
        void frame()
        {
            logicTick(logic, player, 33);
            logic.viewTurn = 0;
            monstersTick(logic, player, 33);
            playerMove(player, MoveCommand(), move, kPlayerTick);
            drain();
        }
    };

    /// Cook the models `g`'s map needs, the way the converter does: its own
    /// actors' and those of whoever may walk in from next door.
    inline void cookModels(Game &g, const char *dir, CookOptions &options)
    {
        std::vector<Entity> entities;
        std::string error;
        if (dir == nullptr || !parseEntities(g.bsp.entities, entities, error))
            return;
        std::vector<ModelNeed> needs = collectModelNeeds(entities);
        for (const Entity &entity : entities)
        {
            if (entity.className() != "trigger_changelevel")
                continue;
            BspFile other;
            std::vector<Entity> otherEntities;
            std::string map(entity.value("map"));
            for (char &c : map)
                c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            if (other.load(std::string(dir) + "/maps/" + map + ".bsp", error) &&
                parseEntities(other.entities, otherEntities, error))
                addVisitorNeeds(entities, otherEntities, std::string(entity.value("landmark")), needs);
        }
        for (const ModelNeed &need : needs)
        {
            MdlFile mdl;
            CookedModel cooked;
            ModelCookReport report;
            std::vector<std::uint8_t> bytes;
            std::vector<std::string> clips;
            if (!mdl.load(std::string(dir) + "/" + need.file, error))
                continue;
            if (need.firstSequence && !mdl.sequences.empty())
                clips.push_back(mdl.sequences[0].label);
            clips.insert(clips.end(), need.clips.begin(), need.clips.end());
            if (!cookModel(mdl, clips, cooked, report, error) || !writeModel(cooked, bytes, error))
                continue;
            g.modelBlocks.emplace_back((bytes.size() + 3) / 4);
            std::memcpy(g.modelBlocks.back().data(), bytes.data(), bytes.size());
            ModelView view;
            view.open(reinterpret_cast<const std::uint8_t *>(g.modelBlocks.back().data()),
                      static_cast<std::uint32_t>(bytes.size()));
            g.modelViews.push_back(view);
            options.models.push_back(need.name);
        }
    }

    inline void finish(Game &g, Globals *globals, const char *modelsFrom = nullptr)
    {
        std::string error;
        CookedLevel level;
        CookReport report;
        CookOptions cookOptions;
        cookModels(g, modelsFrom, cookOptions);
        // What the map's people say, if the game's list is there to be read.
        static SentenceBook book;
        if (modelsFrom != nullptr && book.lines.empty())
        {
            std::ifstream file(std::string(modelsFrom) + "/sound/sentences.txt", std::ios::binary);
            const std::string text((std::istreambuf_iterator<char>(file)),
                                   std::istreambuf_iterator<char>());
            book = parseSentences(text);
        }
        cookOptions.sentences = modelsFrom != nullptr ? &book : nullptr;
        g.ok = cookLevel(g.bsp, cookOptions, TexturePlan(), level, report, error) &&
               writeLevel(level, g.bytes, error) &&
               g.view.open(g.bytes.data(), static_cast<std::uint32_t>(g.bytes.size())) ==
                   LevelError::Ok;
        if (!g.ok)
        {
            std::fprintf(stderr, "game failed: %s\n", error.c_str());
            return;
        }
        const LevelView &v = g.view;
        g.move.standing = Hull{v.clipNodes, v.planes, v.planeTypes, v.models[0].clipStanding};
        g.move.crouched = Hull{v.clipNodes, v.planes, v.planeTypes, v.models[0].clipCrouched};
        g.movers.resize(v.brushCount);
        g.brushStates.resize(v.brushCount);
        g.states.resize(v.logicCount);
        g.cornerState.resize(v.cornerCount + 1);
        for (std::uint32_t i = 0; i < v.brushCount; ++i)
        {
            const Model &model = v.models[v.brushes[i].model];
            g.movers[i].standing = Hull{v.clipNodes, v.planes, v.planeTypes, model.clipStanding};
            g.movers[i].crouched = Hull{v.clipNodes, v.planes, v.planeTypes, model.clipCrouched};
        }
        g.move.nodes = v.nodes;
        g.move.leaves = v.leaves;
        g.move.headNode = v.models[0].headNode;
        g.move.vis = v.vis;
        g.move.visSize = v.visSize;
        g.move.visLeafCount = v.header->visLeafCount;
        g.move.movers = g.movers.data();
        g.move.moverCount = static_cast<int>(g.movers.size());
        int ladderCount = 0;
        for (std::uint32_t i = 0; i < v.logicCount; ++i)
            ladderCount += v.logic[i].kind == static_cast<std::uint8_t>(LogicKind::Ladder);
        g.ladders.resize(ladderCount + 1);
        g.move.ladders = g.ladders.data();
        g.move.ladderCount = ladderCount;

        g.logic.records = v.logic;
        g.logic.recordCount = static_cast<int>(v.logicCount);
        g.logic.aux = v.logicAux;
        g.logic.corners = v.corners;
        g.logic.brushes = v.brushes;
        g.logic.brushCount = static_cast<int>(v.brushCount);
        g.logic.states = g.states.data();
        g.logic.brushStates = g.brushStates.data();
        g.logic.movers = g.movers.data();
        g.logic.world = &g.move;
        g.logic.ladders = g.ladders.data();
        g.logic.globals = globals;
        g.logic.sounds = v.logicSoundCount != 0 ? v.logicSounds : nullptr;
        g.logic.heard = &g.heard;
        g.timesHeard.assign(v.soundCount + 1, 0);
        g.logic.cornerState = g.cornerState.data();
        g.logic.cornerCount = static_cast<int>(v.cornerCount);
        g.actorStates.assign(v.actorCount + kMaxVisitors, ActorState());
        g.actorBoxes.assign(v.actorCount + kMaxVisitors, SolidBox());
        g.actors.states = g.actorStates.data();
        g.actors.count = static_cast<int>(g.actorStates.size());
        g.actors.models = g.modelViews.data();
        g.actors.modelCount = static_cast<int>(g.modelViews.size());
        g.actors.boxes = g.actorBoxes.data();
        g.move.boxes = g.actorBoxes.data();
        g.move.boxCount = static_cast<int>(g.actorBoxes.size());
        g.actors.nodes = v.pathNodes;
        g.actors.next = v.pathNext;
        g.actors.nodeCount = static_cast<int>(v.pathNodeCount);
        g.logic.actors = &g.actors;
        g.logic.nameHashes = v.nameHashes;
        g.logic.nameCount = static_cast<int>(v.nameCount);
        logicInit(g.logic);
        actorsInit(g.actors, v.actors, static_cast<int>(v.actorCount), v.nameHashes, g.move);

        g.player = PlayerState();
        for (std::uint32_t i = 0; i < v.entityCount; ++i)
        {
            if (v.entities[i].classId != static_cast<std::uint16_t>(EntityClass::InfoPlayerStart))
                continue;
            for (int a = 0; a < 3; ++a)
                g.player.origin[a] = v.entities[i].origin[a];
            break;
        }
    }
} // namespace hldstest
