#pragma once

// The game's list of what its textures are made of (sound/materials.txt):
// a letter and a texture's name to a line. It is what says a step on one
// floor rings and on another thuds.

#include <map>
#include <string>

#include "HlTexture.hpp"

namespace hlds
{
    struct MaterialBook
    {
        /// By the name as the game looks it up (hldata::materialKey).
        std::map<std::string, hldata::Material> byName;
        /// What a texture called `name` is made of: concrete, unless the
        /// list says otherwise.
        hldata::Material of(const std::string &name) const;
    };
    MaterialBook parseMaterials(const std::string &text);
} // namespace hlds
