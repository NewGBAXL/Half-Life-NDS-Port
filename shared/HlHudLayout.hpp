#pragma once

// Where the head-up display's pieces go on the top screen: the game's own
// sums (its cl_dll: health.cpp, battery.cpp, ammo.cpp, flashlight.cpp), done
// as it does them at 640 by 480 with its 640 set of pictures, and each place
// then brought to 256 by 192 -- 0.4 of it, as the pictures were (HlUi.hpp,
// kHud640File). So the display is where the game has it, at its size, on a
// screen that size.

#include "HlFixed.hpp"
#include "HlUi.hpp"

namespace hldata
{
    /// A place at 640 by 480, on 256 by 192: 0.4 of it, to the nearest pixel.
    constexpr int hudDs(int at640)
    {
        return (at640 * 4 + (at640 >= 0 ? 5 : -5)) / 10;
    }

    // The 640 set's sizes, as its sprites/hud.txt has them.
    inline constexpr int kHud640Width = 640, kHud640Height = 480;
    inline constexpr int kHud640FontHeight = 24; ///< number_0's height: the game's m_iFontHeight
    inline constexpr int kHud640DigitStep = 20;  ///< ... and its width
    inline constexpr int kHud640CrossSide = 32;
    inline constexpr int kHud640SuitSide = 40;
    inline constexpr int kHud640AmmoSide = 24;
    inline constexpr int kHud640LampSide = 32;
    inline constexpr int kHud640HurtSide = 64;
    inline constexpr int kHud640PainLong = 128, kHud640PainShort = 48;
    inline constexpr int kHud640CrosshairSide = 24;

    /// The bottom row, and the numbers in it (a fifth of a digit lower).
    inline constexpr int kHud640RowY = kHud640Height - kHud640FontHeight - kHud640FontHeight / 2;
    inline constexpr int kHud640NumberY = kHud640RowY + kHud640FontHeight * 2 / 10;

    // Health: the cross half its width in, its number a field of three
    // digits half a digit after it, and a bar (FillRGBA, a tenth of a digit
    // wide and as high as one) half a digit after that.
    inline constexpr int kHud640CrossX = kHud640CrossSide / 2;
    inline constexpr int kHud640HealthX = kHud640CrossSide + kHud640DigitStep / 2;
    inline constexpr int kHud640HealthBarX = kHud640HealthX + 3 * kHud640DigitStep + kHud640DigitStep / 2;
    inline constexpr int kHud640BarWidth = kHud640DigitStep / 10;

    // Armour: the suit three of its widths in and a sixth of its height up,
    // filled from the bottom as far as there is armour; its number right
    // after it.
    inline constexpr int kHud640SuitX = 3 * kHud640SuitSide;
    inline constexpr int kHud640SuitY = kHud640RowY - kHud640SuitSide / 6;
    inline constexpr int kHud640ArmorX = kHud640SuitX + kHud640SuitSide;

    // Ammunition. With a clip: room for eight digits and the icon from the
    // right, the clip, a bar, what is carried for it, then the icon; without
    // one, room for four and the icon. The icon an eighth of itself up. The
    // second kind (the MP5's grenades) a row and a quarter up, as one
    // without a clip.
    inline constexpr int kHud640ClipX = kHud640Width - 8 * kHud640DigitStep - kHud640AmmoSide;
    inline constexpr int kHud640ClipBarX = kHud640ClipX + 3 * kHud640DigitStep + kHud640DigitStep / 2;
    inline constexpr int kHud640SpareX = kHud640ClipBarX + kHud640BarWidth + kHud640DigitStep / 2;
    inline constexpr int kHud640CountX = kHud640Width - 4 * kHud640DigitStep - kHud640AmmoSide;
    /// Where the icon goes after a field of three at `x`.
    constexpr int hud640AmmoIconX(int x)
    {
        return x + 3 * kHud640DigitStep;
    }
    inline constexpr int kHud640AmmoIconUp = kHud640AmmoSide / 8;
    inline constexpr int kHud640SecondY = kHud640NumberY - kHud640FontHeight - kHud640FontHeight / 4;

    // The lamp: at the top right, half its height down, its beam after it.
    inline constexpr int kHud640LampX = kHud640Width - kHud640LampSide - kHud640LampSide / 2;
    inline constexpr int kHud640LampY = kHud640LampSide / 2;
    inline constexpr int kHud640BeamX = kHud640Width - kHud640LampSide / 2;

    /// The marks of where a hurt came from (front, right, back, left): round
    /// the middle, each its own size away from it.
    struct Hud640Place
    {
        int x, y;
    };
    inline constexpr Hud640Place kHud640PainAt[4] = {
        {kHud640Width / 2 - kHud640PainLong / 2, kHud640Height / 2 - kHud640PainShort * 3},
        {kHud640Width / 2 + kHud640PainShort * 2, kHud640Height / 2 - kHud640PainLong / 2},
        {kHud640Width / 2 - kHud640PainLong / 2, kHud640Height / 2 + kHud640PainShort * 2},
        {kHud640Width / 2 - kHud640PainShort * 3, kHud640Height / 2 - kHud640PainLong / 2}};

    /// The pictures of the kinds of hurt: the newest an eighth of one in
    /// from the left and two up from the bottom, each older one a picture
    /// higher.
    constexpr Hud640Place hud640HurtAt(int stacked)
    {
        return {kHud640HurtSide / 8, kHud640Height - kHud640HurtSide * 2 - kHud640HurtSide * stacked};
    }

    inline constexpr int kHud640CrosshairX = kHud640Width / 2 - kHud640CrosshairSide / 2;
    inline constexpr int kHud640CrosshairY = kHud640Height / 2 - kHud640CrosshairSide / 2;

    /// The digits of `value` (0..999) in a field of three from `x640`: how
    /// many of the three are drawn (the last always, the others when they
    /// are not leading zeros); digit `k` (0 the hundreds) is at
    /// hudDs(x640 + k * kHud640DigitStep).
    constexpr int hudFieldFirst(int value)
    {
        return value >= 100 ? 0 : (value >= 10 ? 1 : 2);
    }

    /// How many rows of the suit's picture (from its top) are left empty,
    /// at `armor` (0..100): the game's sum, then brought down.
    constexpr int hudSuitEmptyRows(int armor)
    {
        armor = armor < 0 ? 0 : (armor > 100 ? 100 : armor);
        const int rows = hudDs(kHud640SuitSide * (100 - armor) / 100);
        return rows > kHud640SuitFull.h ? kHud640SuitFull.h : rows;
    }
    /// ... and of the lamp's, from its left, at `percent` (0..100).
    constexpr int hudLampEmptyColumns(int percent)
    {
        percent = percent < 0 ? 0 : (percent > 100 ? 100 : percent);
        const int columns = hudDs(kHud640LampSide * (100 - percent) / 100);
        return columns > kHud640LampFull.w ? kHud640LampFull.w : columns;
    }

    /// The pictures of the kinds of hurt being shown, kept as the game keeps
    /// them (health.cpp's UpdateTiles and DrawDamage): one newly hurt by
    /// goes at the bottom and moves those shown up; each stays two seconds
    /// past the last hurt of its kind, all of them pulsing together, and
    /// goes at a dark moment of the pulse, those above it moving down.
    struct HudHurts
    {
        static constexpr int kKinds = 8; ///< the kHurt* bits, in order
        static constexpr std::uint32_t kLastsMs = 2000;
        /// Where each is, from the bottom (0 the lowest); -1 not shown.
        std::int8_t stacked[kKinds] = {-1, -1, -1, -1, -1, -1, -1, -1};
        std::uint32_t untilMs[kKinds] = {};

        /// How bright they are at `nowMs` (0..256): the game's
        /// |sin(2 t)|, t in seconds.
        static int pulse(std::uint32_t nowMs)
        {
            // (2 t radians is t / pi turns: 65536 / (1000 pi) of a turn a
            // millisecond.)
            fx sine = 0, cosine = 0;
            fxSinCos(static_cast<std::uint16_t>(static_cast<std::uint64_t>(nowMs) * 2086u / 100u), sine, cosine);
            return (sine < 0 ? -sine : sine) >> (kFxShift - 8);
        }
        /// Hurt by `kinds` (kHurt* bits) at `nowMs`.
        void add(std::uint8_t kinds, std::uint32_t nowMs)
        {
            for (int k = 0; k < kKinds; ++k)
            {
                if (!(kinds & (1u << k)))
                    continue;
                untilMs[k] = nowMs + kLastsMs;
                if (stacked[k] >= 0)
                    continue;
                for (std::int8_t &other : stacked)
                {
                    if (other >= 0)
                        ++other;
                }
                stacked[k] = 0;
            }
        }
        /// Those whose time is up go, at a dark moment (`pulse` under 40).
        void tick(std::uint32_t nowMs, int pulse)
        {
            for (int k = 0; k < kKinds; ++k)
            {
                if (stacked[k] < 0 || static_cast<std::int32_t>(nowMs - untilMs[k]) < 0 || pulse >= 40)
                    continue;
                const std::int8_t gone = stacked[k];
                stacked[k] = -1;
                for (std::int8_t &other : stacked)
                {
                    if (other > gone)
                        --other;
                }
            }
        }
        bool any() const
        {
            for (std::int8_t at : stacked)
            {
                if (at >= 0)
                    return true;
            }
            return false;
        }
    };
} // namespace hldata
