#pragma once

// What the screens are drawn with that is not a map's: the fonts, the
// pictures of the head-up display, the picture shown while a map loads, and
// the sounds of the menu. The converter cooks them once for the card, from
// the game's own (ui/ and snd/ under the data folder); the console loads them
// by these names. None of it is in the repository or the ROM: a card without
// them shows the built-in 8x8 letters and no pictures.
//
// std-only and inline: the host tests run it.

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace hldata
{
    // ------------------------------------------------------------- fonts
    //
    // A font file: a FontHeader, `count` widths (a byte each, padded to a
    // multiple of four), then for each glyph `height` rows of 32 bits: two
    // bits a pixel, the leftmost in the lowest bits. 0 is nothing; in an
    // "outlined" font 1 is the dark edge the game draws round its letters
    // and 3 the letter; in any other 1 to 3 are how solid the letter is.

    inline constexpr char kFontMagic[4] = {'H', 'L', 'F', 'N'};
    inline constexpr std::uint16_t kFontVersion = 1;
    inline constexpr int kFontMaxWidth = 16;
    inline constexpr int kFontMaxHeight = 24;

    struct FontHeader
    {
        char magic[4];
        std::uint16_t version;
        std::uint8_t height;
        std::uint8_t first;    ///< the character its first glyph is
        std::uint8_t count;
        std::uint8_t outlined;
        std::uint8_t pad[2];
    };
    static_assert(sizeof(FontHeader) == 12);

    inline constexpr int kFontSmall = 0;  ///< the game's 14-pixel letters
    inline constexpr int kFontMedium = 1; ///< ... its 16-pixel ones
    inline constexpr int kFontTitle = 2;  ///< ... and those of its credits
    inline constexpr int kFontCount = 3;
    inline constexpr const char *const kFontFiles[kFontCount] = {"ui/font0.hfn", "ui/font1.hfn", "ui/font2.hfn"};

    struct FontView
    {
        const FontHeader *header = nullptr;
        const std::uint8_t *widths = nullptr;
        const std::uint32_t *rows = nullptr;

        /// `data` must be aligned for 32-bit words and outlive the view.
        bool open(const std::uint8_t *data, std::size_t size)
        {
            header = nullptr;
            if (data == nullptr || size < sizeof(FontHeader))
                return false;
            const auto *h = reinterpret_cast<const FontHeader *>(data);
            if (std::memcmp(h->magic, kFontMagic, 4) != 0 || h->version != kFontVersion || h->height == 0 ||
                h->height > kFontMaxHeight || h->count == 0 ||
                static_cast<unsigned>(h->first) + h->count > 256)
                return false;
            const std::size_t widthBytes = (static_cast<std::size_t>(h->count) + 3) & ~std::size_t(3);
            if (size < sizeof(FontHeader) + widthBytes + static_cast<std::size_t>(h->count) * h->height * 4)
                return false;
            widths = data + sizeof(FontHeader);
            for (int i = 0; i < h->count; ++i)
            {
                if (widths[i] > kFontMaxWidth)
                    return false;
            }
            rows = reinterpret_cast<const std::uint32_t *>(data + sizeof(FontHeader) + widthBytes);
            header = h;
            return true;
        }
        bool ok() const { return header != nullptr; }
        int height() const { return header != nullptr ? header->height : 0; }
        bool has(unsigned char c) const
        {
            return header != nullptr && c >= header->first && c < header->first + header->count;
        }
        /// How far a character moves the pen (one it has not got: as a space).
        int width(unsigned char c) const
        {
            if (!has(c))
                return has(' ') ? widths[' ' - header->first] : 0;
            return widths[c - header->first];
        }
        const std::uint32_t *glyph(unsigned char c) const
        {
            return has(c) ? rows + static_cast<std::size_t>(c - header->first) * header->height : nullptr;
        }
    };

    // ------------------------------------------------------------- the head-up display
    //
    // One small texture (A3I5: how bright the game's picture was is how
    // solid it is here, and it is drawn in the display's own colour), laid
    // out by the converter exactly so:

    struct UiRect
    {
        std::uint8_t x, y, w, h;
    };
    inline constexpr int kHudAtlasWidth = 128;
    inline constexpr int kHudAtlasHeight = 64;
    inline constexpr const char *kHudAtlasFile = "ui/hud.128x64.htx";
    inline constexpr int kHudDigitWidth = 12;
    inline constexpr int kHudDigitHeight = 16;
    inline constexpr UiRect hudDigit(int n)
    {
        return UiRect{static_cast<std::uint8_t>(12 * n), 0, 12, 16};
    }
    inline constexpr UiRect kHudCross = {0, 16, 16, 16};
    inline constexpr UiRect kHudSuitFull = {16, 16, 20, 20};
    inline constexpr UiRect kHudSuitEmpty = {36, 16, 20, 20};
    inline constexpr UiRect kHudAmmo9mm = {56, 16, 18, 18};
    inline constexpr UiRect kHudAmmo357 = {74, 16, 18, 18};
    inline constexpr UiRect kHudAmmoBuck = {92, 16, 18, 18};
    inline constexpr UiRect kHudAmmoGrenade = {110, 16, 18, 18};
    inline constexpr UiRect kHudAmmoAr = {0, 36, 18, 18};
    inline constexpr UiRect kHudCrosshair = {18, 36, 24, 24};
    inline constexpr UiRect kHudLampEmpty = {70, 38, 18, 16};
    inline constexpr UiRect kHudLampFull = {88, 38, 18, 16};
    inline constexpr UiRect kHudLampBeam = {106, 38, 6, 16};

    // ------------------------------------------------------------- marks
    //
    // What shots, blows, blood and blasts leave on a wall: the game's own
    // decals, sixteen to a cell, in one small texture (A3I5). The game's
    // pictures say how dark each pixel is: here that is how solid, and the
    // colour is whatever the mark is drawn in.

    inline constexpr int kDecalAtlasSize = 64;
    inline constexpr int kDecalCell = 16;
    inline constexpr const char *kDecalAtlasFile = "ui/decals.64x64.htx";
    inline constexpr std::uint8_t kDecalShot = 0;    ///< ... and the three after: a bullet's hole
    inline constexpr std::uint8_t kDecalShots = 4;
    inline constexpr std::uint8_t kDecalDent = 4;    ///< a blow's
    inline constexpr std::uint8_t kDecalBlood = 5;   ///< ... and the two after
    inline constexpr std::uint8_t kDecalBloods = 3;
    inline constexpr std::uint8_t kDecalAlien = 8;   ///< an alien's blood (and 13)
    inline constexpr std::uint8_t kDecalCrack = 9;   ///< glass, shot and not yet broken
    inline constexpr std::uint8_t kDecalSpit = 12;
    inline constexpr std::uint8_t kDecalAlien2 = 13;
    inline constexpr std::uint8_t kDecalScorch = 10; ///< four cells: a blast's
    inline constexpr std::uint8_t kDecalCount = 16;
    /// Where a mark's picture is in the texture.
    inline constexpr UiRect decalRect(std::uint8_t which)
    {
        if (which == kDecalScorch)
            return UiRect{32, 32, 32, 32};
        return UiRect{static_cast<std::uint8_t>((which & 3) * kDecalCell), static_cast<std::uint8_t>((which >> 2) * kDecalCell),
                      kDecalCell, kDecalCell};
    }
    /// The game's name for each (in its decals.wad), for the converter;
    /// null for the cells the scorch covers.
    inline constexpr const char *const kDecalSources[kDecalCount] = {
        "{shot1", "{shot2", "{shot3", "{shot4", "{bigshot1", "{blood1", "{blood2", "{blood3",
        "{yblood1", "{break1", "{scorch1", nullptr, "{spit1", "{yblood2", nullptr, nullptr};

    // ------------------------------------------------------------- weapon icons
    //
    // The pictures the game has for its weapons, for the touch screen's
    // slots: an IconsHeader, then for each icon `height` rows of
    // iconRowWords(width) words, two bits a pixel as a font's glyphs are
    // (0 nothing, 3 as bright as the picture goes). Icon n is weapon n + 1,
    // as large as it goes in its cell and in the middle of it.

    inline constexpr char kIconsMagic[4] = {'H', 'L', 'I', 'C'};
    inline constexpr std::uint16_t kIconsVersion = 1;
    inline constexpr const char *kWeaponIconsFile = "ui/weapons.hic";
    inline constexpr int kWeaponIcons = 6;
    inline constexpr int kWeaponIconWidth = 38;
    inline constexpr int kWeaponIconHeight = 16;
    inline constexpr int kIconMaxWidth = 80;
    inline constexpr int kIconMaxHeight = 24;
    /// The game's title, as one picture of the same form ("HALF-LIFE", its
    /// two halves side by side).
    inline constexpr const char *kTitleFile = "ui/title.hic";
    inline constexpr int kTitleWidth = 244;
    inline constexpr int kTitleHeight = 16;

    struct IconsHeader
    {
        char magic[4];
        std::uint16_t version;
        std::uint8_t count;
        std::uint8_t width;
        std::uint8_t height;
        std::uint8_t pad[3];
    };
    static_assert(sizeof(IconsHeader) == 12);
    inline constexpr int iconRowWords(int width)
    {
        return (width * 2 + 31) / 32;
    }

    struct IconsView
    {
        const IconsHeader *header = nullptr;
        const std::uint32_t *rows = nullptr;

        /// `data` must be aligned for 32-bit words and outlive the view.
        /// `widest`: what the caller has room for.
        bool open(const std::uint8_t *data, std::size_t size, int widest = kIconMaxWidth)
        {
            header = nullptr;
            if (data == nullptr || size < sizeof(IconsHeader))
                return false;
            const auto *h = reinterpret_cast<const IconsHeader *>(data);
            if (std::memcmp(h->magic, kIconsMagic, 4) != 0 || h->version != kIconsVersion || h->count == 0 ||
                h->width == 0 || h->width > widest || h->height == 0 || h->height > kIconMaxHeight)
                return false;
            if (size < sizeof(IconsHeader) +
                           static_cast<std::size_t>(h->count) * h->height * iconRowWords(h->width) * 4)
                return false;
            rows = reinterpret_cast<const std::uint32_t *>(data + sizeof(IconsHeader));
            header = h;
            return true;
        }
        bool ok() const { return header != nullptr; }
        int count() const { return header != nullptr ? header->count : 0; }
        int width() const { return header != nullptr ? header->width : 0; }
        int height() const { return header != nullptr ? header->height : 0; }
        /// 0 to 3; 0 for anything outside.
        int level(int icon, int x, int y) const
        {
            if (icon < 0 || icon >= count() || x < 0 || y < 0 || x >= width() || y >= height())
                return 0;
            const std::uint32_t *row =
                rows + (static_cast<std::size_t>(icon) * header->height + y) * iconRowWords(header->width);
            return static_cast<int>((row[x >> 4] >> ((x & 15) * 2)) & 3);
        }
    };

    // ------------------------------------------------------------- loading
    //
    // The game's own loading picture, in the top 192 rows of a texture.

    inline constexpr const char *kLoadingFile = "ui/loading.256x256.htx";
    inline constexpr int kLoadingSize = 256;
    inline constexpr int kLoadingHeight = 192;

    // ------------------------------------------------------------- menu sounds

    inline constexpr int kUiSoundPress = 0;  ///< a button of the menu
    inline constexpr int kUiSoundTick = 1;   ///< a slider moved a notch
    inline constexpr int kUiSoundWeapon = 2; ///< another weapon in hand
    inline constexpr int kUiSoundLamp = 3;   ///< the lamp's switch
    inline constexpr int kUiSoundCount = 4;
    /// Their files under snd/ on the card (without ".hsn"), and where each
    /// is in the game's sound folder.
    inline constexpr const char *const kUiSoundFiles[kUiSoundCount] = {"ui_press", "ui_tick", "ui_weapon",
                                                                     "ui_lamp"};
    inline constexpr const char *const kUiSoundSources[kUiSoundCount] = {
        "common/launch_select2.wav", "common/launch_dnmenu1.wav", "common/wpn_select.wav",
        "items/flashlight1.wav"};
} // namespace hldata
