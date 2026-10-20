#include "hlds/AviFile.hpp"

#include <algorithm>
#include <cstring>

namespace hlds
{
    namespace
    {
        std::uint32_t le32(const std::uint8_t *p)
        {
            return static_cast<std::uint32_t>(p[0]) | static_cast<std::uint32_t>(p[1]) << 8 |
                   static_cast<std::uint32_t>(p[2]) << 16 | static_cast<std::uint32_t>(p[3]) << 24;
        }
        std::uint16_t le16(const std::uint8_t *p) { return static_cast<std::uint16_t>(p[0] | p[1] << 8); }
        std::uint32_t be24(const std::uint8_t *p)
        {
            return static_cast<std::uint32_t>(p[0]) << 16 | static_cast<std::uint32_t>(p[1]) << 8 | p[2];
        }
        std::uint16_t be16(const std::uint8_t *p) { return static_cast<std::uint16_t>(p[0] << 8 | p[1]); }
        bool is(const std::uint8_t *p, const char *four) { return std::memcmp(p, four, 4) == 0; }

        std::uint8_t clamp8(int v) { return static_cast<std::uint8_t>(v < 0 ? 0 : (v > 255 ? 255 : v)); }

        // A codebook entry (four Y, U, V) as the colour of its pixel `n`.
        void put(RgbaImage &frame, int x, int y, const std::uint8_t *entry, int n)
        {
            if (x < 0 || y < 0 || x >= frame.width || y >= frame.height)
                return;
            const int luma = entry[n];
            const int u = static_cast<std::int8_t>(entry[4]), v = static_cast<std::int8_t>(entry[5]);
            std::uint8_t *p = frame.at(x, y);
            p[0] = clamp8(luma + v * 2);
            p[1] = clamp8(luma - u / 2 - v);
            p[2] = clamp8(luma + u * 2);
            p[3] = 255;
        }
    } // namespace

    // ------------------------------------------------------------- Cinepak

    bool decodeCinepak(const std::uint8_t *data, std::size_t size, int width, int height, CinepakState &state,
                       RgbaImage &frame, std::string &error)
    {
        if (frame.width != width || frame.height != height)
        {
            frame.width = width;
            frame.height = height;
            frame.rgba.assign(static_cast<std::size_t>(width) * height * 4, 0);
            for (std::size_t i = 3; i < frame.rgba.size(); i += 4)
                frame.rgba[i] = 255;
        }
        if (size < 10)
        {
            error = "a Cinepak picture too short for its header";
            return false;
        }
        const int flags = data[0];
        const int strips = be16(data + 8);
        if (static_cast<int>(state.strips.size()) < strips)
            state.strips.resize(static_cast<std::size_t>(strips));
        std::size_t at = 10;
        int top = 0;
        for (int s = 0; s < strips; ++s)
        {
            if (at + 12 > size)
            {
                error = "a Cinepak strip past the end of its picture";
                return false;
            }
            const std::uint8_t *head = data + at;
            const std::size_t stripSize = be24(head + 1);
            const int stripHeight = be16(head + 8);
            if (stripSize < 12 || at + stripSize > size)
            {
                error = "a Cinepak strip of a size that is not";
                return false;
            }
            CinepakState::Strip &strip = state.strips[static_cast<std::size_t>(s)];
            // (A strip with no codebooks of its own has the one above's.)
            if (s > 0 && !(flags & 0x01))
                strip = state.strips[static_cast<std::size_t>(s - 1)];
            const int bottom = top + stripHeight;
            std::size_t chunkAt = at + 12;
            const std::size_t end = at + stripSize;
            while (chunkAt + 4 <= end)
            {
                const std::uint8_t *chunk = data + chunkAt;
                const int id = chunk[0];
                const std::size_t chunkSize = be24(chunk + 1);
                if (chunkSize < 4 || chunkAt + chunkSize > end)
                    break;
                const std::uint8_t *p = chunk + 4;
                const std::uint8_t *pend = chunk + chunkSize;
                if ((id & 0xF0) == 0x20)
                {
                    // A codebook: V4 (0x20) or V1 (0x22); some of it (0x01:
                    // a flag a word for each 32), grey (0x04: no colour).
                    std::uint8_t(*book)[6] = (id & 0x02) ? strip.v1 : strip.v4;
                    const bool partial = (id & 0x01) != 0, grey = (id & 0x04) != 0;
                    std::uint32_t flag = 0, mask = 0;
                    for (int n = 0; n < 256 && p < pend; ++n)
                    {
                        if (partial)
                        {
                            if ((mask >>= 1) == 0)
                            {
                                if (p + 4 > pend)
                                    break;
                                flag = static_cast<std::uint32_t>(p[0]) << 24 | static_cast<std::uint32_t>(p[1]) << 16 |
                                       static_cast<std::uint32_t>(p[2]) << 8 | p[3];
                                p += 4;
                                mask = 0x80000000u;
                            }
                            if (!(flag & mask))
                                continue;
                        }
                        const int bytes = grey ? 4 : 6;
                        if (p + bytes > pend)
                            break;
                        for (int k = 0; k < 4; ++k)
                            book[n][k] = p[k];
                        book[n][4] = grey ? 0 : p[4];
                        book[n][5] = grey ? 0 : p[5];
                        p += bytes;
                    }
                }
                else if ((id & 0xF0) == 0x30)
                {
                    // The blocks of 4x4, left to right, top to bottom:
                    // each one entry of V1 doubled, or four of V4; and in an
                    // inter picture (0x01) those not sent stay as they were.
                    // (0x02: V1 only.)
                    std::uint32_t flag = 0, mask = 0;
                    bool stop = false;
                    for (int y = top; y < bottom && !stop; y += 4)
                    {
                        for (int x = 0; x < width && !stop; x += 4)
                        {
                            auto nextBit = [&]() -> bool
                            {
                                if ((mask >>= 1) == 0)
                                {
                                    if (p + 4 > pend)
                                    {
                                        stop = true;
                                        return false;
                                    }
                                    flag = static_cast<std::uint32_t>(p[0]) << 24 |
                                           static_cast<std::uint32_t>(p[1]) << 16 |
                                           static_cast<std::uint32_t>(p[2]) << 8 | p[3];
                                    p += 4;
                                    mask = 0x80000000u;
                                }
                                return (flag & mask) != 0;
                            };
                            if ((id & 0x01) && !nextBit())
                                continue;
                            if (stop)
                                break;
                            const bool v4 = !(id & 0x02) && nextBit();
                            if (stop)
                                break;
                            if (!v4)
                            {
                                if (p + 1 > pend)
                                {
                                    stop = true;
                                    break;
                                }
                                const std::uint8_t *e = strip.v1[*p++];
                                for (int dy = 0; dy < 4; ++dy)
                                    for (int dx = 0; dx < 4; ++dx)
                                        put(frame, x + dx, y + dy, e, (dy / 2) * 2 + dx / 2);
                            }
                            else
                            {
                                if (p + 4 > pend)
                                {
                                    stop = true;
                                    break;
                                }
                                for (int q = 0; q < 4; ++q)
                                {
                                    const std::uint8_t *e = strip.v4[*p++];
                                    const int qx = x + (q & 1) * 2, qy = y + (q >> 1) * 2;
                                    for (int k = 0; k < 4; ++k)
                                        put(frame, qx + (k & 1), qy + (k >> 1), e, k);
                                }
                            }
                        }
                    }
                }
                chunkAt += chunkSize;
            }
            top = bottom;
            at += stripSize;
        }
        return true;
    }

    // ------------------------------------------------------------- RLE8

    bool decodeRle8(const std::uint8_t *data, std::size_t size, int width, int height,
                    const std::uint8_t *palette, RgbaImage &frame, std::string &error)
    {
        if (frame.width != width || frame.height != height)
        {
            frame.width = width;
            frame.height = height;
            frame.rgba.assign(static_cast<std::size_t>(width) * height * 4, 0);
            for (std::size_t i = 3; i < frame.rgba.size(); i += 4)
                frame.rgba[i] = 255;
        }
        auto set = [&](int x, int row, int index)
        {
            const int y = height - 1 - row; // (the bottom row first)
            if (x < 0 || x >= width || y < 0 || y >= height)
                return;
            std::uint8_t *p = frame.at(x, y);
            p[0] = palette[index * 4 + 2];
            p[1] = palette[index * 4 + 1];
            p[2] = palette[index * 4 + 0];
            p[3] = 255;
        };
        std::size_t at = 0;
        int x = 0, row = 0;
        while (at + 2 <= size)
        {
            const int count = data[at], value = data[at + 1];
            at += 2;
            if (count > 0)
            {
                for (int n = 0; n < count; ++n)
                    set(x++, row, value);
                continue;
            }
            if (value == 0) // end of a row
            {
                x = 0;
                ++row;
            }
            else if (value == 1) // end of the picture
            {
                return true;
            }
            else if (value == 2) // a move on
            {
                if (at + 2 > size)
                    break;
                x += data[at];
                row += data[at + 1];
                at += 2;
            }
            else // so many as they are, to a word
            {
                if (at + static_cast<std::size_t>(value) > size)
                {
                    error = "a run of a picture past its end";
                    return false;
                }
                for (int n = 0; n < value; ++n)
                    set(x++, row, data[at + static_cast<std::size_t>(n)]);
                at += static_cast<std::size_t>(value + (value & 1));
            }
        }
        return true;
    }

    // ------------------------------------------------------------- the file

    bool AviFile::load(const std::vector<std::uint8_t> &bytes, std::string &error)
    {
        *this = AviFile();
        if (bytes.size() < 12 || !is(bytes.data(), "RIFF") || !is(bytes.data() + 8, "AVI "))
        {
            error = "not an AVI file";
            return false;
        }
        // The streams: which is the pictures and which the sound, and how.
        int videoStream = -1, audioStream = -1, stream = -1;
        std::uint32_t videoCodec = 0;
        int videoBits = 0, audioBits = 8;
        std::uint8_t palette[256 * 4] = {};
        CinepakState cinepak;
        RgbaImage picture;
        std::string why;
        bool ok = true;
        // Every chunk, going into the lists as they come.
        auto walk = [&](auto &&self, std::size_t from, std::size_t to) -> void
        {
            std::size_t at = from;
            while (ok && at + 8 <= to)
            {
                const std::uint8_t *head = bytes.data() + at;
                const std::uint32_t size = le32(head + 4);
                const std::size_t body = at + 8;
                if (body + size > bytes.size())
                    break;
                if (is(head, "LIST") || is(head, "RIFF"))
                {
                    self(self, body + 4, body + size);
                }
                else if (is(head, "avih") && size >= 4)
                {
                    frameMicros = le32(bytes.data() + body);
                }
                else if (is(head, "strh") && size >= 48)
                {
                    ++stream;
                    const std::uint8_t *h = bytes.data() + body;
                    if (is(h, "vids"))
                    {
                        videoStream = stream;
                        videoCodec = le32(h + 4);
                        const std::uint32_t scale = le32(h + 20), rate = le32(h + 24);
                        if (rate != 0)
                            frameMicros = static_cast<std::uint32_t>(1000000ull * scale / rate);
                    }
                    else if (is(h, "auds"))
                    {
                        audioStream = stream;
                    }
                }
                else if (is(head, "strf"))
                {
                    const std::uint8_t *f = bytes.data() + body;
                    if (stream == videoStream && size >= 40)
                    {
                        width = static_cast<int>(le32(f + 4));
                        height = static_cast<int>(le32(f + 8));
                        if (height < 0)
                            height = -height;
                        videoBits = le16(f + 14);
                        const std::uint32_t compression = le32(f + 16);
                        if (compression != 0 && compression != 1 && videoCodec == 0)
                            videoCodec = compression;
                        if (compression == 1)
                            videoCodec = 1; // (BI_RLE8)
                        const std::uint32_t headerSize = le32(f);
                        std::uint32_t colours = le32(f + 32);
                        if (colours == 0 && videoBits <= 8)
                            colours = 1u << videoBits;
                        for (std::uint32_t c = 0; c < colours && c < 256 && headerSize + c * 4 + 4 <= size; ++c)
                            std::memcpy(palette + c * 4, f + headerSize + c * 4, 4);
                    }
                    else if (stream == audioStream && size >= 16)
                    {
                        channels = le16(f + 2);
                        sampleRate = static_cast<int>(le32(f + 4));
                        audioBits = le16(f + 14);
                    }
                }
                else if (size > 0 && head[0] >= '0' && head[0] <= '9' && head[1] >= '0' && head[1] <= '9')
                {
                    // A stream's chunk: "00dc", "01wb".
                    const int which = (head[0] - '0') * 10 + (head[1] - '0');
                    const std::uint8_t *d = bytes.data() + body;
                    if (which == videoStream && (head[2] == 'd' || head[2] == 'D'))
                    {
                        const bool cvid = videoCodec == 0x64697663u || videoCodec == 0x44495643u; // "cvid", "CVID"
                        if (cvid)
                            ok = decodeCinepak(d, size, width, height, cinepak, picture, why);
                        else if (videoCodec == 1 && videoBits == 8)
                            ok = decodeRle8(d, size, width, height, palette, picture, why);
                        else
                        {
                            ok = false;
                            why = "pictures made in a way this does not read";
                        }
                        if (ok)
                            frames.push_back(picture);
                    }
                    else if (which == audioStream && head[2] == 'w')
                    {
                        if (audioBits == 8)
                            for (std::uint32_t i = 0; i < size; ++i)
                                samples.push_back(static_cast<std::int16_t>((d[i] - 128) << 8));
                        else
                            for (std::uint32_t i = 0; i + 1 < size; i += 2)
                                samples.push_back(static_cast<std::int16_t>(le16(d + i)));
                    }
                }
                at = body + size + (size & 1);
            }
        };
        walk(walk, 12, bytes.size());
        if (!ok)
        {
            error = why;
            return false;
        }
        if (videoStream < 0 || frames.empty())
        {
            error = "no pictures in it";
            return false;
        }
        return true;
    }
} // namespace hlds
