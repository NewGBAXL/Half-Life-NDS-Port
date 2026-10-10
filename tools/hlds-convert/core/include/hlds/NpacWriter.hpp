#pragma once

// NPAC: Nitro Engine Advanced's archive, written here and read there.
//
// The game's data goes on the card as one archive per map (and one for what
// every map shares) instead of thousands of loose files: a load is then one
// file opened and read mostly front to back, and the members that shrink
// well are stored LZ77-compressed and inflated by the engine as they are
// opened. The console side is NEANPAC.c, mounted as a drive; nothing in this
// port reads the container itself.
//
// The layout is NARC's with one chunk more (the engine's
// tools/npac/npac_format.py is the reference, and tests/test_npac.cpp checks
// this writer against that tool when it is at hand):
//
//     header   "NPAC", FFFEh, 0100h, file size, 16, number of chunks
//     "BTAF"   count, then {start, end} of each member in the image
//     "BTNF"   a NitroROM file name table: directories and names
//     "CMPT"   method | size << 8 for each member (left out when nothing
//              is compressed)
//     "GMIF"   the members, each padded to a word with FFh
//
// Members keep the ORDER they are given in: within the archive's root, and
// then directory by directory in the order each directory is first named.
// That order is the order the console reads them in.

#include <cstdint>
#include <string>
#include <vector>

namespace hlds
{
    inline constexpr std::uint8_t kNpacStored = 0x00;
    inline constexpr std::uint8_t kNpacLz77 = 0x10;

    /// Compression is kept only where it pays: a member under this size is
    /// read in one go whatever is done to it...
    inline constexpr std::size_t kNpacMinCompressBytes = 1024;
    /// ...and one that does not come down to this share of itself costs the
    /// console more to inflate than the card saves.
    inline constexpr int kNpacKeepPercent = 75;

    struct NpacMember
    {
        std::string path;                ///< "level.0", "tex/wall_64x64.htx"
        std::vector<std::uint8_t> data;  ///< as the console is to see it
        bool mayCompress = true;
        // Filled in by buildNpac / readNpac:
        std::uint8_t method = kNpacStored;
        std::uint32_t storedBytes = 0;
    };

    /// False, with why, for a name the name table cannot hold, a name given
    /// twice, a member over 16 MB, or a root file named after a directory was
    /// (the root's files come first in the archive).
    bool buildNpac(std::vector<NpacMember> &members, bool compress,
                   std::vector<std::uint8_t> &out, std::string &error);

    /// The converter's own reader, for checking what it wrote: every member,
    /// inflated, in the archive's order. Refuses anything out of bounds.
    bool readNpac(const std::vector<std::uint8_t> &bytes, std::vector<NpacMember> &members,
                  std::string &error);
} // namespace hlds
