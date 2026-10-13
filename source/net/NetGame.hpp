#pragma once

// A game between consoles, on this one: the radio (NetLink) joined to the
// match (shared/HlNetGame.hpp) and to the game.
//
// frame() is called once a frame whatever is on the screen: it reads what
// has come, keeps account of who is there, and -- while no map is loaded --
// says this console's part of the lobby. With a map loaded, the game calls
// tick() after its own tick instead: that is where what the player did goes
// out and what the others did comes in.

#include <string>
#include <vector>

#include "HlNetGame.hpp"

namespace hlrt
{
    class NetGame
    {
    public:
        enum class Phase
        {
            Off,
            Hosting,  ///< a host, in its lobby
            Scanning, ///< looking for hosts
            Joining,  ///< asking one to be let in
            Lobby,    ///< a client, in the host's lobby
            Playing,  ///< a map is loaded and the match is on
        };

        Phase phase() const { return phase_; }
        bool on() const { return phase_ != Phase::Off; }
        bool playing() const { return phase_ == Phase::Playing; }
        bool isHost() const { return match_.role == hldata::NetMatch::Role::Host; }
        const hldata::NetMatch &match() const { return match_; }
        hldata::NetMatch &match() { return match_; }

        /// The maps a match can be played on (the card's, by the manifest).
        void setMaps(std::vector<std::string> maps) { maps_ = std::move(maps); }
        const std::vector<std::string> &maps() const { return maps_; }

        // ------------------------------------------------------------ lobby
        bool host();  ///< be a host; false if the radio would not start
        bool scan();  ///< look for hosts
        /// The hosts found so far: up to `most` of them, each a line to
        /// show ("name 2/4") and what to give join().
        struct Found
        {
            char line[32];
            int index;
        };
        int found(Found *out, int most);
        bool join(int index);
        /// A console started by Download Play: the host is whoever started
        /// it.
        bool joinStarter();
        /// Out of it altogether: the others are told, the radio is silent.
        void leave();

        /// The host: begin the match on `map`. (It then loads the map
        /// itself, as the clients do when they hear.)
        void start(const std::string &map, int fragLimit, int minuteLimit);
        /// Whether a match has been started and its map is to be loaded:
        /// which map. True once per start, on every console.
        bool startAsked(std::string &map);
        /// The map is loaded: the match begins here.
        void mapLoaded();

        // ---------------------------------------------------------- frames
        void frame();
        /// After the game's tick (see HlNetGame.hpp's netTick).
        void tick(std::uint16_t yaw, std::int32_t pitch, std::uint32_t did, bool lamp, int ms);
        /// The local player used the map's record `index`.
        void used(int index);

        /// Something to say on the screen, if there is: a line of it into
        /// `line`. False when there is nothing more.
        bool notice(char *line, int room);
        /// Why this console is out of the game, if it is and did not ask to
        /// be: the host left, the link was lost. Empty otherwise; cleared
        /// by reading.
        const char *ended();
        /// The radio's own account of itself, for the debug readout.
        void status(char *line, int room) const;

    private:
        Phase phase_ = Phase::Off;
        hldata::NetMatch match_;
        std::vector<std::string> maps_;
        std::vector<hldata::NetSpawn> spawns_;
        unsigned clientsWere_ = 0;
        int lostFrames_ = 0;
        const char *ended_ = "";
        std::string pendingMap_;
        bool startPending_ = false;
        std::uint32_t sent_ = 0, heard_ = 0;

        void send();
        void lost(const char *why);
    };

    extern NetGame netGame;
} // namespace hlrt
