#pragma once

// The entity lump: a text list of { "key" "value" ... } blocks.
//
// Keys keep the order they were written in and may repeat -- a multi_manager
// lists its targets as keys, and two targets may share a name -- so an entity
// is a vector of pairs rather than a map.

#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace hlds
{
    struct Entity
    {
        std::vector<std::pair<std::string, std::string>> pairs;

        /// The first value stored under `key`, or `fallback`.
        std::string_view value(std::string_view key, std::string_view fallback = {}) const;
        std::string_view className() const { return value("classname"); }
        /// Three numbers separated by spaces. False if the key is absent or short.
        bool vec3(std::string_view key, double out[3]) const;
    };

    /// Parse the whole lump. A malformed lump is an error rather than a partial
    /// result: every later stage keys off entity indices.
    bool parseEntities(std::string_view text, std::vector<Entity> &out, std::string &error);
} // namespace hlds
