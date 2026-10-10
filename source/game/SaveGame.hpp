#pragma once

// The saves on the card: the files themselves. Which files there are, how
// one is written safely and the list of them are shared/HlSaveFiles.hpp's
// business (and run in the host tests); what is IN a save is HlSave.hpp's,
// HlSaveState.hpp's and Game's. This is only "a folder on the card".

#include "HlSaveFiles.hpp"

namespace hlrt::SaveGame
{
    /// The list of saves: read once (what a write left half-way is put
    /// right then), and kept up as saves are made.
    const hldata::SaveIndex &index();
    /// Read the save in `slot` (a kSlot*), if there is one and it is whole.
    bool read(hldata::SaveData &out, int slot);
    /// Write it. False if the card would not take it: the save that was
    /// there is then there still.
    bool write(const hldata::SaveData &save, int slot, const hldata::SaveHeading &heading);
} // namespace hlrt::SaveGame
