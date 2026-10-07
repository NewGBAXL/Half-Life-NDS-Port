#pragma once

// The conversion as a whole: a game folder in, a card's worth of files out.
//
// This is the layer the command line and the GUI both drive. It owns the
// decisions that are about a RUN rather than about one file -- where things go
// on the card, which maps exist, what the manifest says afterwards -- and
// reports through plain callbacks, so that it knows nothing about a terminal
// or a window.

#include <atomic>
#include <filesystem>
#include <functional>
#include <string>
#include <vector>

#include "HlManifest.hpp"
#include "hlds/LightBake.hpp"

namespace hlds
{
    inline constexpr const char *kConverterVersion = "0.1.0";
    /// The map a new game starts in.
    inline constexpr const char *kDefaultStartMap = "c0a0";

    struct ConvertOptions
    {
        std::filesystem::path gameDir;  ///< the game's "valve" folder
        std::filesystem::path cardRoot; ///< the root of the SD card
        std::filesystem::path ptexconv; ///< empty: look for it
        bool textures = true;
        double lightGamma = LightBake::kDefaultGamma;
        bool guardedMerges = true; ///< CookOptions::guardedMerges
        std::string startMap; ///< empty: keep the manifest's, or the default
    };

    struct MapResult
    {
        std::string name;
        bool ok = false;
        std::string error;
        std::vector<std::string> warnings;
        std::size_t levelBytes = 0;
        int surfaces = 0;
        int faces = 0;        ///< drawn faces before grouping
        int faceVertices = 0; ///< vertices one face at a time would cost
        int vertices = 0;     ///< vertices actually cooked
        int streamKb = 0;
        int worstPolygons = 0;
        int worstVertices = 0;
        int texturesUsed = 0;
        int texturesEncoded = 0;
        int texelKb = 0;
        int alphaTexelKb = 0; ///< A3I5 texels, a byte each
        int brushes = 0;
        int logicRecords = 0;
        int unsupportedEntities = 0; ///< entities whose class the port does not handle yet
        int unsupportedClasses = 0;
        int models = 0;      ///< models cooked for the map's actors
        int actors = 0;
        int modelKb = 0;     ///< their files, skins apart
        int modelTexelKb = 0; ///< their skins, in texels
        int sounds = 0;       ///< sounds the map plays
        int soundKb = 0;      ///< what they take in memory on the console
        int soundsHalved = 0; ///< of them, cooked at half the rate to fit
        int soundsMissing = 0; ///< named by the map, not in the game folder
        int musicTracks = 0;  ///< tracks of music the map starts
    };

    class Converter
    {
    public:
        explicit Converter(ConvertOptions options);

        /// <card>/_nds/hl_nds/data -- the DS builds the same path
        /// (Global::dataDir).
        static std::filesystem::path dataDir(const std::filesystem::path &cardRoot);

        /// What can be checked before any work is done: the folders exist, the
        /// game folder holds maps, the encoder can be found. Empty when all is
        /// well, else a sentence for the user.
        std::string preflight() const;

        /// The game's maps, lower-case, without extension, sorted.
        std::vector<std::string> listMaps() const;

        /// Cook one map and its textures. Does not touch the manifest.
        MapResult cookMap(const std::string &name);

        /// Cook several maps and then write the manifest, which lists every
        /// map cooked now or by an earlier run that is still on the card.
        /// `progress` is called before each map with its index; `cancel`, if
        /// given, is checked between maps. Returns the number that failed.
        int cookMaps(const std::vector<std::string> &names,
                     const std::function<void(std::size_t, const std::string &)> &progress,
                     const std::function<void(const MapResult &)> &done,
                     const std::atomic<bool> *cancel, std::string &error);

    private:
        bool findMap(const std::string &name, std::filesystem::path &out) const;

        ConvertOptions options_;
    };
} // namespace hlds
