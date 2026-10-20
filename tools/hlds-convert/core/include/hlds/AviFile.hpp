#pragma once

// The game's own films (valve/media): the Valve intro and the animated
// HALF-LIFE of the menu. Read here without anything outside the converter:
// a RIFF/AVI file, its pictures in the two ways the game's are made --
// Cinepak (valve.avi) and 8-bit run-length (logo.avi) -- and its sound as it
// is, unsigned 8- or signed 16-bit samples.

#include <cstdint>
#include <string>
#include <vector>

#include "hlds/TextureImage.hpp"

namespace hlds
{
    struct AviFile
    {
        int width = 0;
        int height = 0;
        /// How long a picture is shown, in microseconds.
        std::uint32_t frameMicros = 0;
        /// Every picture, in order, as it is to be seen.
        std::vector<RgbaImage> frames;
        /// The sound, if there is any: mono or interleaved, as 16-bit
        /// samples whatever they were.
        std::vector<std::int16_t> samples;
        int sampleRate = 0;
        int channels = 0;

        /// False -- with `error` -- for a file that is not one, or whose
        /// pictures are made some other way.
        bool load(const std::vector<std::uint8_t> &bytes, std::string &error);
    };

    /// The decoders on their own (the tests feed them made-up data).
    /// A Cinepak picture onto `frame` (which keeps what an inter frame leaves
    /// alone, and the codebooks between pictures: `state`).
    struct CinepakState
    {
        struct Strip
        {
            std::uint8_t v1[256][6] = {};
            std::uint8_t v4[256][6] = {};
        };
        std::vector<Strip> strips;
    };
    bool decodeCinepak(const std::uint8_t *data, std::size_t size, int width, int height, CinepakState &state,
                       RgbaImage &frame, std::string &error);
    /// An 8-bit run-length picture (BI_RLE8) onto `frame`, through `palette`
    /// (BGRA, 256 entries): bottom row first, as a DIB is.
    bool decodeRle8(const std::uint8_t *data, std::size_t size, int width, int height,
                    const std::uint8_t *palette, RgbaImage &frame, std::string &error);
} // namespace hlds
