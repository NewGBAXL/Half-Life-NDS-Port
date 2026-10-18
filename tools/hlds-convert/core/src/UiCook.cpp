#include "hlds/UiCook.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>

#include "hlds/AtomicWrite.hpp"

namespace hlds
{
    namespace
    {
        std::uint32_t u32At(const std::vector<std::uint8_t> &bytes, std::size_t at)
        {
            return static_cast<std::uint32_t>(bytes[at]) | static_cast<std::uint32_t>(bytes[at + 1]) << 8 |
                   static_cast<std::uint32_t>(bytes[at + 2]) << 16 | static_cast<std::uint32_t>(bytes[at + 3]) << 24;
        }
    } // namespace

    bool readWadLump(const std::filesystem::path &wad, const std::string &name, std::vector<std::uint8_t> &out,
                     std::string &error)
    {
        std::vector<std::uint8_t> bytes;
        if (!readFile(wad, bytes, error))
            return false;
        if (bytes.size() < 12 || std::memcmp(bytes.data(), "WAD3", 4) != 0)
        {
            error = wad.filename().string() + " is not a WAD";
            return false;
        }
        const std::uint32_t count = u32At(bytes, 4), dir = u32At(bytes, 8);
        if (dir > bytes.size() || count > (bytes.size() - dir) / 32)
        {
            error = wad.filename().string() + " is cut short";
            return false;
        }
        for (std::uint32_t i = 0; i < count; ++i)
        {
            const std::size_t at = dir + 32u * i;
            std::string lump;
            for (int c = 0; c < 16 && bytes[at + 16 + c] != 0; ++c)
                lump.push_back(static_cast<char>(std::toupper(bytes[at + 16 + c])));
            std::string wanted;
            for (char c : name)
                wanted.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(c))));
            if (lump != wanted)
                continue;
            const std::uint32_t pos = u32At(bytes, at), size = u32At(bytes, at + 4);
            if (bytes[at + 13] != 0 || pos > bytes.size() || size > bytes.size() - pos)
            {
                error = name + " in " + wad.filename().string() + " cannot be read";
                return false;
            }
            out.assign(bytes.begin() + pos, bytes.begin() + pos + size);
            return true;
        }
        error = wad.filename().string() + " has no " + name;
        return false;
    }

    bool parseQFont(const std::vector<std::uint8_t> &lump, QFont &out, std::string &error)
    {
        out = QFont();
        if (lump.size() < 16 + 1024)
        {
            error = "the font is cut short";
            return false;
        }
        const std::uint32_t height = u32At(lump, 4), rowHeight = u32At(lump, 12);
        const std::size_t pixels = 16 + 1024;
        if (height == 0 || height > 1024 || rowHeight == 0 || rowHeight > hldata::kFontMaxHeight ||
            lump.size() < pixels + 256u * height + 2 + 768)
        {
            error = "the font is not of a shape this knows";
            return false;
        }
        out.rowHeight = static_cast<int>(rowHeight);
        out.imageHeight = static_cast<int>(height);
        for (int c = 0; c < 256; ++c)
        {
            out.glyphs[c].start = static_cast<std::uint16_t>(lump[16 + 4 * c] | lump[17 + 4 * c] << 8);
            out.glyphs[c].width = static_cast<std::uint16_t>(lump[18 + 4 * c] | lump[19 + 4 * c] << 8);
        }
        out.pixels.assign(lump.begin() + pixels, lump.begin() + pixels + 256u * height);
        const std::size_t palette = pixels + 256u * height + 2;
        out.palette.assign(lump.begin() + palette, lump.begin() + palette + 768);
        return true;
    }

    std::vector<std::uint8_t> cookFont(const QFont &font, bool outlined)
    {
        const int first = 32, count = 96;
        // What is behind the letters: whichever index most of the picture is.
        std::size_t seen[256] = {};
        for (std::uint8_t p : font.pixels)
            ++seen[p];
        const int back = static_cast<int>(std::max_element(seen, seen + 256) - seen);
        auto level = [&](std::uint8_t index) -> std::uint32_t
        {
            if (index == back || index == 255)
                return 0;
            const int bright = std::max({font.palette[index * 3], font.palette[index * 3 + 1],
                                         font.palette[index * 3 + 2]});
            if (outlined)
                return bright < 112 ? 1 : 3;
            return bright >= 170 ? 3 : (bright >= 85 ? 2 : (bright > 0 ? 1 : 0));
        };
        hldata::FontHeader header{};
        std::memcpy(header.magic, hldata::kFontMagic, 4);
        header.version = hldata::kFontVersion;
        header.height = static_cast<std::uint8_t>(font.rowHeight);
        header.first = first;
        header.count = count;
        header.outlined = outlined ? 1 : 0;
        std::vector<std::uint8_t> out(sizeof(header) + count + static_cast<std::size_t>(count) * font.rowHeight * 4, 0);
        std::memcpy(out.data(), &header, sizeof(header));
        for (int c = 0; c < count; ++c)
        {
            const QFont::Glyph &glyph = font.glyphs[first + c];
            const int width = std::min<int>(glyph.width, hldata::kFontMaxWidth);
            out[sizeof(header) + c] = static_cast<std::uint8_t>(width);
            const int x0 = glyph.start % 256, y0 = glyph.start / 256;
            for (int row = 0; row < font.rowHeight; ++row)
            {
                std::uint32_t bits = 0;
                for (int x = 0; x < width; ++x)
                {
                    const int px = x0 + x, py = y0 + row;
                    if (px < 256 && py < font.imageHeight)
                        bits |= level(font.pixels[py * 256 + px]) << (x * 2);
                }
                const std::size_t at = sizeof(header) + count + (static_cast<std::size_t>(c) * font.rowHeight + row) * 4;
                out[at] = static_cast<std::uint8_t>(bits);
                out[at + 1] = static_cast<std::uint8_t>(bits >> 8);
                out[at + 2] = static_cast<std::uint8_t>(bits >> 16);
                out[at + 3] = static_cast<std::uint8_t>(bits >> 24);
            }
        }
        return out;
    }

    bool parseQPic(const std::vector<std::uint8_t> &lump, IndexedImage &out, std::string &error)
    {
        out = IndexedImage();
        if (lump.size() < 8)
        {
            error = "the picture is cut short";
            return false;
        }
        const std::uint32_t width = u32At(lump, 0), height = u32At(lump, 4);
        if (width == 0 || height == 0 || width > 1024 || height > 1024 ||
            lump.size() < 8 + static_cast<std::size_t>(width) * height + 2 + 768)
        {
            error = "the picture is not of a shape this knows";
            return false;
        }
        out.width = width;
        out.height = height;
        out.pixels.assign(lump.begin() + 8, lump.begin() + 8 + width * height);
        const std::size_t palette = 8 + static_cast<std::size_t>(width) * height + 2;
        out.palette.assign(lump.begin() + palette, lump.begin() + palette + 768);
        return true;
    }

    RgbaImage hudAtlas(const SprFile &hud, const SprFile &crosshairs)
    {
        RgbaImage atlas;
        atlas.width = hldata::kHudAtlasWidth;
        atlas.height = hldata::kHudAtlasHeight;
        atlas.rgba.assign(static_cast<std::size_t>(atlas.width) * atlas.height * 4, 0);
        const RgbaImage sheet = hud.frames.empty() ? RgbaImage() : spriteFrameImage(hud, 0, false);
        const RgbaImage cross = crosshairs.frames.empty() ? RgbaImage() : spriteFrameImage(crosshairs, 0, false);
        auto copy = [&](const RgbaImage &from, int sx, int sy, hldata::UiRect to)
        {
            for (int y = 0; y < to.h; ++y)
            {
                for (int x = 0; x < to.w; ++x)
                {
                    if (sx + x >= from.width || sy + y >= from.height)
                        continue;
                    std::memcpy(atlas.at(to.x + x, to.y + y), from.at(sx + x, sy + y), 4);
                }
            }
        };
        // Where each is in the game's 320 set (its sprites/hud.txt and
        // weapon_*.txt say so).
        for (int n = 0; n < 10; ++n)
            copy(sheet, 12 * n, 0, hldata::hudDigit(n));
        copy(sheet, 0, 72, hldata::kHudCross);
        copy(sheet, 0, 52, hldata::kHudSuitFull);
        copy(sheet, 20, 52, hldata::kHudSuitEmpty);
        copy(sheet, 0, 16, hldata::kHudAmmo9mm);
        copy(sheet, 18, 16, hldata::kHudAmmo357);
        copy(sheet, 54, 16, hldata::kHudAmmoBuck);
        copy(sheet, 36, 34, hldata::kHudAmmoGrenade);
        copy(sheet, 36, 16, hldata::kHudAmmoAr);
        copy(cross, 24, 0, hldata::kHudCrosshair);
        copy(sheet, 34, 72, hldata::kHudLampEmpty);
        copy(sheet, 16, 72, hldata::kHudLampFull);
        copy(sheet, 52, 72, hldata::kHudLampBeam);
        copy(sheet, 72, 16, hldata::kHudAmmoBolt);
        copy(sheet, 54, 34, hldata::kHudAmmoSatchel);
        copy(sheet, 72, 34, hldata::kHudAmmoTripmine);
        copy(sheet, 90, 34, hldata::kHudAmmoSnark);
        return atlas;
    }

    std::vector<std::uint8_t> weaponIcons(const SprFile &hud, int width, int height)
    {
        // Where each weapon's picture is in the sheet, 80 by 20 (the game's
        // sprites/weapon_*.txt say so), in the order of kWeapon*: the
        // crowbar, the pistol, the shotgun, the grenade, the MP5, the .357;
        // the crossbow, the satchel, the tripmine, the snark.
        constexpr int kFrom[hldata::kWeaponIcons][2] = {{0, 0},   {0, 40},   {0, 160},   {160, 0},  {0, 120},
                                                        {0, 80},  {80, 0},   {160, 40},  {160, 80}, {160, 120}};
        constexpr int kWide = 80, kHigh = 20;
        const RgbaImage sheet = hud.frames.empty() ? RgbaImage() : spriteFrameImage(hud, 0, false);
        const int words = hldata::iconRowWords(width);
        std::vector<std::uint8_t> out(sizeof(hldata::IconsHeader) +
                                      static_cast<std::size_t>(hldata::kWeaponIcons) * height * words * 4, 0);
        hldata::IconsHeader header{};
        std::memcpy(header.magic, hldata::kIconsMagic, 4);
        header.version = hldata::kIconsVersion;
        header.count = hldata::kWeaponIcons;
        header.width = static_cast<std::uint8_t>(width);
        header.height = static_cast<std::uint8_t>(height);
        std::memcpy(out.data(), &header, sizeof(header));
        auto solid = [&](int px, int py) -> int
        {
            return px >= 0 && py >= 0 && px < sheet.width && py < sheet.height ? sheet.at(px, py)[3] : 0;
        };
        for (int n = 0; n < hldata::kWeaponIcons; ++n)
        {
            // What of its box the picture fills -- a crowbar is long and
            // thin, a pistol sits in one corner of it -- is what is fitted
            // to the icon, in its middle, and never made larger than it is.
            int x0 = kWide, y0 = kHigh, x1 = -1, y1 = -1;
            for (int y = 0; y < kHigh; ++y)
            {
                for (int x = 0; x < kWide; ++x)
                {
                    if (solid(kFrom[n][0] + x, kFrom[n][1] + y) < 48)
                        continue;
                    x0 = std::min(x0, x);
                    y0 = std::min(y0, y);
                    x1 = std::max(x1, x);
                    y1 = std::max(y1, y);
                }
            }
            if (x1 < x0)
                continue;
            const int boxW = x1 - x0 + 1, boxH = y1 - y0 + 1;
            const double scale = std::min(1.0, std::min(static_cast<double>(width) / boxW, static_cast<double>(height) / boxH));
            const int wide = std::max(1, static_cast<int>(boxW * scale + 0.5)), high = std::max(1, static_cast<int>(boxH * scale + 0.5));
            const int left = (width - wide) / 2, top = (height - high) / 2;
            for (int y = 0; y < high; ++y)
            {
                for (int x = 0; x < wide; ++x)
                {
                    // The part of the picture this pixel is, averaged.
                    const int sx0 = x0 + x * boxW / wide, sx1 = std::max(sx0 + 1, x0 + (x + 1) * boxW / wide);
                    const int sy0 = y0 + y * boxH / high, sy1 = std::max(sy0 + 1, y0 + (y + 1) * boxH / high);
                    int sum = 0, most = 0;
                    for (int sy = sy0; sy < sy1; ++sy)
                    {
                        for (int sx = sx0; sx < sx1; ++sx)
                        {
                            const int a = solid(kFrom[n][0] + sx, kFrom[n][1] + sy);
                            sum += a;
                            most = std::max(most, a);
                        }
                    }
                    // (Thin lines are most of these pictures: a pixel is
                    // as much its brightest part as its average.)
                    const int value = (sum / ((sx1 - sx0) * (sy1 - sy0)) + most) / 2;
                    const std::uint32_t level = value < 40 ? 0u : (value < 100 ? 1u : (value < 170 ? 2u : 3u));
                    const int dx = left + x, dy = top + y;
                    const std::size_t at = sizeof(header) +
                                           ((static_cast<std::size_t>(n) * height + dy) * words + (dx >> 4)) * 4;
                    std::uint32_t word;
                    std::memcpy(&word, out.data() + at, 4);
                    word |= level << ((dx & 15) * 2);
                    std::memcpy(out.data() + at, &word, 4);
                }
            }
        }
        return out;
    }

    std::vector<std::uint8_t> titlePicture(const SprFile &hud)
    {
        // (Where the two halves are, the game's sprites/hud.txt says.)
        const RgbaImage sheet = hud.frames.empty() ? RgbaImage() : spriteFrameImage(hud, 0, false);
        const int width = hldata::kTitleWidth, height = hldata::kTitleHeight;
        const int words = hldata::iconRowWords(width);
        std::vector<std::uint8_t> out(sizeof(hldata::IconsHeader) + static_cast<std::size_t>(height) * words * 4, 0);
        hldata::IconsHeader header{};
        std::memcpy(header.magic, hldata::kIconsMagic, 4);
        header.version = hldata::kIconsVersion;
        header.count = 1;
        header.width = static_cast<std::uint8_t>(width);
        header.height = static_cast<std::uint8_t>(height);
        std::memcpy(out.data(), &header, sizeof(header));
        for (int y = 0; y < height; ++y)
        {
            for (int x = 0; x < width; ++x)
            {
                // "HALF-" is 128 wide at (0, 48), "LIFE" 116 at (128, 48).
                const int px = x, py = 48 + y;
                const int value = px < sheet.width && py < sheet.height ? sheet.at(px, py)[3] : 0;
                const std::uint32_t level = value < 40 ? 0u : (value < 100 ? 1u : (value < 170 ? 2u : 3u));
                const std::size_t at = sizeof(header) + (static_cast<std::size_t>(y) * words + (x >> 4)) * 4;
                std::uint32_t word;
                std::memcpy(&word, out.data() + at, 4);
                word |= level << ((x & 15) * 2);
                std::memcpy(out.data() + at, &word, 4);
            }
        }
        return out;
    }

    RgbaImage decalAtlas(const WadFile &decals, int &missing)
    {
        RgbaImage atlas;
        atlas.width = hldata::kDecalAtlasWidth;
        atlas.height = hldata::kDecalAtlasSize;
        atlas.rgba.assign(static_cast<std::size_t>(atlas.width) * atlas.height * 4, 0);
        missing = 0;
        for (int n = 0; n < hldata::kDecalCount; ++n)
        {
            if (hldata::kDecalSources[n] == nullptr)
                continue;
            IndexedImage picture;
            std::string why;
            if (!decals.read(hldata::kDecalSources[n], picture, why) || picture.width == 0 || picture.height == 0)
            {
                ++missing;
                continue;
            }
            // (A decal's palette runs from nothing to its colour: the index
            // is how much of the colour there is.)
            const hldata::UiRect cell = hldata::decalRect(static_cast<std::uint8_t>(n));
            for (int y = 0; y < cell.h; ++y)
            {
                for (int x = 0; x < cell.w; ++x)
                {
                    const std::uint32_t x0 = x * picture.width / cell.w, x1 = std::max(x0 + 1, (x + 1) * picture.width / cell.w);
                    const std::uint32_t y0 = y * picture.height / cell.h, y1 = std::max(y0 + 1, (y + 1) * picture.height / cell.h);
                    std::uint32_t sum = 0;
                    for (std::uint32_t sy = y0; sy < y1; ++sy)
                    {
                        for (std::uint32_t sx = x0; sx < x1; ++sx)
                            sum += picture.pixels[sy * picture.width + sx];
                    }
                    int solid = static_cast<int>(sum / ((x1 - x0) * (y1 - y0)));
                    // (The texture has eight steps of see-through: the
                    // faint spray round a stain, which is most of how big
                    // it looks, would be under the first of them. It is
                    // made a little more of.)
                    solid = std::min(255, solid * 7 / 4);
                    // (Nothing along a cell's edge: a mark does not run on
                    // into its neighbour's.)
                    if (x == 0 || y == 0 || x == cell.w - 1 || y == cell.h - 1)
                        solid = 0;
                    std::uint8_t *px = atlas.at(cell.x + x, cell.y + y);
                    px[0] = px[1] = px[2] = 255;
                    px[3] = static_cast<std::uint8_t>(solid);
                }
            }
        }
        return atlas;
    }

    RgbaImage loadingTexture(const IndexedImage &picture)
    {
        const RgbaImage scaled = resample(toRgba(picture, false), hldata::kLoadingSize, hldata::kLoadingHeight);
        RgbaImage out;
        out.width = out.height = hldata::kLoadingSize;
        out.rgba.assign(static_cast<std::size_t>(out.width) * out.height * 4, 0);
        for (std::size_t i = 3; i < out.rgba.size(); i += 4)
            out.rgba[i] = 255;
        std::memcpy(out.rgba.data(), scaled.rgba.data(), scaled.rgba.size());
        return out;
    }
} // namespace hlds
