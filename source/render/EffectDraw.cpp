#include "render/EffectDraw.hpp"

#include <nds.h>

#include <NEAMain.h>

#include "game/Game.hpp"
#include "game/Player.hpp"
#include "render/Camera.hpp"
#include "render/WorldDraw.hpp"

namespace hlrt
{
    EffectDraw effectDraw;

    namespace
    {
        using namespace hldata;

        /// A corner, in the world (Q12 units), as the level's vertices are
        /// sent: quarter units.
        void corner(const fx *p)
        {
            GFX_VERTEX16 = ((static_cast<std::uint32_t>(p[1] >> 10) & 0xFFFF) << 16) |
                           (static_cast<std::uint32_t>(p[0] >> 10) & 0xFFFF);
            GFX_VERTEX16 = static_cast<std::uint32_t>(p[2] >> 10) & 0xFFFF;
        }

        /// A square of half-width `r` units at `c`, across the directions
        /// `u` and `v` (Q12, unit).
        void square(const fx *c, const std::int32_t *u, const std::int32_t *v, int r)
        {
            fx p[4][3];
            for (int i = 0; i < 3; ++i)
            {
                p[0][i] = c[i] - u[i] * r - v[i] * r;
                p[1][i] = c[i] + u[i] * r - v[i] * r;
                p[2][i] = c[i] + u[i] * r + v[i] * r;
                p[3][i] = c[i] - u[i] * r + v[i] * r;
            }
            GFX_BEGIN = GL_QUADS;
            for (const fx *point : p)
                corner(point);
        }

        /// A strip `w` units either side of the line from `a` to `b`, across
        /// the direction `u`.
        void strip(const fx *a, const fx *b, const std::int32_t *u, int w)
        {
            fx p[4][3];
            for (int i = 0; i < 3; ++i)
            {
                p[0][i] = a[i] - u[i] * w;
                p[1][i] = a[i] + u[i] * w;
                p[2][i] = b[i] + u[i] * w;
                p[3][i] = b[i] - u[i] * w;
            }
            GFX_BEGIN = GL_QUADS;
            for (const fx *point : p)
                corner(point);
        }

        void look(int alpha, int id, std::uint16_t colour)
        {
            NEA_PolyFormat(alpha, id, static_cast<NEA_LightEnum>(0), NEA_CULL_NONE,
                           worldDraw.polygonMode());
            GFX_COLOR = colour;
        }
    } // namespace

    void EffectDraw::draw()
    {
        const ActorWorld &a = game.actors;
        bool any = false;
        for (const Effect &effect : a.effects)
            any = any || effect.kind != 0;
        for (const Projectile &shot : a.shots)
            any = any || shot.active;
        if (!any || player.noclip)
            return;

        camera.apply();
        GFX_TEX_FORMAT = 0;
        static const std::int32_t kEast[3] = {kFxOne, 0, 0}, kNorth[3] = {0, kFxOne, 0};

        for (const Projectile &shot : a.shots)
        {
            if (!shot.active)
                continue;
            if (shot.kind == kShotSpit)
            {
                look(31, 0, RGB15(12, 28, 6));
                square(shot.pos, camera.right, camera.up, 4);
            }
            else
            {
                look(31, 0, RGB15(6, 9, 5));
                square(shot.pos, camera.right, camera.up, 3);
            }
        }
        for (const Effect &effect : a.effects)
        {
            if (effect.kind == 0 || effect.wholeMs <= 0)
                continue;
            // How far through its moment it is, in 32nds, and how much of it
            // is left to see (the hardware's alpha: 1 to 31).
            const int gone = ((effect.wholeMs - effect.leftMs) * 32) / effect.wholeMs;
            const int left = 31 - gone > 1 ? 31 - gone : 1;
            switch (effect.kind)
            {
            case kEffectBolt:
                // Two strips crossed, so that it has width from anywhere.
                look(31, 0, RGB15(14, 31, 14));
                strip(effect.a, effect.b, camera.up, 2);
                strip(effect.a, effect.b, camera.right, 2);
                break;
            case kEffectBurst:
                look(left > 24 ? 24 : left, 40, RGB15(31, 24, 8));
                square(effect.a, camera.right, camera.up, 16 + (effect.size * gone) / 32);
                break;
            case kEffectRing:
                // Flat on the ground, spreading.
                look(left > 16 ? 16 : left, 41, RGB15(16, 22, 31));
                square(effect.a, kEast, kNorth, 16 + (effect.size * gone) / 32);
                break;
            case kEffectSplat:
                look(left, 42, RGB15(12, 28, 6));
                square(effect.a, camera.right, camera.up, effect.size / 2 + 2);
                break;
            default:
                break;
            }
        }
        NEA_PolyFormat(31, 0, static_cast<NEA_LightEnum>(0), WorldDraw::kCull, worldDraw.polygonMode());
        GFX_COLOR = 0x7FFF;
    }
} // namespace hlrt
