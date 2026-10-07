#pragma once

// The running map: everything about it that changes.
//
// The level file is read-only tables. This holds the other half -- where each
// brush entity is right now, what each logic record is doing, the collision's
// picture of both -- and advances it, once per displayed frame:
//
//   1. the entity logic (shared/HlLogic.hpp): timers, doors and lifts moving,
//      triggers the player has walked into. Moving brushes carry and push the
//      player here.
//   2. USE, if the player pressed it.
//   3. the player's own movement, against the world as the logic left it.
//
// The order matters: a lift moves and takes the player with it BEFORE the
// player's move looks for the ground, or they would fall through it for a
// frame every frame.
//
// One arrays-and-a-loop design on purpose (see HlLogic.hpp). `game` is it.

#include <string>
#include <vector>

#include "HlMonsters.hpp"
#include "HlSave.hpp"
#include "HlTransition.hpp"

namespace hlrt
{
    class Game
    {
    public:
        /// Everything the player can collide with. The player's movement and
        /// the logic both work on this one.
        hldata::MoveWorld world;
        hldata::LogicWorld logic;
        /// The map's actors: its own, and room for those who arrive with the
        /// player.
        hldata::ActorWorld actors;
        /// What the player holds, as an actor of its own: it is drawn at the
        /// eye, turned as the view is. Inactive on a map without one.
        hldata::ActorState held;
        /// What has been heard and not yet played (audio/Sound empties it).
        hldata::SoundQueue heard;
        /// The switches that outlive a map. Not reset by create() or
        /// destroy(): only a new game clears them.
        hldata::Globals globals;

        /// Allocate and initialise the live state for the loaded level.
        bool create();
        void destroy();

        /// Advance by `vblanks` sixtieths of a second.
        void frame(int vblanks);

        /// The ChangeLevel record the player has set off, or -1. main() acts
        /// on it between frames.
        int pendingChange() const { return logic.pendingChange; }
        /// What crosses to the next map, for the pending change.
        hldata::Arrival makeArrival() const;
        /// Put the player where `arrival` says they are in this map. False if
        /// it cannot (no such landmark): they stay at the map's own start.
        bool applyArrival(const hldata::Arrival &arrival);

        // ---- what outlives a map, and saving it ----

        /// Forget everything a game in progress has built up: a new game.
        void resetProgress();
        /// Note what is broken in the map that is loaded, under its name's
        /// hash. Call before the map is let go, and before saving.
        void rememberMap(std::uint32_t mapHash);
        /// Break again what was broken the last time this map was here. Call
        /// once the map is up.
        void restoreMap(std::uint32_t mapHash);
        /// Everything a save holds, for the map called `map`.
        hldata::SaveData snapshot(const std::string &map);
        /// Take a save's switches and broken things as the game's own. Call
        /// BEFORE loading its map: the map reads the switches as it starts.
        void adoptProgress(const hldata::SaveData &save);
        /// Put the player (and the train they rode) where the save has them.
        /// Call once its map is up.
        void placeFromSave(const hldata::SaveData &save);

        /// Where each brush entity is, for the renderer.
        const hldata::BrushState *brushStates() const { return brushStates_; }

        Game() = default;
        Game(const Game &) = delete;
        Game &operator=(const Game &) = delete;

    private:
        hldata::BrushState *brushStates_ = nullptr;
        hldata::MoverHull *movers_ = nullptr;
        hldata::LogicState *states_ = nullptr;
        hldata::LadderVolume *ladders_ = nullptr;
        std::uint8_t *cornerState_ = nullptr;
        hldata::ActorState *actorStates_ = nullptr;
        hldata::SolidBox *actorBoxes_ = nullptr;
        bool dimmed_ = false;
        /// Put in hand the model of the weapon the player has out.
        void takeInHand();
        hldata::WeaponState weapon_;
        int inHand_ = -1; ///< the weapon `held` is the model of
        int lastHealth_ = 100;
        std::uint32_t stepCount_ = 0;
        std::vector<hldata::SaveMapState> visited_;
    };

    extern Game game;
} // namespace hlrt
