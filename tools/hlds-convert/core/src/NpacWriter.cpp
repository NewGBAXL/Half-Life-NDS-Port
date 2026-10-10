#include "hlds/NpacWriter.hpp"

#include <algorithm>
#include <cstring>
#include <map>
#include <set>

#include "hlds/Lz77.hpp"

namespace hlds
{
    namespace
    {
        constexpr std::uint16_t kRootDir = 0xF000;
        constexpr std::size_t kMaxDirs = 4096;
        constexpr std::size_t kMaxFiles = 61440;
        constexpr std::size_t kMaxName = 127;

        void put16(std::vector<std::uint8_t> &out, std::uint32_t v)
        {
            out.push_back(static_cast<std::uint8_t>(v));
            out.push_back(static_cast<std::uint8_t>(v >> 8));
        }
        void put32(std::vector<std::uint8_t> &out, std::uint32_t v)
        {
            put16(out, v & 0xFFFF);
            put16(out, v >> 16);
        }
        void putChunk(std::vector<std::uint8_t> &out, const char *magic,
                      const std::vector<std::uint8_t> &payload)
        {
            out.insert(out.end(), magic, magic + 4);
            put32(out, static_cast<std::uint32_t>(payload.size() + 8));
            out.insert(out.end(), payload.begin(), payload.end());
        }

        bool nameFits(const std::string &name)
        {
            if (name.empty() || name.size() > kMaxName)
                return false;
            for (const char c : name)
            {
                if (c < 0x20 || c > 0x7E || std::strchr("\\/?\"<>*:;|", c) != nullptr)
                    return false;
            }
            return true;
        }

        struct Dir
        {
            std::string name;
            int parent = -1;
            std::vector<int> subdirs;
            std::vector<std::size_t> files; ///< indices into the members
            std::map<std::string, int> subdirOf;
            std::set<std::string> fileNames;
            std::uint16_t firstFile = 0;
            std::uint32_t subtable = 0;
        };

        std::uint16_t get16(const std::uint8_t *p)
        {
            return static_cast<std::uint16_t>(p[0] | (p[1] << 8));
        }
        std::uint32_t get32(const std::uint8_t *p)
        {
            return p[0] | (p[1] << 8) | (p[2] << 16) | (std::uint32_t(p[3]) << 24);
        }
    } // namespace

    bool buildNpac(std::vector<NpacMember> &members, bool compress,
                   std::vector<std::uint8_t> &out, std::string &error)
    {
        out.clear();
        if (members.size() > kMaxFiles)
        {
            error = "too many files for one archive";
            return false;
        }

        // The tree, in the order things are first named.
        std::vector<Dir> dirs(1);
        for (std::size_t m = 0; m < members.size(); ++m)
        {
            const std::string &path = members[m].path;
            int dir = 0;
            std::size_t from = 0;
            for (;;)
            {
                const std::size_t slash = path.find('/', from);
                const std::string part = path.substr(from, slash == std::string::npos ? slash : slash - from);
                if (!nameFits(part))
                {
                    error = "a name an archive cannot hold: " + path;
                    return false;
                }
                if (slash == std::string::npos)
                {
                    if (!dirs[dir].fileNames.insert(part).second || dirs[dir].subdirOf.count(part) != 0)
                    {
                        error = "named twice in one archive: " + path;
                        return false;
                    }
                    dirs[dir].files.push_back(m);
                    break;
                }
                const auto found = dirs[dir].subdirOf.find(part);
                if (found != dirs[dir].subdirOf.end())
                {
                    dir = found->second;
                }
                else
                {
                    if (dirs[dir].fileNames.count(part) != 0 || dirs.size() >= kMaxDirs)
                    {
                        error = "cannot make the folder of: " + path;
                        return false;
                    }
                    const int made = static_cast<int>(dirs.size());
                    dirs[dir].subdirOf[part] = made;
                    dirs[dir].subdirs.push_back(made);
                    Dir fresh;
                    fresh.name = part;
                    fresh.parent = dir;
                    dirs.push_back(std::move(fresh));
                    dir = made;
                }
                from = slash + 1;
            }
        }

        // File numbers run through the directories in the order of the
        // directory table, and the image holds the members in that order.
        std::vector<std::size_t> order;
        for (Dir &dir : dirs)
        {
            dir.firstFile = static_cast<std::uint16_t>(order.size());
            order.insert(order.end(), dir.files.begin(), dir.files.end());
        }
        // That is only the order they were given in if no directory's files
        // are interleaved with another's: the caller is told, not surprised.
        for (std::size_t i = 0; i < order.size(); ++i)
        {
            if (order[i] != i)
            {
                error = "members of one folder must be given together, the root's first: " +
                        members[i].path;
                return false;
            }
        }

        std::vector<std::uint8_t> image, fat, cmpt;
        bool anyCompressed = false;
        put16(fat, static_cast<std::uint32_t>(members.size()));
        put16(fat, 0);
        put16(cmpt, static_cast<std::uint32_t>(members.size()));
        put16(cmpt, 0);
        for (NpacMember &member : members)
        {
            if (member.data.size() > kLz77MaxBytes)
            {
                error = "over 16 MB: " + member.path;
                return false;
            }
            member.method = kNpacStored;
            const std::vector<std::uint8_t> *stored = &member.data;
            std::vector<std::uint8_t> packed;
            if (compress && member.mayCompress && member.data.size() >= kNpacMinCompressBytes)
            {
                packed = lz77Compress(member.data.data(), member.data.size());
                if (!packed.empty() && packed.size() * 100 <= member.data.size() * kNpacKeepPercent)
                {
                    // Nothing goes on a card that has not been seen to come
                    // back as it went in.
                    std::vector<std::uint8_t> back;
                    std::string why;
                    if (!lz77Decompress(packed.data(), packed.size(), back, why) || back != member.data)
                    {
                        error = "compression does not undo itself: " + member.path;
                        return false;
                    }
                    member.method = kNpacLz77;
                    stored = &packed;
                    anyCompressed = true;
                }
            }
            member.storedBytes = static_cast<std::uint32_t>(stored->size());
            const std::uint32_t start = static_cast<std::uint32_t>(image.size());
            image.insert(image.end(), stored->begin(), stored->end());
            put32(fat, start);
            put32(fat, start + member.storedBytes);
            put32(cmpt, member.method | (static_cast<std::uint32_t>(member.data.size()) << 8));
            while (image.size() % 4 != 0)
                image.push_back(0xFF);
        }

        // The name table: eight bytes a directory, then each directory's
        // list -- its folders, its files, a zero.
        std::vector<std::uint8_t> subtables;
        const std::uint32_t mainBytes = static_cast<std::uint32_t>(dirs.size() * 8);
        for (Dir &dir : dirs)
        {
            dir.subtable = mainBytes + static_cast<std::uint32_t>(subtables.size());
            for (const int sub : dir.subdirs)
            {
                const std::string &name = dirs[sub].name;
                subtables.push_back(static_cast<std::uint8_t>(0x80 | name.size()));
                subtables.insert(subtables.end(), name.begin(), name.end());
                put16(subtables, kRootDir + static_cast<std::uint32_t>(sub));
            }
            for (const std::size_t file : dir.files)
            {
                const std::string &path = members[file].path;
                const std::size_t slash = path.rfind('/');
                const std::string name = slash == std::string::npos ? path : path.substr(slash + 1);
                subtables.push_back(static_cast<std::uint8_t>(name.size()));
                subtables.insert(subtables.end(), name.begin(), name.end());
            }
            subtables.push_back(0);
        }
        std::vector<std::uint8_t> fnt;
        for (std::size_t d = 0; d < dirs.size(); ++d)
        {
            put32(fnt, dirs[d].subtable);
            put16(fnt, dirs[d].firstFile);
            // (The root's last field is how many directories there are.)
            put16(fnt, d == 0 ? static_cast<std::uint32_t>(dirs.size())
                              : kRootDir + static_cast<std::uint32_t>(dirs[d].parent));
        }
        fnt.insert(fnt.end(), subtables.begin(), subtables.end());
        while (fnt.size() % 4 != 0)
            fnt.push_back(0xFF);

        std::vector<std::uint8_t> body;
        putChunk(body, "BTAF", fat);
        putChunk(body, "BTNF", fnt);
        if (anyCompressed)
            putChunk(body, "CMPT", cmpt);
        putChunk(body, "GMIF", image);

        out.insert(out.end(), {'N', 'P', 'A', 'C'});
        put16(out, 0xFFFE);
        put16(out, 0x0100);
        put32(out, static_cast<std::uint32_t>(16 + body.size()));
        put16(out, 16);
        put16(out, anyCompressed ? 4 : 3);
        out.insert(out.end(), body.begin(), body.end());
        return true;
    }

    bool readNpac(const std::vector<std::uint8_t> &bytes, std::vector<NpacMember> &members,
                  std::string &error)
    {
        members.clear();
        const std::uint8_t *data = bytes.data();
        const std::size_t size = bytes.size();
        if (size < 16 || std::memcmp(data, "NPAC", 4) != 0 || get16(data + 4) != 0xFFFE ||
            get32(data + 8) != size || get16(data + 12) != 16)
        {
            error = "not an archive";
            return false;
        }
        const std::uint8_t *fat = nullptr, *fnt = nullptr, *cmpt = nullptr, *image = nullptr;
        std::size_t fatBytes = 0, fntBytes = 0, cmptBytes = 0, imageBytes = 0;
        std::size_t at = 16;
        for (int c = 0; c < get16(data + 14); ++c)
        {
            if (at + 8 > size || get32(data + at + 4) < 8 || at + get32(data + at + 4) > size)
            {
                error = "archive chunk out of bounds";
                return false;
            }
            const std::size_t bytesIn = get32(data + at + 4) - 8;
            const std::uint8_t *payload = data + at + 8;
            if (std::memcmp(data + at, "BTAF", 4) == 0)
                fat = payload, fatBytes = bytesIn;
            else if (std::memcmp(data + at, "BTNF", 4) == 0)
                fnt = payload, fntBytes = bytesIn;
            else if (std::memcmp(data + at, "CMPT", 4) == 0)
                cmpt = payload, cmptBytes = bytesIn;
            else if (std::memcmp(data + at, "GMIF", 4) == 0)
                image = payload, imageBytes = bytesIn;
            at += bytesIn + 8;
        }
        if (fat == nullptr || fnt == nullptr || image == nullptr || fatBytes < 4 || fntBytes < 8)
        {
            error = "archive is missing a chunk";
            return false;
        }
        const std::size_t count = get16(fat);
        if (fatBytes < 4 + count * 8 || (cmpt != nullptr && (cmptBytes < 4 + count * 4 || get16(cmpt) != count)))
        {
            error = "archive tables are short";
            return false;
        }

        // Names, by walking the directory table.
        std::vector<std::string> paths(count);
        std::vector<char> named(count, 0);
        const std::size_t dirCount = get16(fnt + 6);
        if (dirCount == 0 || dirCount > kMaxDirs || dirCount * 8 > fntBytes)
        {
            error = "archive name table is damaged";
            return false;
        }
        std::vector<std::string> prefix(dirCount);
        std::vector<char> known(dirCount, 0);
        known[0] = 1;
        for (std::size_t d = 0; d < dirCount; ++d)
        {
            // (A directory is always listed after its parent by this writer
            // and by the engine's.)
            if (!known[d])
            {
                error = "archive name table is out of order";
                return false;
            }
            std::size_t p = get32(fnt + d * 8);
            std::size_t file = get16(fnt + d * 8 + 4);
            for (;;)
            {
                if (p >= fntBytes)
                {
                    error = "archive name table is damaged";
                    return false;
                }
                const std::uint8_t type = fnt[p++];
                if (type == 0)
                    break;
                const std::size_t length = type & 0x7F;
                const bool isDir = (type & 0x80) != 0;
                if (length == 0 || p + length + (isDir ? 2 : 0) > fntBytes)
                {
                    error = "archive name table is damaged";
                    return false;
                }
                const std::string name(reinterpret_cast<const char *>(fnt + p), length);
                p += length;
                if (isDir)
                {
                    const std::size_t id = get16(fnt + p) - kRootDir;
                    p += 2;
                    if (id >= dirCount || known[id])
                    {
                        error = "archive name table is damaged";
                        return false;
                    }
                    known[id] = 1;
                    prefix[id] = prefix[d] + name + "/";
                }
                else
                {
                    if (file >= count || named[file])
                    {
                        error = "archive name table is damaged";
                        return false;
                    }
                    named[file] = 1;
                    paths[file] = prefix[d] + name;
                    ++file;
                }
            }
        }

        members.resize(count);
        for (std::size_t f = 0; f < count; ++f)
        {
            const std::uint32_t start = get32(fat + 4 + f * 8);
            const std::uint32_t end = get32(fat + 8 + f * 8);
            if (!named[f] || start > end || end > imageBytes)
            {
                error = "archive member out of bounds";
                return false;
            }
            NpacMember &member = members[f];
            member.path = paths[f];
            member.storedBytes = end - start;
            const std::uint32_t word = cmpt != nullptr ? get32(cmpt + 4 + f * 4) : 0;
            member.method = static_cast<std::uint8_t>(word);
            if (member.method == kNpacStored)
            {
                member.data.assign(image + start, image + end);
            }
            else if (member.method == kNpacLz77)
            {
                if (!lz77Decompress(image + start, end - start, member.data, error))
                    return false;
                // What the engine sizes its buffer from must be what the
                // stream itself says: the BIOS believes the stream.
                if (member.data.size() != (word >> 8))
                {
                    error = "archive member's size disagrees with its data: " + member.path;
                    return false;
                }
            }
            else
            {
                error = "archive member compressed in a way this port does not use";
                return false;
            }
        }
        return true;
    }
} // namespace hlds
