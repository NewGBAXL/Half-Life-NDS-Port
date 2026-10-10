#include "hlds/Lz77.hpp"

#include <algorithm>
#include <cstring>

namespace hlds
{
    namespace
    {
        constexpr int kWindow = 4096;
        constexpr int kMinRun = 3;
        constexpr int kMaxRun = 18;
        constexpr std::uint32_t kNone = 0xFFFFFFFFu;

        struct Match
        {
            std::uint16_t length = 0; ///< 0: none of three bytes or more
            std::uint16_t back = 0;   ///< 1..4096
        };

        /// The longest run each position can borrow from the 4096 bytes
        /// before it, found through chains of positions that begin with the
        /// same three bytes.
        std::vector<Match> longestMatches(const std::uint8_t *data, std::size_t size)
        {
            std::vector<Match> matches(size);
            if (size < kMinRun)
                return matches;
            constexpr std::size_t kBuckets = 1u << 15;
            std::vector<std::uint32_t> head(kBuckets, kNone);
            std::vector<std::uint32_t> previous(size, kNone);
            auto bucket = [&](std::size_t i)
            {
                const std::uint32_t v = data[i] | (data[i + 1] << 8) | (data[i + 2] << 16);
                return (v * 2654435761u) >> 17;
            };
            for (std::size_t i = 0; i + kMinRun <= size; ++i)
            {
                const std::uint32_t b = bucket(i);
                const int most = static_cast<int>(std::min<std::size_t>(kMaxRun, size - i));
                Match best;
                for (std::uint32_t at = head[b]; at != kNone && i - at <= kWindow; at = previous[at])
                {
                    int length = 0;
                    while (length < most && data[at + length] == data[i + length])
                        ++length;
                    if (length > best.length)
                    {
                        best.length = static_cast<std::uint16_t>(length);
                        best.back = static_cast<std::uint16_t>(i - at);
                        if (length == most)
                            break;
                    }
                }
                if (best.length >= kMinRun)
                    matches[i] = best;
                previous[i] = head[b];
                head[b] = static_cast<std::uint32_t>(i);
            }
            return matches;
        }
    } // namespace

    std::vector<std::uint8_t> lz77Compress(const std::uint8_t *data, std::size_t size)
    {
        if (size > kLz77MaxBytes)
            return {};
        const std::vector<Match> matches = longestMatches(data, size);

        // The cheapest way to the end from every position, from the end
        // backwards: a byte costs nine bits, a run seventeen, and a position
        // that can borrow eighteen bytes can as well borrow any fewer.
        std::vector<std::uint32_t> cost(size + 1, 0);
        std::vector<std::uint8_t> take(size, 1);
        for (std::size_t i = size; i-- > 0;)
        {
            cost[i] = cost[i + 1] + 9;
            for (int length = kMinRun; length <= matches[i].length; ++length)
            {
                if (cost[i + length] + 17 < cost[i])
                {
                    cost[i] = cost[i + length] + 17;
                    take[i] = static_cast<std::uint8_t>(length);
                }
            }
        }

        std::vector<std::uint8_t> out;
        out.reserve(size / 2 + 16);
        out.push_back(0x10);
        out.push_back(static_cast<std::uint8_t>(size));
        out.push_back(static_cast<std::uint8_t>(size >> 8));
        out.push_back(static_cast<std::uint8_t>(size >> 16));
        std::size_t flagAt = 0;
        int items = 8;
        for (std::size_t i = 0; i < size;)
        {
            if (items == 8)
            {
                flagAt = out.size();
                out.push_back(0);
                items = 0;
            }
            if (take[i] >= kMinRun)
            {
                const int length = take[i];
                const int back = matches[i].back - 1;
                out[flagAt] |= static_cast<std::uint8_t>(0x80 >> items);
                out.push_back(static_cast<std::uint8_t>(((length - kMinRun) << 4) | (back >> 8)));
                out.push_back(static_cast<std::uint8_t>(back));
                i += length;
            }
            else
            {
                out.push_back(data[i]);
                ++i;
            }
            ++items;
        }
        while (out.size() % 4 != 0)
            out.push_back(0);
        return out;
    }

    bool lz77Decompress(const std::uint8_t *stream, std::size_t size,
                        std::vector<std::uint8_t> &out, std::string &error)
    {
        out.clear();
        if (size < 4 || stream[0] != 0x10)
        {
            error = "not an LZ77 stream";
            return false;
        }
        const std::size_t want = stream[1] | (stream[2] << 8) | (std::size_t(stream[3]) << 16);
        out.reserve(want);
        std::size_t at = 4;
        while (out.size() < want)
        {
            if (at >= size)
            {
                error = "LZ77 stream ends early";
                return false;
            }
            const std::uint8_t flags = stream[at++];
            for (int item = 0; item < 8 && out.size() < want; ++item)
            {
                if ((flags & (0x80 >> item)) == 0)
                {
                    if (at >= size)
                    {
                        error = "LZ77 stream ends early";
                        return false;
                    }
                    out.push_back(stream[at++]);
                    continue;
                }
                if (at + 2 > size)
                {
                    error = "LZ77 stream ends early";
                    return false;
                }
                const std::size_t length = (stream[at] >> 4) + kMinRun;
                const std::size_t back = (((stream[at] & 0x0F) << 8) | stream[at + 1]) + 1;
                at += 2;
                if (back > out.size())
                {
                    error = "LZ77 run reaches before the start";
                    return false;
                }
                if (out.size() + length > want)
                {
                    error = "LZ77 run reaches past the end";
                    return false;
                }
                for (std::size_t k = 0; k < length; ++k)
                    out.push_back(out[out.size() - back]);
            }
        }
        // Nothing but padding may follow.
        if (size - at > 3)
        {
            error = "LZ77 stream is longer than its data";
            return false;
        }
        return true;
    }
} // namespace hlds
