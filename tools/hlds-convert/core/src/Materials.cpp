#include "hlds/Materials.hpp"

namespace hlds
{
    hldata::Material MaterialBook::of(const std::string &name) const
    {
        char key[13];
        hldata::materialKey(name.c_str(), key);
        const auto found = byName.find(key);
        return found == byName.end() ? hldata::Material::Concrete : found->second;
    }

    MaterialBook parseMaterials(const std::string &text)
    {
        MaterialBook book;
        std::size_t at = 0;
        while (at < text.size())
        {
            std::size_t end = text.find('\n', at);
            if (end == std::string::npos)
                end = text.size();
            const std::string line = text.substr(at, end - at);
            at = end + 1;
            std::size_t i = line.find_first_not_of(" \t\r");
            // (A comment, or nothing.)
            if (i == std::string::npos || line[i] == '/' || i + 1 >= line.size())
                continue;
            const char letter = static_cast<char>(line[i] >= 'a' && line[i] <= 'z' ? line[i] - 32 : line[i]);
            i = line.find_first_not_of(" \t", i + 1);
            if (i == std::string::npos)
                continue;
            std::size_t stop = line.find_first_of(" \t\r", i);
            if (stop == std::string::npos)
                stop = line.size();
            // (The game keeps the first twelve letters of the name, as it
            // stands: the same key a texture's own name is cut to.)
            std::string name = line.substr(i, stop - i);
            for (char &c : name)
                c = c >= 'A' && c <= 'Z' ? static_cast<char>(c + 32) : c;
            if (name.size() > 12)
                name.resize(12);
            // (The first of two lines for one name stands, as in the game's
            // own search.)
            book.byName.emplace(name, hldata::materialOfLetter(letter));
        }
        return book;
    }
} // namespace hlds
