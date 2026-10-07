#include "hlds/AtomicWrite.hpp"

#include <fstream>

namespace hlds
{
    bool writeFileAtomic(const std::filesystem::path &path,
                         const std::vector<std::uint8_t> &bytes, std::string &error)
    {
        std::error_code ec;
        if (path.has_parent_path())
        {
            std::filesystem::create_directories(path.parent_path(), ec);
            if (ec)
            {
                error = "cannot create " + path.parent_path().string() + ": " + ec.message();
                return false;
            }
        }
        std::filesystem::path temp = path;
        temp += ".tmp";
        {
            std::ofstream out(temp, std::ios::binary | std::ios::trunc);
            if (!out)
            {
                error = "cannot write " + temp.string();
                return false;
            }
            out.write(reinterpret_cast<const char *>(bytes.data()),
                      static_cast<std::streamsize>(bytes.size()));
            out.flush();
            if (!out)
            {
                error = "write failed for " + temp.string() + " (is the card full?)";
                out.close();
                std::filesystem::remove(temp, ec);
                return false;
            }
        }
        std::filesystem::rename(temp, path, ec);
        if (ec)
        {
            error = "cannot move " + temp.string() + " into place: " + ec.message();
            std::filesystem::remove(temp, ec);
            return false;
        }
        return true;
    }

    bool readFile(const std::filesystem::path &path, std::vector<std::uint8_t> &bytes,
                  std::string &error)
    {
        std::ifstream in(path, std::ios::binary);
        if (!in)
        {
            error = "cannot open " + path.string();
            return false;
        }
        bytes.assign((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        return true;
    }
} // namespace hlds
