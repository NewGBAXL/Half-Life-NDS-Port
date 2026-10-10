#pragma once

// The DS BIOS's LZ77 ("LZ10"), written on the PC and undone on the console.
//
// The stream is exactly what the BIOS routine reads (and what Nitro Engine
// Advanced's archives hand it): a word holding 0x10 and the size the data
// comes back to, then groups of eight items under a flag byte, most
// significant bit first -- a clear bit a byte as it is, a set bit two bytes
// naming a run of 3 to 18 bytes that begins 1 to 4096 bytes back. The end is
// padded with zeros to a whole word.
//
// The console never runs the decoder below: the BIOS does that work. It is
// here so the converter can prove, of every stream it writes, that it comes
// back to what went in -- and, in the tests, that a damaged one is refused
// rather than run past the end of anything.

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace hlds
{
    /// The most the size field holds: 24 bits.
    inline constexpr std::size_t kLz77MaxBytes = 0xFFFFFF;

    /// Always succeeds for sizes up to kLz77MaxBytes (the result may be
    /// LARGER than the input: whether it is worth keeping is the caller's
    /// business). Empty for anything bigger.
    std::vector<std::uint8_t> lz77Compress(const std::uint8_t *data, std::size_t size);

    /// False, with why, for anything that is not a whole and well-formed
    /// stream: a run reaching before the start, data past its stated size,
    /// a stream that ends early.
    bool lz77Decompress(const std::uint8_t *stream, std::size_t size,
                        std::vector<std::uint8_t> &out, std::string &error);
} // namespace hlds
