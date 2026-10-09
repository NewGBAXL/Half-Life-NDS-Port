#pragma once

// The loaded level's textures, as NEA materials.
//
// One material per entry of the level's texture table, in the same order, so
// the renderer goes from a surface's texture index to something it can bind
// with one array read. Each comes from its own .htx file on the card
// (shared/HlTexture.hpp), already in one of the hardware's own formats -- 4x4
// compressed, or A3I5 for textures with holes -- so the file's pieces are
// handed to NEA as they are.
//
// A texture that is missing or unreadable is not an error. Its material stays
// null and the renderer draws those surfaces in their baked light alone --
// a grey wall is a better thing to show a player than a refusal to start.

#include <cstdint>

#include <NEAMain.h>

namespace hlrt
{
    class TextureCache
    {
    public:
        /// Load every texture of the loaded level. False only when there is no
        /// memory for the table itself.
        bool create();
        void destroy();
        /// Drop the table WITHOUT deleting the materials: for when NEA itself
        /// has just been torn down and has already freed them.
        void forget();

        /// Null for a texture that could not be loaded.
        NEA_Material *material(unsigned index) const { return materials_[index]; }
        /// The same choice as two command parameters, for a display list
        /// (where a register cannot be written): the texture's (0: none) and
        /// its palette's.
        const std::uint32_t *bind(unsigned index) const { return binds_ + index * 2; }
        /// Which texture is shown now where a surface names `index`: itself,
        /// or -- for one that is more than one picture -- the picture of
        /// its cycle that is due; and with `switched`, of its other face's
        /// (a brush whose "frame" is set), if it has one.
        unsigned shown(unsigned index, bool switched = false) const;
        /// Once a frame: which picture of every cycle is due (ten a second,
        /// as the game has them).
        void frame();
        int count() const { return count_; }
        int missing() const { return missing_; }

        /// Load one cooked texture file into a new material and palette of
        /// its own. `expect*` are what the file has to be to be the right
        /// one (0 / -1: anything). False, with nothing left allocated, if it
        /// is missing, wrong, or does not fit in video memory.
        static bool loadFile(const char *path, bool liquid, int expectWidth, int expectHeight,
                             int expectA3I5, NEA_Material *&material, NEA_Palette *&palette);

        TextureCache() = default;
        TextureCache(const TextureCache &) = delete;
        TextureCache &operator=(const TextureCache &) = delete;

    private:
        bool loadOne(unsigned index);

        NEA_Material **materials_ = nullptr;
        NEA_Palette **palettes_ = nullptr;
        std::uint32_t *binds_ = nullptr;
        int count_ = 0;
        int missing_ = 0;
        std::uint32_t tick_ = 0;
    };

    extern TextureCache textureCache;
} // namespace hlrt
