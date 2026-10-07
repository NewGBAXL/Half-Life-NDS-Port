#pragma once

// The controls, the way DS first-person shooters have them: the stylus on the
// touch screen turns the view, the D-pad moves. ABXY mirror the D-pad so a
// left-handed player can hold the stylus in the other hand.
//
// The touch screen is one large aim pad. Tapping it twice quickly jumps, as
// in the DS's own first-person shooters (and holding the second tap keeps
// the jump held, for swimming up); the drag of that second tap still turns
// the view. Along the bottom are USE in the middle and DUCK on the right,
// with WEAPON above DUCK. A touch that starts on a button is a button press
// and never turns the view, however far it then slides. L fires.
//
// poll() is called once per frame and leaves the result in plain fields, so
// nothing else touches the hardware's input registers.

#include <cstdint>

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
        bool weaponPressed = false; ///< the WEAPON button went down this frame
        bool noclipPressed = false; ///< Select went down this frame
        bool startPressed = false;  ///< Start went down this frame
        /// The stylus touched down this frame, at (tapX, tapY). Reported
        /// wherever it lands; what it means is the caller's business.
        bool tapped = false;
        int tapX = 0;
        int tapY = 0;
        int lookDx = 0;      ///< stylus travel since the last poll, in pixels
        int lookDy = 0;

        void poll();

        // The buttons' rectangles on the touch screen, shared with the code
        // that draws them (BottomScreen).
        /// Two taps this close together -- a third of a second, forty pixels
        /// -- are a double tap.
        static constexpr std::uint32_t kDoubleTapBlanks = 20;
        static constexpr int kDoubleTapReach = 40;
        static constexpr int kButtonWidth = 64;
        static constexpr int kButtonTop = 152;
        static constexpr int kUseLeft = 96; ///< the USE button: kButtonWidth wide from here
        // The WEAPON button, on the right above DUCK: the next weapon.
        static constexpr int kWeaponLeft = 192;
        static constexpr int kWeaponTop = 88;
        static constexpr int kWeaponBottom = 128;

    private:
        enum class Touch
        {
            None,
            Aim,
            Duck,
            Use,
            Weapon,
        };

        Touch touch_ = Touch::None;
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
