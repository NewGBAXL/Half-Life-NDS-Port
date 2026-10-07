#pragma once

// The player's settings: <card>/_nds/hl_nds/data/settings.dat
//
// Written by the game, never by the converter. A yas archive behind the same
// kind of fixed header as the manifest (HlManifest.hpp), for the same reason:
// yas reports a damaged archive by throwing, the ARM9 has no exceptions, so
// everything that can be checked is checked before yas runs. A file that does
// not check out is not an error -- it is a first boot, and the defaults apply.

#include <cstdint>
#include <cstring>
#include <vector>

#include <yas/serialize.hpp>
#include <yas/std_types.hpp>

#include "HlLevel.hpp"

namespace hldata
{
    /// How the world is drawn.
    ///
    /// The hardware draws 2048 polygons and 6144 vertices a frame. TWO-PASS
    /// draws the screen as two halves on alternate frames, each with that
    /// whole allowance: twice the capacity at 30 frames a second. SINGLE-PASS
    /// is 60 frames a second and drops the far end of the view where a map
    /// asks for more than one pass can hold; it is the default. AUTO picks
    /// per map from the worst case the converter measured.
    ///
    /// There are two ways of putting the two halves together. TWO-PASS shows
    /// one half live beside a capture of the other, straight from video
    /// memory. TWO-PASS (DMA) copies both halves into a picture in main
    /// memory and shows that, whole, when it is complete: a frame can never
    /// be half of one moment and half of another, and a third texture bank
    /// is free, for 190 KB of memory and a little more delay.
    enum class RenderMode : std::uint8_t
    {
        TwoPass,
        SinglePass,
        Auto,
        TwoPassDma, ///< after Auto: the numbers are in settings files
        Count
    };

    struct SettingsData
    {
        std::uint8_t renderMode = static_cast<std::uint8_t>(RenderMode::SinglePass);
        /// On a DSi: the DSP plays the music in stereo and adds the rooms'
        /// echo. Off unless asked for: a DSi then sounds as a DS does.
        std::uint8_t dspAudio = 0;

        template <class Ar>
        void serialize(Ar &ar)
        {
            ar &renderMode &dspAudio;
        }
    };

    inline constexpr char kSettingsMagic[4] = {'H', 'L', 'S', 'T'};
    inline constexpr std::uint16_t kSettingsVersion = 3;

    struct SettingsHeader
    {
        char magic[4];
        std::uint16_t version;
        std::uint16_t payloadBytes;
        std::uint32_t crc32;
    };
    static_assert(sizeof(SettingsHeader) == 12);

    inline std::vector<std::uint8_t> writeSettings(const SettingsData &data)
    {
        constexpr std::size_t flags = yas::mem | yas::binary | yas::no_header;
        yas::mem_ostream os;
        yas::binary_oarchive<yas::mem_ostream, flags> oa(os);
        oa &data;
        const auto buf = os.get_intrusive_buffer();

        SettingsHeader header{};
        std::memcpy(header.magic, kSettingsMagic, sizeof(header.magic));
        header.version = kSettingsVersion;
        header.payloadBytes = static_cast<std::uint16_t>(buf.size);
        header.crc32 = crc32(reinterpret_cast<const std::uint8_t *>(buf.data),
                             static_cast<std::uint32_t>(buf.size));
        std::vector<std::uint8_t> out(sizeof(header) + buf.size);
        std::memcpy(out.data(), &header, sizeof(header));
        std::memcpy(out.data() + sizeof(header), buf.data, buf.size);
        return out;
    }

    /// False -- leaving `out` at its defaults -- for anything that is not a
    /// settings file of this version, and for values out of range.
    inline bool readSettings(const std::uint8_t *bytes, std::size_t size, SettingsData &out)
    {
        constexpr std::size_t flags = yas::mem | yas::binary | yas::no_header;
        SettingsHeader header;
        if (size < sizeof(header))
            return false;
        std::memcpy(&header, bytes, sizeof(header));
        if (std::memcmp(header.magic, kSettingsMagic, sizeof(header.magic)) != 0 ||
            header.version != kSettingsVersion || header.payloadBytes != size - sizeof(header) ||
            crc32(bytes + sizeof(header), header.payloadBytes) != header.crc32)
            return false;

        SettingsData read;
        yas::mem_istream is(bytes + sizeof(header), header.payloadBytes);
        yas::binary_iarchive<yas::mem_istream, flags> ia(is);
        ia &read;
        if (read.renderMode >= static_cast<std::uint8_t>(RenderMode::Count) || read.dspAudio > 1)
            return false;
        out = read;
        return true;
    }
} // namespace hldata
