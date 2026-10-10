#include "global_header.hpp"

#include "game/SaveGame.hpp"

#include <sys/stat.h>
#include <unistd.h>

#include "Global.hpp"

namespace hlrt::SaveGame
{
    namespace
    {
        /// The saves' folder on the card.
        struct FatCard : hldata::SaveCard
        {
            std::string path(const char *name) const { return global.savesDir() + name; }
            bool read(const char *name, std::vector<std::uint8_t> &out, std::size_t most) override
            {
                out.clear();
                std::FILE *file = std::fopen(path(name).c_str(), "rb");
                if (file == nullptr)
                    return false;
                // (As long as it is, and no longer: a save is tens of
                // kilobytes, the most one may be far more.)
                std::fseek(file, 0, SEEK_END);
                const long length = std::ftell(file);
                std::fseek(file, 0, SEEK_SET);
                const std::size_t want = length < 0 ? 0 : (static_cast<std::size_t>(length) < most ? length : most);
                out.resize(want);
                const std::size_t got = want != 0 ? std::fread(out.data(), 1, want, file) : 0;
                std::fclose(file);
                out.resize(got);
                return got == want;
            }
            bool write(const char *name, const std::uint8_t *bytes, std::size_t size) override
            {
                std::string dir = global.savesDir();
                if (!dir.empty() && dir.back() == '/')
                    dir.pop_back();
                mkdir(dir.c_str(), 0777); // there already, usually
                std::FILE *file = std::fopen(path(name).c_str(), "wb");
                if (file == nullptr)
                    return false;
                const bool wrote = std::fwrite(bytes, 1, size, file) == size;
                return std::fclose(file) == 0 && wrote;
            }
            bool remove(const char *name) override { return unlink(path(name).c_str()) == 0; }
            bool rename(const char *from, const char *to) override
            {
                return ::rename(path(from).c_str(), path(to).c_str()) == 0;
            }
        };
        FatCard card;
        hldata::SaveIndex list;
        bool listed = false;

        void need()
        {
            if (listed || !global.hasFat)
                return;
            hldata::saveIndexLoad(card, list);
            listed = true;
        }
    } // namespace

    const hldata::SaveIndex &index()
    {
        need();
        return list;
    }

    bool read(hldata::SaveData &out, int slot)
    {
        if (!global.hasFat)
            return false;
        need();
        return hldata::saveSlotRead(card, slot, out);
    }

    bool write(const hldata::SaveData &save, int slot, const hldata::SaveHeading &heading)
    {
        if (!global.hasFat)
            return false;
        need();
        return hldata::saveSlotWrite(card, list, slot, save, heading);
    }
} // namespace hlrt::SaveGame
