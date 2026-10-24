#pragma once

// Which polygon ID each see-through thing is drawn under (6 bits; 0 is what
// is solid). The hardware does not blend a see-through pixel over one of the
// same ID: two things drawn one over the other must have different ones, and
// one thing's own faces the same one -- a glass box's back does not darken
// its front. The world, the effects, the models and the head-up display (in
// two-pass, a 3D part of the same frame) all take theirs from here.

namespace hldata
{
    /// See-through brushes and effects, far to near, round and round.
    inline constexpr int kIdSeeThroughFirst = 1;
    inline constexpr int kIdSeeThroughCount = 47; ///< 1..47
    /// Models fading away (ModelDraw), round and round: two overlapping
    /// show through each other.
    inline constexpr int kIdFadeFirst = 48;
    inline constexpr int kIdFadeCount = 4; ///< 48..51
    inline constexpr int kIdMarks = 52;    ///< marks on the walls (EffectDraw)
    inline constexpr int kIdHudHurt = 53;  ///< the kinds of hurt, pulsing
    inline constexpr int kIdHudPainFirst = 54; ///< the marks of where a hurt came from: 54..57
    inline constexpr int kIdHudFlash = 58;     ///< the red of being hurt
    inline constexpr int kIdLamp = 62;         ///< the lamp's light on the world
    inline constexpr int kIdHudSuit = 63;      ///< the display's pictures

    static_assert(kIdSeeThroughFirst + kIdSeeThroughCount <= kIdFadeFirst);
    static_assert(kIdFadeFirst + kIdFadeCount <= kIdMarks);
    static_assert(kIdMarks < kIdHudHurt && kIdHudHurt < kIdHudPainFirst);
    static_assert(kIdHudPainFirst + 4 <= kIdHudFlash);
    static_assert(kIdHudFlash < kIdLamp && kIdLamp < kIdHudSuit && kIdHudSuit <= 63);
} // namespace hldata
