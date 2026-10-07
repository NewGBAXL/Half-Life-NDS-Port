#include "data/LevelFile.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace hlrt
{
    Level level;

    void Level::unload()
    {
        view = hldata::LevelView();
        std::free(block_);
        block_ = nullptr;
        size_ = 0;
    }

    bool Level::peekHeader(const std::string &path, hldata::LevelHeader &header)
    {
        std::FILE *file = std::fopen(path.c_str(), "rb");
        if (file == nullptr)
            return false;
        const bool got = std::fread(&header, sizeof(header), 1, file) == 1;
        std::fclose(file);
        return got &&
               std::memcmp(header.magic, hldata::kLevelMagic, sizeof(hldata::kLevelMagic)) == 0 &&
               header.version == hldata::kLevelVersion;
    }

    bool Level::load(const std::string &path, std::string &error)
    {
        unload();

        std::FILE *file = std::fopen(path.c_str(), "rb");
        if (file == nullptr)
        {
            error = "Map not found on the card.";
            return false;
        }
        std::fseek(file, 0, SEEK_END);
        const long size = std::ftell(file);
        std::fseek(file, 0, SEEK_SET);
        // The size comes from the card: it is checked before it is trusted with
        // an allocation. No cooked map is anywhere near 8 MB.
        if (size < static_cast<long>(hldata::kLevelDataStart) || size > 8 * 1024 * 1024)
        {
            std::fclose(file);
            error = "Map file is damaged. Convert again.";
            return false;
        }

        block_ = static_cast<std::uint8_t *>(std::malloc(static_cast<std::size_t>(size)));
        if (block_ == nullptr)
        {
            std::fclose(file);
            error = "Not enough memory for this map.";
            return false;
        }
        const std::size_t got = std::fread(block_, 1, static_cast<std::size_t>(size), file);
        std::fclose(file);
        if (got != static_cast<std::size_t>(size))
        {
            unload();
            error = "Map could not be read from the card.";
            return false;
        }

        const hldata::LevelError result = view.open(block_, static_cast<std::uint32_t>(size));
        if (result != hldata::LevelError::Ok)
        {
            unload();
            error = result == hldata::LevelError::BadVersion
                        ? "Map is from another version. Convert again."
                        : "Map file is damaged. Convert again.";
            return false;
        }
        size_ = static_cast<std::size_t>(size);
        return true;
    }
} // namespace hlrt
