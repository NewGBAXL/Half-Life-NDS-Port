#pragma once

// Write a file so that it is either complete under its final name or not there
// at all.
//
// The converter writes to an SD card, and a cook interrupted half way -- a
// pulled card, a closed window -- must not leave a truncated level that the DS
// then tries to load. The bytes go to "<name>.tmp" first and are renamed into
// place only once they are all written.

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace hlds
{
    /// Creates missing parent directories. On failure the final name is left as
    /// it was (absent, or the previous complete file).
    bool writeFileAtomic(const std::filesystem::path &path,
                         const std::vector<std::uint8_t> &bytes, std::string &error);

    bool readFile(const std::filesystem::path &path, std::vector<std::uint8_t> &bytes,
                  std::string &error);
} // namespace hlds
