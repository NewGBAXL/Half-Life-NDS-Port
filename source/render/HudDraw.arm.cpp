#include "global_header.hpp"

#include "render/HudDraw.hpp"
#include "audio/Sound.hpp"
#include <cstring>
#include <cstdlib>
#include <cstdio>

#include "render/FrameFreeze.hpp"

#include "Global.hpp"
#include "HlFont.hpp"
#include "HlLamp.hpp"
#include "HlPlayerMove.hpp"
#include "HlUi.hpp"
#include "data/TextureCache.hpp"
#include "render/ScreenFade.hpp"
#include "game/Game.hpp"
#include "render/Camera.hpp"

namespace hlrt
{
    HudDraw hudDraw;

    namespace
    {
        using namespace hldata;

        // A point of the screen as a place in front of the eye. The view is
        // 74 degrees high (Camera::kFovDegrees): at a depth of kDepth units
        // (of the sixteenths the matrix below scales by) a pixel is kPixel
        // of them, 96 * kPixel being kDepth * tan(37 degrees). The depth is
        // six of the game's units: past the near plane (four), nearer than
        // a wall can be.
        // (kPixel is the camera's hudPixel: it follows the picture.)
        constexpr int kDepth = 3058;

        void beginFlat()
        {
            MATRIX_CONTROL = GL_MODELVIEW;
            MATRIX_IDENTITY = 0;
            MATRIX_SCALE = 256; // a sixteenth: whole numbers below are exact
            MATRIX_SCALE = 256;
            MATRIX_SCALE = 256;
        }

        void corner(int sx, int sy, int s, int t)
        {
            GFX_TEX_COORD = TEXTURE_PACK(s << 4, t << 4);
            GFX_VERTEX16 = (static_cast<std::uint32_t>((96 - sy) * camera.hudPixel) << 16) |
                           (static_cast<std::uint32_t>((sx - 128) * camera.hudPixel) & 0xFFFF);
            GFX_VERTEX16 = static_cast<std::uint32_t>(-kDepth) & 0xFFFF;
        }

        /// The texture's `from` at (x, y) of the screen, `w` by `h` there.
        void quad(int x, int y, int w, int h, UiRect from)
        {
            GFX_BEGIN = GL_QUADS;
            corner(x, y, from.x, from.y);
            corner(x, y + h, from.x, from.y + from.h);
            corner(x + w, y + h, from.x + from.w, from.y + from.h);
            corner(x + w, y, from.x + from.w, from.y);
        }

        constexpr std::uint16_t kOrange = RGB15(31, 20, 2);
        constexpr std::uint16_t kRed = RGB15(31, 3, 2);
        constexpr std::uint16_t kDim = RGB15(14, 9, 1);

        // ------------------------------------------------- the display in 2D

        /// The display's pictures (ui/hud.128x128.htx), as the card has them:
        /// A3I5 texels, whose top three bits are how bright (the game's
        /// display is added to the picture, its brightness its alpha here).
        std::uint8_t atlas[kHudAtlasWidth * kHudAtlasHeight];

        /// Its sprites' sheet: video bank F, 256 by 128 in sixteen colours,
        /// which follows TopText's (bank G) in the sprites' tiles -- tile 512
        /// on, the same 32 to a row. Laid out as:
        ///   rows  0..31, all across:   the bottom row of the screen (y 160..191)
        ///   rows 32..63, x 192..255:   the row above it on the right (y 144..175):
        ///                              a second kind of ammunition
        ///   rows 32..63, x 0..31:      the crosshair
        ///   rows 32..47, x 32..63:     the lamp
        ///   rows 64..127, x 0..63:     a solid block (colour 1): the red of
        ///                              being hurt, any size, from one place
        hldata::Canvas sheet;
        bool sheetMade = false;
        constexpr int kSheetTile = 512;
        constexpr int tileOf(int x, int y)
        {
            return kSheetTile + (y >> 3) * 32 + (x >> 3);
        }

        /// Which of the sprites' palettes says what colours are: a ramp of
        /// seven steps of brightness in each (colours 1..7); the display's
        /// own also has its red in 8..14 (low health, in the same row).
        constexpr int kBankHud = 1, kBankWhite = 2, kBankDim = 3, kBankRed = 4, kBankFlash = 5, kBankPain = 6;

        /// The sprites: after TopText's sixteen.
        constexpr int kFirstSprite = 16;
        constexpr int kSprites = 32;
        std::uint16_t shadow[kSprites][3];
        std::uint16_t flashColour = 0, painColour[4] = {0, 0, 0, 0};
        /// What the vertical blank puts in the sprites' table: shadow, once
        /// it is whole (a frame's sprites half made are never shown).
        std::uint16_t shown[kSprites][3];
        std::uint16_t shownFlash = 0, shownPain[4] = {0, 0, 0, 0};

        void ramp(int bank, int first, std::uint16_t colour)
        {
            for (int level = 1; level <= 7; ++level)
            {
                const int r = (colour & 31) * level / 7, g = ((colour >> 5) & 31) * level / 7,
                          b = ((colour >> 10) & 31) * level / 7;
                SPRITE_PALETTE[bank * 16 + first + level - 1] = static_cast<std::uint16_t>(r | g << 5 | b << 10);
            }
        }

        /// A picture of the atlas onto the sheet at (x, y), brightness as its
        /// colour (+ `base`: 7 for the red of the display's own palette).
        /// `skip` columns at its left are left out.
        void blit(int x, int y, UiRect from, int base = 0, int skip = 0)
        {
            for (int dy = 0; dy < from.h; ++dy)
            {
                for (int dx = skip; dx < from.w; ++dx)
                {
                    const int level = atlas[(from.y + dy) * kHudAtlasWidth + from.x + dx] >> 5;
                    if (level != 0)
                        sheet.pixel(x + dx, y + dy, level + base);
                }
            }
        }
        /// `value` in the display's digits, its last digit ending at `right`.
        /// Returns where it starts.
        int digits(int right, int y, int value, int base)
        {
            value = value < 0 ? 0 : (value > 999 ? 999 : value);
            int x = right;
            do
            {
                x -= kHudDigitWidth;
                blit(x, y, hudDigit(value % 10), base);
                value /= 10;
            } while (value != 0);
            return x;
        }

        /// Sprite `n` of ours: `shape` (0 square, 1 wide, 2 tall) and
        /// `size` (0..3) as the hardware has them, from the sheet's (sx, sy),
        /// in palette `bank`, added to the picture.
        void sprite(int n, int x, int y, int shape, int size, int sx, int sy, int bank)
        {
            shadow[n][0] = static_cast<std::uint16_t>((y & 0xFF) | 0x0400 | (shape << 14));
            shadow[n][1] = static_cast<std::uint16_t>((x & 0x1FF) | (size << 14));
            shadow[n][2] = static_cast<std::uint16_t>(tileOf(sx, sy) | (bank << 12));
        }
        void hide(int n)
        {
            shadow[n][0] = 0x0200;
        }

        NEA_Material *loadingShown = nullptr;
        // The title screen's pictures (showMenu), and the intro's (playIntro).
        NEA_Material *menuShown = nullptr;
        NEA_Material *logoShown = nullptr;
        NEA_Material *introShown = nullptr;

        /// A picture over the whole screen, in the top 192 rows of its
        /// 256-wide texture.
        void screenPicture(NEA_Material *material)
        {
            NEA_MaterialUse(material);
            NEA_PolyFormat(31, 0, static_cast<NEA_LightEnum>(0), NEA_CULL_NONE, static_cast<NEA_OtherFormatEnum>(0));
            GFX_COLOR = 0x7FFF;
            beginFlat();
            GFX_BEGIN = GL_QUADS;
            corner(0, 0, 0, 0);
            corner(0, 192, 0, kLoadingHeight);
            corner(256, 192, kLoadingSize, kLoadingHeight);
            corner(256, 0, kLoadingSize, 0);
        }

        void drawMenu()
        {
            if (menuShown == nullptr)
                return;
            screenPicture(menuShown);
            if (logoShown == nullptr)
                return;
            // The animated title over it, across the top as the game has it:
            // see-through by its own alpha, and on the picture's own depth
            // (drawn where that depth is: over it).
            NEA_MaterialUse(logoShown);
            NEA_PolyFormat(31, 1, static_cast<NEA_LightEnum>(0), NEA_CULL_NONE, NEA_DEPTH_TEST_EQUAL);
            GFX_COLOR = 0x7FFF;
            beginFlat();
            constexpr int kTop = 10;
            GFX_BEGIN = GL_QUADS;
            corner(0, kTop, 0, 0);
            corner(0, kTop + kLogoHeight, 0, kLogoHeight);
            corner(256, kTop + kLogoHeight, kLogoWidth, kLogoHeight);
            corner(256, kTop, kLogoWidth, 0);
        }

        // Where the intro's picture is on the screen, and how big.
        int introX = 0, introY = 0, introW = 0, introH = 0;

        void drawIntro()
        {
            if (introShown == nullptr)
                return;
            NEA_MaterialUse(introShown);
            NEA_PolyFormat(31, 0, static_cast<NEA_LightEnum>(0), NEA_CULL_NONE, static_cast<NEA_OtherFormatEnum>(0));
            GFX_COLOR = 0x7FFF;
            beginFlat();
            GFX_BEGIN = GL_QUADS;
            corner(introX, introY, 0, 0);
            corner(introX, introY + introH, 0, introH);
            corner(introX + introW, introY + introH, introW, introH);
            corner(introX + introW, introY, introW, 0);
        }

        void drawLoading()
        {
            if (loadingShown == nullptr)
                return;
            NEA_MaterialUse(loadingShown);
            NEA_PolyFormat(31, 0, static_cast<NEA_LightEnum>(0), NEA_CULL_NONE, static_cast<NEA_OtherFormatEnum>(0));
            GFX_COLOR = 0x7FFF;
            beginFlat();
            // (The picture is in the top 192 rows of its texture.)
            GFX_BEGIN = GL_QUADS;
            corner(0, 0, 0, 0);
            corner(0, 192, 0, kLoadingHeight);
            corner(256, 192, kLoadingSize, kLoadingHeight);
            corner(256, 0, kLoadingSize, 0);
        }
    } // namespace

    void HudDraw::load()
    {
        if (lampMaterial_ == nullptr)
        {
            // The lamp's light: a round picture, solid at its middle and
            // thinning evenly to nothing at its rim, in thirty-two steps (and
            // nothing at all along its edges: past them the hardware goes
            // on with the edge). Made here: it is not the game's.
            static std::uint8_t texels[kLampTexels * kLampTexels];
            // (White: what colour the light is drawn in is the wall's own,
            // given with each surface -- WorldDraw's lampPass.)
            static const std::uint16_t colours[8] = {0x7FFF, 0x7FFF, 0x7FFF, 0x7FFF, 0x7FFF, 0x7FFF, 0x7FFF, 0x7FFF};
            const int mid2 = kLampTexels - 1; // twice the middle
            for (int y = 0; y < kLampTexels; ++y)
            {
                for (int x = 0; x < kLampTexels; ++x)
                {
                    const int dx = 2 * x - mid2, dy = 2 * y - mid2;
                    // (Of 256: how far from the middle to the rim, one texel
                    // short of the edge.)
                    const int far = static_cast<int>(isqrt64(static_cast<std::uint64_t>(dx * dx + dy * dy) << 16)) /
                                    (mid2 - 2);
                    // (The game's light: brightest at its very middle, and
                    // less by the same amount for every step out.)
                    int solid = far >= 256 ? 0 : 31 * (256 - far) / 256;
                    if (x == 0 || y == 0 || x == kLampTexels - 1 || y == kLampTexels - 1)
                        solid = 0;
                    texels[y * kLampTexels + x] = static_cast<std::uint8_t>(solid << 3);
                }
            }
            NEA_Material *material = NEA_MaterialCreate();
            NEA_Palette *palette = NEA_PaletteCreate();
            if (material != nullptr && palette != nullptr &&
                NEA_MaterialTexLoad(material, NEA_A5PAL8, kLampTexels, kLampTexels, NEA_TEXGEN_POSITION, texels) == 1 &&
                NEA_PaletteLoad(palette, colours, 8, NEA_A5PAL8) == 1)
            {
                NEA_MaterialSetPalette(material, palette);
                lampMaterial_ = material;
            }
        }
        if (decalMaterial_ == nullptr && global.hasFat)
        {
            NEA_Material *material = nullptr;
            NEA_Palette *palette = nullptr;
            if (TextureCache::loadFile(global.commonPath(kDecalAtlasFile).c_str(), false, kDecalAtlasWidth, kDecalAtlasSize,
                                       1, material, palette))
            {
                decalMaterial_ = material;
                decalPalette_ = palette;
            }
        }
        // In two-pass, a texture of the display's pictures as well (drawn as
        // quads: drawQuads).
        if (global.twoPass && material_ == nullptr && global.hasFat)
        {
            NEA_Material *material = nullptr;
            NEA_Palette *palette = nullptr;
            if (TextureCache::loadFile(global.commonPath(kHudAtlasFile).c_str(), false, kHudAtlasWidth, kHudAtlasHeight,
                                       1, material, palette))
            {
                material_ = material;
                palette_ = palette;
            }
        }
        // The display's pictures: read, not uploaded (its sprites are made
        // from them: draw()).
        if (atlas_ || !global.hasFat)
            return;
        static std::uint8_t body[kHudAtlasWidth * kHudAtlasHeight + 64];
        hldata::TextureHeader header;
        if (TextureCache::readFile(global.commonPath(kHudAtlasFile).c_str(), kHudAtlasWidth, kHudAtlasHeight, 1, header,
                                   body, sizeof(body)) &&
            header.texelBytes == sizeof(atlas))
        {
            std::memcpy(atlas, body, sizeof(atlas));
            atlas_ = true;
            sheetMade = false;
        }
    }

    void HudDraw::forget()
    {
        // (NEA has let go of them itself, with everything else.)
        lampMaterial_ = nullptr;
        decalMaterial_ = decalPalette_ = nullptr;
        material_ = palette_ = nullptr;
        loadingMaterial_ = loadingPalette_ = nullptr;
        loadingShown = nullptr;
    }

    void HudDraw::setStatus(bool suit, int health, int armor, int weapon, int clip, int spare, int extra)
    {
        suit_ = suit;
        health_ = health;
        armor_ = armor;
        weapon_ = weapon;
        clip_ = clip;
        spare_ = spare;
        extra_ = extra;
    }

    int HudDraw::polygons() const
    {
        return global.twoPass ? kPolygons3D : 0;
    }
    int HudDraw::vertices() const
    {
        return global.twoPass ? kVertices3D : 0;
    }

    void HudDraw::draw()
    {
        if (global.twoPass)
        {
            // (No sprites of ours: bank F is the engine's line.)
            for (int n = 0; n < kSprites; ++n)
                hide(n);
            flashColour = 0;
            for (std::uint16_t &c : painColour)
                c = 0;
            drawQuads();
        }
        else
        {
            compose();
        }
        const int state = enterCriticalSection();
        std::memcpy(shown, shadow, sizeof(shown));
        shownFlash = flashColour;
        std::memcpy(shownPain, painColour, sizeof(shownPain));
        leaveCriticalSection(state);
    }

    void HudDraw::compose()
    {
        static int shownHealth = -1, shownArmor = -1, shownWeapon = -1, shownClip = -1, shownSpare = -1,
                   shownExtra = -2, shownLamp = -1;
        for (int n = 0; n < kSprites; ++n)
            hide(n);
        flashColour = 0;
        for (std::uint16_t &c : painColour)
            c = 0;
        if (screenFade.curtained() || frameFreeze.frozen() || !atlas_)
            return;

        // The sprites on, in the tiles' 2D layout (TopText's), added to the
        // picture; bank F theirs. (Whoever sets the video mode up -- the 3D
        // engine brought up again -- takes all that away: done again.)
        if (VRAM_F_CR != (VRAM_ENABLE | VRAM_F_MAIN_SPRITE_0x06404000))
        {
            vramSetBankF(VRAM_F_MAIN_SPRITE_0x06404000);
            sheetMade = false;
        }
        REG_DISPCNT = (REG_DISPCNT & ~(DISPLAY_SPR_1D | DISPLAY_SPR_1D_BMP)) | DISPLAY_SPR_ACTIVE;
        REG_BLDCNT = 0x3F00;
        REG_BLDALPHA = 16 | (16 << 8);
        if (!sheetMade)
        {
            sheet.tiles = reinterpret_cast<std::uint32_t *>(0x06404000);
            sheet.width = 256;
            sheet.height = 128;
            sheet.fill(0, 0, 256, 128, 0);
            sheet.fill(0, 64, 64, 64, 1);
            blit((32 - kHudCrosshair.w) / 2, 32 + (32 - kHudCrosshair.h) / 2, kHudCrosshair);
            ramp(kBankHud, 1, kOrange);
            ramp(kBankHud, 8, kRed);
            ramp(kBankWhite, 1, 0x7FFF);
            ramp(kBankDim, 1, kDim);
            ramp(kBankRed, 1, kRed);
            shownHealth = shownArmor = shownWeapon = shownClip = shownSpare = shownLamp = -1;
            shownExtra = -2;
            sheetMade = true;
        }

        // Being hurt, which wants no suit: the screen reddened all over (a
        // sheet of twelve sprites, as bright as the hurt) and a red bar on
        // each side of its middle the hurt came from.
        if (flash_ > 0)
        {
            const int red = flash_ * 14 / 256;
            flashColour = static_cast<std::uint16_t>(red > 31 ? 31 : red);
            for (int k = 0; k < 12; ++k)
                sprite(8 + k, (k % 4) * 64, (k / 4) * 64, 0, 3, 0, 64, kBankFlash);
        }
        static constexpr std::int16_t kMarks[4][4] = {{104, 52, 48, 5}, {166, 72, 5, 48}, {104, 135, 48, 5}, {85, 72, 5, 48}};
        for (int n = 0; n < 4; ++n)
        {
            if (pain_[n] <= 0)
                continue;
            const int level = pain_[n] * 24 / 256;
            painColour[n] = static_cast<std::uint16_t>((31 * level / 24) | ((3 * level / 24) << 5) | ((2 * level / 24) << 10));
            const int x = kMarks[n][0], y = kMarks[n][1];
            if (kMarks[n][2] > kMarks[n][3])
            {
                sprite(20 + n * 2, x, y - 1, 1, 1, 0, 64, kBankPain + n);      // 32 by 8
                sprite(21 + n * 2, x + 32, y - 1, 1, 0, 0, 64, kBankPain + n); // 16 by 8
            }
            else
            {
                sprite(20 + n * 2, x - 1, y, 2, 1, 0, 64, kBankPain + n);      // 8 by 32
                sprite(21 + n * 2, x - 1, y + 32, 2, 0, 0, 64, kBankPain + n); // 8 by 16
            }
        }
        if (!suit_)
            return;

        const bool gun = weapon_ == kWeaponGlock || weapon_ == kWeaponShotgun || weapon_ == kWeaponMp5 ||
                         weapon_ == kWeaponPython || weapon_ == kWeaponCrossbow || weapon_ == kWeaponRpg;
        // (What is thrown or put down, and what is fed from one store: a
        // count of itself.)
        const bool counted = weapon_ == kWeaponGrenade || weapon_ == kWeaponSatchel || weapon_ == kWeaponTripmine ||
                             weapon_ == kWeaponSnark || weapon_ == kWeaponGauss || weapon_ == kWeaponEgon ||
                             weapon_ == kWeaponHornetgun;
        // The bottom row, drawn again when what it says has changed.
        const int healthShown = health_ < 25 ? -1000 - health_ : health_;
        if (healthShown != shownHealth || armor_ != shownArmor || weapon_ != shownWeapon || clip_ != shownClip ||
            spare_ != shownSpare || extra_ != shownExtra)
        {
            shownHealth = healthShown;
            shownArmor = armor_;
            shownWeapon = weapon_;
            shownClip = clip_;
            shownSpare = spare_;
            shownExtra = extra_;
            sheet.fill(0, 0, 256, 32, 0);
            sheet.fill(192, 32, 64, 32, 0);
            // (Screen y 160 is the sheet's row 0.)
            const int red = health_ < 25 ? 7 : 0;
            blit(6, 170 - 160, kHudCross, red);
            digits(62, 170 - 160, health_, red);
            blit(70, 168 - 160, armor_ > 0 ? kHudSuitFull : kHudSuitEmpty);
            digits(128, 170 - 160, armor_, 0);
            if (gun || counted)
            {
                const UiRect icon = weapon_ == kWeaponShotgun    ? kHudAmmoBuck
                                    : weapon_ == kWeaponPython   ? kHudAmmo357
                                    : weapon_ == kWeaponGrenade  ? kHudAmmoGrenade
                                    : weapon_ == kWeaponCrossbow ? kHudAmmoBolt
                                    : weapon_ == kWeaponSatchel  ? kHudAmmoSatchel
                                    : weapon_ == kWeaponTripmine ? kHudAmmoTripmine
                                    : weapon_ == kWeaponSnark    ? kHudAmmoSnark
                                    : weapon_ == kWeaponRpg      ? kHudAmmoRocket
                                    : weapon_ == kWeaponGauss || weapon_ == kWeaponEgon ? kHudAmmoUranium
                                    : weapon_ == kWeaponHornetgun ? kHudAmmoHornet
                                                                 : kHudAmmo9mm;
                blit(236, 169 - 160, icon);
                if (gun)
                {
                    const int at = digits(232, 170 - 160, spare_, 0);
                    digits(at - 10, 170 - 160, clip_, 0);
                }
                else
                {
                    digits(232, 170 - 160, clip_, 0);
                }
                if (extra_ >= 0)
                {
                    // (Screen y 144 is the sheet's row 32, there.)
                    blit(236, 148 - 144 + 32, kHudAmmoAr);
                    digits(232, 149 - 144 + 32, extra_, 0);
                }
            }
        }
        for (int k = 0; k < 4; ++k)
            sprite(k, k * 64, 160, 1, 3, k * 64, 0, kBankHud);
        if ((gun || counted) && extra_ >= 0)
            sprite(4, 192, 144, 1, 3, 192, 32, kBankHud);

        // The lamp, at the top right as the game has it: its outline, as
        // much of it filled as there is in it, and its beam while alight.
        // Dim while it is out, red when it is nearly empty.
        if (lampSuit_)
        {
            const int lampShown = lampPercent_ * 2 + (lampOn_ ? 1 : 0);
            if (lampShown != shownLamp)
            {
                shownLamp = lampShown;
                sheet.fill(32, 32, 32, 16, 0);
                blit(32, 32, kHudLampEmpty);
                const int full = kHudLampFull.w * lampPercent_ / 100;
                if (full > 0)
                    blit(32, 32, kHudLampFull, 0, kHudLampFull.w - full);
                if (lampOn_)
                    blit(32 + kHudLampFull.w, 32, kHudLampBeam);
            }
            const int bank = lampPercent_ < 20 ? kBankRed : (lampOn_ ? kBankHud : kBankDim);
            sprite(5, 256 - 8 - kHudLampBeam.w - kHudLampFull.w, 6, 1, 2, 32, 32, bank); // 32 by 16
        }
        // The crosshair, for what is aimed (unless the COMMAND line's
        // crosshair 0 took it away).
        if (gun && game.crosshair)
            sprite(6, 128 - 16, 96 - 16, 0, 2, 0, 32, kBankWhite);
    }

    /// The display as 3D quads, in front of everything, from the display's
    /// texture: as it was drawn before it was sprites (the game's own way --
    /// its pictures added to the picture, their brightness their alpha).
    void HudDraw::drawQuads()
    {
        if (screenFade.curtained() || frameFreeze.frozen())
            return;
        // Being hurt, which wants no suit: the screen reddened all over, and
        // a red bar on each side of its middle the hurt came from.
        if (flash_ > 0 || pain_[0] > 0 || pain_[1] > 0 || pain_[2] > 0 || pain_[3] > 0)
        {
            GFX_TEX_FORMAT = 0;
            beginFlat();
            auto plain = [](int x, int y, int w, int h)
            {
                GFX_BEGIN = GL_QUADS;
                corner(x, y, 0, 0);
                corner(x, y + h, 0, 0);
                corner(x + w, y + h, 0, 0);
                corner(x + w, y, 0, 0);
            };
            if (flash_ > 0)
            {
                NEA_PolyFormat(1 + flash_ * 13 / 256, 61, static_cast<NEA_LightEnum>(0), NEA_CULL_NONE,
                               static_cast<NEA_OtherFormatEnum>(0));
                GFX_COLOR = RGB15(31, 0, 0);
                plain(0, 0, 256, 192);
            }
            static constexpr std::int16_t kMarks[4][4] = {{104, 52, 48, 5}, {166, 72, 5, 48}, {104, 135, 48, 5}, {85, 72, 5, 48}};
            GFX_COLOR = kRed;
            for (int n = 0; n < 4; ++n)
            {
                if (pain_[n] <= 0)
                    continue;
                NEA_PolyFormat(1 + pain_[n] * 24 / 256, 60 - n, static_cast<NEA_LightEnum>(0), NEA_CULL_NONE,
                               static_cast<NEA_OtherFormatEnum>(0));
                plain(kMarks[n][0], kMarks[n][1], kMarks[n][2], kMarks[n][3]);
            }
            GFX_COLOR = 0x7FFF;
        }
        if (material_ == nullptr || !suit_)
            return;
        NEA_MaterialUse(static_cast<NEA_Material *>(material_));
        // (No fog, no lights; number 63 of the polygons that blend.)
        NEA_PolyFormat(31, 63, static_cast<NEA_LightEnum>(0), NEA_CULL_NONE, static_cast<NEA_OtherFormatEnum>(0));
        beginFlat();
        auto picture = [](int x, int y, UiRect from) { quad(x, y, from.w, from.h, from); };
        auto number = [&](int right, int y, int value)
        {
            value = value < 0 ? 0 : (value > 999 ? 999 : value);
            int x = right;
            do
            {
                x -= kHudDigitWidth;
                picture(x, y, hudDigit(value % 10));
                value /= 10;
            } while (value != 0);
            return x;
        };
        // Health and armour.
        GFX_COLOR = health_ < 25 ? kRed : kOrange;
        picture(6, 170, kHudCross);
        number(62, 170, health_);
        GFX_COLOR = kOrange;
        picture(70, 168, armor_ > 0 ? kHudSuitFull : kHudSuitEmpty);
        number(128, 170, armor_);
        // The lamp: its outline, as much of it filled as there is in it, its
        // beam while alight; dim while out, red when nearly empty.
        if (lampSuit_)
        {
            GFX_COLOR = lampPercent_ < 20 ? kRed : (lampOn_ ? kOrange : kDim);
            const int x = 256 - 8 - kHudLampBeam.w - kHudLampFull.w, y = 6;
            picture(x, y, kHudLampEmpty);
            const int full = kHudLampFull.w * lampPercent_ / 100;
            if (full > 0)
            {
                UiRect part = kHudLampFull;
                part.x = static_cast<std::uint8_t>(part.x + part.w - full);
                part.w = static_cast<std::uint8_t>(full);
                quad(x + kHudLampFull.w - full, y, full, part.h, part);
            }
            if (lampOn_)
                picture(x + kHudLampFull.w, y, kHudLampBeam);
            GFX_COLOR = kOrange;
        }
        // What is in hand (as compose() has it).
        const bool gun = weapon_ == kWeaponGlock || weapon_ == kWeaponShotgun || weapon_ == kWeaponMp5 ||
                         weapon_ == kWeaponPython || weapon_ == kWeaponCrossbow || weapon_ == kWeaponRpg;
        const bool counted = weapon_ == kWeaponGrenade || weapon_ == kWeaponSatchel || weapon_ == kWeaponTripmine ||
                             weapon_ == kWeaponSnark || weapon_ == kWeaponGauss || weapon_ == kWeaponEgon ||
                             weapon_ == kWeaponHornetgun;
        if (gun || counted)
        {
            const UiRect icon = weapon_ == kWeaponShotgun    ? kHudAmmoBuck
                                : weapon_ == kWeaponPython   ? kHudAmmo357
                                : weapon_ == kWeaponGrenade  ? kHudAmmoGrenade
                                : weapon_ == kWeaponCrossbow ? kHudAmmoBolt
                                : weapon_ == kWeaponSatchel  ? kHudAmmoSatchel
                                : weapon_ == kWeaponTripmine ? kHudAmmoTripmine
                                : weapon_ == kWeaponSnark    ? kHudAmmoSnark
                                : weapon_ == kWeaponRpg      ? kHudAmmoRocket
                                : weapon_ == kWeaponGauss || weapon_ == kWeaponEgon ? kHudAmmoUranium
                                : weapon_ == kWeaponHornetgun ? kHudAmmoHornet
                                                             : kHudAmmo9mm;
            picture(236, 169, icon);
            if (gun)
            {
                const int at = number(232, 170, spare_);
                number(at - 10, 170, clip_);
            }
            else
            {
                number(232, 170, clip_);
            }
            if (extra_ >= 0)
            {
                picture(236, 148, kHudAmmoAr);
                number(232, 149, extra_);
            }
        }
        // The crosshair, for what is aimed.
        if (gun && game.crosshair)
        {
            GFX_COLOR = 0x7FFF;
            picture(128 - kHudCrosshair.w / 2, 96 - kHudCrosshair.h / 2, kHudCrosshair);
        }
        GFX_COLOR = 0x7FFF;
    }

    void HudDraw::onVblank()
    {
        auto *oam = reinterpret_cast<volatile std::uint16_t *>(0x07000000) + kFirstSprite * 4;
        for (int n = 0; n < kSprites; ++n)
        {
            oam[n * 4] = shown[n][0];
            oam[n * 4 + 1] = shown[n][1];
            oam[n * 4 + 2] = shown[n][2];
        }
        SPRITE_PALETTE[kBankFlash * 16 + 1] = shownFlash;
        for (int n = 0; n < 4; ++n)
            SPRITE_PALETTE[(kBankPain + n) * 16 + 1] = shownPain[n];
    }

    bool HudDraw::showLoading()
    {
        if (loadingMaterial_ == nullptr)
        {
            NEA_Material *material = nullptr;
            NEA_Palette *palette = nullptr;
            if (!global.hasFat || !TextureCache::loadFile(global.commonPath(kLoadingFile).c_str(), false, kLoadingSize,
                                                          kLoadingSize, 0, material, palette))
                return false;
            loadingMaterial_ = material;
            loadingPalette_ = palette;
        }
        loadingShown = static_cast<NEA_Material *>(loadingMaterial_);
        // Enough pictures of it that it is what the whole screen shows (both
        // halves of it, in two-pass).
        for (int n = 0; n < global.processFramesWhole(); ++n)
        {
            NEA_WaitForVBL(static_cast<NEA_UpdateFlags>(0));
            global.process(drawLoading);
        }
        NEA_WaitForVBL(static_cast<NEA_UpdateFlags>(0));
        // What is on the screen now is kept as a picture (FrameFreeze), as
        // the last frame of a map is when one map gives way to another, and
        // the texture goes: nothing drawn out of texture memory is on the
        // screen while a map's textures are written into it, and the map
        // has all of video memory to itself.
        frameFreeze.freeze();
        hideLoading();
        return true;
    }

    bool HudDraw::playIntro()
    {
        if (!global.hasFat)
            return false;
        std::FILE *file = std::fopen(global.dataPath(kIntroFile).c_str(), "rb");
        if (file == nullptr)
            return false;
        IntroHeader header;
        if (std::fread(&header, sizeof(header), 1, file) != 1 || std::memcmp(header.magic, kIntroMagic, 4) != 0 ||
            header.version != kIntroVersion || header.frames == 0 || header.frameBytes != kIntroFrameBytes ||
            header.textureSize != kIntroTextureSize || header.frameMicros == 0 ||
            header.pictureWidth > kIntroTextureSize || header.pictureHeight > kIntroTextureSize)
        {
            std::fclose(file);
            return false;
        }
        introX = header.pictureX;
        introY = header.pictureY;
        introW = header.pictureWidth;
        introH = header.pictureHeight;
        // One place in video memory for its picture, and one for its colours:
        // each picture written there in turn, in the blank before it is shown.
        NEA_TextureRegion region{};
        NEA_PaletteRegion colours{};
        NEA_Material *material = NEA_MaterialCreate();
        NEA_Palette *palette = NEA_PaletteCreate();
        auto *frame = static_cast<std::uint8_t *>(std::malloc(kIntroFrameBytes));
        bool ok = material != nullptr && palette != nullptr && frame != nullptr &&
                  NEA_TextureRegionReserve4x4(kIntroTexelBytes, &region) >= kIntroTexelBytes &&
                  NEA_PaletteRegionReserve(kIntroPaletteColours * 2, &colours) >= kIntroPaletteColours * 2;
        ok = ok &&
             NEA_MaterialTexBindFixed(material, NEA_TEX4X4, kIntroTextureSize, kIntroTextureSize,
                                      static_cast<NEA_TextureFlags>(0), region.texels) == 1 &&
             NEA_PaletteBindFixed(palette, colours.base, kIntroPaletteColours, NEA_TEX4X4) == 1;
        // The pictures are read in order, one ahead of the one shown (a few
        // kilobytes from the card each fifteenth of a second): never a wait
        // for the card in the blank, nor so long a frame that the sound runs
        // dry. One fallen behind is skipped to.
        int filePicture = 0; // where the file is
        int pending = -1;    // the picture read and waiting to be shown
        auto readPicture = [&](int n)
        {
            if (n != filePicture &&
                std::fseek(file, static_cast<long>(sizeof(header) + static_cast<std::size_t>(n) * kIntroFrameBytes),
                           SEEK_SET) != 0)
                return false;
            if (std::fread(frame, kIntroFrameBytes, 1, file) != 1)
                return false;
            filePicture = n + 1;
            pending = n;
            return true;
        };
        int shown = -1;
        if (ok && readPicture(0))
        {
            NEA_MaterialSetPalette(material, palette);
            sound.setMusic(kIntroTrack, 0);
            const std::uint32_t start = Global::vblanks();
            // (Not a press that was down when it began.)
            scanKeys();
            std::uint32_t was = keysHeld();
            while (true)
            {
                NEA_WaitForVBL(static_cast<NEA_UpdateFlags>(0));
                const std::uint64_t micros = static_cast<std::uint64_t>(Global::vblanks() - start) * 1000000u / 60u;
                const int due = static_cast<int>(micros / header.frameMicros);
                if (pending >= 0 && pending <= due)
                {
                    NEA_VramWrite(region.texels, frame, kIntroTexelBytes);
                    NEA_VramWrite(NEA_Texture4x4IndexAddress(region.texels), frame + kIntroTexelBytes, kIntroIndexBytes);
                    NEA_VramWrite(colours.base, frame + kIntroTexelBytes + kIntroIndexBytes, kIntroPaletteColours * 2);
                    shown = pending;
                    pending = -1;
                    introShown = material;
                }
                NEA_Process(drawIntro);
                // (The screen is dark from start-up: let up once there is
                // a picture to see.)
                if (shown >= 0 && screenFade.curtained())
                    screenFade.curtainUp();
                sound.frame();
                scanKeys();
                const std::uint32_t held = keysHeld();
                if (held & ~was)
                    break; // (a button, or the stylus: enough)
                was = held;
                if (due >= header.frames)
                    break;
                // The next picture, now, while this one is shown.
                if (pending < 0)
                {
                    const int next = shown + 1 > due ? shown + 1 : due;
                    if (next < header.frames && !readPicture(next))
                        break;
                }
            }
            sound.setMusic(-1, 0);
        }
        // Black again, before its memory goes.
        introShown = nullptr;
        NEA_WaitForVBL(static_cast<NEA_UpdateFlags>(0));
        NEA_Process(drawIntro);
        NEA_WaitForVBL(static_cast<NEA_UpdateFlags>(0));
        if (material != nullptr)
            NEA_MaterialDelete(material);
        if (palette != nullptr)
            NEA_PaletteDelete(palette);
        if (region.texels != nullptr)
            NEA_TextureRegionRelease(&region);
        if (colours.base != nullptr)
            NEA_PaletteRegionRelease(&colours);
        std::free(frame);
        std::fclose(file);
        return shown >= 0;
    }

    bool HudDraw::showMenu()
    {
        if (!global.hasFat)
            return false;
        if (menuMaterial_ == nullptr)
        {
            NEA_Material *material = nullptr;
            NEA_Palette *palette = nullptr;
            if (!TextureCache::loadFile(global.commonPath(kMenuFile).c_str(), false, kLoadingSize, kLoadingSize, 0,
                                        material, palette))
                return false;
            menuMaterial_ = material;
            menuPalette_ = palette;
        }
        // The animated title: its pictures read from the card as they come.
        if (logoMaterial_ == nullptr)
        {
            std::FILE *file = std::fopen(global.commonPath(kLogoFile).c_str(), "rb");
            LogoHeader header;
            NEA_Material *material = NEA_MaterialCreate();
            NEA_Palette *palette = NEA_PaletteCreate();
            // (A grey ramp: what the texels say of their brightness.)
            static std::uint16_t greys[32];
            for (int n = 0; n < 32; ++n)
                greys[n] = static_cast<std::uint16_t>(n | n << 5 | n << 10);
            if (file != nullptr && std::fread(&header, sizeof(header), 1, file) == 1 &&
                std::memcmp(header.magic, kLogoMagic, 4) == 0 && header.version == kLogoVersion &&
                header.width == kLogoWidth && header.height == kLogoHeight && header.frames > 0 &&
                header.frameMicros > 0 && material != nullptr && palette != nullptr &&
                NEA_MaterialTexBlank(material, NEA_A3PAL32, kLogoWidth, kLogoTextureHeight,
                                     static_cast<NEA_TextureFlags>(0)) == 1 &&
                NEA_PaletteLoad(palette, greys, 32, NEA_A3PAL32) == 1)
            {
                NEA_MaterialSetPalette(material, palette);
                logoMaterial_ = material;
                logoPalette_ = palette;
                logoFile_ = file;
                logoFrames_ = header.frames;
                logoMicros_ = header.frameMicros;
                logoFrame_ = -1;
            }
            else
            {
                if (file != nullptr)
                    std::fclose(file);
                if (material != nullptr)
                    NEA_MaterialDelete(material);
                if (palette != nullptr)
                    NEA_PaletteDelete(palette);
            }
        }
        menuSince_ = Global::vblanks();
        menuShown = static_cast<NEA_Material *>(menuMaterial_);
        logoShown = nullptr;
        return true;
    }

    void HudDraw::menuFrame()
    {
        if (menuShown == nullptr)
            return;
        // The title's picture due now (round and round), into its texture in
        // this blank, if it is another.
        if (logoMaterial_ != nullptr)
        {
            auto *material = static_cast<NEA_Material *>(logoMaterial_);
            const std::uint64_t micros = static_cast<std::uint64_t>(Global::vblanks() - menuSince_) * 1000000u / 60u;
            const int due = static_cast<int>((micros / logoMicros_) % static_cast<std::uint64_t>(logoFrames_));
            if (due != logoFrame_)
            {
                auto *file = static_cast<std::FILE *>(logoFile_);
                auto *texels = static_cast<std::uint8_t *>(NEA_MaterialTexGetData(material));
                constexpr int kBytes = kLogoWidth * kLogoHeight;
                if (texels != nullptr &&
                    std::fseek(file, static_cast<long>(sizeof(LogoHeader) + static_cast<std::size_t>(due) * kBytes),
                               SEEK_SET) == 0 &&
                    std::fread(texels, kBytes, 1, file) == 1)
                {
                    NEA_MaterialTexSetDirty(material);
                    NEA_MaterialTexVramUpdate(material);
                    logoFrame_ = due;
                    logoShown = material;
                }
            }
        }
        NEA_Process(drawMenu);
    }

    void HudDraw::hideMenu()
    {
        if (menuShown == nullptr)
            return;
        // What is on the screen stays there as a picture (as the loading
        // picture does), and its textures go.
        for (int n = 0; n < 2; ++n)
        {
            NEA_WaitForVBL(static_cast<NEA_UpdateFlags>(0));
            NEA_Process(drawMenu);
        }
        NEA_WaitForVBL(static_cast<NEA_UpdateFlags>(0));
        frameFreeze.freeze();
        menuShown = logoShown = nullptr;
        if (menuMaterial_ != nullptr)
            NEA_MaterialDelete(static_cast<NEA_Material *>(menuMaterial_));
        if (menuPalette_ != nullptr)
            NEA_PaletteDelete(static_cast<NEA_Palette *>(menuPalette_));
        if (logoMaterial_ != nullptr)
            NEA_MaterialDelete(static_cast<NEA_Material *>(logoMaterial_));
        if (logoPalette_ != nullptr)
            NEA_PaletteDelete(static_cast<NEA_Palette *>(logoPalette_));
        if (logoFile_ != nullptr)
            std::fclose(static_cast<std::FILE *>(logoFile_));
        menuMaterial_ = menuPalette_ = logoMaterial_ = logoPalette_ = logoFile_ = nullptr;
    }

    void HudDraw::hideLoading()
    {
        loadingShown = nullptr;
        if (loadingMaterial_ != nullptr)
            NEA_MaterialDelete(static_cast<NEA_Material *>(loadingMaterial_));
        if (loadingPalette_ != nullptr)
            NEA_PaletteDelete(static_cast<NEA_Palette *>(loadingPalette_));
        loadingMaterial_ = loadingPalette_ = nullptr;
    }
} // namespace hlrt
