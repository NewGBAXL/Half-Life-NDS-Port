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

            // The file being written or read a piece at a time (SaveWriter),
            // kept open from one piece to the next: opening it is the slow
            // part.
            std::FILE *open_ = nullptr;
            std::string openName_;
            bool openWriting_ = false;
            bool openFailed_ = false;
            bool close()
            {
                if (open_ == nullptr)
                    return true;
                const bool ok = std::fclose(open_) == 0 && !openFailed_;
                open_ = nullptr;
                openName_.clear();
                openFailed_ = false;
                return ok;
            }
            std::FILE *keep(const char *name, bool writing)
            {
                if (open_ != nullptr && openName_ == name && openWriting_ == writing)
                    return open_;
                close();
                if (writing)
                {
                    std::string dir = global.savesDir();
                    if (!dir.empty() && dir.back() == '/')
                        dir.pop_back();
                    mkdir(dir.c_str(), 0777); // there already, usually
                }
                open_ = std::fopen(path(name).c_str(), writing ? "wb" : "rb");
                if (open_ != nullptr)
                {
                    openName_ = name;
                    openWriting_ = writing;
                }
                return open_;
            }
            bool append(const char *name, const std::uint8_t *bytes, std::size_t size, bool first) override
            {
                if (first)
                    close();
                else if (open_ == nullptr || openName_ != name || !openWriting_)
                    return false; // (it was let go of in between: begun again by the writer)
                std::FILE *file = keep(name, true);
                if (file == nullptr)
                    return false;
                if (std::fwrite(bytes, 1, size, file) != size)
                {
                    openFailed_ = true;
                    return false;
                }
                return true;
            }
            bool readAt(const char *name, std::size_t offset, std::size_t size, std::vector<std::uint8_t> &out) override
            {
                out.clear();
                std::FILE *file = keep(name, false);
                if (file == nullptr || std::fseek(file, static_cast<long>(offset), SEEK_SET) != 0)
                    return false;
                out.resize(size);
                out.resize(std::fread(out.data(), 1, size, file));
                return true;
            }
            bool finish(const char *name) override
            {
                return open_ == nullptr || openName_ != name ? true : close();
            }

            bool read(const char *name, std::vector<std::uint8_t> &out, std::size_t most) override
            {
                close();
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
                close();
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
            bool remove(const char *name) override
            {
                close();
                return unlink(path(name).c_str()) == 0;
            }
            bool rename(const char *from, const char *to) override
            {
                close();
                return ::rename(path(from).c_str(), path(to).c_str()) == 0;
            }
        };
        FatCard card;
        hldata::SaveIndex list;
        hldata::SaveWriter writer;
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
        finish();
        return hldata::saveSlotRead(card, slot, out);
    }

    bool write(const hldata::SaveData &save, int slot, const hldata::SaveHeading &heading)
    {
        if (!global.hasFat)
            return false;
        need();
        finish();
        return hldata::saveSlotWrite(card, list, slot, save, heading);
    }

    bool begin(const hldata::SaveData &save, int slot, const hldata::SaveHeading &heading)
    {
        if (!global.hasFat || writer.busy())
            return false;
        need();
        return writer.begin(card, list, slot, save, heading);
    }

    hldata::SaveWriter::Result step()
    {
        return writer.step();
    }

    bool busy()
    {
        return writer.busy();
    }

    bool finish()
    {
        return !writer.busy() || writer.finish();
    }
} // namespace hlrt::SaveGame
