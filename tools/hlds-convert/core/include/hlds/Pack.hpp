#pragma once

// From the converter's own files to what goes on the card.
//
// Cooking writes loose files -- levels, textures, models, sounds -- into a
// stage folder on the PC, where a texture or a sound shared by fifty maps is
// made once and found again by the next run. The console is given something
// else: one archive per map, holding all that the map loads in the order it
// loads it (NpacWriter.hpp), and one more for what belongs to no map. On a
// card, opening a file is the slow part; a map used to be two hundred of
// them.
//
// What a map needs is read out of its level, with the very functions the
// console names its files by, so the two cannot disagree about a name.

#include <filesystem>
#include <string>
#include <vector>

namespace hlds
{
    struct PackReport
    {
        int members = 0;
        int compressed = 0;
        int missing = 0;            ///< named by the level, not in the stage
        std::size_t rawBytes = 0;   ///< what the members come to
        std::size_t cardBytes = 0;  ///< the archive
    };

    /// <stage>/maps/<map>.hlm and all it names -> <card>/maps/<map>.npac.
    /// `stage` and `card` are the two data folders (Converter::dataDir).
    bool packMap(const std::filesystem::path &stage, const std::filesystem::path &card,
                 const std::string &map, bool compress, PackReport &report, std::string &error);

    /// <stage>/ui/* and the menu's sounds -> <card>/common.npac.
    bool packCommon(const std::filesystem::path &stage, const std::filesystem::path &card,
                    bool compress, PackReport &report, std::string &error);

    /// A map's archive read back: every member inflated and the level put
    /// together again, as the console will. False if anything is amiss.
    bool unpackLevel(const std::filesystem::path &archive, std::vector<std::uint8_t> &level,
                     std::size_t &archiveBytes, std::string &error);
} // namespace hlds
