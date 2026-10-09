#pragma once

// The controls, the way DS first-person shooters have them: the stylus on the
// touch screen turns the view, the D-pad moves. ABXY mirror the D-pad so a
// left-handed player can hold the stylus in the other hand.
//
// The touch screen is an aim pad with controls round it, where
// shared/HlTouchLayout.hpp says: MENU, USE and RELOAD in a bar along the top, JUMP
// and DUCK in a column beside the pad, a slot for each weapon along the
// bottom. Tapping the pad twice quickly jumps as well, as in the DS's own
// first-person shooters (and holding the second tap keeps the jump held,
// for swimming up); the drag of that second tap still turns the view. A
// touch that starts on a control is that control and never turns the view,
// however far it then slides. L fires.
//
// poll() is called once per frame and leaves the result in plain fields, so
// nothing else touches the hardware's input registers.

#include <cstdint>

#include "HlTouchLayout.hpp"

namespace hlrt
{
    class Input
    {
    public:
        int moveForward = 0; ///< -1, 0 or 1
        int moveSide = 0;    ///< -1 left, 1 right
        bool jump = false;
        bool duck = false;
        bool usePressed = false; ///< the USE button went down this frame
        bool attack = false;     ///< L is held: swing, or fire
        bool second = false;     ///< R is held: the weapon's other use, or reload
        /// A weapon's slot was touched this frame: that weapon (a kWeapon*),
        /// or 0.
        int weaponPick = 0;
        bool menuPressed = false; ///< the MENU tab was touched this frame
        bool reloadPressed = false; ///< ... RELOAD was
        /// What the stylus is on (a kTouch*; kTouchAim also when it is up):
        /// for whoever draws the controls.
        int touchOn = hldata::kTouchAim;
        /// The lamp's switch: its button on the touch screen, or Select.
        bool lampPressed = false;
        bool startPressed = false;  ///< Start went down this frame
        /// The stylus touched down this frame, at (tapX, tapY). Reported
        /// wherever it lands; what it means is the caller's business.
        bool tapped = false;
        int tapX = 0;
        int tapY = 0;
        int lookDx = 0;      ///< stylus travel since the last poll, in pixels
        int lookDy = 0;

        void poll();

        /// Two taps this close together -- a third of a second, forty pixels
        /// -- are a double tap.
        static constexpr std::uint32_t kDoubleTapBlanks = 20;
        static constexpr int kDoubleTapReach = 40;

    private:
        int touch_ = hldata::kTouchAim; ///< what this touch started on
        bool touching_ = false;
        bool jumping_ = false;            ///< this touch is the second of a double tap
        std::uint32_t lastTapBlank_ = 0;  ///< when the aim pad was last touched
        int lastTapX_ = -1000;
        int lastTapY_ = -1000;
        std::uint32_t lastHeld_ = 0;
        int lastX_ = 0;
        int lastY_ = 0;
    };

    extern Input input;
} // namespace hlrt
