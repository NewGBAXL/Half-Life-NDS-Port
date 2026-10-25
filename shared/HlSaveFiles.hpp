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

        // A file a piece at a time (SaveWriter: a save written over many
        // frames). As they are here, made of the calls above -- a card that
        // can keep a file open between pieces does better.

        /// `bytes` added to the end of a file; `first`: the file made anew
        /// with them. False if not all of them were written.
        virtual bool append(const char *name, const std::uint8_t *bytes, std::size_t size, bool first)
        {
            if (first)
                return write(name, bytes, size);
            std::vector<std::uint8_t> was;
            if (!read(name, was, static_cast<std::size_t>(-1)))
                return false;
            was.insert(was.end(), bytes, bytes + size);
            return write(name, was.data(), was.size());
        }
        /// Up to `size` bytes of a file from `offset` (fewer at its end).
        virtual bool readAt(const char *name, std::size_t offset, std::size_t size, std::vector<std::uint8_t> &out)
        {
            if (!read(name, out, offset + size))
                return false;
            out.erase(out.begin(), out.begin() + static_cast<std::ptrdiff_t>(offset < out.size() ? offset : out.size()));
            return true;
        }
        /// Done with a file read or written a piece at a time. False if what
        /// was written did not all reach the card.
        virtual bool finish(const char *name)
        {
            (void)name;
            return true;
        }
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

    /// A run's map file's name ("run/c1a1.mst").
    inline void runFileOf(const char *map, char (&out)[48]) { std::snprintf(out, sizeof(out), "run/%.31s.mst", map); }

    /// The CRC of `size` bytes of a file from `offset`, read a piece at a
    /// time into `scratch`; false if they cannot all be read.
    inline bool cardCrc(SaveCard &card, const char *name, std::size_t offset, std::size_t size,
                        std::vector<std::uint8_t> &scratch, std::uint32_t &crc)
    {
        crc = kCrcStart;
        for (std::size_t at = 0; at < size;)
        {
            const std::size_t want = size - at < 4096 ? size - at : 4096;
            if (!card.readAt(name, offset + at, want, scratch) || scratch.size() != want)
                return false;
            crc = crc32Update(crc, scratch.data(), want);
            at += want;
        }
        crc = crc32Finish(crc);
        return true;
    }

    /// A save file's trailer (the maps left it holds): its heading and
    /// table, read from where the payload ends (`at`). False if there is
    /// none, or it is not whole; the files themselves are not read.
    inline bool saveTrailer(SaveCard &card, const char *name, std::size_t at, VisitedHead &head,
                            std::vector<RunEntry> &table, std::vector<std::uint8_t> &scratch)
    {
        table.clear();
        if (!card.readAt(name, at, sizeof(head), scratch) || scratch.size() != sizeof(head))
            return false;
        std::memcpy(&head, scratch.data(), sizeof(head));
        if (std::memcmp(head.magic, kVisitedMagic, 4) != 0 || head.count > kStoredMapsMost ||
            head.bytes < head.count * sizeof(RunEntry))
            return false;
        const std::size_t tableBytes = head.count * sizeof(RunEntry);
        if (tableBytes != 0 && (!card.readAt(name, at + sizeof(head), tableBytes, scratch) || scratch.size() != tableBytes))
            return false;
        if (crc32(scratch.data(), static_cast<std::uint32_t>(tableBytes)) != head.tableCrc)
            return false;
        table.resize(head.count);
        if (tableBytes != 0)
            std::memcpy(table.data(), scratch.data(), tableBytes);
        std::size_t files = 0;
        for (const RunEntry &entry : table)
            files += entry.bytes;
        return (head.flags & kVisitedByRef) ? head.bytes == tableBytes : head.bytes == tableBytes + files;
    }

    /// Whether a save file is whole, all of it: its payload, and the maps
    /// left that follow it, each file its own CRC.
    inline bool saveFileWhole(SaveCard &card, const char *name)
    {
        std::vector<std::uint8_t> bytes;
        if (!card.read(name, bytes, kSaveHeadBytes) || bytes.size() != kSaveHeadBytes)
            return false;
        SaveHeader header;
        std::memcpy(&header, bytes.data(), sizeof(header));
        if (header.payloadBytes > kSaveMaxBytes || !card.read(name, bytes, kSaveHeadBytes + header.payloadBytes) ||
            !saveWhole(bytes.data(), bytes.size()))
            return false;
        // (Before version 9, nothing after it.)
        std::vector<std::uint8_t> scratch;
        VisitedHead head;
        std::vector<RunEntry> table;
        const std::size_t at = kSaveHeadBytes + header.payloadBytes;
        if (!saveTrailer(card, name, at, head, table, scratch))
        {
            std::vector<std::uint8_t> more;
            return header.version < 9 && card.readAt(name, at, 1, more) && more.empty();
        }
        std::size_t file = at + sizeof(head) + table.size() * sizeof(RunEntry);
        for (const RunEntry &entry : table)
        {
            if ((head.flags & kVisitedByRef) || entry.bytes < 8)
                break;
            std::uint32_t crc = 0;
            if (!cardCrc(card, name, file, entry.bytes - 8, scratch, crc) || crc != entry.crc)
                return false;
            file += entry.bytes;
        }
        return true;
    }

    /// One slot put right after a write that was cut off, and what it is
    /// now. The new file, if it is whole, is the save (the old one goes);
    /// if it is not, it goes.
    inline SaveSlot saveRecover(SaveCard &card, int slot)
    {
        SaveSlot out;
        const std::string fresh = detail::freshName(slot);
        std::vector<std::uint8_t> bytes;
        if (card.read(fresh.c_str(), bytes, kSaveHeadBytes))
        {
            if (saveFileWhole(card, fresh.c_str()))
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

    /// What a save's card work is done in, a step at a time.
    inline constexpr std::size_t kSaveStepBytes = 4096;

    /// A save written into a slot: beside the old one first, which goes
    /// only when the new one is whole on the card (and read back to be the
    /// same). The heading is given its kind and its number here. One step()
    /// does one piece of the card's work -- a few kilobytes written or read,
    /// a file removed or renamed -- so a game can write a save over a few
    /// frames and go on being played. Cut off anywhere, it leaves what
    /// saveRecover puts right; failed, the old save is still there.
    class SaveWriter
    {
    public:
        enum class Result
        {
            Busy,
            Done,
            Failed
        };

        /// False (and nothing begun) if the save cannot be made at all.
        /// `run`: the maps left, kept on the card, to be in the save after
        /// the rest -- copied in from their files, or (`byRef`) only named.
        bool begin(SaveCard &card, SaveIndex &index, int slot, const SaveData &data, SaveHeading heading,
                   const RunIndex *run = nullptr, bool byRef = false)
        {
            stage_ = Stage::Idle;
            if (slot < 0 || slot >= kSlotCount)
                return false;
            heading.kind = saveSlotKind(slot);
            heading.serial = index.nextSerial;
            bytes_ = writeSave(data, heading);
            if (bytes_.size() > kSaveMaxBytes + kSaveHeadBytes)
                return false;
            // The maps left: a heading and their table, here; their files
            // after, copied a piece at a time (Copy).
            files_.clear();
            VisitedHead head{};
            std::memcpy(head.magic, kVisitedMagic, 4);
            head.flags = byRef ? kVisitedByRef : 0;
            if (run != nullptr)
            {
                head.count = static_cast<std::uint16_t>(run->count);
                head.runId = run->runId;
                head.generation = run->generation;
                for (int k = 0; k < run->count; ++k)
                    files_.push_back(run->entries[k]);
            }
            const std::size_t tableBytes = files_.size() * sizeof(RunEntry);
            std::size_t fileBytes = 0;
            for (const RunEntry &entry : files_)
                fileBytes += entry.bytes;
            head.bytes = static_cast<std::uint32_t>(tableBytes + (byRef ? 0 : fileBytes));
            head.tableCrc = crc32(reinterpret_cast<const std::uint8_t *>(files_.data()), static_cast<std::uint32_t>(tableBytes));
            const std::size_t at = bytes_.size();
            bytes_.resize(at + sizeof(head) + tableBytes);
            std::memcpy(bytes_.data() + at, &head, sizeof(head));
            if (tableBytes != 0)
                std::memcpy(bytes_.data() + at + sizeof(head), files_.data(), tableBytes);
            if (byRef)
                files_.clear();
            card_ = &card;
            index_ = &index;
            slot_ = slot;
            heading_ = heading;
            fresh_ = detail::freshName(slot);
            at_ = 0;
            stage_ = Stage::Write;
            return true;
        }

        bool busy() const { return stage_ != Stage::Idle; }
        int slot() const { return slot_; }

        Result step()
        {
            switch (stage_)
            {
            case Stage::Idle:
                return Result::Failed;
            case Stage::Write:
            {
                const std::size_t n = piece();
                if (!card_->append(fresh_.c_str(), bytes_.data() + at_, n, at_ == 0))
                    return fail();
                at_ += n;
                if (at_ == bytes_.size())
                {
                    stage_ = files_.empty() ? Stage::Close : Stage::Copy;
                    file_ = 0;
                    fileAt_ = 0;
                }
                return Result::Busy;
            }
            case Stage::Copy:
            {
                // A piece of a map left's file, from the run's folder into
                // the save.
                const RunEntry &entry = files_[file_];
                char name[48];
                runFileOf(entry.name, name);
                const std::size_t left = entry.bytes - fileAt_;
                const std::size_t n = left < kSaveStepBytes ? left : kSaveStepBytes;
                if (!card_->readAt(name, fileAt_, n, piece_) || piece_.size() != n ||
                    !card_->append(fresh_.c_str(), piece_.data(), n, false))
                    return fail();
                fileAt_ += n;
                if (fileAt_ == entry.bytes)
                {
                    card_->finish(name);
                    fileAt_ = 0;
                    if (++file_ == files_.size())
                        stage_ = Stage::Close;
                }
                return Result::Busy;
            }
            case Stage::Close:
                if (!card_->finish(fresh_.c_str()))
                    return fail();
                at_ = 0;
                stage_ = Stage::Check;
                return Result::Busy;
            case Stage::Check:
            {
                // (Read back: what is on the card is what was meant to be --
                // and no longer.)
                const std::size_t n = piece();
                std::vector<std::uint8_t> back;
                const bool last = at_ + n == bytes_.size() && files_.empty();
                if (!card_->readAt(fresh_.c_str(), at_, n + (last ? 1 : 0), back) ||
                    back.size() != n || std::memcmp(back.data(), bytes_.data() + at_, n) != 0)
                    return fail();
                at_ += n;
                if (at_ == bytes_.size())
                {
                    stage_ = files_.empty() ? Stage::Swap : Stage::CheckFiles;
                    file_ = 0;
                    fileAt_ = at_;
                    if (files_.empty())
                        card_->finish(fresh_.c_str());
                }
                return Result::Busy;
            }
            case Stage::CheckFiles:
            {
                // (Each file copied in is as it was: its own CRC.)
                const RunEntry &entry = files_[file_];
                std::uint32_t crc = 0;
                if (entry.bytes < 8 || !cardCrc(*card_, fresh_.c_str(), fileAt_, entry.bytes - 8, piece_, crc) ||
                    crc != entry.crc)
                    return fail();
                fileAt_ += entry.bytes;
                if (++file_ == files_.size())
                {
                    // (... and nothing after the last.)
                    std::vector<std::uint8_t> more;
                    if (!card_->readAt(fresh_.c_str(), fileAt_, 1, more) || !more.empty())
                        return fail();
                    card_->finish(fresh_.c_str());
                    stage_ = Stage::Swap;
                }
                return Result::Busy;
            }
            case Stage::Swap:
                card_->remove(saveSlotFile(slot_));
                stage_ = Stage::Rename;
                return Result::Busy;
            case Stage::Rename:
                stage_ = Stage::Idle;
                if (!card_->rename(fresh_.c_str(), saveSlotFile(slot_)))
                {
                    letGo();
                    return Result::Failed; // (the new one is still there under its other name: saveRecover finds it)
                }
                index_->slots[slot_].status = kSaveGood;
                index_->slots[slot_].heading = heading_;
                ++index_->nextSerial;
                saveIndexWrite(*card_, *index_); // (only a copy: a failure here costs a rebuild)
                letGo();
                return Result::Done;
            }
            return Result::Failed;
        }

        /// Every step that is left, now.
        bool finish()
        {
            Result r = Result::Busy;
            while (busy() && (r = step()) == Result::Busy)
            {
            }
            return r == Result::Done;
        }

    private:
        enum class Stage
        {
            Idle,
            Write,
            Copy,
            Close,
            Check,
            CheckFiles,
            Swap,
            Rename
        };

        std::size_t piece() const
        {
            const std::size_t left = bytes_.size() - at_;
            return left < kSaveStepBytes ? left : kSaveStepBytes;
        }
        Result fail()
        {
            card_->finish(fresh_.c_str());
            card_->remove(fresh_.c_str());
            stage_ = Stage::Idle;
            letGo();
            return Result::Failed;
        }
        /// Every block it took, given back: none of it is to stay in the
        /// middle of a DS's heap.
        void letGo()
        {
            std::vector<std::uint8_t>().swap(bytes_);
            std::vector<RunEntry>().swap(files_);
            std::vector<std::uint8_t>().swap(piece_);
            std::string().swap(fresh_);
        }

        Stage stage_ = Stage::Idle;
        SaveCard *card_ = nullptr;
        SaveIndex *index_ = nullptr;
        int slot_ = -1;
        SaveHeading heading_;
        std::string fresh_;
        std::vector<std::uint8_t> bytes_;
        std::size_t at_ = 0;
        std::vector<RunEntry> files_; ///< the maps left whose files are copied in
        std::size_t file_ = 0, fileAt_ = 0;
        std::vector<std::uint8_t> piece_;
    };

    /// The same, all at once. False if the card would not take it -- the
    /// old save is then still there.
    inline bool saveSlotWrite(SaveCard &card, SaveIndex &index, int slot, const SaveData &data, SaveHeading heading,
                              const RunIndex *run = nullptr, bool byRef = false)
    {
        SaveWriter writer;
        return writer.begin(card, index, slot, data, heading, run, byRef) && writer.finish();
    }

    /// The save in `slot`: its payload read and checked; and of its trailer,
    /// the table of the maps left it holds (their files stay on the card,
    /// for runFromSave: HlMapStore.hpp).
    inline bool saveSlotRead(SaveCard &card, int slot, SaveData &out, SaveHeading *heading = nullptr)
    {
        if (slot < 0 || slot >= kSlotCount)
            return false;
        std::vector<std::uint8_t> bytes;
        if (!card.read(saveSlotFile(slot), bytes, kSaveHeadBytes) || bytes.size() != kSaveHeadBytes)
            return false;
        SaveHeader header;
        std::memcpy(&header, bytes.data(), sizeof(header));
        if (header.payloadBytes > kSaveMaxBytes ||
            !card.read(saveSlotFile(slot), bytes, kSaveHeadBytes + header.payloadBytes) ||
            !readSave(bytes.data(), bytes.size(), out, heading))
            return false;
        VisitedHead head;
        std::vector<std::uint8_t> scratch;
        if (saveTrailer(card, saveSlotFile(slot), kSaveHeadBytes + header.payloadBytes, head, out.visited, scratch))
        {
            out.visitedAt = static_cast<std::uint32_t>(kSaveHeadBytes + header.payloadBytes + sizeof(head) +
                                                       out.visited.size() * sizeof(RunEntry));
            out.visitedByRef = (head.flags & kVisitedByRef) != 0;
            out.visitedRunId = head.runId;
            out.visitedGeneration = head.generation;
        }
        card.finish(saveSlotFile(slot));
        return true;
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
