#pragma once

// Going from one map to the next.
//
// A Half-Life chapter is a string of maps joined end to end. Where two meet,
// each has an info_landmark of the same name at what is the same place in
// both, and the player who walks into a trigger_changelevel in one arrives in
// the other standing as far from its landmark as they were from the first's,
// moving the same way and looking the same way.
//
// The DS cannot hold two maps at once, so everything that crosses is put in
// one small struct -- the Arrival -- before the old map is let go, and applied
// once the new one is up. Names mean nothing across maps as ids (those are
// per map), so everything here that names something does so by nameHash().
//
// Also here: the GLOBAL STATES, the few named switches that outlive a map
// (env_global), which travel in the same struct.
//
// std-only and inline: the host tests run it.

#include "HlLogic.hpp"

namespace hldata
{
    /// What crosses a level change.
    struct Arrival
    {
        bool valid = false;        ///< false: start the map at its own player start
        std::uint32_t landmark = 0; ///< nameHash of the landmark both maps share
        fx offset[3] = {0, 0, 0};  ///< the player's origin, less the old map's landmark
        fx velocity[3] = {0, 0, 0};
        std::uint16_t yaw = 0;
        std::int16_t pitch = 0;
        bool ducked = false;
        std::int32_t health = 100;
        Inventory inv;
        /// A name to fire in the new map, and how long after arriving
        /// ("changetarget" and "changedelay"); 0 = none.
        std::uint32_t changeTarget = 0;
        std::int32_t changeDelayMs = 0;
        /// The track train the player is riding, by its global name's hash;
        /// 0 = none. The next map has a train of the same name, which takes
        /// this one's place: where it is from the landmark, which way it
        /// faces and how fast it is going.
        std::uint32_t train = 0;
        fx trainOffset[3] = {0, 0, 0};
        std::uint16_t trainYaw = 0;
        fx trainSpeed = 0;
        bool trainSelfDriven = false;
        /// The actors that come along: those with a name who stand near the
        /// landmark (the guard who meets the tram is waiting in the map
        /// before the one he opens its door in). Each as where it is from
        /// the landmark and what it is.
        struct Visitor
        {
            std::uint32_t model = 0; ///< nameHash of its model's name; 0 = slot unused
            std::uint32_t name = 0;  ///< nameHash of its own
            fx offset[3] = {0, 0, 0};
            std::uint16_t yaw = 0;
            std::uint8_t body = 0;
            std::uint8_t kind = 0;
            std::uint8_t flags = 0;
            std::uint8_t light = 24;
        };
        Visitor visitors[kMaxVisitors];
    };

    /// How near the landmark an actor has to be to come along.
    inline constexpr fx kVisitorRange = fxFromInt(2048);

    /// The landmark called `hash`: the first of that name, as in the game.
    inline const Landmark *findLandmark(const Landmark *landmarks, std::uint32_t count,
                                        std::uint32_t hash)
    {
        for (std::uint32_t i = 0; i < count; ++i)
        {
            if (landmarks[i].hash == hash)
                return &landmarks[i];
        }
        return nullptr;
    }

    /// The name id that `hash` has in this map; 0 if nothing here is called
    /// that.
    inline std::uint16_t nameIdOfHash(const std::uint32_t *nameHashes, std::uint32_t count,
                                      std::uint32_t hash)
    {
        if (hash == 0)
            return 0;
        for (std::uint32_t i = 1; i < count; ++i)
        {
            if (nameHashes[i] == hash)
                return static_cast<std::uint16_t>(i);
        }
        return 0;
    }

    /// Pack what crosses, for the ChangeLevel record `index` of the map being
    /// left. The view angles are the caller's to fill in: they are not part of
    /// the movement state.
    inline Arrival makeArrival(const LogicWorld &w, const Landmark *landmarks,
                               std::uint32_t landmarkCount, const PlayerState &player, int index,
                               const ModelRef *modelRefs = nullptr)
    {
        const LogicRecord &record = w.records[index];
        Arrival arrival;
        arrival.valid = true;
        arrival.landmark = record.extra[1];
        const Landmark *here = findLandmark(landmarks, landmarkCount, arrival.landmark);
        for (int a = 0; a < 3; ++a)
        {
            arrival.offset[a] = player.origin[a] - (here != nullptr ? here->origin[a] : 0);
            arrival.velocity[a] = player.velocity[a];
        }
        // A map without the landmark it names cannot say where the player is
        // relative to anything: they will start at the next map's own start.
        if (here == nullptr)
            arrival.landmark = 0;
        arrival.ducked = player.ducked;
        arrival.health = player.health;
        arrival.inv = player.inv;
        // The train under the player's feet, if it is one that is known by
        // name in the next map.
        for (int i = 0; i < w.recordCount && here != nullptr; ++i)
        {
            const LogicRecord &train = w.records[i];
            // "Under their feet" is "they are in it": a passenger in the
            // middle of a jump is a passenger still.
            if (train.kind != static_cast<std::uint8_t>(LogicKind::TrackTrain) ||
                train.extra[2] == 0 || train.brush == kNoBrush ||
                !detail::playerAboard(w, player, train.brush))
                continue;
            const BrushState &car = w.brushStates[train.brush];
            arrival.train = train.extra[2];
            for (int a = 0; a < 3; ++a)
                arrival.trainOffset[a] = car.pos[a] - here->origin[a];
            arrival.trainYaw = car.angles[kYaw];
            arrival.trainSpeed = w.states[i].speed;
            arrival.trainSelfDriven = w.states[i].running;
            break;
        }
        if (w.actors != nullptr && here != nullptr && modelRefs != nullptr)
        {
            // The nearest few, if there are more than can come.
            fx far[kMaxVisitors];
            int taken = 0;
            for (int i = 0; i < w.actors->count; ++i)
            {
                const ActorState &actor = w.actors->states[i];
                if (!actor.active || actor.nameHash == 0 || actor.model >= w.actors->modelCount)
                    continue;
                const fx d[3] = {actor.pos[0] - here->origin[0], actor.pos[1] - here->origin[1],
                                 actor.pos[2] - here->origin[2]};
                if (d[0] > kVisitorRange || d[0] < -kVisitorRange || d[1] > kVisitorRange ||
                    d[1] < -kVisitorRange || d[2] > kVisitorRange || d[2] < -kVisitorRange)
                    continue;
                // And only whom the landmark can see, as in the game: someone
                // two rooms away has no place at the same offset next door.
                if (w.world != nullptr)
                {
                    const fx chest[3] = {actor.pos[0], actor.pos[1], actor.pos[2] + fxFromInt(36)};
                    if (!leafSees(*w.world, worldLeaf(*w.world, here->origin),
                                  worldLeaf(*w.world, chest)))
                        continue;
                }
                const fx distance = fxLength(d);
                int at = taken < kMaxVisitors ? taken : kMaxVisitors - 1;
                if (taken >= kMaxVisitors && distance >= far[at])
                    continue;
                while (at > 0 && far[at - 1] > distance)
                {
                    arrival.visitors[at] = arrival.visitors[at - 1];
                    far[at] = far[at - 1];
                    --at;
                }
                far[at] = distance;
                Arrival::Visitor &visitor = arrival.visitors[at];
                visitor.model = nameHash(modelRefs[actor.model].name);
                visitor.name = actor.nameHash;
                for (int a = 0; a < 3; ++a)
                    visitor.offset[a] = d[a];
                visitor.yaw = actor.yaw;
                visitor.body = actor.body;
                visitor.kind = actor.kind;
                visitor.flags = actor.flags;
                visitor.light = actor.light;
                if (taken < kMaxVisitors)
                    ++taken;
            }
        }
        arrival.changeTarget = record.extra[2];
        arrival.changeDelayMs = static_cast<std::int32_t>(record.extra[3]);
        return arrival;
    }

    /// Put the player where they should be in the map just loaded, and queue
    /// what the old map asked to have fired here. False if this map has no
    /// such landmark: the caller starts the player at the map's own start.
    inline bool applyArrival(LogicWorld &w, const Landmark *landmarks, std::uint32_t landmarkCount,
                             const std::uint32_t *nameHashes, std::uint32_t nameCount,
                             PlayerState &player, const Arrival &arrival,
                             const ModelRef *modelRefs = nullptr)
    {
        if (!arrival.valid)
            return false;
        player.health = arrival.health;
        player.inv = arrival.inv;
        const std::uint16_t fire = nameIdOfHash(nameHashes, nameCount, arrival.changeTarget);
        if (fire != 0)
            logicEnqueue(w, fire, 0, kUseToggle, -1, arrival.changeDelayMs);

        const Landmark *here = arrival.landmark != 0
                                   ? findLandmark(landmarks, landmarkCount, arrival.landmark)
                                   : nullptr;
        if (here == nullptr)
            return false;
        for (int a = 0; a < 3; ++a)
        {
            player.origin[a] = here->origin[a] + arrival.offset[a];
            player.velocity[a] = arrival.velocity[a];
        }
        player.ducked = arrival.ducked;
        player.viewHeight = arrival.ducked ? kDuckViewHeight : kViewHeight;

        // Whoever came along takes one of the map's spare places -- unless
        // the map has someone of that name already, or was not given the
        // model.
        for (const Arrival::Visitor &visitor : arrival.visitors)
        {
            if (visitor.model == 0 || w.actors == nullptr || modelRefs == nullptr)
                continue;
            int model = -1, slot = -1;
            bool here2 = false;
            for (int m = 0; m < w.actors->modelCount; ++m)
            {
                if (nameHash(modelRefs[m].name) == visitor.model)
                    model = m;
            }
            for (int i = 0; i < w.actors->count; ++i)
            {
                const ActorState &other = w.actors->states[i];
                if (other.active && other.nameHash == visitor.name)
                    here2 = true;
                if (!other.active && slot < 0)
                    slot = i;
            }
            if (model < 0 || slot < 0 || here2)
                continue;
            // Where they would stand must be somewhere a person can: on a
            // floor, with room. Otherwise they are left behind.
            if (w.world != nullptr)
            {
                ActorState probe;
                for (int a = 0; a < 3; ++a)
                    probe.pos[a] = here->origin[a] + visitor.offset[a];
                if (!actorToFloor(*w.world, probe, fxFromInt(18), fxFromInt(64)))
                    continue;
            }
            ActorState &actor = w.actors->states[slot];
            actor = ActorState();
            actor.active = true;
            actor.model = static_cast<std::uint16_t>(model);
            actor.nameHash = visitor.name;
            actor.name = nameIdOfHash(nameHashes, nameCount, visitor.name);
            for (int a = 0; a < 3; ++a)
                actor.pos[a] = here->origin[a] + visitor.offset[a];
            actor.yaw = visitor.yaw;
            actor.body = visitor.body;
            actor.kind = visitor.kind;
            actor.flags = visitor.flags;
            actor.light = visitor.light;
            const ModelView *view = actorModel(*w.actors, actor);
            if (view != nullptr && view->header->clipCount != 0)
            {
                const int idle = view->findClip(nameHash(
                    visitor.kind == static_cast<std::uint8_t>(ActorKind::Gman) ? "idle01" : "idle1"));
                actor.restClip = static_cast<std::int16_t>(idle < 0 ? 0 : idle);
            }
            actorRest(actor);
            if (w.world != nullptr)
                actorToFloor(*w.world, actor, fxFromInt(18), fxFromInt(64));
            actorPlaceBox(*w.actors, slot);
        }

        // The train they came in on: this map's train of the same name takes
        // up where that one was, on whichever node of its track is nearest.
        for (int i = 0; i < w.recordCount && arrival.train != 0; ++i)
        {
            const LogicRecord &train = w.records[i];
            if (train.kind != static_cast<std::uint8_t>(LogicKind::TrackTrain) ||
                train.extra[2] != arrival.train || train.brush == kNoBrush)
                continue;
            const fx pos[3] = {here->origin[0] + arrival.trainOffset[0],
                               here->origin[1] + arrival.trainOffset[1],
                               here->origin[2] + arrival.trainOffset[2]};
            logicPlaceBrush(w, train.brush, pos, arrival.trainYaw);
            LogicState &state = w.states[i];
            state.speed = arrival.trainSpeed;
            state.running = arrival.trainSelfDriven;
            const std::uint16_t node =
                detail::trackNearest(w, pos, static_cast<fx>(train.extra[1]));
            if (node != kNoCorner)
                state.bits = node;
            // Standing on it from the first instant, so that it carries them
            // from the first instant.
            player.onGround = true;
            player.groundMover = static_cast<std::int16_t>(train.brush);
            break;
        }
        return true;
    }
} // namespace hldata
