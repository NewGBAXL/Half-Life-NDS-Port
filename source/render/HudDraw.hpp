#pragma once

// The head-up display, over the 3D picture as the game has it: health and
// armour at the bottom left, what is in the gun and what is carried for it
// at the bottom right, a crosshair in the middle. And the picture shown
// while a map loads.
//
// Both are the game's own pictures, cooked to the card (shared/HlUi.hpp); a
// card without them has neither. They are drawn as flat quads a little way
// in front of the eye, through the same projection as the world: that is
// right whatever the view, and nearer than anything else can be.

#include <cstdint>

namespace hlrt
{
    class HudDraw
    {
    public:
        /// What it costs a frame, at most: taken off the world's budget.
        static constexpr int kPolygons = 24;
        static constexpr int kVertices = 96;
        /// ... and what the lamp's light may cost besides, while it is
        /// alight: the surfaces in its beam are drawn twice.
        static constexpr int kLampPolygons = 120;
        static constexpr int kLampVertices = 480;
        bool lampAlight() const { return lampOn_; }

        /// Load the display's texture. After the 3D engine is brought up
        /// (and again each time it is: forget() first).
        void load();
        void forget();

        /// What to show. `suit`: the player has one (without it, nothing).
        /// `weapon` is a kWeapon*; `extra` what it has besides, or -1.
        void setStatus(bool suit, int health, int armor, int weapon, int clip, int spare, int extra);
        /// The lamp: alight or not, and how much is in it (0..100).
        void setLamp(bool on, int percent, bool suit = true)
        {
            lampOn_ = on;
            lampPercent_ = percent;
            lampSuit_ = suit;
        }
        /// Being hurt: how red the screen is (0..256) and how strongly each
        /// of the four marks round its middle shows where from (front,
        /// right, back, left).
        void setHurt(int flash, const int *pain)
        {
            flash_ = flash;
            for (int i = 0; i < 4; ++i)
                pain_[i] = pain[i];
        }
        /// Last thing in a pass.
        void draw();
        /// The marks' texture (an NEA_Material: HlUi.hpp, kDecalAtlasFile),
        /// or null on a card that has not got it.
        void *decalMaterial() const { return decalMaterial_; }
        /// The lamp's picture of light (an NEA_Material, kLampTexels a
        /// side), which the world's lamp pass projects; null if it could
        /// not be made.
        void *lampMaterial() const { return lampMaterial_; }
        static constexpr int kLampTexels = 32;

        /// Put the loading picture on the top screen and leave it there:
        /// returns once it is what the screen shows. False if the card has
        /// not got it (nothing is changed). hideLoading() when the map is in.
        bool showLoading();
        void hideLoading();

    private:
        void *material_ = nullptr; ///< NEA_Material
        void *lampMaterial_ = nullptr;
        void *decalMaterial_ = nullptr;
        void *decalPalette_ = nullptr;
        void *palette_ = nullptr;
        void *loadingMaterial_ = nullptr;
        void *loadingPalette_ = nullptr;
        bool suit_ = false;
        bool lampOn_ = false;
        bool lampSuit_ = false; ///< the suit is worn: its lamp is shown, alight or not
        int flash_ = 0;
        int pain_[4] = {0, 0, 0, 0};
        int lampPercent_ = 100;
        int health_ = 100, armor_ = 0, weapon_ = 0, clip_ = 0, spare_ = 0, extra_ = -1;
    };

    extern HudDraw hudDraw;
} // namespace hlrt
