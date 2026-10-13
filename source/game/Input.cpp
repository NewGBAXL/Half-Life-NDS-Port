#include "game/Input.hpp"

#include <nds.h>

#include "Global.hpp"
#include "Settings.hpp"

namespace hlrt
{
    Input input;

    HL_ARM void Input::poll()
    {
        scanKeys();
        const std::uint32_t held = keysHeld();

        moveForward = ((held & (KEY_UP | KEY_X)) ? 1 : 0) - ((held & (KEY_DOWN | KEY_B)) ? 1 : 0);
        moveSide = ((held & (KEY_RIGHT | KEY_A)) ? 1 : 0) - ((held & (KEY_LEFT | KEY_Y)) ? 1 : 0);
        // Edges are worked out here, not taken from keysDown(): in two-pass
        // mode NEA scans the keys as well, and keysDown() only reports what
        // changed since the LAST scan, whoever made it.
        lampPressed = (held & ~lastHeld_ & KEY_SELECT) != 0;
        startPressed = (held & ~lastHeld_ & KEY_START) != 0;
        tapped = false;
        usePressed = false;
        weaponPick = 0;
        menuPressed = false;
        reloadPressed = false;
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
                // Where a touch STARTS decides what it is for its whole life.
                touch_ = hldata::touchHit(hldata::touchLayout(settings.leftHanded), touch.px, touch.py);
                // USE, MENU, RELOAD and a weapon's slot are presses, not holds: they
                // happen on the frame the stylus lands on them.
                usePressed = touch_ == hldata::kTouchUse;
                menuPressed = touch_ == hldata::kTouchMenu;
                reloadPressed = touch_ == hldata::kTouchReload;
                lampPressed = lampPressed || touch_ == hldata::kTouchLamp;
                if (touch_ >= hldata::kTouchSlot)
                    weaponPick = touch_ - hldata::kTouchSlot + 1;
                jumping_ = false;
                if (touch_ == hldata::kTouchAim)
                {
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
            else if (touch_ == hldata::kTouchAim)
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
            touch_ = hldata::kTouchAim;
            jumping_ = false;
        }

        // Jumping and crouching are on the touch screen; L is the hand that
        // holds the weapon, R its other use (the MP5's launcher; a reload).
        jump = jumping_ || touch_ == hldata::kTouchJump;
        duck = touch_ == hldata::kTouchDuck;
        touchOn = touching_ ? touch_ : hldata::kTouchAim;
        attack = (held & KEY_L) != 0;
        second = (held & KEY_R) != 0;
    }
} // namespace hlrt
