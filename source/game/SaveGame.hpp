#pragma once

// The save file on the card: reading it and writing it. What is IN a save is
// shared/HlSave.hpp's business and Game's; this is only the file.

#include "HlSave.hpp"

namespace hlrt::SaveGame
{
    /// Read the save, if there is one and it is whole.
    bool read(hldata::SaveData &out);
    /// Write it: to a file of another name first, which then takes the
    /// save's place, so that switching off half-way leaves the old save.
    bool write(const hldata::SaveData &save);
} // namespace hlrt::SaveGame
