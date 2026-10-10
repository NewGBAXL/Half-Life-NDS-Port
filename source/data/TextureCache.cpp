#include "data/TextureCache.hpp"

#include <cstdio>
#include <cstdlib>
#include <string>

#include "Global.hpp"
#include "HlTexture.hpp"
#include "data/LevelFile.hpp"

namespace hlrt
{
    TextureCache textureCache;

    void TextureCache::destroy()
    {
        for (int i = 0; i < count_; ++i)
        {
            if (materials_[i] != nullptr)
                NEA_MaterialDelete(materials_[i]);
            if (palettes_[i] != nullptr)
                NEA_PaletteDelete(palettes_[i]);
        }
        std::free(materials_);
        std::free(palettes_);
        std::free(binds_);
        std::free(tints_);
        binds_ = nullptr;
        tints_ = nullptr;
        materials_ = nullptr;
        palettes_ = nullptr;
        count_ = 0;
        missing_ = 0;
    }

    void TextureCache::forget()
    {
        std::free(materials_);
        std::free(palettes_);
        std::free(binds_);
        std::free(tints_);
        binds_ = nullptr;
        tints_ = nullptr;
        materials_ = nullptr;
        palettes_ = nullptr;
        count_ = 0;
        missing_ = 0;
    }

    bool TextureCache::create()
    {
        destroy();
        const int count = static_cast<int>(level.view.textureCount);
        materials_ = static_cast<NEA_Material **>(std::calloc(count + 1, sizeof(NEA_Material *)));
        palettes_ = static_cast<NEA_Palette **>(std::calloc(count + 1, sizeof(NEA_Palette *)));
        binds_ = static_cast<std::uint32_t *>(std::calloc(count + 1, 2 * sizeof(std::uint32_t)));
        tints_ = static_cast<std::uint16_t *>(std::malloc((count + 1) * sizeof(std::uint16_t)));
        for (int i = 0; tints_ != nullptr && i <= count; ++i)
            tints_[i] = 0x7FFF;
        if (materials_ == nullptr || palettes_ == nullptr || binds_ == nullptr || tints_ == nullptr)
        {
            destroy();
            return false;
        }
        count_ = count;
        for (int i = 0; i < count; ++i)
        {
            if (!loadOne(static_cast<unsigned>(i)))
            {
                ++missing_;
                continue;
            }
            binds_[i * 2] = NEA_MaterialGetTexParam(materials_[i]);
            binds_[i * 2 + 1] = NEA_PaletteGetBase(palettes_[i]);
        }
        return true;
    }

    unsigned TextureCache::shown(unsigned index, bool switched) const
    {
        const hldata::TextureRef &ref = level.view.textures[index];
        unsigned first = index, pictures = ref.frames;
        if (switched && ref.other != hldata::kNoTexture)
        {
            first = ref.other;
            pictures = ref.otherFrames;
        }
        return pictures > 1 ? first + tick_ % pictures : first;
    }

    void TextureCache::frame()
    {
        tick_ = Global::vblanks() / 6;
    }

    bool TextureCache::loadOne(unsigned index)
    {
        const hldata::TextureRef &ref = level.view.textures[index];
        char fileName[40];
        const bool a3i5 = (ref.flags & hldata::kTextureA3I5) != 0;
        hldata::textureFileName(ref.name, ref.width, ref.height, a3i5, fileName);
        const std::string path = global.texDir() + fileName;
        // (A liquid's coordinates go through the texture matrix, and a
        // conveyor belt's.)
        return loadFile(path.c_str(), (ref.flags & (hldata::kTextureLiquid | hldata::kTextureScroll)) != 0, ref.width,
                        ref.height, a3i5 ? 1 : 0, materials_[index], palettes_[index], &tints_[index]);
    }

    bool TextureCache::loadFile(const char *path, bool liquid, int expectWidth, int expectHeight,
                                int expectA3I5, NEA_Material *&outMaterial, NEA_Palette *&outPalette,
                                std::uint16_t *tint)
    {
        outMaterial = nullptr;
        outPalette = nullptr;
        std::FILE *file = std::fopen(path, "rb");
        if (file == nullptr)
            return false;
        std::fseek(file, 0, SEEK_END);
        const long size = std::ftell(file);
        std::fseek(file, 0, SEEK_SET);

        // The header is checked against the file's real size before anything
        // in it is used to size an allocation or a read.
        hldata::TextureHeader header;
        std::uint8_t *data = nullptr;
        bool ok = size > static_cast<long>(sizeof(header)) &&
                  std::fread(&header, sizeof(header), 1, file) == 1 &&
                  hldata::textureHeaderValid(header, static_cast<std::uint32_t>(size));
        const bool a3i5 = ok && (header.flags & hldata::kTextureA3I5) != 0;
        ok = ok && (expectWidth == 0 || header.width == expectWidth) &&
             (expectHeight == 0 || header.height == expectHeight) &&
             (expectA3I5 < 0 || a3i5 == (expectA3I5 != 0));
        if (ok)
        {
            const std::size_t body = static_cast<std::size_t>(size) - sizeof(header);
            data = static_cast<std::uint8_t *>(std::malloc(body));
            ok = data != nullptr && std::fread(data, 1, body, file) == body;
        }
        std::fclose(file);
        if (!ok)
        {
            std::free(data);
            return false;
        }

        NEA_Material *material = NEA_MaterialCreate();
        NEA_Palette *palette = NEA_PaletteCreate();
        // A liquid's coordinates go through the texture matrix (WorldDraw
        // makes that sway); everything else's are used as they are.
        const auto flags = static_cast<NEA_TextureFlags>(
            NEA_TEXTURE_WRAP_S | NEA_TEXTURE_WRAP_T | (liquid ? NEA_TEXGEN_TEXCOORD : 0));
        // NEA copies the pieces into VRAM, so the buffer is only needed for
        // the length of these calls. The two formats differ in what the pieces
        // are, not in where they sit: texels, then (4x4 only) the index, then
        // the palette.
        const std::uint8_t *paletteData = data + header.texelBytes + header.indexBytes;
        if (tint != nullptr && header.paletteColours != 0)
        {
            // Its colour, taken all together: the mean of its palette.
            std::uint32_t r = 0, g = 0, b = 0;
            for (int n = 0; n < header.paletteColours; ++n)
            {
                const std::uint16_t c = static_cast<std::uint16_t>(paletteData[n * 2] | (paletteData[n * 2 + 1] << 8));
                r += c & 31;
                g += (c >> 5) & 31;
                b += (c >> 10) & 31;
            }
            *tint = static_cast<std::uint16_t>((r / header.paletteColours) | ((g / header.paletteColours) << 5) |
                                               ((b / header.paletteColours) << 10));
        }
        ok = material != nullptr && palette != nullptr;
        if (ok && a3i5)
        {
            ok = NEA_MaterialTexLoad(material, NEA_A3PAL32, header.width, header.height, flags,
                                     data) == 1 &&
                 NEA_PaletteLoad(palette, paletteData, header.paletteColours, NEA_A3PAL32) == 1;
        }
        else if (ok)
        {
            ok = NEA_MaterialTex4x4Load(material, header.width, header.height, flags, data,
                                        data + header.texelBytes) == 1 &&
                 NEA_PaletteLoadSize(palette, paletteData,
                                     static_cast<std::size_t>(header.paletteColours) * 2,
                                     NEA_TEX4X4) == 1;
        }
        std::free(data);
        if (!ok)
        {
            // Most likely out of texture or palette memory. Give back whatever
            // half of it was taken.
            if (material != nullptr)
                NEA_MaterialDelete(material);
            if (palette != nullptr)
                NEA_PaletteDelete(palette);
            return false;
        }
        NEA_MaterialSetPalette(material, palette);
        outMaterial = material;
        outPalette = palette;
        return true;
    }
} // namespace hlrt
