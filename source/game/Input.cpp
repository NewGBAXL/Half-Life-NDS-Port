#include "game/Input.hpp"

#include <nds.h>

#include "Global.hpp"

namespace hlrt
{
    Input input;

    void Input::poll()
    {
        scanKeys();
        const std::uint32_t held = keysHeld();

        moveForward = ((held & (KEY_UP | KEY_X)) ? 1 : 0) - ((held & (KEY_DOWN | KEY_B)) ? 1 : 0);
        moveSide = ((held & (KEY_RIGHT | KEY_A)) ? 1 : 0) - ((held & (KEY_LEFT | KEY_Y)) ? 1 : 0);
        // Edges are worked out here, not taken from keysDown(): in two-pass
        // mode NEA scans the keys as well, and keysDown() only reports what
        // changed since the LAST scan, whoever made it.
        noclipPressed = (held & ~lastHeld_ & KEY_SELECT) != 0;
        startPressed = (held & ~lastHeld_ & KEY_START) != 0;
        tapped = false;
        usePressed = false;
        weaponPressed = false;
        lastHeld_ = held;

        lookDx = 0;
        lookDy = 0;
        if (held & KEY_TOUCH)
        {
            touchPosition touch;
            touchRead(&touch);
            if (!touching_)
            {
                tapped = true;
                tapX = touch.px;
                tapY = touch.py;
                // USE is a press, not a hold: it happens on the frame the
                // stylus lands on the button.
                usePressed = touch.py >= kButtonTop && touch.px >= kUseLeft &&
                             touch.px < kUseLeft + kButtonWidth;
                weaponPressed = touch.px >= kWeaponLeft && touch.py >= kWeaponTop &&
                                touch.py < kWeaponBottom;
                // Where a touch STARTS decides what it is for its whole life.
                jumping_ = false;
                if (weaponPressed)
                    touch_ = Touch::Weapon;
                else if (touch.py >= kButtonTop && touch.px >= 256 - kButtonWidth)
                    touch_ = Touch::Duck;
                else if (touch.py >= kButtonTop && touch.px >= kUseLeft &&
                         touch.px < kUseLeft + kButtonWidth)
                    touch_ = Touch::Use;
                else
                {
                    touch_ = Touch::Aim;
                    // The second of two taps close together, in time and in
                    // place, is a jump.
                    const std::uint32_t now = Global::vblanks();
                    const int dx = touch.px - lastTapX_, dy = touch.py - lastTapY_;
                    jumping_ = now - lastTapBlank_ <= kDoubleTapBlanks && dx > -kDoubleTapReach &&
                               dx < kDoubleTapReach && dy > -kDoubleTapReach && dy < kDoubleTapReach;
                    // (A third tap is not a second double tap with the
                    // second.)
                    lastTapBlank_ = jumping_ ? now - 1000 : now;
                    lastTapX_ = touch.px;
                    lastTapY_ = touch.py;
                }
            }
            else if (touch_ == Touch::Aim)
            {
                // The first frame of a touch only set the reference point:
                // otherwise putting the stylus down would jerk the view by
                // however far it landed from where it last lifted.
                lookDx = touch.px - lastX_;
                lookDy = touch.py - lastY_;
            }
            lastX_ = touch.px;
            lastY_ = touch.py;
            touching_ = true;
        }
        else
        {
            touching_ = false;
            touch_ = Touch::None;
            jumping_ = false;
        }

        // Jumping and crouching are on the touch screen; L is the hand that
        // holds the weapon. R does nothing yet.
        jump = jumping_;
        duck = touch_ == Touch::Duck;
        attack = (held & KEY_L) != 0;
    }
} // namespace hlrt
