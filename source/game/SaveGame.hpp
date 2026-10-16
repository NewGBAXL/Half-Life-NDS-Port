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

    /// The same, a step at a time (hldata::SaveWriter): begun now, with the
    /// game as it is now, and then step() once a frame until it says Done or
    /// Failed. False if it cannot be begun (no card, or one is being written
    /// already). A write(), a read or leaving the map finishes it first.
    bool begin(const hldata::SaveData &save, int slot, const hldata::SaveHeading &heading);
    hldata::SaveWriter::Result step();
    bool busy();
    /// What is left of it, at once (false if it failed).
    bool finish();
} // namespace hlrt::SaveGame
