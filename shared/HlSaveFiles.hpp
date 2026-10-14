#pragma once

// The saves on the card: which files there are, how one is written so that
// switching off half-way loses nothing, and the list of them.
//
// Nine files. One is the game's own (AUTOSAVE: written at a map's autosave
// points and on arriving in a map), one is QUICK (the menu's SAVE), six are
// the player's to choose and overwrite, and one more, never shown, is the
// state on coming into the map (what someone who dies again and again is
// taken back to). Each begins with a heading (HlSave.hpp) that says what it
// is, and an index beside them holds the nine headings, so that showing the
// list opens one file and not nine: opening a file on a card is the slow
// part. The index is only a copy -- it is made again from the files if it is
// not there or cannot be believed.
//
// A save is written to a file of another name beside the old one, and takes
// its place only when it is whole. Whatever is found half-way at the next
// start -- the new one there and the old one gone, or both -- is put right
// then (saveRecover).
//
// All of it is over a small interface to "a folder of files" (SaveCard), so
// that the host tests run it against a folder of their own, and against one
// that fails where they tell it to. std-only and inline.

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "HlSave.hpp"

namespace hldata
{
    inline constexpr int kSlotAuto = 0;
    inline constexpr int kSlotQuick = 1;
    inline constexpr int kSlotManual = 2;  ///< ... and the five after it
    inline constexpr int kSlotManuals = 6;
    inline constexpr int kSlotsListed = 8; ///< what the player is shown
    inline constexpr int kSlotArrival = 8;
    inline constexpr int kSlotCount = 9;

    inline const char *saveSlotFile(int slot)
    {
        static const char *const kNames[kSlotCount] = {"auto.sav",  "quick.sav", "save1.sav", "save2.sav", "save3.sav",
                                                       "save4.sav", "save5.sav", "save6.sav", "arrival.sav"};
        return slot >= 0 && slot < kSlotCount ? kNames[slot] : "";
    }
    inline constexpr const char *kSaveIndexFile = "index.dat";
    inline std::uint8_t saveSlotKind(int slot)
    {
        return slot == kSlotAuto ? kSaveKindAuto
                                 : (slot == kSlotQuick ? kSaveKindQuick
                                                       : (slot == kSlotArrival ? kSaveKindArrival : kSaveKindManual));
    }

    /// A folder of files, by name.
    struct SaveCard
    {
        virtual ~SaveCard() = default;
        /// The first `most` bytes of a file (all of it, if it is shorter).
        /// False if it is not there or cannot be read.
        virtual bool read(const char *name, std::vector<std::uint8_t> &out, std::size_t most) = 0;
        /// A file made (or made again) with exactly these bytes. False if
        /// not all of them were written.
        virtual bool write(const char *name, const std::uint8_t *bytes, std::size_t size) = 0;
        virtual bool remove(const char *name) = 0;
        /// (To a name that is not there: FAT will not rename over a file.)
        virtual bool rename(const char *from, const char *to) = 0;
    };

    struct SaveSlot
    {
        int status = kSaveNone; ///< kSave*
        SaveHeading heading;
        /// For kSaveOther: the numbers its header had (0: not known), to say
        /// why it is not read (saveRefusal). Not in the index: found again.
        std::uint16_t version = 0, minReader = 0, levelVersion = 0;
    };
    struct SaveIndex
    {
        SaveSlot slots[kSlotCount];
        std::uint32_t nextSerial = 1;
    };

    namespace detail
    {
        inline constexpr char kIndexMagic[4] = {'H', 'L', 'S', 'I'};
        struct IndexFile
        {
            char magic[4];
            std::uint16_t version;
            std::uint16_t levelVersion;
            std::uint32_t nextSerial;
            std::uint32_t crc; ///< of the slots
            struct
            {
                std::uint32_t status;
                SaveHeading heading;
            } slots[kSlotCount];
        };

        inline std::string freshName(int slot)
        {
            return std::string(saveSlotFile(slot)) + ".new";
        }

        inline void noteHeader(const std::vector<std::uint8_t> &bytes, SaveSlot &slot)
        {
            SaveHeader header;
            if (slot.status != kSaveOther || bytes.size() < sizeof(header))
                return;
            std::memcpy(&header, bytes.data(), sizeof(header));
            slot.version = header.version;
            slot.minReader = header.minReader;
            slot.levelVersion = header.levelVersion;
        }
    } // namespace detail

    /// One slot put right after a write that was cut off, and what it is
    /// now. The new file, if it is whole, is the save (the old one goes);
    /// if it is not, it goes.
    inline SaveSlot saveRecover(SaveCard &card, int slot)
    {
        SaveSlot out;
        const std::string fresh = detail::freshName(slot);
        std::vector<std::uint8_t> bytes;
        if (card.read(fresh.c_str(), bytes, kSaveMaxBytes + kSaveHeadBytes + 1))
        {
            if (saveWhole(bytes.data(), bytes.size()))
            {
                card.remove(saveSlotFile(slot));
                card.rename(fresh.c_str(), saveSlotFile(slot));
            }
            else
            {
                card.remove(fresh.c_str());
            }
        }
        bytes.clear();
        if (card.read(saveSlotFile(slot), bytes, kSaveHeadBytes))
        {
            out.status = readSaveHeading(bytes.data(), bytes.size(), out.heading);
            detail::noteHeader(bytes, out);
        }
        return out;
    }

    inline bool saveIndexWrite(SaveCard &card, const SaveIndex &index)
    {
        detail::IndexFile file{};
        std::memcpy(file.magic, detail::kIndexMagic, sizeof(file.magic));
        file.version = kSaveVersion;
        file.levelVersion = kLevelVersion;
        file.nextSerial = index.nextSerial;
        for (int i = 0; i < kSlotCount; ++i)
        {
            file.slots[i].status = static_cast<std::uint32_t>(index.slots[i].status);
            file.slots[i].heading = index.slots[i].heading;
        }
        file.crc = crc32(reinterpret_cast<const std::uint8_t *>(file.slots), sizeof(file.slots));
        return card.write(kSaveIndexFile, reinterpret_cast<const std::uint8_t *>(&file), sizeof(file));
    }

    /// The list made again from the files themselves (each put right
    /// first), and written.
    inline void saveIndexRebuild(SaveCard &card, SaveIndex &index)
    {
        index = SaveIndex();
        for (int i = 0; i < kSlotCount; ++i)
        {
            index.slots[i] = saveRecover(card, i);
            if (index.slots[i].status == kSaveGood && index.slots[i].heading.serial >= index.nextSerial)
                index.nextSerial = index.slots[i].heading.serial + 1;
        }
        saveIndexWrite(card, index);
    }

    /// The list: from the index if it is there and whole, else from the
    /// files. (Once, at the start: whatever a write left half-way is put
    /// right before the index is believed -- `recover` false to skip that,
    /// for a list asked for again.)
    inline void saveIndexLoad(SaveCard &card, SaveIndex &index, bool recover = true)
    {
        std::vector<std::uint8_t> bytes;
        detail::IndexFile file;
        bool good = card.read(kSaveIndexFile, bytes, sizeof(file) + 1) && bytes.size() == sizeof(file);
        if (good)
        {
            std::memcpy(&file, bytes.data(), sizeof(file));
            good = std::memcmp(file.magic, detail::kIndexMagic, sizeof(file.magic)) == 0 && file.version == kSaveVersion &&
                   file.levelVersion == kLevelVersion &&
                   file.crc == crc32(reinterpret_cast<const std::uint8_t *>(file.slots), sizeof(file.slots));
        }
        // (A new file left beside a save means the index is behind.)
        for (int i = 0; good && recover && i < kSlotCount; ++i)
        {
            std::vector<std::uint8_t> probe;
            if (card.read(detail::freshName(i).c_str(), probe, 1))
                good = false;
        }
        if (!good)
        {
            saveIndexRebuild(card, index);
            return;
        }
        index.nextSerial = file.nextSerial;
        for (int i = 0; i < kSlotCount; ++i)
        {
            index.slots[i].status = static_cast<int>(file.slots[i].status);
            index.slots[i].heading = file.slots[i].heading;
            index.slots[i].heading.map[sizeof(index.slots[i].heading.map) - 1] = 0;
            index.slots[i].heading.chapter[sizeof(index.slots[i].heading.chapter) - 1] = 0;
            if (index.slots[i].status == kSaveOther)
            {
                std::vector<std::uint8_t> head;
                if (card.read(saveSlotFile(i), head, kSaveHeadBytes))
                    detail::noteHeader(head, index.slots[i]);
            }
        }
    }

    /// A save written into a slot: beside the old one first, which goes
    /// only when the new one is whole on the card. The heading is given its
    /// kind and its number here. False if the card would not take it -- the
    /// old save is then still there.
    inline bool saveSlotWrite(SaveCard &card, SaveIndex &index, int slot, const SaveData &data, SaveHeading heading)
    {
        if (slot < 0 || slot >= kSlotCount)
            return false;
        heading.kind = saveSlotKind(slot);
        heading.serial = index.nextSerial;
        const std::vector<std::uint8_t> bytes = writeSave(data, heading);
        if (bytes.size() > kSaveMaxBytes + kSaveHeadBytes)
            return false;
        const std::string fresh = detail::freshName(slot);
        if (!card.write(fresh.c_str(), bytes.data(), bytes.size()))
        {
            card.remove(fresh.c_str());
            return false;
        }
        // (Read back: what is on the card is what was meant to be.)
        std::vector<std::uint8_t> back;
        if (!card.read(fresh.c_str(), back, bytes.size() + 1) || back != bytes)
        {
            card.remove(fresh.c_str());
            return false;
        }
        card.remove(saveSlotFile(slot));
        if (!card.rename(fresh.c_str(), saveSlotFile(slot)))
            return false; // (the new one is still there under its other name: saveRecover finds it)
        index.slots[slot].status = kSaveGood;
        index.slots[slot].heading = heading;
        ++index.nextSerial;
        saveIndexWrite(card, index); // (only a copy: a failure here costs a rebuild)
        return true;
    }

    inline bool saveSlotRead(SaveCard &card, int slot, SaveData &out, SaveHeading *heading = nullptr)
    {
        if (slot < 0 || slot >= kSlotCount)
            return false;
        std::vector<std::uint8_t> bytes;
        if (!card.read(saveSlotFile(slot), bytes, kSaveMaxBytes + kSaveHeadBytes + 1))
            return false;
        return readSave(bytes.data(), bytes.size(), out, heading);
    }

    /// The most recent save that can be loaded: of those shown, or with
    /// `arrival` of all nine. -1: none.
    inline int saveMostRecent(const SaveIndex &index, bool arrival = false)
    {
        int best = -1;
        for (int i = 0; i < (arrival ? kSlotCount : kSlotsListed); ++i)
        {
            if (index.slots[i].status == kSaveGood &&
                (best < 0 || index.slots[i].heading.serial > index.slots[best].heading.serial))
                best = i;
        }
        return best;
    }

    /// A line for the list: how long was played, the map, when (if the
    /// console's clock said), the chapter -- "1:02 c1a1 12/10 14:05
    /// UNFORESEEN CONSEQUENCES" -- as much of it as there is.
    inline std::string saveLine(const SaveSlot &slot)
    {
        if (slot.status == kSaveNone)
            return "EMPTY";
        if (slot.status == kSaveOther)
            return slot.version != 0 ? saveRefusal(slot.version, slot.minReader, slot.levelVersion)
                                     : "FROM ANOTHER VERSION";
        if (slot.status == kSaveBad)
            return "DAMAGED";
        const SaveHeading &h = slot.heading;
        char text[112];
        const unsigned hours = h.playSeconds / 3600, minutes = h.playSeconds / 60 % 60;
        if (h.year != 0)
            std::snprintf(text, sizeof(text), "%u:%02u %s %02u/%02u %02u:%02u %s", hours, minutes, h.map, h.day, h.month,
                          h.hour, h.minute, h.chapter);
        else
            std::snprintf(text, sizeof(text), "%u:%02u %s %s", hours, minutes, h.map, h.chapter);
        return text;
    }
} // namespace hldata
