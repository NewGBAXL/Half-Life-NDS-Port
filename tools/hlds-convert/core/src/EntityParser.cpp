#include "hlds/EntityParser.hpp"

#include <cstdlib>

namespace hlds
{
    std::string_view Entity::value(std::string_view key, std::string_view fallback) const
    {
        for (const auto &pair : pairs)
        {
            if (pair.first == key)
                return pair.second;
        }
        return fallback;
    }

    bool Entity::vec3(std::string_view key, double out[3]) const
    {
        const std::string text(value(key));
        if (text.empty())
            return false;
        const char *p = text.c_str();
        for (int i = 0; i < 3; ++i)
        {
            char *end = nullptr;
            out[i] = std::strtod(p, &end);
            if (end == p)
                return false;
            p = end;
        }
        return true;
    }

    namespace
    {
        void skipSpace(std::string_view text, std::size_t &pos)
        {
            while (pos < text.size() &&
                   (text[pos] == ' ' || text[pos] == '\t' || text[pos] == '\r' ||
                    text[pos] == '\n' || text[pos] == '\0'))
            {
                ++pos;
            }
        }

        bool readQuoted(std::string_view text, std::size_t &pos, std::string &out)
        {
            if (pos >= text.size() || text[pos] != '"')
                return false;
            const std::size_t close = text.find('"', pos + 1);
            if (close == std::string_view::npos)
                return false;
            out.assign(text.substr(pos + 1, close - pos - 1));
            pos = close + 1;
            return true;
        }
    } // namespace

    bool parseEntities(std::string_view text, std::vector<Entity> &out, std::string &error)
    {
        out.clear();
        std::size_t pos = 0;
        while (true)
        {
            skipSpace(text, pos);
            if (pos >= text.size())
                return true;
            if (text[pos] != '{')
            {
                error = "entity lump: expected '{' at byte " + std::to_string(pos);
                out.clear();
                return false;
            }
            ++pos;

            Entity entity;
            while (true)
            {
                skipSpace(text, pos);
                if (pos >= text.size())
                {
                    error = "entity lump: the last entity is not closed";
                    out.clear();
                    return false;
                }
                if (text[pos] == '}')
                {
                    ++pos;
                    break;
                }
                std::string key;
                std::string value;
                if (!readQuoted(text, pos, key))
                {
                    error = "entity lump: expected a key at byte " + std::to_string(pos);
                    out.clear();
                    return false;
                }
                skipSpace(text, pos);
                if (!readQuoted(text, pos, value))
                {
                    error = "entity lump: key '" + key + "' has no value";
                    out.clear();
                    return false;
                }
                entity.pairs.emplace_back(std::move(key), std::move(value));
            }
            out.push_back(std::move(entity));
        }
    }
} // namespace hlds
