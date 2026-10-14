// The saves on the card (shared/HlSaveFiles.hpp): nine files and a list of
// them; a save written so that being cut off at any point loses nothing; the
// list made again when it cannot be believed; the most recent found.
// Against a "card" in memory that can be told to fail at any step.

#include <cstdio>
#include <map>
#include <string>

#include "HlSaveFiles.hpp"
#include "TestBspBuilder.hpp"

using namespace hldata;
using namespace hldstest;

namespace
{
    /// A folder in memory. `budget`: how many more steps (a write, a remove,
    /// a rename) it will do before it "loses power": the step it stops at
    /// is half done if it is a write (half the bytes), not done otherwise,
    /// and nothing after it happens. -1: it never stops.
    struct MemoryCard : SaveCard
    {
        std::map<std::string, std::vector<std::uint8_t>> files;
        int budget = -1;
        int steps = 0;
        bool spend()
        {
            ++steps;
            if (budget < 0)
                return true;
            if (budget == 0)
                return false;
            --budget;
            return true;
        }
        bool read(const char *name, std::vector<std::uint8_t> &out, std::size_t most) override
        {
            const auto at = files.find(name);
            if (at == files.end())
                return false;
            out.assign(at->second.begin(), at->second.begin() + std::min(most, at->second.size()));
            return true;
        }
        bool write(const char *name, const std::uint8_t *bytes, std::size_t size) override
        {
            if (!spend())
            {
                if (budget == 0)
                {
                    files[name].assign(bytes, bytes + size / 2); // (cut off half-way)
                    budget = -2;                                 // (... and dead from here on)
                }
                return false;
            }
            files[name].assign(bytes, bytes + size);
            return true;
        }
        bool remove(const char *name) override
        {
            if (budget == -2 || !spend())
            {
                budget = -2;
                return false;
            }
            return files.erase(name) != 0;
        }
        bool rename(const char *from, const char *to) override
        {
            if (budget == -2 || !spend())
            {
                budget = -2;
                return false;
            }
            const auto at = files.find(from);
            if (at == files.end() || files.count(to) != 0)
                return false;
            files[to] = at->second;
            files.erase(at);
            return true;
        }
    };

    SaveData game(const char *map, int health)
    {
        SaveData data;
        data.map = map;
        data.health = health;
        data.state.assign(3000, static_cast<std::uint8_t>(health)); // (as a map's state would fill it)
        return data;
    }
    SaveHeading heading(const char *map, const char *chapter, unsigned seconds)
    {
        SaveHeading h;
        std::snprintf(h.map, sizeof(h.map), "%s", map);
        std::snprintf(h.chapter, sizeof(h.chapter), "%s", chapter);
        h.playSeconds = seconds;
        return h;
    }
} // namespace

int main()
{
    // An empty card: an empty list, nothing most recent.
    {
        MemoryCard card;
        SaveIndex index;
        saveIndexLoad(card, index);
        bool empty = true;
        for (const SaveSlot &slot : index.slots)
            empty = empty && slot.status == kSaveNone;
        check(empty && saveMostRecent(index) < 0 && saveLine(index.slots[0]) == "EMPTY", "a new card has no saves");
        check(card.files.count(kSaveIndexFile) == 1, "... and is given its list");
    }

    // Saves made, listed, read back; the most recent known; each kind in
    // its own file.
    MemoryCard card;
    SaveIndex index;
    saveIndexLoad(card, index);
    check(saveSlotWrite(card, index, kSlotAuto, game("c1a0", 90), heading("c1a0", "ANOMALOUS MATERIALS", 300)),
          "the game's own save is written");
    check(saveSlotWrite(card, index, kSlotManual + 2, game("c1a1", 55), heading("c1a1", "UNFORESEEN CONSEQUENCES", 3725)),
          "one of the player's is written");
    check(saveSlotWrite(card, index, kSlotQuick, game("c1a1a", 40), heading("c1a1a", "UNFORESEEN CONSEQUENCES", 4000)),
          "the quick one is written");
    check(card.files.count("auto.sav") && card.files.count("save3.sav") && card.files.count("quick.sav") &&
              !card.files.count("save3.sav.new"),
          "each in its own file, with nothing left beside it");
    check(index.slots[kSlotAuto].heading.kind == kSaveKindAuto && index.slots[kSlotManual + 2].heading.kind == kSaveKindManual &&
              index.slots[kSlotQuick].heading.kind == kSaveKindQuick,
          "each knows what kind it is");
    check(saveMostRecent(index) == kSlotQuick, "the most recent is the last one made");
    check(saveLine(index.slots[kSlotManual + 2]) == "1:02 c1a1 UNFORESEEN CONSEQUENCES", "a line of the list says how long, where, and the chapter");
    {
        SaveSlot dated = index.slots[kSlotManual + 2];
        dated.heading.year = 2026;
        dated.heading.month = 10;
        dated.heading.day = 9;
        dated.heading.hour = 14;
        dated.heading.minute = 5;
        check(saveLine(dated) == "1:02 c1a1 09/10 14:05 UNFORESEEN CONSEQUENCES", "... and when, if the console's clock said");
    }
    {
        SaveData back;
        SaveHeading h;
        check(saveSlotRead(card, kSlotManual + 2, back, &h) && back.map == "c1a1" && back.health == 55 &&
                  back.state.size() == 3000 && h.playSeconds == 3725,
              "a save read back is what was written, its map's state with it");
        check(!saveSlotRead(card, kSlotManual, back), "an empty slot reads as nothing");
    }
    // The save on coming into a map is there to go back to, and not shown.
    check(saveSlotWrite(card, index, kSlotArrival, game("c1a2", 77), heading("c1a2", "OFFICE COMPLEX", 4100)) &&
              saveMostRecent(index) == kSlotQuick && saveMostRecent(index, true) == kSlotArrival,
          "the save made on arriving is the most recent of all, and not among those shown");

    // The list is only a copy: gone, or damaged, it is made again.
    {
        const SaveIndex before = index;
        card.files.erase(kSaveIndexFile);
        SaveIndex again;
        saveIndexLoad(card, again);
        bool same = again.nextSerial == before.nextSerial;
        for (int i = 0; i < kSlotCount; ++i)
            same = same && again.slots[i].status == before.slots[i].status &&
                   again.slots[i].heading.serial == before.slots[i].heading.serial;
        check(same && card.files.count(kSaveIndexFile) == 1, "the list is made again from the files when it is gone");
        card.files[kSaveIndexFile][40] ^= 0x55;
        SaveIndex third;
        saveIndexLoad(card, third);
        check(third.slots[kSlotQuick].status == kSaveGood && third.nextSerial == before.nextSerial,
              "... and when it is damaged");
    }

    // A file that is not a save of this version is said to be what it is.
    {
        MemoryCard other = card;
        other.files["save1.sav"] = other.files["quick.sav"];
        other.files["save1.sav"][4] = static_cast<std::uint8_t>(kSaveVersion + 1); // (its version)
        other.files["save1.sav"][5] = 0;
        other.files["save2.sav"] = other.files["quick.sav"];
        other.files["save2.sav"][sizeof(SaveHeader) + 9] ^= 1; // (its heading)
        other.files["save4.sav"] = {'n', 'o', 't', ' ', 'o', 'n', 'e'};
        other.files.erase(kSaveIndexFile);
        SaveIndex listed;
        saveIndexLoad(other, listed);
        check(listed.slots[kSlotManual].status == kSaveOther && saveLine(listed.slots[kSlotManual]) == "FROM ANOTHER VERSION",
              "a save of another version is listed as that");
        check(listed.slots[kSlotManual + 1].status == kSaveBad && listed.slots[kSlotManual + 3].status == kSaveNone,
              "a damaged one as damaged; what is no save at all as nothing");
        SaveData back;
        check(!saveSlotRead(other, kSlotManual, back) && !saveSlotRead(other, kSlotManual + 1, back),
              "... and neither is loaded");
    }

    // Cut off at every step of a write: afterwards there is a whole save in
    // the slot -- the old one or the new -- and never neither.
    {
        MemoryCard fresh = card;
        fresh.budget = -1;
        fresh.steps = 0;
        SaveIndex scratch = index;
        saveSlotWrite(fresh, scratch, kSlotQuick, game("c1a3", 12), heading("c1a3", "WE'VE GOT HOSTILES", 5000));
        const int steps = fresh.steps;
        check(steps >= 4, "a write is several steps");
        bool always = true, sometimesOld = false, sometimesNew = false;
        for (int cut = 0; cut <= steps; ++cut)
        {
            MemoryCard torn = card;
            torn.budget = cut;
            SaveIndex during = index;
            saveSlotWrite(torn, during, kSlotQuick, game("c1a3", 12), heading("c1a3", "WE'VE GOT HOSTILES", 5000));
            // The power comes back.
            torn.budget = -1;
            SaveIndex after;
            saveIndexLoad(torn, after);
            SaveData back;
            const bool whole = after.slots[kSlotQuick].status == kSaveGood && saveSlotRead(torn, kSlotQuick, back) &&
                               (back.map == "c1a1a" || back.map == "c1a3") &&
                               std::string(after.slots[kSlotQuick].heading.map) == back.map;
            always = always && whole && torn.files.count("quick.sav.new") == 0;
            sometimesOld = sometimesOld || back.map == "c1a1a";
            sometimesNew = sometimesNew || back.map == "c1a3";
            // (And the others are as they were.)
            always = always && saveSlotRead(torn, kSlotAuto, back) && back.map == "c1a0";
        }
        check(always, "cut off at any step, the slot holds a whole save and the list agrees with it");
        check(sometimesOld && sometimesNew, "... the old one if it was cut off early, the new one if late");
    }

    // A card that will not take it: said, and the old save still there.
    {
        MemoryCard full = card;
        full.budget = 0;
        SaveIndex during = index;
        check(!saveSlotWrite(full, during, kSlotAuto, game("c1a4", 5), heading("c1a4", "BLAST PIT", 6000)),
              "a write the card refuses is said to have failed");
        full.budget = -1;
        SaveIndex after;
        saveIndexLoad(full, after);
        SaveData back;
        check(saveSlotRead(full, kSlotAuto, back) && back.map == "c1a0", "... and the save that was there is there still");
    }

    if (g_failures == 0)
        std::printf("test_savefiles: ok\n");
    return g_failures == 0 ? 0 : 1;
}
