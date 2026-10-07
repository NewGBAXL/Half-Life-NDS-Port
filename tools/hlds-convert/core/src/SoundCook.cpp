#include "hlds/SoundCook.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>
#include <fstream>

#define MINIMP3_IMPLEMENTATION
#define MINIMP3_NO_SIMD
#include "minimp3/minimp3.h"

// The DSP program's own arithmetic (it is plain C, and keeps its state in
// statics: this file's copy is this file's).
extern "C"
{
#include "dsp/hl_dsp.h"
}

namespace hlds
{
    using namespace hldata;

    namespace
    {
        std::uint32_t le32(const std::uint8_t *p)
        {
            return p[0] | (p[1] << 8) | (p[2] << 16) | (static_cast<std::uint32_t>(p[3]) << 24);
        }
        std::uint16_t le16(const std::uint8_t *p) { return static_cast<std::uint16_t>(p[0] | (p[1] << 8)); }

        // The IMA tables, as the DS's sound chip has them.
        const int kIndexStep[8] = {-1, -1, -1, -1, 2, 4, 6, 8};
        const int kStep[89] = {
            7,     8,     9,     10,    11,    12,    13,    14,    16,    17,    19,   21,   23,
            25,    28,    31,    34,    37,    41,    45,    50,    55,    60,    66,   73,   80,
            88,    97,    107,   118,   130,   143,   157,   173,   190,   209,   230,  253,  279,
            307,   337,   371,   408,   449,   494,   544,   598,   658,   724,   796,  876,  963,
            1060,  1166,  1282,  1411,  1552,  1707,  1878,  2066,  2272,  2499,  2749, 3024, 3327,
            3660,  4026,  4428,  4871,  5358,  5894,  6484,  7132,  7845,  8630,  9493, 10442, 11487,
            12635, 13899, 15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794, 32767};

        /// One nibble's worth, both ways: what the chip adds for `code` at
        /// `index`, applied to `value`.
        void stepState(int code, int &value, int &index)
        {
            const int step = kStep[index];
            int diff = step >> 3;
            if (code & 1)
                diff += step >> 2;
            if (code & 2)
                diff += step >> 1;
            if (code & 4)
                diff += step;
            if (code & 8)
                value = std::max(value - diff, -0x7FFF);
            else
                value = std::min(value + diff, 0x7FFF);
            index = std::clamp(index + kIndexStep[code & 7], 0, 88);
        }
    } // namespace

    bool parseWav(const std::vector<std::uint8_t> &bytes, WavData &out, std::string &error)
    {
        out = WavData();
        if (bytes.size() < 12 || std::memcmp(bytes.data(), "RIFF", 4) != 0 ||
            std::memcmp(bytes.data() + 8, "WAVE", 4) != 0)
        {
            error = "not a WAV file";
            return false;
        }
        int channels = 0, bits = 0, format = 0;
        const std::uint8_t *data = nullptr;
        std::size_t dataBytes = 0;
        for (std::size_t at = 12; at + 8 <= bytes.size();)
        {
            const std::uint8_t *chunk = bytes.data() + at;
            const std::size_t size = std::min<std::size_t>(le32(chunk + 4), bytes.size() - at - 8);
            if (std::memcmp(chunk, "fmt ", 4) == 0 && size >= 16)
            {
                format = le16(chunk + 8);
                channels = le16(chunk + 10);
                out.rate = static_cast<int>(le32(chunk + 12));
                bits = le16(chunk + 22);
            }
            else if (std::memcmp(chunk, "data", 4) == 0)
            {
                data = chunk + 8;
                dataBytes = size;
            }
            else if (std::memcmp(chunk, "cue ", 4) == 0 && size >= 28)
            {
                // The game loops a sound from its first cue point.
                out.loopStart = static_cast<int>(le32(chunk + 8 + 4 + 20));
            }
            at += 8 + size + (size & 1);
        }
        if (format != 1 || (bits != 8 && bits != 16) || channels < 1 || channels > 2 ||
            out.rate < 4000 || out.rate > 48000 || data == nullptr)
        {
            error = "a WAV of a kind the game does not use";
            return false;
        }
        const std::size_t frame = static_cast<std::size_t>(channels) * (bits / 8);
        const std::size_t count = dataBytes / frame;
        out.samples.resize(count);
        for (std::size_t i = 0; i < count; ++i)
        {
            int sum = 0;
            for (int c = 0; c < channels; ++c)
            {
                const std::uint8_t *p = data + i * frame + static_cast<std::size_t>(c) * (bits / 8);
                sum += bits == 8 ? (static_cast<int>(p[0]) - 128) * 256
                                 : static_cast<std::int16_t>(le16(p));
            }
            out.samples[i] = static_cast<std::int16_t>(sum / channels);
        }
        if (out.loopStart >= static_cast<int>(count))
            out.loopStart = -1;
        return true;
    }

    bool readWav(const std::filesystem::path &path, WavData &out, std::string &error)
    {
        std::ifstream file(path, std::ios::binary);
        if (!file)
        {
            error = "cannot be opened";
            return false;
        }
        const std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(file)),
                                              std::istreambuf_iterator<char>());
        return parseWav(bytes, out, error);
    }

    std::vector<std::int16_t> resample(const std::vector<std::int16_t> &samples, int from, int to)
    {
        if (from == to || samples.empty() || from <= 0 || to <= 0)
            return samples;
        const std::size_t count =
            static_cast<std::size_t>((static_cast<std::uint64_t>(samples.size()) * to) / from);
        std::vector<std::int16_t> out(count);
        // Going down, each new sample is the mean of the stretch of old ones
        // it stands for (which keeps what is above the new rate's reach from
        // folding back as noise); going up, a straight line between two.
        const double ratio = static_cast<double>(from) / to;
        for (std::size_t i = 0; i < count; ++i)
        {
            const double start = i * ratio;
            if (ratio > 1.0)
            {
                const std::size_t a = static_cast<std::size_t>(start);
                const std::size_t b = std::min(samples.size(), static_cast<std::size_t>(start + ratio + 0.5));
                long sum = 0;
                for (std::size_t n = a; n < std::max(b, a + 1) && n < samples.size(); ++n)
                    sum += samples[n];
                const long span = static_cast<long>(std::max(b, a + 1) - a);
                out[i] = static_cast<std::int16_t>(sum / span);
            }
            else
            {
                const std::size_t a = std::min(samples.size() - 1, static_cast<std::size_t>(start));
                const std::size_t b = std::min(samples.size() - 1, a + 1);
                const double part = start - static_cast<double>(a);
                out[i] = static_cast<std::int16_t>(std::lround(samples[a] * (1.0 - part) + samples[b] * part));
            }
        }
        return out;
    }

    CookedSound encodeSound(const WavData &wav, int rate, bool loops)
    {
        CookedSound out;
        out.rate = rate;
        std::vector<std::int16_t> samples = resample(wav.samples, wav.rate, rate);
        if (samples.empty())
            samples.assign(8, 0);
        // The data is whole words: four bytes of header, then eight samples
        // a word. Something that loops loses its odd samples (padding would
        // be a click every time round); something that does not is padded
        // with its last.
        if (loops && samples.size() >= 16)
            samples.resize(samples.size() & ~std::size_t(7));
        else
            samples.resize((samples.size() + 7) & ~std::size_t(7), samples.back());
        out.samples = static_cast<std::uint32_t>(samples.size());

        int value = samples[0];
        int index = 0;
        // Start at the step nearest the first difference, so that the first
        // few samples are not spent catching up.
        if (samples.size() > 1)
        {
            const int first = std::abs(samples[1] - samples[0]);
            while (index < 88 && kStep[index] < first)
                ++index;
        }
        out.data.assign(4 + samples.size() / 2, 0);
        out.data[0] = static_cast<std::uint8_t>(value & 0xFF);
        out.data[1] = static_cast<std::uint8_t>((value >> 8) & 0xFF);
        out.data[2] = static_cast<std::uint8_t>(index);
        value = std::clamp(value, -0x7FFF, 0x7FFF);
        for (std::size_t i = 0; i < samples.size(); ++i)
        {
            // The code whose step lands nearest the sample.
            const int step = kStep[index];
            int delta = samples[i] - value;
            int code = 0;
            if (delta < 0)
            {
                code = 8;
                delta = -delta;
            }
            int diff = step >> 3;
            if (delta >= step)
            {
                code |= 4;
                delta -= step;
            }
            if (delta >= step >> 1)
            {
                code |= 2;
                delta -= step >> 1;
            }
            if (delta >= step >> 2)
                code |= 1;
            (void)diff;
            stepState(code, value, index);
            out.data[4 + i / 2] |= static_cast<std::uint8_t>(code << ((i & 1) * 4));
        }
        if (loops)
        {
            int from = wav.loopStart >= 0
                           ? static_cast<int>((static_cast<std::int64_t>(wav.loopStart) * rate) / wav.rate)
                           : 0;
            // The word the sample is in; never the header's (the chip takes
            // its state for the loop as it passes the point, and has none
            // before the first sample).
            std::uint32_t word = static_cast<std::uint32_t>((4 + from / 2) / 4);
            const std::uint32_t words = static_cast<std::uint32_t>(out.data.size() / 4);
            out.loopWord = std::clamp<std::uint32_t>(word, 1, words > 1 ? words - 1 : 1);
        }
        return out;
    }

    std::vector<std::int16_t> decodeSound(const CookedSound &sound)
    {
        std::vector<std::int16_t> out;
        if (sound.data.size() < 4)
            return out;
        int value = static_cast<std::int16_t>(sound.data[0] | (sound.data[1] << 8));
        int index = std::min<int>(sound.data[2] & 0x7F, 88);
        const std::size_t count = std::min<std::size_t>(sound.samples, (sound.data.size() - 4) * 2);
        out.reserve(count);
        for (std::size_t i = 0; i < count; ++i)
        {
            const int code = (sound.data[4 + i / 2] >> ((i & 1) * 4)) & 0xF;
            stepState(code, value, index);
            out.push_back(static_cast<std::int16_t>(value));
        }
        return out;
    }

    std::vector<std::uint8_t> packSound(const CookedSound &sound, std::uint32_t sourceHash)
    {
        SoundHeader header{};
        std::memcpy(header.magic, kSoundMagic, 4);
        header.version = kSoundVersion;
        header.rate = static_cast<std::uint16_t>(sound.rate);
        header.dataBytes = static_cast<std::uint32_t>(sound.data.size());
        header.loopWord = sound.loopWord;
        header.samples = sound.samples;
        header.sourceHash = sourceHash;
        std::vector<std::uint8_t> out(sizeof(header) + sound.data.size());
        std::memcpy(out.data(), &header, sizeof(header));
        std::memcpy(out.data() + sizeof(header), sound.data.data(), sound.data.size());
        return out;
    }

    bool unpackSound(const std::vector<std::uint8_t> &bytes, CookedSound &out, SoundHeader &header)
    {
        if (bytes.size() < sizeof(header))
            return false;
        std::memcpy(&header, bytes.data(), sizeof(header));
        if (!soundHeaderValid(header, static_cast<std::uint32_t>(bytes.size())))
            return false;
        out.rate = header.rate;
        out.loopWord = header.loopWord;
        out.samples = header.samples;
        out.data.assign(bytes.begin() + sizeof(header), bytes.end());
        return true;
    }

    std::string soundCardName(const std::string &gamePath, int rate, bool loops)
    {
        std::string name = gamePath;
        if (name.size() > 4 && name.substr(name.size() - 4) == ".wav")
            name.resize(name.size() - 4);
        for (char &c : name)
        {
            const bool plain = (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '-';
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            if (!plain && !(c >= 'a' && c <= 'z'))
                c = '_';
        }
        // A name the level's record cannot hold keeps its end, which is the
        // part that tells sounds apart.
        const std::string tail = "." + std::to_string(rate / 1000) + (loops ? "l" : "k");
        const std::size_t most = sizeof(SoundRef::name) - 1 - tail.size();
        if (name.size() > most)
            name = name.substr(name.size() - most);
        return name + tail;
    }

    const Sentence *SentenceBook::find(const std::string &name) const
    {
        std::string key;
        for (char c : name)
        {
            if (c != '!')
                key.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(c))));
        }
        auto it = lines.find(key);
        // A group's name stands for its lines, NAME0 on: the first will do.
        if (it == lines.end())
            it = lines.find(key + "0");
        return it == lines.end() ? nullptr : &it->second;
    }

    SentenceBook parseSentences(const std::string &text)
    {
        SentenceBook book;
        std::size_t at = 0;
        while (at < text.size())
        {
            std::size_t end = text.find('\n', at);
            if (end == std::string::npos)
                end = text.size();
            std::string line = text.substr(at, end - at);
            at = end + 1;
            const std::size_t comment = line.find("//");
            if (comment != std::string::npos)
                line.resize(comment);
            // What is in brackets says how a word is spoken (pitch, speed,
            // where it is cut): none of it is kept.
            std::string plain;
            int depth = 0;
            for (char c : line)
            {
                if (c == '(')
                    ++depth;
                else if (c == ')')
                    depth = std::max(0, depth - 1);
                else if (depth == 0)
                    plain.push_back(c == '\t' || c == '\r' ? ' ' : c);
            }
            std::vector<std::string> tokens;
            std::string token;
            for (char c : plain + " ")
            {
                if (c == ' ')
                {
                    if (!token.empty())
                        tokens.push_back(token);
                    token.clear();
                }
                else
                {
                    token.push_back(c);
                }
            }
            if (tokens.size() < 2)
                continue;
            std::string name;
            for (char c : tokens[0])
                name.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(c))));
            Sentence sentence;
            // The folder is said once, with the first word, and holds for
            // the line; with none, the words are the announcer's.
            std::string folder = "vox/";
            for (std::size_t n = 1; n < tokens.size(); ++n)
            {
                std::string word;
                for (char c : tokens[n])
                    word.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
                const std::size_t slash = word.rfind('/');
                if (slash != std::string::npos)
                {
                    folder = word.substr(0, slash + 1);
                    word = word.substr(slash + 1);
                }
                // Commas and full stops are pauses, which are sounds too
                // ("_comma", "_period") -- left out: a line is its words.
                while (!word.empty() && (word.back() == ',' || word.back() == '.'))
                    word.pop_back();
                if (word.empty())
                    continue;
                sentence.words.push_back(folder + word + ".wav");
            }
            if (!sentence.words.empty())
                book.lines[name] = std::move(sentence);
        }
        return book;
    }

    std::vector<std::uint8_t> encodeMusicStereo(const std::vector<std::int16_t> &left,
                                                const std::vector<std::int16_t> &right, int rate)
    {
        const std::vector<std::int16_t> channel[2] = {resample(left, rate, kStereoRate),
                                                      resample(right, rate, kStereoRate)};
        const std::size_t pairs = std::min(channel[0].size(), channel[1].size());
        const std::size_t perBlock = static_cast<std::size_t>(kMusicBlockWords) * 2;
        const std::size_t blocks = (pairs + perBlock - 1) / perBlock;

        StereoHeader header{};
        std::memcpy(header.magic, kStereoMagic, 4);
        header.version = kSoundVersion;
        header.rate = static_cast<std::uint16_t>(kStereoRate);
        header.blocks = static_cast<std::uint32_t>(blocks);
        header.pairs = static_cast<std::uint32_t>(pairs);
        std::vector<std::uint16_t> words;
        words.reserve(blocks * (kMusicBlockWords + 4));

        // The decoders' states are the DSP's own variables: each sample is
        // coded, then the DSP's decoder is run on the code, so that what is
        // coded next is coded against exactly what the DSP will have.
        hlDspInit();
        for (std::size_t block = 0; block < blocks; ++block)
        {
            words.push_back(static_cast<std::uint16_t>(hlValue[0]));
            words.push_back(static_cast<std::uint16_t>(hlIndex[0]));
            words.push_back(static_cast<std::uint16_t>(hlValue[1]));
            words.push_back(static_cast<std::uint16_t>(hlIndex[1]));
            for (int w = 0; w < kMusicBlockWords; ++w)
            {
                std::uint16_t word = 0;
                for (int half = 0; half < 2; ++half)
                {
                    const std::size_t at = block * perBlock + static_cast<std::size_t>(w) * 2 + half;
                    for (std::uint16_t c = 0; c < 2; ++c)
                    {
                        // (Past the end of the track: silence.)
                        const int want = at < pairs ? channel[c][at] : 0;
                        const int step = hlStepTable[hlIndex[c]];
                        // The code whose step, as the decoder takes it
                        // ((2c + 1) / 8 of the step), lands nearest.
                        int delta = want - hlValue[c];
                        std::uint16_t code = 0;
                        if (delta < 0)
                        {
                            code = 8;
                            delta = -delta;
                        }
                        const int size = std::clamp((delta * 4) / step, 0, 7);
                        code |= static_cast<std::uint16_t>(size);
                        hlDecode(c, code);
                        word |= static_cast<std::uint16_t>(code << (half * 8 + c * 4));
                    }
                }
                words.push_back(word);
            }
        }
        std::vector<std::uint8_t> out(sizeof(header) + words.size() * 2);
        std::memcpy(out.data(), &header, sizeof(header));
        std::memcpy(out.data() + sizeof(header), words.data(), words.size() * 2);
        return out;
    }

    bool decodeMusicStereo(const std::vector<std::uint8_t> &file, std::size_t fromBlock,
                           std::vector<std::int16_t> &left, std::vector<std::int16_t> &right)
    {
        left.clear();
        right.clear();
        StereoHeader header;
        if (file.size() < sizeof(header))
            return false;
        std::memcpy(&header, file.data(), sizeof(header));
        const std::size_t blockBytes = static_cast<std::size_t>(kMusicBlockWords + 4) * 2;
        if (std::memcmp(header.magic, kStereoMagic, 4) != 0 || header.version != kSoundVersion ||
            file.size() != sizeof(header) + header.blocks * blockBytes)
            return false;
        hlDspInit();
        for (std::size_t block = fromBlock; block < header.blocks; ++block)
        {
            const std::uint8_t *at = file.data() + sizeof(header) + block * blockBytes;
            auto word = [&](int n) { return static_cast<std::uint16_t>(at[n * 2] | (at[n * 2 + 1] << 8)); };
            if (block == fromBlock)
            {
                // Taken up here: the states the block starts from.
                hlValue[0] = static_cast<std::int16_t>(word(0));
                hlIndex[0] = static_cast<std::int16_t>(word(1));
                hlValue[1] = static_cast<std::int16_t>(word(2));
                hlIndex[1] = static_cast<std::int16_t>(word(3));
            }
            for (int w = 0; w < kMusicBlockWords; ++w)
            {
                const std::uint16_t packed = word(4 + w);
                for (int half = 0; half < 2; ++half)
                {
                    hlDecode(0, (packed >> (half * 8)) & 15);
                    hlDecode(1, (packed >> (half * 8 + 4)) & 15);
                    left.push_back(hlValue[0]);
                    right.push_back(hlValue[1]);
                }
            }
        }
        return true;
    }

    bool decodeMp3(const std::vector<std::uint8_t> &bytes, WavData &out, std::string &error,
                   std::vector<std::int16_t> *left, std::vector<std::int16_t> *right)
    {
        out = WavData();
        if (left != nullptr)
            left->clear();
        if (right != nullptr)
            right->clear();
        static mp3dec_t decoder;
        mp3dec_init(&decoder);
        std::vector<mp3d_sample_t> pcm(MINIMP3_MAX_SAMPLES_PER_FRAME);
        std::size_t at = 0;
        while (at < bytes.size())
        {
            mp3dec_frame_info_t info{};
            const int got = mp3dec_decode_frame(&decoder, bytes.data() + at,
                                                static_cast<int>(std::min<std::size_t>(bytes.size() - at, 1 << 20)),
                                                pcm.data(), &info);
            if (info.frame_bytes == 0)
                break;
            at += static_cast<std::size_t>(info.frame_bytes);
            if (got <= 0)
                continue;
            out.rate = info.hz;
            for (int i = 0; i < got; ++i)
            {
                int sum = 0;
                for (int c = 0; c < info.channels; ++c)
                    sum += pcm[static_cast<std::size_t>(i) * info.channels + c];
                out.samples.push_back(static_cast<std::int16_t>(sum / info.channels));
                if (left != nullptr)
                    left->push_back(pcm[static_cast<std::size_t>(i) * info.channels]);
                if (right != nullptr)
                    right->push_back(pcm[static_cast<std::size_t>(i) * info.channels + (info.channels - 1)]);
            }
        }
        if (out.samples.empty() || out.rate == 0)
        {
            error = "not an MP3 that can be decoded";
            return false;
        }
        return true;
    }
} // namespace hlds
