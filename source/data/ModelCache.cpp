#include "data/ModelCache.hpp"

#include <cstdio>
#include <cstdlib>
#include <new>
#include <string>

#include "Global.hpp"
#include "data/LevelFile.hpp"
#include "data/TextureCache.hpp"

namespace hlrt
{
    ModelCache modelCache;

    namespace
    {
        /// A model is tens of kilobytes; one that claims to be megabytes is
        /// not one.
        constexpr long kMaxModelBytes = 1024 * 1024;
    } // namespace

    void ModelCache::release(bool engineGone)
    {
        for (int i = 0; i < count_; ++i)
        {
            if (blocks_ != nullptr)
                std::free(blocks_[i]);
            if (!engineGone && materials_ != nullptr && materials_[i] != nullptr)
                NEA_MaterialDelete(materials_[i]);
            if (!engineGone && palettes_ != nullptr && palettes_[i] != nullptr)
                NEA_PaletteDelete(palettes_[i]);
        }
        std::free(views_);
        std::free(blocks_);
        std::free(materials_);
        std::free(palettes_);
        std::free(kept_);
        kept_ = nullptr;
        wanted_ = resident_ = -1;
        views_ = nullptr;
        blocks_ = nullptr;
        materials_ = nullptr;
        palettes_ = nullptr;
        count_ = 0;
        missing_ = 0;
    }

    void ModelCache::destroy() { release(false); }
    void ModelCache::forget() { release(true); }

    void ModelCache::forgetSkins()
    {
        for (int i = 0; i < count_; ++i)
        {
            materials_[i] = nullptr;
            palettes_[i] = nullptr;
        }
        resident_ = -1; // (gone with the old engine; wanted_ stays, and is put back by apply)
    }

    void ModelCache::apply()
    {
        if (resident_ == wanted_ || kept_ == nullptr)
            return;
        if (resident_ >= 0 && materials_[resident_] != nullptr)
            NEA_MaterialTexVramUnload(materials_[resident_]);
        resident_ = -1;
        if (wanted_ < 0)
            return;
        if (wanted_ >= count_ || kept_[wanted_] == 0 || materials_[wanted_] == nullptr)
        {
            wanted_ = -1; // not one that is kept in main memory: it is drawn as it is
            return;
        }
        if (NEA_MaterialTexVramLoad(materials_[wanted_]) == 1)
        {
            resident_ = wanted_;
        }
        else
        {
            // No room in video memory: drawn bare, as a skin that did not fit always was.
            kept_[wanted_] = 0;
            wanted_ = -1;
            ++bare_;
        }
    }

    int ModelCache::reservedBytes() const
    {
        if (kept_ == nullptr)
            return 0;
        int most = 0, now = 0;
        for (int i = 0; i < count_; ++i)
        {
            if (kept_[i] == 0 || views_[i].header == nullptr)
                continue;
            // 4x4: the texels (a quarter of a byte each) and the index (an eighth).
            const int bytes = views_[i].header->atlasWidth * views_[i].header->atlasHeight * 3 / 8;
            most = bytes > most ? bytes : most;
            if (i == resident_)
                now = bytes;
        }
        return most > now ? most - now : 0;
    }

    void ModelCache::loadSkins()
    {
        bare_ = 0;
        for (int i = 0; i < count_; ++i)
        {
            if (views_[i].header == nullptr || views_[i].header->atlasWidth == 0 ||
                materials_[i] != nullptr)
                continue;
            const std::string skin =
                global.mapPath("mdl/") +
                hldata::modelSkinFile(level.view.modelRefs[i].name, views_[i].header->atlasWidth,
                                      views_[i].header->atlasHeight);
            // The weapons' are kept in main memory until one is taken up:
            // of the six, one is in hand.
            const bool weapon = level.view.modelRefs[i].name[0] == 'v' && level.view.modelRefs[i].name[1] == '_';
            if (kept_ != nullptr)
                kept_[i] = weapon ? 1 : 0;
            if (!TextureCache::loadFile(skin.c_str(), false, views_[i].header->atlasWidth,
                                        views_[i].header->atlasHeight, 0, materials_[i], palettes_[i], nullptr,
                                        weapon))
            {
                ++bare_;
                if (kept_ != nullptr)
                    kept_[i] = 0;
            }
        }
    }

    bool ModelCache::create()
    {
        destroy();
        const int count = static_cast<int>(level.view.modelRefCount);
        views_ = static_cast<hldata::ModelView *>(std::calloc(count + 1, sizeof(hldata::ModelView)));
        blocks_ = static_cast<std::uint8_t **>(std::calloc(count + 1, sizeof(std::uint8_t *)));
        materials_ = static_cast<NEA_Material **>(std::calloc(count + 1, sizeof(NEA_Material *)));
        palettes_ = static_cast<NEA_Palette **>(std::calloc(count + 1, sizeof(NEA_Palette *)));
        kept_ = static_cast<std::uint8_t *>(std::calloc(count + 1, 1));
        if (views_ == nullptr || blocks_ == nullptr || materials_ == nullptr || palettes_ == nullptr ||
            kept_ == nullptr)
        {
            destroy();
            return false;
        }
        count_ = count;
        for (int i = 0; i < count; ++i)
        {
            new (&views_[i]) hldata::ModelView();
            const char *name = level.view.modelRefs[i].name;
            const std::string path = global.mapPath("mdl/") + name + ".hmd";
            // A DSi has the memory for the file with the game's own mesh as
            // its finest level, where the converter made one.
            std::FILE *file = nullptr;
            if (global.isDSi)
                file = std::fopen((global.mapPath("mdl/") + name + ".dsi.hmd").c_str(), "rb");
            if (file == nullptr)
                file = std::fopen(path.c_str(), "rb");
            if (file == nullptr)
            {
                ++missing_;
                continue;
            }
            std::fseek(file, 0, SEEK_END);
            const long size = std::ftell(file);
            std::fseek(file, 0, SEEK_SET);
            // The size is the card's word: bounded before it is trusted with
            // an allocation. (malloc's blocks are aligned for the words the
            // file is made of.)
            std::uint8_t *block = nullptr;
            if (size > 0 && size <= kMaxModelBytes)
                block = static_cast<std::uint8_t *>(std::malloc(static_cast<std::size_t>(size)));
            const bool got = block != nullptr &&
                             std::fread(block, 1, static_cast<std::size_t>(size), file) ==
                                 static_cast<std::size_t>(size);
            std::fclose(file);
            if (!got || !views_[i].open(block, static_cast<std::uint32_t>(size)))
            {
                std::free(block);
                views_[i] = hldata::ModelView();
                ++missing_;
                continue;
            }
            blocks_[i] = block;
        }
        // Their skins. Without one (not on the card, or no room in video
        // memory) a model is drawn in its lighting alone.
        loadSkins();
        return true;
    }
} // namespace hlrt
