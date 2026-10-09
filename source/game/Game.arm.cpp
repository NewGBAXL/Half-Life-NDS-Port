#include "game/Game.hpp"

#include <cstdlib>
#include <cstring>
#include <new>

#include "DebugLog.hpp"
#include "data/LevelFile.hpp"
#include "data/ModelCache.hpp"
#include "game/Input.hpp"
#include "game/Player.hpp"
#include "game/Trace.hpp"
#include "audio/Sound.hpp"
#include "render/HudDraw.hpp"
#include "render/ScreenFade.hpp"
#include "render/TopText.hpp"
#include "render/BottomScreen.hpp"
#include "render/Camera.hpp"

// ARM code (the file name says so): the logic is inline in shared/ and is
// instantiated here, with its 64-bit arithmetic.

namespace hlrt
{
    Game game;

    namespace
    {
        using namespace hldata;

        /// The longest stretch of time advanced in one go. A frame that took
        /// longer -- a load, a hitch -- is played in slow motion rather than
        /// letting a door cover half its travel between two collision checks.
        constexpr int kMaxVblanks = 4;
    } // namespace

    void Game::destroy()
    {
        std::free(brushStates_);
        std::free(movers_);
        std::free(states_);
        std::free(ladders_);
        std::free(cornerState_);
        cornerState_ = nullptr;
        std::free(actorStates_);
        std::free(actorBoxes_);
        actorStates_ = nullptr;
        actorBoxes_ = nullptr;
        actors = ActorWorld();
        ladders_ = nullptr;
        brushStates_ = nullptr;
        movers_ = nullptr;
        states_ = nullptr;
        world = MoveWorld();
        logic = LogicWorld();
        topText.clear();
        // (A map is come into with the lamp out and full: it is not kept.)
        lamp = LampState();
    }

    bool Game::create()
    {
        destroy();
        // The chapter's name, half a second into the map (not from a save:
        // placeFromSave says so).
        startMessagesMs_ = 500;
        const LevelView &view = level.view;

        // + 1: a level with no brushes or no logic still gets a valid pointer.
        brushStates_ =
            static_cast<BrushState *>(std::calloc(view.brushCount + 1, sizeof(BrushState)));
        movers_ = static_cast<MoverHull *>(std::calloc(view.brushCount + 1, sizeof(MoverHull)));
        states_ = static_cast<LogicState *>(std::calloc(view.logicCount + 1, sizeof(LogicState)));
        int ladderCount = 0;
        for (std::uint32_t i = 0; i < view.logicCount; ++i)
            ladderCount += view.logic[i].kind == static_cast<std::uint8_t>(LogicKind::Ladder);
        ladders_ = static_cast<LadderVolume *>(std::calloc(ladderCount + 1, sizeof(LadderVolume)));
        cornerState_ = static_cast<std::uint8_t *>(std::calloc(view.cornerCount + 1, 1));
        const int actorCount = static_cast<int>(view.actorCount) + kMaxVisitors;
        actorStates_ = static_cast<ActorState *>(std::calloc(actorCount, sizeof(ActorState)));
        actorBoxes_ = static_cast<SolidBox *>(std::calloc(actorCount, sizeof(SolidBox)));
        if (brushStates_ == nullptr || movers_ == nullptr || states_ == nullptr ||
            ladders_ == nullptr || cornerState_ == nullptr || actorStates_ == nullptr ||
            actorBoxes_ == nullptr)
        {
            destroy();
            return false;
        }
        for (std::uint32_t i = 0; i < view.brushCount; ++i)
        {
            const Model &model = view.models[view.brushes[i].model];
            new (&movers_[i]) MoverHull();
            movers_[i].standing =
                Hull{view.clipNodes, view.planes, view.planeTypes, model.clipStanding};
            movers_[i].crouched =
                Hull{view.clipNodes, view.planes, view.planeTypes, model.clipCrouched};
        }

        world.standing =
            Hull{view.clipNodes, view.planes, view.planeTypes, view.models[0].clipStanding};
        world.crouched =
            Hull{view.clipNodes, view.planes, view.planeTypes, view.models[0].clipCrouched};
        world.large = Hull{view.clipNodes, view.planes, view.planeTypes, view.models[0].clipLarge};
        world.movers = movers_;
        world.moverCount = static_cast<int>(view.brushCount);
        world.trace = traceFast;
        world.nodes = view.nodes;
        world.leaves = view.leaves;
        world.headNode = view.models[0].headNode;
        world.vis = view.vis;
        world.visSize = view.visSize;
        world.visLeafCount = view.header->visLeafCount;
        world.ladders = ladders_;
        world.ladderCount = ladderCount;

        logic.records = view.logic;
        logic.recordCount = static_cast<int>(view.logicCount);
        logic.aux = view.logicAux;
        logic.corners = view.corners;
        logic.brushes = view.brushes;
        logic.brushCount = static_cast<int>(view.brushCount);
        logic.states = states_;
        logic.brushStates = brushStates_;
        logic.movers = movers_;
        logic.ladders = ladders_;
        logic.world = &world;
        logic.globals = &globals;
        // The models must be loaded by now (main loads them before this): an
        // actor is set up from its model's clips.
        actors.states = actorStates_;
        actors.count = actorCount;
        actors.models = modelCache.views();
        actors.modelCount = modelCache.count();
        gibModelsFind(actors, view.modelRefs, static_cast<int>(view.modelRefCount));
        // (A DSi has the memory for twice as much lying about.)
        // (A DSi has the memory for twice as much going on at once.)
        actors.gibLimit = isDSiMode() ? kMaxGibs : kMaxGibs / 2;
        actors.shotLimit = isDSiMode() ? kMaxProjectiles : kMaxProjectiles / 2;
        actors.effectLimit = isDSiMode() ? kMaxEffects : kMaxEffects / 2;
        actors.decalLimit = isDSiMode() ? kMaxDecals : 24;
        actors.pieceLimit = isDSiMode() ? kMaxPieces : 24;
        actors.boxes = actorBoxes_;
        world.boxes = actorBoxes_;
        world.boxCount = actorCount;
        logic.actors = &actors;
        logic.nameHashes = view.nameHashes;
        logic.sprites = view.sprites;
        logic.speech = view.speechCount != 0 ? view.speech : nullptr;
        logic.spriteCount = static_cast<int>(view.spriteCount);
        logic.nameCount = static_cast<int>(view.nameCount);
        logic.sounds = view.logicSoundCount != 0 ? view.logicSounds : nullptr;
        logic.heard = &heard;
        heard.count = 0;
        logic.cornerState = cornerState_;
        logic.cornerCount = static_cast<int>(view.cornerCount);
        // Places every brush where the map starts it, for the renderer and
        // the collision alike.
        logicInit(logic);
        actorsInit(actors, view.actors, static_cast<int>(view.actorCount), view.nameHashes, world);
        actors.nodes = view.pathNodes;
        actors.next = view.pathNext;
        actors.nodeCount = static_cast<int>(view.pathNodeCount);
        weapon_ = WeaponState();
        held = ActorState();
        inHand_ = -1;
        return true;
    }

    void Game::takeInHand()
    {
        static const char *const kModels[] = {nullptr,     "v_crowbar", "v_9mmhandgun", "v_shotgun",
                                              "v_grenade", "v_9mmar",   "v_357"};
        static const char *const kIdle[] = {nullptr, "idle1", "idle1", "sm_idle", "idle", "longidle", "idle1"};
        const LevelView &view = level.view;
        const int weapon = player.state.inv.current;
        inHand_ = weapon;
        held = ActorState();
        if (weapon <= kWeaponNone || weapon >= kWeaponCount)
            return;
        // By name: a map has the models of what can be held in it.
        for (std::uint32_t m = 0; m < view.modelRefCount; ++m)
        {
            if (std::strcmp(view.modelRefs[m].name, kModels[weapon]) != 0)
                continue;
            held.active = true;
            held.model = static_cast<std::uint16_t>(m);
            held.kind = static_cast<std::uint8_t>(ActorKind::Generic);
            held.light = 28;
            const ModelView *model = actorModel(actors, held);
            if (model == nullptr)
            {
                held.active = false;
                return;
            }
            if (model->header->clipCount != 0)
            {
                const int idle = model->findClip(nameHash(kIdle[weapon]));
                held.restClip = static_cast<std::int16_t>(idle < 0 ? 0 : idle);
            }
            actorRest(held);
            // (The MP5's is called "deploy".)
            if (!actorPlay(actors, held, nameHash("draw"), true))
                actorPlay(actors, held, nameHash("deploy"), true);
            return;
        }
    }

    Arrival Game::makeArrival() const
    {
        const LevelView &view = level.view;
        Arrival arrival = hldata::makeArrival(logic, view.landmarks, view.landmarkCount,
                                              player.state, logic.pendingChange, view.modelRefs);
        arrival.yaw = camera.yaw;
        arrival.pitch = camera.pitch;
        return arrival;
    }

    bool Game::applyArrival(const Arrival &arrival)
    {
        const LevelView &view = level.view;
        if (!hldata::applyArrival(logic, view.landmarks, view.landmarkCount, view.nameHashes,
                                  view.nameCount, player.state, arrival, view.modelRefs))
            return false;
        camera.yaw = arrival.yaw;
        camera.pitch = arrival.pitch;
        return true;
    }

    void Game::resetProgress()
    {
        globals = Globals();
        visited_.clear();
    }

    void Game::rememberMap(std::uint32_t mapHash)
    {
        if (states_ == nullptr)
            return;
        SaveMapState *slot = nullptr;
        for (SaveMapState &map : visited_)
        {
            if (map.map == mapHash)
                slot = &map;
        }
        if (slot == nullptr)
        {
            visited_.emplace_back();
            slot = &visited_.back();
            slot->map = mapHash;
        }
        slot->broken.assign((logic.recordCount + 7) / 8, 0);
        for (int i = 0; i < logic.recordCount; ++i)
        {
            if (states_[i].dead && detail::isBreakable(logic.records[i]))
                slot->broken[i >> 3] |= static_cast<std::uint8_t>(1u << (i & 7));
        }
        // Who is gone: what was picked up, and what was killed. (Not the
        // places a monstermaker keeps: those start empty anyway.)
        slot->gone.assign((actors.recordCount + 7) / 8, 0);
        for (int i = 0; i < actors.recordCount; ++i)
        {
            const ActorState &actor = actorStates_[i];
            if (actors.records[i].flags & kActorDormant)
                continue;
            if (!actor.active || (fights(actor.kind) && actor.ai >= kAiDying))
                slot->gone[i >> 3] |= static_cast<std::uint8_t>(1u << (i & 7));
        }
    }

    void Game::restoreMap(std::uint32_t mapHash)
    {
        for (const SaveMapState &map : visited_)
        {
            if (map.map != mapHash)
                continue;
            for (int i = 0; i < logic.recordCount && (i >> 3) < static_cast<int>(map.broken.size());
                 ++i)
            {
                // Only what breaks, whatever the file says: a bit against
                // anything else is ignored.
                if (!(map.broken[i >> 3] & (1u << (i & 7))) ||
                    !detail::isBreakable(logic.records[i]))
                    continue;
                states_[i].dead = true;
                if (logic.records[i].brush != kNoBrush)
                    logicSetBrushActive(logic, logic.records[i].brush, false);
            }
            for (int i = 0; i < actors.recordCount && (i >> 3) < static_cast<int>(map.gone.size());
                 ++i)
            {
                // Only what can be killed or picked up, whatever the file
                // says.
                const std::uint8_t kind = actors.records[i].kind;
                if (!(map.gone[i >> 3] & (1u << (i & 7))) ||
                    !(fights(kind) || kind == static_cast<std::uint8_t>(ActorKind::Item)))
                    continue;
                actorStates_[i].active = false;
                actorPlaceBox(actors, i);
            }
        }
    }

    SaveData Game::snapshot(const std::string &map)
    {
        rememberMap(nameHash(map.c_str()));
        SaveData save;
        save.map = map;
        for (int a = 0; a < 3; ++a)
        {
            save.origin[a] = player.state.origin[a];
            save.velocity[a] = player.state.velocity[a];
        }
        save.yaw = camera.yaw;
        save.pitch = camera.pitch;
        save.ducked = player.state.ducked ? 1 : 0;
        save.health = player.state.health;
        const Inventory &inv = player.state.inv;
        save.weapons = inv.have;
        save.weapon = inv.current;
        save.clipGlock = inv.clipGlock;
        save.clipShotgun = inv.clipShotgun;
        save.ammo9mm = inv.ammo9mm;
        save.ammoBuck = inv.ammoBuck;
        save.armor = inv.armor;
        save.grenades = inv.grenades;
        save.clipMp5 = inv.clipMp5;
        save.clipPython = inv.clipPython;
        save.ammo357 = inv.ammo357;
        save.ammoAr = inv.ammoAr;
        for (int i = 0; i < globals.count; ++i)
        {
            save.globalHash.push_back(globals.hash[i]);
            save.globalState.push_back(globals.state[i]);
        }
        for (int i = 0; i < logic.recordCount; ++i)
        {
            const LogicRecord &train = logic.records[i];
            if (train.kind != static_cast<std::uint8_t>(LogicKind::TrackTrain) ||
                train.extra[2] == 0 || train.brush == kNoBrush ||
                !detail::playerAboard(logic, player.state, train.brush))
                continue;
            save.train = train.extra[2];
            for (int a = 0; a < 3; ++a)
                save.trainPos[a] = brushStates_[train.brush].pos[a];
            save.trainYaw = brushStates_[train.brush].angles[kYaw];
            save.trainSpeed = states_[i].speed;
            save.trainSelfDriven = states_[i].running ? 1 : 0;
            break;
        }
        save.maps = visited_;
        return save;
    }

    void Game::adoptProgress(const SaveData &save)
    {
        globals = Globals();
        for (std::size_t i = 0; i < save.globalHash.size(); ++i)
            globals.set(save.globalHash[i], save.globalState[i] > kGlobalDead ? kGlobalOff
                                                                              : save.globalState[i]);
        visited_ = save.maps;
    }

    void Game::placeFromSave(const SaveData &save)
    {
        startMessagesMs_ = -1;
        for (int a = 0; a < 3; ++a)
        {
            player.state.origin[a] = save.origin[a];
            player.state.velocity[a] = save.velocity[a];
        }
        player.state.ducked = save.ducked != 0;
        player.state.viewHeight = player.state.ducked ? kDuckViewHeight : kViewHeight;
        player.state.health = save.health > 0 && save.health <= 100 ? save.health : 100;
        // What a file says they carry is kept within what can be carried.
        auto within = [](int value, int most)
        { return static_cast<std::int16_t>(value < 0 ? 0 : (value > most ? most : value)); };
        Inventory &inv = player.state.inv;
        inv.have = save.weapons & ((1u << kWeaponCount) - 1u); // (bit 0 is the suit)
        inv.current = save.weapon < kWeaponCount ? save.weapon : kWeaponNone;
        inv.clipGlock = within(save.clipGlock, 17);
        inv.clipShotgun = within(save.clipShotgun, 8);
        inv.ammo9mm = within(save.ammo9mm, 250);
        inv.ammoBuck = within(save.ammoBuck, 125);
        inv.armor = within(save.armor, 100);
        inv.grenades = within(save.grenades, 10);
        inv.clipMp5 = within(save.clipMp5, 50);
        inv.clipPython = within(save.clipPython, 6);
        inv.ammo357 = within(save.ammo357, 36);
        inv.ammoAr = within(save.ammoAr, 10);
        if (inv.grenades == 0)
            inv.have &= static_cast<std::uint8_t>(~(1u << kWeaponGrenade));
        camera.yaw = save.yaw;
        camera.pitch = save.pitch;
        camera.clampPitch();
        for (int i = 0; i < logic.recordCount && save.train != 0; ++i)
        {
            const LogicRecord &train = logic.records[i];
            if (train.kind != static_cast<std::uint8_t>(LogicKind::TrackTrain) ||
                train.extra[2] != save.train || train.brush == kNoBrush)
                continue;
            logicPlaceBrush(logic, train.brush, save.trainPos, save.trainYaw);
            states_[i].speed = save.trainSpeed;
            states_[i].running = save.trainSelfDriven != 0;
            const std::uint16_t node =
                detail::trackNearest(logic, save.trainPos, static_cast<fx>(train.extra[1]));
            if (node != kNoCorner)
                states_[i].bits = node;
            player.state.onGround = true;
            player.state.groundMover = static_cast<std::int16_t>(train.brush);
            break;
        }
    }

    void Game::frame(int vblanks)
    {
        if (vblanks < 1)
            vblanks = 1;
        if (vblanks > kMaxVblanks)
            vblanks = kMaxVblanks;

        if (!player.noclip)
        {
            const DebugLog::Perf::Scope scope(DebugLog::Section::Logic);
            // Sixtieths of a second to milliseconds.
            logicTick(logic, player.state, (vblanks * 1000 + 30) / 60);
            // What the player rides has turned: so has their view.
            camera.yaw = static_cast<std::uint16_t>(camera.yaw + logic.viewTurn);
            logic.viewTurn = 0;
            if (logic.faceSet)
            {
                camera.yaw = logic.faceYaw;
                camera.pitch = 0;
                logic.faceSet = false;
            }

            if (input.usePressed)
            {
                // From the eyes, along the view as it was drawn last frame.
                const fx eye[3] = {player.state.origin[0], player.state.origin[1],
                                   player.state.origin[2] + player.state.viewHeight};
                const int target = logicFindUsable(logic, eye, camera.forward);
                if (target >= 0)
                    logicUse(logic, player.state, target, -1, kUseSet);
            }
        }
        // What fights, what lies about, and what is in hand.
        const int ms = (vblanks * 1000 + 30) / 60;

        // What the map writes on the screen: as it is come into, and when
        // something in it says.
        {
            const LevelView &view = level.view;
            if (startMessagesMs_ >= 0 && (startMessagesMs_ -= ms) < 0)
            {
                for (std::uint32_t i = 0; i < view.messageCount; ++i)
                {
                    if (view.messages[i].flags & kMessageAtStart)
                        topText.show(view.messages[i], view.strings + view.messages[i].text);
                }
            }
            for (int n = 0; n < logic.shownCount; ++n)
            {
                if (logic.shown[n] < view.messageCount)
                    topText.show(view.messages[logic.shown[n]], view.strings + view.messages[logic.shown[n]].text);
            }
            logic.shownCount = 0;
            topText.tick(ms);
        }

        // The lamp: its switch and its battery. (What it lights is the
        // renderer's business: HudDraw::drawLamp, ModelDraw.)
        {
            const bool suit = (player.state.inv.have & kHaveSuit) != 0;
            if (input.lampPressed && player.state.health > 0 && lampToggle(lamp, suit))
                sound.ui(kUiSoundLamp);
            if (lampTick(lamp, ms))
                sound.ui(kUiSoundLamp);
            lampLit = false;
            if (lamp.on && !player.noclip)
            {
                // Where it points: what the middle of the view falls on.
                // The lamp itself is carried a little to the right of the
                // eyes and below them, and shines at that point: so what is
                // near is lit a little off the view's middle, and what is
                // far in it.
                const fx eye[3] = {player.state.origin[0], player.state.origin[1],
                                   player.state.origin[2] + player.state.viewHeight};
                fx end[3], at[3], normal[3];
                for (int a = 0; a < 3; ++a)
                    end[a] = eye[a] + camera.forward[a] * kLampReach;
                int distance = kLampReach;
                if (traceShotWall(world, eye, end, at, normal))
                {
                    std::int64_t along = 0;
                    for (int a = 0; a < 3; ++a)
                        along += static_cast<std::int64_t>(at[a] - eye[a]) * camera.forward[a];
                    distance = static_cast<int>(along >> 24);
                    distance = distance < 16 ? 16 : (distance > kLampReach ? kLampReach : distance);
                }
                LampBeam &beam = lampBeam;
                fx axis[3];
                for (int a = 0; a < 3; ++a)
                {
                    beam.from[a] = eye[a] + camera.right[a] * 7 - camera.up[a] * 9;
                    axis[a] = eye[a] + camera.forward[a] * distance - beam.from[a];
                }
                const fx length = fxLength(axis);
                if (length > 0)
                {
                    for (int a = 0; a < 3; ++a)
                        beam.axis[a] = static_cast<fx>((static_cast<std::int64_t>(axis[a]) << 12) / length);
                    // Across it: the view's right, squared up to the beam,
                    // and what is square to both.
                    std::int32_t lean = 0;
                    for (int a = 0; a < 3; ++a)
                        lean += camera.right[a] * beam.axis[a];
                    lean >>= 12;
                    fx across[3];
                    for (int a = 0; a < 3; ++a)
                        across[a] = camera.right[a] - ((beam.axis[a] * lean) >> 12);
                    const fx wide = fxLength(across);
                    if (wide > 0)
                    {
                        for (int a = 0; a < 3; ++a)
                            beam.u[a] = static_cast<fx>((static_cast<std::int64_t>(across[a]) << 12) / wide);
                        beam.v[0] = (beam.axis[1] * beam.u[2] - beam.axis[2] * beam.u[1]) >> 12;
                        beam.v[1] = (beam.axis[2] * beam.u[0] - beam.axis[0] * beam.u[2]) >> 12;
                        beam.v[2] = (beam.axis[0] * beam.u[1] - beam.axis[1] * beam.u[0]) >> 12;
                        beam.radius = lampRadius(distance);
                        beam.reach = distance + beam.radius * 2 < kLampReach ? distance + beam.radius * 2 : kLampReach;
                        beam.strength = lampStrength(distance);
                        lampLit = beam.strength > 0;
                    }
                }
            }
            bottomScreen.setLamp(suit, lamp.on, lampPercent(lamp));
            hudDraw.setLamp(lamp.on, lampPercent(lamp));
            hudDraw.setHurt(player.view.flash, player.view.pain);
        }
        if (!player.noclip)
        {
            const DebugLog::Perf::Scope scope(DebugLog::Section::Logic);
            monstersTick(logic, player.state, ms);
            const fx eye[3] = {player.state.origin[0], player.state.origin[1],
                               player.state.origin[2] + player.state.viewHeight};
            const bool alive = player.state.health > 0;
            const std::uint32_t did = weaponsTick(logic, player.state, weapon_, eye, camera.forward,
                                                  camera.right, camera.up, alive && input.attack,
                                                  false, ms, alive && input.second, alive ? input.weaponPick : 0,
                                                  alive && input.reloadPressed);
            // What has broken: its pieces, each a patch of the thing's
            // own texture, thrown from where it was.
            for (int n = 0; n < logic.brokeCount; ++n)
            {
                const LevelView &view = level.view;
                const LogicRecord &record = view.logic[logic.broke[n]];
                if (record.brush == kNoBrush || record.brush >= view.brushCount)
                    continue;
                const Brush &brush = view.brushes[record.brush];
                const Model &model = view.models[brush.model];
                const fx *pos = brushStates_[record.brush].pos;
                fx mins[3], maxs[3];
                for (int a = 0; a < 3; ++a)
                {
                    mins[a] = fxFromInt(brush.mins[a]) + pos[a] - brush.origin[a];
                    maxs[a] = fxFromInt(brush.maxs[a]) + pos[a] - brush.origin[a];
                }
                std::uint16_t textures[8];
                int textureCount = 0;
                for (int s = 0; s < model.surfaceCount && textureCount < 8; ++s)
                {
                    const std::uint16_t texture = view.surfaces[model.firstSurface + s].texture;
                    if (texture != kNoTexture)
                        textures[textureCount++] = texture;
                }
                piecesBurst(actors, mins, maxs, static_cast<int>(record.extra[1]), textures, textureCount);
                // (The marks that were on it go with it.)
                for (Decal &decal : actors.decals)
                {
                    if (decal.active && decal.brush == static_cast<std::int16_t>(record.brush))
                        decal.active = false;
                }
            }
            logic.brokeCount = 0;
            // A round fired throws its casing out of the breech: to the
            // right, up a little, and it falls.
            if ((did & kWeaponFired) &&
                (inHand_ == kWeaponGlock || inHand_ == kWeaponMp5 || inHand_ == kWeaponShotgun))
            {
                static std::uint32_t toss = 3;
                toss = toss * 1664525u + 1013904223u;
                fx from[3], velocity[3];
                for (int a = 0; a < 3; ++a)
                {
                    from[a] = eye[a] + camera.forward[a] * 14 + camera.right[a] * 5 - camera.up[a] * 5;
                    velocity[a] = player.state.velocity[a] + camera.right[a] * (70 + static_cast<int>((toss >> 9) % 40)) +
                                  camera.up[a] * (90 + static_cast<int>((toss >> 15) % 40)) + camera.forward[a] * 15;
                }
                Piece &shell = pieceAdd(actors, kPieceShell, from, velocity, 2500);
                shell.half = 1;
                shell.colour = inHand_ == kWeaponShotgun ? RGB15(24, 5, 4) : RGB15(27, 21, 6);
            }
            // A shot kicks the view, each weapon its own way.
            if (did & (kWeaponFired | kWeaponSecondary))
            {
                static std::uint32_t chance = 7;
                chance = chance * 1664525u + 1013904223u;
                viewKick(player.view, (did & kWeaponSecondary) ? 10 * kViewDegree : weaponKick(inHand_, chance >> 8));
            }
            if (did & kWeaponDrawn)
                sound.ui(kUiSoundWeapon);
            if (player.state.inv.current != inHand_)
                takeInHand();
            if (held.active)
            {
                const bool crowbar = inHand_ == kWeaponCrowbar;
                if (did & kWeaponSecondary)
                    actorPlay(actors, held, nameHash("grenade"), true);
                else if (did & kWeaponFired)
                    actorPlay(actors, held,
                              nameHash(crowbar ? ((did & kWeaponHit) ? "attack1" : "attack1miss")
                                               : (inHand_ == kWeaponGrenade
                                                      ? "throw1"
                                                      : (inHand_ == kWeaponPython ? "fire1" : "shoot"))),
                              true);
                else if (did & kWeaponReload)
                    actorPlay(actors, held, nameHash("reload"), true);
            }
            actors.got = 0;
        }
        const fx stoodAt[3] = {player.state.origin[0], player.state.origin[1], player.state.origin[2]};
        player.frame(vblanks);
        // Their own footsteps, and what a blow sounds like from inside.
        if (!player.noclip)
        {
            for (int n = playerFootsteps(player.state, stoodAt); n > 0; --n)
                heard.add(static_cast<std::uint16_t>(static_cast<int>(CommonSound::Step1) +
                                                     (stepCount_++ & 3)),
                          nullptr, 70, 0);
            if (player.state.health < lastHealth_ && player.state.health > 0)
                heard.add(CommonSound::Pain, nullptr, 110);
        }
        lastHealth_ = player.state.health;
        // What is held: its clip moves on, and a swing that is over goes back
        // to being held still.
        if (held.active)
        {
            actorAdvance(actors, held, (vblanks * 1000 + 30) / 60);
            if (held.once && held.finished)
                actorRest(held);
        }
        {
            Inventory &inv = player.state.inv;
            const bool grenade = inv.current == kWeaponGrenade;
            const std::int16_t *clip = gunClip(inv), *spare = gunSpare(inv);
            bottomScreen.setStatus(inv.current, grenade ? inv.grenades : (clip != nullptr ? *clip : 0),
                                   grenade ? 0 : (spare != nullptr ? *spare : 0), inv.have,
                                   clip != nullptr && spare != nullptr && *clip < gunKind(inv.current).holds &&
                                       *spare > 0);
            hudDraw.setStatus((inv.have & kHaveSuit) != 0, player.state.health, inv.armor, inv.current,
                              grenade ? inv.grenades : (clip != nullptr ? *clip : 0),
                              grenade ? 0 : (spare != nullptr ? *spare : 0),
                              inv.current == kWeaponMp5 ? inv.ammoAr : -1);
        }
        // With the eyes under, the picture dims: what there is of "under
        // water" until there is fog.
        const bool under = player.state.waterLevel == 3 && !player.noclip;
        screenFade.setDim(under);
    }
} // namespace hlrt
