#include "global_header.hpp"

#include "game/SaveGame.hpp"

#include <sys/stat.h>
#include <unistd.h>

#include "Global.hpp"

namespace hlrt::SaveGame
{
    namespace
    {
        std::string savePath(int slot) { return global.savesDir() + (slot == kArrival ? "slot1.sav" : "slot0.sav"); }
    } // namespace

    bool read(hldata::SaveData &out, int slot)
    {
        if (!global.hasFat)
            return false;
        std::FILE *file = std::fopen(savePath(slot).c_str(), "rb");
        if (file == nullptr)
            return false;
        // A byte more than a save may be, so that one that is too long is
        // seen to be.
        std::vector<std::uint8_t> bytes(hldata::kSaveMaxBytes + 1);
        const std::size_t size = std::fread(bytes.data(), 1, bytes.size(), file);
        std::fclose(file);
        return hldata::readSave(bytes.data(), size, out);
    }

    bool write(const hldata::SaveData &save, int slot)
    {
        if (!global.hasFat)
            return false;
        const std::vector<std::uint8_t> bytes = hldata::writeSave(save);
        std::string dir = global.savesDir();
        if (!dir.empty() && dir.back() == '/')
            dir.pop_back();
        mkdir(dir.c_str(), 0777); // there already, usually

        const std::string path = savePath(slot);
        const std::string fresh = path + ".new";
        std::FILE *file = std::fopen(fresh.c_str(), "wb");
        if (file == nullptr)
            return false;
        const bool wrote = std::fwrite(bytes.data(), 1, bytes.size(), file) == bytes.size();
        if (std::fclose(file) != 0 || !wrote)
        {
            unlink(fresh.c_str());
            return false;
        }
        // FAT will not rename over a file that is there.
        unlink(path.c_str());
        return rename(fresh.c_str(), path.c_str()) == 0;
    }
} // namespace hlrt::SaveGame
