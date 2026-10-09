#pragma once

// The loaded level's sprites, as NEA materials: one for each entry of the
// level's Sprites section, in the same order (shared/HlSprite.hpp). A sprite
// that is missing or does not fit is not an error: nothing is drawn for it.

#include <NEAMain.h>

#include "HlLevel.hpp"

namespace hlrt
{
    class SpriteCache
    {
    public:
        /// Load every sprite of the loaded level.
        bool create();
        void destroy();
        /// Drop the table WITHOUT deleting the materials: for when NEA has
        /// just been torn down and has freed them itself.
        void forget();

        int count() const { return count_; }
        /// Null for a sprite there is not.
        NEA_Material *material(unsigned index) const
        {
            return index < static_cast<unsigned>(count_) ? materials_[index] : nullptr;
        }
        /// How its atlas is cut up, and how big it is; null with the material.
        const hldata::SpriteRef *ref(unsigned index) const;

    private:
        NEA_Material **materials_ = nullptr;
        NEA_Palette **palettes_ = nullptr;
        int count_ = 0;
    };

    extern SpriteCache spriteCache;
} // namespace hlrt
