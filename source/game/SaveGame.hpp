#pragma once

// The save file on the card: reading it and writing it. What is IN a save is
// shared/HlSave.hpp's business and Game's; this is only the file.

#include "HlSave.hpp"

namespace hlrt::SaveGame
{
    /// Which save: the latest, or the one made on coming into the map
    /// (HlSave.hpp: what a death goes back to).
    inline constexpr int kLatest = 0;
    inline constexpr int kArrival = 1;
    /// Read the save, if there is one and it is whole.
    bool read(hldata::SaveData &out, int slot = kLatest);
    /// Write it: to a file of another name first, which then takes the
    /// save's place, so that switching off half-way leaves the old save.
    bool write(const hldata::SaveData &save, int slot = kLatest);
} // namespace hlrt::SaveGame
