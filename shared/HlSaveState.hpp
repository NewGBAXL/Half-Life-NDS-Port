#pragma once

// The whole state of the map the player is in, for a save: every door,
// trigger and train as it is, every actor where it stands and what it is
// doing, what is in flight, the marks on the walls, the player themselves.
//
// It is laid over a map that has just been started afresh: the map's own
// data (what the level file holds) is never in a save, only what has
// changed since it began. So a save is of one cooked map exactly -- its
// checksum and its counts are in the state's heading, and laid over any
// other it is refused.
//
// Written with yas, a field at a time, and not as the structures lie in
// memory: a pointer is never written (a field is, only by its type), the
// file is the same from the PC's tests and from the console, and there is
// no padding in it. What that way of writing would otherwise risk -- a
// field added to the game and forgotten here -- cannot happen: the fields
// are not listed here at all, each structure is taken apart whole
// (HlFields.hpp), and one whose count of fields has changed does not build.
//
// yas says a bad archive by throwing, and the console is built without
// exceptions: so nothing is read that has not first been shown to be this
// build's own writing -- the heading's fingerprint of every structure's
// layout, the map's checksum and counts, and the exact length all that
// implies. (And the save it lies in is checksummed whole: HlSave.hpp.)
//
// std-only and inline: the host tests run it (test_savestate).

#include <cstdint>
#include <cstring>
#include <type_traits>
#include <vector>

#include <yas/serialize.hpp>

#include "HlFields.hpp"
#include "HlLamp.hpp"
#include "HlMonsters.hpp"
#include "HlTransition.hpp"
#include "HlView.hpp"

namespace hldata
{
    // How many fields each has. (Wrong, it does not build: the compiler
    // says how many there are.)
    HL_FIELDS(BrushState, 6);
    HL_FIELDS(LogicState, 10);
    HL_FIELDS(LogicEvent, 6);
    HL_FIELDS(LogicWorld, 61);
    HL_FIELDS(Hull, 4);
    HL_FIELDS(MoverHull, 10);
    HL_FIELDS(LadderVolume, 3);
    HL_FIELDS(ActorState, 68);
    HL_FIELDS(Projectile, 6);
    HL_FIELDS(Effect, 8);
    HL_FIELDS(Gib, 10);
    HL_FIELDS(Decal, 9);
    HL_FIELDS(Piece, 14);
    HL_FIELDS(ActorWorld, 42);
    HL_FIELDS(Inventory, 12);
    HL_FIELDS(PlayerState, 32);
    HL_FIELDS(ViewState, 8);
    HL_FIELDS(WeaponState, 25);
    HL_FIELDS(LampState, 2);
    HL_FIELDS(Arrival::Visitor, 8);
    HL_FIELDS(Arrival, 17);

    namespace detail
    {
        inline constexpr std::size_t kStateFlags = yas::mem | yas::binary | yas::no_header;

        /// Fields left out of what is written or read, by where they lie
        /// and how big they are: how a state written before some fields
        /// were added is read (see skipNewer).
        struct Skips
        {
            const void *at[8] = {};
            std::size_t size[8] = {};
            int count = 0;
            void add(const void *where, std::size_t bytes)
            {
                at[count] = where;
                size[count++] = bytes;
            }
            bool has(const void *where, std::size_t bytes) const
            {
                for (int i = 0; i < count; ++i)
                {
                    if (at[i] == where && size[i] == bytes)
                        return true;
                }
                return false;
            }
        };

        /// Writes each value.
        struct StateOut
        {
            yas::mem_ostream os;
            yas::binary_oarchive<yas::mem_ostream, kStateFlags> ar{os};
            Skips skips;
            template <class T>
            void value(T &v)
            {
                ar &v;
            }
        };
        /// Reads each value.
        struct StateIn
        {
            yas::mem_istream is;
            yas::binary_iarchive<yas::mem_istream, kStateFlags> ar{is};
            Skips skips;
            StateIn(const std::uint8_t *bytes, std::size_t size) : is(bytes, size) {}
            template <class T>
            void value(T &v)
            {
                ar &v;
            }
        };
        /// Reads and writes nothing: adds up how long it would be, and
        /// makes a fingerprint of the order and the sizes of it all.
        struct StateMeasure
        {
            std::uint32_t bytes = 0;
            std::uint32_t print = 2166136261u;
            Skips skips;
            template <class T>
            void value(T &)
            {
                bytes += sizeof(T);
                print = (print ^ static_cast<std::uint32_t>(sizeof(T) + (std::is_signed_v<T> ? 16 : 0))) * 16777619u;
            }
        };

        /// One thing, by what it is: a number or a switch is a value; an
        /// array is each of its elements; a structure is each of its
        /// fields; and a pointer is not state at all -- it is to the map's
        /// own data, or to an array that is dealt with by whoever owns it.
        template <class A, class T>
        void stateField(A &a, T &v)
        {
            if (a.skips.has(&v, sizeof(T)))
                return;
            if constexpr (std::is_pointer_v<T> || std::is_member_pointer_v<T> || std::is_function_v<T>)
            {
            }
            else if constexpr (std::is_array_v<T>)
            {
                for (auto &element : v)
                    stateField(a, element);
            }
            else if constexpr (std::is_enum_v<T>)
            {
                auto under = static_cast<std::underlying_type_t<T>>(v);
                a.value(under);
                v = static_cast<T>(under);
            }
            else if constexpr (std::is_arithmetic_v<T>)
            {
                a.value(v);
            }
            else
            {
                eachField(v, [&](auto &field) { stateField(a, field); });
            }
        }

        template <class A, class T>
        void stateArray(A &a, T *first, int count)
        {
            for (int i = 0; first != nullptr && i < count; ++i)
                stateField(a, first[i]);
        }

        /// What was added to a map's state for games between consoles (save
        /// version 7): a state written before it, and so without these, is
        /// read as if they were not there, and they keep what they began
        /// with.
        inline void skipNewer(Skips &skips, LogicWorld &w)
        {
            skips.add(&w.sharing, sizeof(w.sharing));
            skips.add(&w.struck, sizeof(w.struck));
            skips.add(&w.struckCount, sizeof(w.struckCount));
        }
        inline void skipNewer(Skips &skips, ActorWorld &actors)
        {
            skips.add(&actors.shared, sizeof(actors.shared));
            skips.add(&actors.wanted, sizeof(actors.wanted));
        }

        /// All of a map's state, through `a`. `older`: as it was written
        /// before the fields skipNewer leaves out.
        template <class A>
        void mapState(A &a, LogicWorld &w, PlayerState &player, bool older = false)
        {
            if (older)
                skipNewer(a.skips, w);
            stateField(a, w);
            stateArray(a, w.states, w.recordCount);
            stateArray(a, w.brushStates, w.brushCount);
            stateArray(a, w.movers, w.brushCount);
            stateArray(a, w.cornerState, w.cornerCount);
            if (w.world != nullptr)
                stateArray(a, w.ladders, w.world->ladderCount);
            if (w.actors != nullptr)
            {
                ActorWorld &actors = *w.actors;
                if (older)
                    skipNewer(a.skips, actors);
                stateField(a, actors);
                stateArray(a, actors.states, actors.count);
                stateArray(a, actors.linkShut, actors.linkCount);
            }
            stateField(a, player);
        }

        inline constexpr char kStateMagic[4] = {'H', 'L', 'M', 'S'};
        struct StateHeading
        {
            char magic[4];
            std::uint32_t print;    ///< of the layout of all that is written
            std::uint32_t level;    ///< the cooked map's checksum
            std::uint32_t bytes;    ///< of what follows
            std::int32_t records, brushes, actors, links, corners, ladders;
        };
        static_assert(sizeof(StateHeading) == 40);

        inline StateHeading stateHeading(LogicWorld &w, PlayerState &player, std::uint32_t level,
                                         bool older = false)
        {
            StateMeasure measure;
            mapState(measure, w, player, older);
            StateHeading h{};
            std::memcpy(h.magic, kStateMagic, sizeof(h.magic));
            h.print = measure.print;
            h.level = level;
            h.bytes = measure.bytes;
            h.records = w.recordCount;
            h.brushes = w.brushCount;
            h.actors = w.actors != nullptr ? w.actors->count : 0;
            h.links = w.actors != nullptr ? w.actors->linkCount : 0;
            h.corners = w.cornerCount;
            h.ladders = w.world != nullptr ? w.world->ladderCount : 0;
            return h;
        }
    } // namespace detail

    /// The state of the map `w` is of, as it is now. `level`: the cooked
    /// map's own checksum (LevelHeader::crc32).
    /// `older`: written as save version 6 had it, without the fields added
    /// since (for the tests of reading one).
    inline std::vector<std::uint8_t> takeMapState(LogicWorld &w, PlayerState &player, std::uint32_t level,
                                                  bool older = false)
    {
        const detail::StateHeading heading = detail::stateHeading(w, player, level, older);
        detail::StateOut out;
        detail::mapState(out, w, player, older);
        const auto buf = out.os.get_intrusive_buffer();
        std::vector<std::uint8_t> bytes(sizeof(heading) + buf.size);
        std::memcpy(bytes.data(), &heading, sizeof(heading));
        std::memcpy(bytes.data() + sizeof(heading), buf.data, buf.size);
        return bytes;
    }

    /// Which way `bytes` is a state that can be laid over the map `w` is
    /// of, of this cooked map and whole: 1 as this build writes it, 2 as
    /// save version 6 did (without the fields added since), 0 not at all.
    inline int mapStateFormat(const std::uint8_t *bytes, std::size_t size, LogicWorld &w, PlayerState &player,
                              std::uint32_t level)
    {
        detail::StateHeading have;
        if (size < sizeof(have))
            return 0;
        std::memcpy(&have, bytes, sizeof(have));
        if (size != sizeof(have) + have.bytes)
            return 0;
        for (int older = 0; older < 2; ++older)
        {
            const detail::StateHeading want = detail::stateHeading(w, player, level, older != 0);
            if (std::memcmp(&have, &want, sizeof(have)) == 0)
                return 1 + older;
        }
        return 0;
    }

    /// Whether `bytes` is a state that can be laid over the map `w` is of.
    inline bool mapStateFits(const std::uint8_t *bytes, std::size_t size, LogicWorld &w, PlayerState &player,
                             std::uint32_t level)
    {
        return mapStateFormat(bytes, size, w, player, level) != 0;
    }

    /// How long the map state at the head of `bytes` says it is, heading
    /// and all; 0 if that is not one, or is not all there.
    inline std::size_t mapStateLength(const std::uint8_t *bytes, std::size_t size)
    {
        detail::StateHeading have;
        if (size < sizeof(have))
            return 0;
        std::memcpy(&have, bytes, sizeof(have));
        if (std::memcmp(have.magic, detail::kStateMagic, sizeof(have.magic)) != 0 || have.bytes > size - sizeof(have))
            return 0;
        return sizeof(have) + have.bytes;
    }

    /// The state laid over the map, which has just been begun afresh. False
    /// (and nothing touched) if it does not fit. Afterwards every actor's
    /// box is where the actor is.
    inline bool layMapState(const std::uint8_t *bytes, std::size_t size, LogicWorld &w, PlayerState &player,
                            std::uint32_t level)
    {
        const int format = mapStateFormat(bytes, size, w, player, level);
        if (format == 0)
            return false;
        detail::StateIn in(bytes + sizeof(detail::StateHeading), size - sizeof(detail::StateHeading));
        detail::mapState(in, w, player, format == 2);
        // (What the monsters thought is not kept: they think afresh.)
        if (w.actors != nullptr)
            aiRestart(*w.actors);
        if (w.actors != nullptr && w.actors->boxes != nullptr)
        {
            for (int i = 0; i < w.actors->count; ++i)
                actorPlaceBox(*w.actors, i);
        }
        // (The brushes are where the save has them: so is the table of
        // where they are.)
        if (w.world != nullptr)
            moverReachAll(*w.world);
        return true;
    }

    /// Anything else that is all fields (what the console keeps beside the
    /// map: the weapon in hand, the lamp, who came along), the same way.
    template <class T>
    void appendState(std::vector<std::uint8_t> &bytes, T &thing)
    {
        detail::StateOut out;
        detail::stateField(out, thing);
        const auto buf = out.os.get_intrusive_buffer();
        bytes.insert(bytes.end(), reinterpret_cast<const std::uint8_t *>(buf.data),
                     reinterpret_cast<const std::uint8_t *>(buf.data) + buf.size);
    }
    /// How long `thing` is when written: for reading it back from where it
    /// was put.
    template <class T>
    std::size_t stateBytes(T &thing)
    {
        detail::StateMeasure measure;
        detail::stateField(measure, thing);
        return measure.bytes;
    }
    template <class T>
    bool readState(const std::uint8_t *bytes, std::size_t size, std::size_t &at, T &thing)
    {
        const std::size_t length = stateBytes(thing);
        if (at + length > size)
            return false;
        detail::StateIn in(bytes + at, length);
        detail::stateField(in, thing);
        at += length;
        return true;
    }
} // namespace hldata

#include "HlAiSave.hpp"
