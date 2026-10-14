#pragma once

// The saved game: one small file that says where the player is and what of
// the world does not grow back.
//
// It is NOT the PC game's save, which writes every entity of every map of the
// chapter. A map that is come back to starts as it started, with two
// exceptions that would otherwise be glaring: what has been BROKEN stays
// broken (a bitset per map visited), and the global switches keep their
// states. Loading a save puts the player where they stood, facing as they
// faced, on the train they were riding if they were riding one -- exactly
// what walking through a level change carries, plus a position.
//
// On the card it is the same shape as the settings file: a fixed header with
// a magic, a version, the payload's length and its CRC, then a yas archive.
// Every check that can be made without yas is made before yas runs, because
// yas reports a bad archive by throwing and the DS build has no exceptions.

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include <yas/serialize.hpp>
#include <yas/std_types.hpp>

#include "HlLevel.hpp"

namespace hldata
{
    /// What is broken in one map: a bit per logic record. And who is gone
    /// from it -- killed, or picked up: a bit per actor.
    struct SaveMapState
    {
        std::uint32_t map = 0; ///< nameHash of the map's name
        std::vector<std::uint8_t> broken;
        std::vector<std::uint8_t> gone;

        template <class Ar>
        void serialize(Ar &ar)
        {
            ar &map &broken &gone;
        }
    };

    struct SaveData
    {
        std::string map; ///< lower-case, no extension
        std::int32_t origin[3] = {0, 0, 0};
        std::int32_t velocity[3] = {0, 0, 0};
        std::uint16_t yaw = 0;
        std::int16_t pitch = 0;
        std::uint8_t ducked = 0;
        std::int32_t health = 100;
        // What they carry (Inventory).
        std::uint8_t weapons = 0;
        std::uint8_t weapon = 0;
        std::int16_t clipGlock = 0;
        std::int16_t clipShotgun = 0;
        std::int16_t ammo9mm = 0;
        std::int16_t ammoBuck = 0;
        std::int16_t armor = 0;
        std::int16_t grenades = 0;
        std::int16_t clipMp5 = 0;
        std::int16_t clipPython = 0;
        std::int16_t ammo357 = 0;
        std::int16_t ammoAr = 0;
        /// The lamp: alight or not, and what is left in it (0..100000).
        std::uint8_t lampOn = 0;
        std::int32_t lampCharge = 99000;

        std::vector<std::uint32_t> globalHash;
        std::vector<std::uint8_t> globalState;

        /// The track train the player is riding (the hash of its global
        /// name), or 0; and where it is.
        std::uint32_t train = 0;
        std::int32_t trainPos[3] = {0, 0, 0};
        std::uint16_t trainYaw = 0;
        std::int32_t trainSpeed = 0;
        std::uint8_t trainSelfDriven = 0;

        std::vector<SaveMapState> maps;

        /// The whole state of the map the player is in, as HlSaveState.hpp
        /// writes it (an archive of its own, with its own checks): every
        /// door, trigger and actor as it was. Empty: none -- the map is
        /// then begun again, with only what is above put right.
        std::vector<std::uint8_t> state;

        template <class Ar>
        void serialize(Ar &ar)
        {
            ar &map &origin[0] &origin[1] &origin[2] &velocity[0] &velocity[1] &velocity[2] &yaw
                &pitch &ducked &health &weapons &weapon &clipGlock &clipShotgun &ammo9mm &ammoBuck
                &armor &grenades &clipMp5 &clipPython &ammo357 &ammoAr &lampOn &lampCharge &globalHash &globalState &train &trainPos[0] &trainPos[1]
                &trainPos[2] &trainYaw &trainSpeed &trainSelfDriven &maps &state;
        }
    };

    inline constexpr char kSaveMagic[4] = {'H', 'L', 'S', 'G'};
    inline constexpr std::uint16_t kSaveVersion = 6;
    /// A save with a map's whole state in it is tens of kilobytes; one that
    /// claims to be more than this is not one.
    inline constexpr std::uint32_t kSaveMaxBytes = 256 * 1024;

    // What kind of save a file is.
    inline constexpr std::uint8_t kSaveKindAuto = 0;    ///< the game's own: an autosave point, or arriving in a map
    inline constexpr std::uint8_t kSaveKindQuick = 1;   ///< the menu's SAVE
    inline constexpr std::uint8_t kSaveKindManual = 2;  ///< one of the player's six
    inline constexpr std::uint8_t kSaveKindArrival = 3; ///< on coming into the map: what dying again and again goes back to

    /// What a save says of itself, at its head, where it can be read without
    /// the rest: for the list.
    struct SaveHeading
    {
        std::uint32_t serial = 0;      ///< goes up with every save made: the highest is the most recent
        std::uint32_t playSeconds = 0; ///< how long has been played
        /// When, by the console's clock; year 0 if it could not be read.
        std::uint16_t year = 0;
        std::uint8_t month = 0, day = 0, hour = 0, minute = 0;
        std::uint8_t kind = kSaveKindAuto;
        std::uint8_t health = 0;
        char map[32] = {};     ///< lower-case, no extension
        char chapter[44] = {}; ///< the chapter's title, as the game shows it
    };
    static_assert(sizeof(SaveHeading) == 92); // (no padding in it: it is checksummed as it lies)

    struct SaveHeader
    {
        char magic[4];
        std::uint16_t version;
        std::uint16_t levelVersion; ///< a save is of maps cooked in this format
        std::uint32_t payloadBytes;
        std::uint32_t crc32;        ///< of the payload
        std::uint32_t headingCrc;   ///< of the heading, which follows this
        std::uint32_t spare;
    };
    static_assert(sizeof(SaveHeader) == 24);
    inline constexpr std::size_t kSaveHeadBytes = sizeof(SaveHeader) + sizeof(SaveHeading);

    // What a file turned out to be.
    inline constexpr int kSaveNone = 0;  ///< no save: not there, or not one
    inline constexpr int kSaveGood = 1;
    inline constexpr int kSaveOther = 2; ///< a save, of another version of the game or of its maps
    inline constexpr int kSaveBad = 3;   ///< damaged

    // ------------------------------------------------------------- dying
    //
    // Where a death goes back to. There are two saves: the latest (a
    // trigger of the map's, the menu's SAVE) and the one made on coming
    // into the map. A save made a moment before certain death would
    // otherwise be a loop with no way out: so dying soon after going back,
    // three times running, goes back to the coming-in instead -- and if
    // that is no better either, to the map's own start.

    inline constexpr int kQuickDeathMs = 20000;
    inline constexpr int kBackLatest = 0;
    inline constexpr int kBackArrival = 1;
    inline constexpr int kBackStart = 2;
    struct DeathLoop
    {
        int quick = 0; ///< deaths on end, each soon after going back
    };
    /// A death, `sinceBackMs` after the game last went back (or began).
    inline int deathChoice(DeathLoop &loop, int sinceBackMs)
    {
        loop.quick = sinceBackMs < kQuickDeathMs ? loop.quick + 1 : 0;
        if (loop.quick >= 6)
        {
            loop.quick = 0;
            return kBackStart;
        }
        return loop.quick >= 3 ? kBackArrival : kBackLatest;
    }

    inline std::vector<std::uint8_t> writeSave(const SaveData &data, const SaveHeading &heading = SaveHeading())
    {
        constexpr std::size_t flags = yas::mem | yas::binary | yas::no_header;
        yas::mem_ostream os;
        yas::binary_oarchive<yas::mem_ostream, flags> oa(os);
        oa &data;
        const auto buf = os.get_intrusive_buffer();

        SaveHeader header{};
        std::memcpy(header.magic, kSaveMagic, sizeof(header.magic));
        header.version = kSaveVersion;
        header.levelVersion = kLevelVersion;
        header.payloadBytes = static_cast<std::uint32_t>(buf.size);
        header.crc32 = crc32(reinterpret_cast<const std::uint8_t *>(buf.data),
                             static_cast<std::uint32_t>(buf.size));
        header.headingCrc = crc32(reinterpret_cast<const std::uint8_t *>(&heading), sizeof(heading));
        std::vector<std::uint8_t> out(kSaveHeadBytes + buf.size);
        std::memcpy(out.data(), &header, sizeof(header));
        std::memcpy(out.data() + sizeof(header), &heading, sizeof(heading));
        std::memcpy(out.data() + kSaveHeadBytes, buf.data, buf.size);
        return out;
    }

    /// What the first bytes of a file say it is (kSave*), and its heading
    /// if it has a whole one. `size` is how many bytes of it were read: at
    /// least kSaveHeadBytes of a save.
    inline int readSaveHeading(const std::uint8_t *bytes, std::size_t size, SaveHeading &out)
    {
        SaveHeader header;
        if (size < sizeof(header))
            return kSaveNone;
        std::memcpy(&header, bytes, sizeof(header));
        if (std::memcmp(header.magic, kSaveMagic, sizeof(header.magic)) != 0)
            return kSaveNone;
        if (header.version != kSaveVersion || !levelSaveVersionOk(header.levelVersion))
            return kSaveOther;
        if (size < kSaveHeadBytes || header.payloadBytes > kSaveMaxBytes ||
            crc32(bytes + sizeof(header), sizeof(SaveHeading)) != header.headingCrc)
            return kSaveBad;
        std::memcpy(&out, bytes + sizeof(header), sizeof(out));
        // (They are shown: they end where they end.)
        out.map[sizeof(out.map) - 1] = 0;
        out.chapter[sizeof(out.chapter) - 1] = 0;
        return kSaveGood;
    }

    /// Whether these bytes are a whole save of this version: every check
    /// that can be made without reading the archive.
    inline bool saveWhole(const std::uint8_t *bytes, std::size_t size)
    {
        SaveHeading heading;
        SaveHeader header;
        if (readSaveHeading(bytes, size, heading) != kSaveGood)
            return false;
        std::memcpy(&header, bytes, sizeof(header));
        return header.payloadBytes == size - kSaveHeadBytes &&
               crc32(bytes + kSaveHeadBytes, header.payloadBytes) == header.crc32;
    }

    /// False for anything that is not a save of this version, damaged or not;
    /// `out` is then untouched.
    inline bool readSave(const std::uint8_t *bytes, std::size_t size, SaveData &out, SaveHeading *heading = nullptr)
    {
        constexpr std::size_t flags = yas::mem | yas::binary | yas::no_header;
        if (size > kSaveMaxBytes + kSaveHeadBytes || !saveWhole(bytes, size))
            return false;
        const std::uint32_t payload = static_cast<std::uint32_t>(size - kSaveHeadBytes);

        SaveData read;
        yas::mem_istream is(bytes + kSaveHeadBytes, payload);
        yas::binary_iarchive<yas::mem_istream, flags> ia(is);
        ia &read;

        // What the archive says is still only what a file says: the map's
        // name becomes a path, and the lists are walked.
        if (read.map.empty() || read.map.size() > 31 ||
            read.globalHash.size() != read.globalState.size() || read.globalHash.size() > 64 ||
            read.maps.size() > 256)
            return false;
        for (const char c : read.map)
        {
            const bool plain = (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_';
            if (!plain)
                return false;
        }
        if (heading != nullptr)
            readSaveHeading(bytes, size, *heading);
        out = std::move(read);
        return true;
    }
} // namespace hldata
