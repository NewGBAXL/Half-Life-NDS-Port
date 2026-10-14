#include "global_header.hpp"

#include "render/HudDraw.hpp"

#include "render/FrameFreeze.hpp"

#include "Global.hpp"
#include "HlLamp.hpp"
#include "HlPlayerMove.hpp"
#include "HlUi.hpp"
#include "data/TextureCache.hpp"
#include "render/ScreenFade.hpp"

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
        constexpr int kPixel = 24;
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
            GFX_VERTEX16 = (static_cast<std::uint32_t>((96 - sy) * kPixel) << 16) |
                           (static_cast<std::uint32_t>((sx - 128) * kPixel) & 0xFFFF);
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
        void picture(int x, int y, UiRect from)
        {
            quad(x, y, from.w, from.h, from);
        }

        /// `value` in the display's digits, its last digit ending at
        /// `right`. Returns where it starts.
        int number(int right, int y, int value)
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
        }

        constexpr std::uint16_t kOrange = RGB15(31, 20, 2);
        constexpr std::uint16_t kRed = RGB15(31, 3, 2);

        NEA_Material *loadingShown = nullptr;
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
        if (material_ != nullptr || !global.hasFat)
            return;
        NEA_Material *material = nullptr;
        NEA_Palette *palette = nullptr;
        if (TextureCache::loadFile(global.commonPath(kHudAtlasFile).c_str(), false, kHudAtlasWidth, kHudAtlasHeight, 1,
                                   material, palette))
        {
            material_ = material;
            palette_ = palette;
        }
    }

    void HudDraw::forget()
    {
        // (NEA has let go of them itself, with everything else.)
        material_ = palette_ = nullptr;
        lampMaterial_ = nullptr;
        decalMaterial_ = decalPalette_ = nullptr;
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

    void HudDraw::draw()
    {
        if (screenFade.curtained())
            return;
        // Being hurt, which wants no suit and no pictures: the screen
        // reddened all over (the game's own way: a red sheet, see-through),
        // and a red bar on each side of its middle the hurt came from.
        if (flash_ > 0 || pain_[0] > 0 || pain_[1] > 0 || pain_[2] > 0 || pain_[3] > 0)
        {
            GFX_TEX_FORMAT = 0;
            beginFlat();
            auto sheet = [](int x, int y, int w, int h)
            {
                GFX_BEGIN = GL_QUADS;
                corner(x, y, 0, 0);
                corner(x, y + h, 0, 0);
                corner(x + w, y + h, 0, 0);
                corner(x + w, y, 0, 0);
            };
            if (flash_ > 0)
            {
                const int alpha = 1 + flash_ * 13 / 256;
                NEA_PolyFormat(alpha, 61, static_cast<NEA_LightEnum>(0), NEA_CULL_NONE, static_cast<NEA_OtherFormatEnum>(0));
                GFX_COLOR = RGB15(31, 0, 0);
                sheet(0, 0, 256, 192);
            }
            // (Front above the middle, back below, the sides beside.)
            static constexpr std::int16_t kMarks[4][4] = {{104, 52, 48, 5}, {166, 72, 5, 48}, {104, 135, 48, 5}, {85, 72, 5, 48}};
            GFX_COLOR = RGB15(31, 3, 2);
            for (int n = 0; n < 4; ++n)
            {
                if (pain_[n] <= 0)
                    continue;
                NEA_PolyFormat(1 + pain_[n] * 24 / 256, 60 - n, static_cast<NEA_LightEnum>(0), NEA_CULL_NONE,
                               static_cast<NEA_OtherFormatEnum>(0));
                sheet(kMarks[n][0], kMarks[n][1], kMarks[n][2], kMarks[n][3]);
            }
            GFX_COLOR = 0x7FFF;
        }
        if (material_ == nullptr || !suit_)
            return;
        NEA_MaterialUse(static_cast<NEA_Material *>(material_));
        // (No fog, no lights; number 63 of the polygons that blend.)
        NEA_PolyFormat(31, 63, static_cast<NEA_LightEnum>(0), NEA_CULL_NONE, static_cast<NEA_OtherFormatEnum>(0));
        beginFlat();

        // Health and armour.
        GFX_COLOR = health_ < 25 ? kRed : kOrange;
        picture(6, 170, kHudCross);
        number(62, 170, health_);
        GFX_COLOR = kOrange;
        picture(70, 168, armor_ > 0 ? kHudSuitFull : kHudSuitEmpty);
        number(128, 170, armor_);

        // The lamp, at the top right as the game has it: its outline, as
        // much of it filled as there is in it, and its beam while alight.
        // Always there with the suit, as the game's is: dim while it is out.
        if (lampSuit_)
        {
            GFX_COLOR = lampPercent_ < 20 ? kRed : (lampOn_ ? kOrange : RGB15(14, 9, 1));
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

        // What is in hand: in the gun, and carried for it, with its picture.
        const bool gun = weapon_ == kWeaponGlock || weapon_ == kWeaponShotgun || weapon_ == kWeaponMp5 ||
                         weapon_ == kWeaponPython;
        if (gun || weapon_ == kWeaponGrenade)
        {
            const UiRect icon = weapon_ == kWeaponShotgun  ? kHudAmmoBuck
                                : weapon_ == kWeaponPython ? kHudAmmo357
                                : weapon_ == kWeaponGrenade ? kHudAmmoGrenade
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
        if (gun)
        {
            GFX_COLOR = 0x7FFF;
            picture(128 - kHudCrosshair.w / 2, 96 - kHudCrosshair.h / 2, kHudCrosshair);
        }
        GFX_COLOR = 0x7FFF;
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
        // Enough pictures of it that it is what the whole screen shows.
        for (int n = 0; n < 2; ++n)
        {
            NEA_WaitForVBL(static_cast<NEA_UpdateFlags>(0));
            NEA_Process(drawLoading);
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
