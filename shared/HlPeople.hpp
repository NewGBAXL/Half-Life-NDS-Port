#pragma once

// The people of the place: who the player USEs. What they do then -- and the
// rest of what they do -- is the game's own talkmonster (HlAiTalk.hpp,
// HlAiPeople.hpp): he follows, or says why not, or stays.
//
// Included at the end of HlMonsters.hpp. std-only and inline: the host
// tests run it.

namespace hldata
{
    // ActorState::ai, for a person (5 and up is dying and dead, as for
    // everything else): whether he follows the player, as saves have kept it.
    inline constexpr std::uint8_t kPersonIdle = 0;
    inline constexpr std::uint8_t kPersonFollow = 1;
    inline constexpr std::uint8_t kPersonFlee = 2; ///< (saves from before: taken as idle)

    /// USE, from `eye` along `forward` (Q12, unit): the scientist or guard
    /// in front of the player within arm's reach is USEd (FollowerUse). True
    /// if there was one.
    inline bool personUse(LogicWorld &w, PlayerState &player, const fx *eye, const fx *forward)
    {
        if (w.actors == nullptr)
            return false;
        ActorWorld &a = *w.actors;
        int best = -1;
        fx bestD = fxFromInt(88);
        for (int i = 0; i < a.count; ++i)
        {
            const ActorState &actor = a.states[i];
            if (!actor.active || actor.ai >= kAiDying ||
                (actor.kind != static_cast<std::uint8_t>(ActorKind::Scientist) &&
                 actor.kind != static_cast<std::uint8_t>(ActorKind::Guard)))
                continue;
            // (Across the floor: the player looks level, and a person's
            // middle is well below their eyes. Within arm's reach, more or
            // less faced, and on the same floor.)
            const fx to[3] = {actor.pos[0] - eye[0], actor.pos[1] - eye[1], 0};
            const fx up = actor.pos[2] + fxFromInt(36) - eye[2];
            const fx distance = fxLength(to);
            const fx flat[3] = {forward[0], forward[1], 0};
            const fx facing = fxLength(flat);
            const std::int64_t ahead = static_cast<std::int64_t>(to[0]) * forward[0] +
                                       static_cast<std::int64_t>(to[1]) * forward[1];
            if (up > fxFromInt(72) || up < -fxFromInt(72) || facing <= 0)
                continue;
            if (distance < bestD && ahead * 10 > (static_cast<std::int64_t>(distance) * facing * 7))
            {
                best = i;
                bestD = distance;
            }
        }
        if (best < 0)
            return false;
        AiMonster *m = aiOf(a, best);
        const AiKind *kind = aiKindOf(a.states[best].kind);
        if (m == nullptr || kind == nullptr || a.ai == nullptr)
            return true;
        AiCtx c = aiCtx(w, player, best, *m, *kind);
        if (m->maxHealth == 0)
            aiInit(c);
        talk::followerUse(c);
        return true;
    }
} // namespace hldata
