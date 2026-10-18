#pragma once

// What the buttons and the stylus mean: from what is held this frame to what
// the player is doing.
//
// Two layouts, chosen in the options.
//
// CLASSIC is the way DS first-person shooters have it: the stylus turns the
// view, the D-pad moves, and A B X Y do what the D-pad does, for whichever
// thumb is free. One shoulder button fires, the other is the weapon's second
// use. Everything else is on the touch screen (HlTouchLayout.hpp).
//
// MODERN keeps the D-pad and the shoulders and gives the four face buttons
// work of their own: the top one the weapon before, the left one the weapon
// after, the bottom one a reload, the right one a jump. With the second
// shoulder button held, down on the pad crouches -- and stays crouched --
// and up stands. One short tap of the stylus on the aim pad is USE.
//
// Either way, two quick taps on the aim pad jump (and holding the second
// keeps the jump held, for swimming up); a touch that starts on a control is
// that control and never turns the view, however far it then slides.
//
// THE HAND. "Pad" above is the D-pad for someone with the stylus in their
// right hand; with it in the left, the pad is A B X Y -- by where they are:
// X up, B down, Y left, A right -- the D-pad has the face buttons' work, and
// the shoulders change places: the hand that holds the console is the one
// that moves and fires.
//
// THE STICKS. The pad and the stylus are two sticks, and which does what can
// be changed an axis at a time: the pad's left and right can turn the view
// while the stylus's sideways travel sidesteps, and its up and down can look
// while the stylus's moves. A stylus that moves is a stick: how far it is
// from where it came down, past a dead zone, is the direction.
//
// std-only and inline: the host tests run it (test_controls).

#include <cstdint>

#include "HlTouchLayout.hpp"

namespace hldata
{
    // The buttons, as the console's key register has them.
    inline constexpr std::uint32_t kButtonA = 1u << 0;
    inline constexpr std::uint32_t kButtonB = 1u << 1;
    inline constexpr std::uint32_t kButtonSelect = 1u << 2;
    inline constexpr std::uint32_t kButtonStart = 1u << 3;
    inline constexpr std::uint32_t kButtonRight = 1u << 4;
    inline constexpr std::uint32_t kButtonLeft = 1u << 5;
    inline constexpr std::uint32_t kButtonUp = 1u << 6;
    inline constexpr std::uint32_t kButtonDown = 1u << 7;
    inline constexpr std::uint32_t kButtonR = 1u << 8;
    inline constexpr std::uint32_t kButtonL = 1u << 9;
    inline constexpr std::uint32_t kButtonX = 1u << 10;
    inline constexpr std::uint32_t kButtonY = 1u << 11;

    inline constexpr std::uint8_t kControlsClassic = 0;
    inline constexpr std::uint8_t kControlsModern = 1;
    inline constexpr std::uint8_t kControlsSchemes = 2;

    struct ControlSettings
    {
        std::uint8_t scheme = kControlsClassic;
        bool leftHanded = false;
        bool padTurns = false; ///< the pad's left and right turn; the stylus sidesteps
        bool padLooks = false; ///< the pad's up and down look; the stylus moves
    };

    /// Two taps this close together -- a third of a second, forty pixels --
    /// are a double tap.
    inline constexpr std::uint32_t kDoubleTapBlanks = 20;
    inline constexpr int kDoubleTapReach = 40;
    /// A touch lifted this soon (a fifth of a second), having gone no
    /// further than this, was a tap.
    inline constexpr std::uint32_t kTapBlanks = 12;
    inline constexpr int kTapReach = 6;
    /// The stylus as a stick: nearer than this to where it came down, it
    /// is at rest.
    inline constexpr int kStickDeadZone = 10;
    /// The pad as the stick that turns: as much, each sixtieth of a second,
    /// as the stylus dragged this many pixels (three is about 125 degrees a
    /// second), and as much up and down.
    inline constexpr int kPadTurnPixels = 3;
    inline constexpr int kPadLookPixels = 2;

    /// What is held this frame.
    struct ControlsIn
    {
        std::uint32_t held = 0; ///< kButton*
        bool touching = false;
        int touchX = 0, touchY = 0;
        std::uint32_t blank = 0; ///< the count of sixtieths of a second so far
    };

    /// What that means.
    struct ControlsOut
    {
        int moveForward = 0; ///< -1, 0 or 1
        int moveSide = 0;    ///< -1 left, 1 right
        bool jump = false;
        bool duck = false;
        bool usePressed = false;
        bool attack = false;
        bool second = false;
        int weaponPick = 0; ///< a slot was touched: its kind of weapon (1..5: weaponOfCategory), or 0
        int weaponStep = 0; ///< the weapon before (-1) or after (1) the one in hand
        bool menuPressed = false;
        bool reloadPressed = false;
        int touchOn = kTouchAim;
        bool lampPressed = false;
        bool startPressed = false;
        bool tapped = false; ///< the stylus came down this frame, at (tapX, tapY)
        int tapX = 0, tapY = 0;
        int lookDx = 0; ///< how far to turn, as pixels of stylus travel
        int lookDy = 0;
    };

    /// What has to be remembered from one frame to the next.
    struct ControlsState
    {
        std::uint32_t lastHeld = 0;
        std::uint32_t lastBlank = 0;
        int touch = kTouchAim; ///< what this touch started on
        bool touching = false;
        bool jumping = false; ///< this touch is the second of a double tap
        bool strayed = false; ///< ... has gone too far to be a tap
        std::uint32_t downBlank = 0;
        int downX = 0, downY = 0;
        std::uint32_t lastTapBlank = 0; ///< when the aim pad was last touched
        int lastTapX = -1000, lastTapY = -1000;
        int lastX = 0, lastY = 0;
        bool crouched = false; ///< MODERN: crouching until told to stand
    };

    /// One frame of the controls.
    inline ControlsOut controlsStep(const ControlSettings &set, const ControlsIn &in, ControlsState &state)
    {
        ControlsOut out;
        const std::uint32_t held = in.held;
        const std::uint32_t pressed = held & ~state.lastHeld;
        const bool modern = set.scheme == kControlsModern;

        // Which buttons are the pad, which the four with work of their own,
        // and which shoulder fires.
        const std::uint32_t padUp = set.leftHanded ? kButtonX : kButtonUp;
        const std::uint32_t padDown = set.leftHanded ? kButtonB : kButtonDown;
        const std::uint32_t padLeft = set.leftHanded ? kButtonY : kButtonLeft;
        const std::uint32_t padRight = set.leftHanded ? kButtonA : kButtonRight;
        const std::uint32_t faceTop = set.leftHanded ? kButtonUp : kButtonX;
        const std::uint32_t faceBottom = set.leftHanded ? kButtonDown : kButtonB;
        const std::uint32_t faceLeft = set.leftHanded ? kButtonLeft : kButtonY;
        const std::uint32_t faceRight = set.leftHanded ? kButtonRight : kButtonA;
        const std::uint32_t trigger = set.leftHanded ? kButtonR : kButtonL;
        const std::uint32_t other = set.leftHanded ? kButtonL : kButtonR;

        // (CLASSIC: the face buttons are a second pad.)
        const std::uint32_t up = modern ? padUp : (padUp | faceTop);
        const std::uint32_t down = modern ? padDown : (padDown | faceBottom);
        const std::uint32_t left = modern ? padLeft : (padLeft | faceLeft);
        const std::uint32_t right = modern ? padRight : (padRight | faceRight);

        out.attack = (held & trigger) != 0;
        out.second = (held & other) != 0;
        // Edges are worked out here, from what was held last frame.
        out.lampPressed = (pressed & kButtonSelect) != 0;
        out.startPressed = (pressed & kButtonStart) != 0;

        int padForward = ((held & up) ? 1 : 0) - ((held & down) ? 1 : 0);
        const int padSide = ((held & right) ? 1 : 0) - ((held & left) ? 1 : 0);
        bool faceJump = false;
        if (modern)
        {
            if (pressed & faceTop)
                out.weaponStep = -1;
            else if (pressed & faceLeft)
                out.weaponStep = 1;
            out.reloadPressed = (pressed & faceBottom) != 0;
            faceJump = (held & faceRight) != 0;
            // With the second shoulder held, the pad's up and down are how
            // the player stands, and not a step.
            if (held & other)
            {
                if (pressed & padDown)
                    state.crouched = true;
                if (pressed & padUp)
                    state.crouched = false;
                padForward = 0;
            }
        }
        else
        {
            state.crouched = false;
        }

        // The stylus.
        int dragX = 0, dragY = 0;   // its travel this frame, on the aim pad
        int stickX = 0, stickY = 0; // ... and how far it is from where it came down
        if (in.touching)
        {
            if (!state.touching)
            {
                out.tapped = true;
                out.tapX = in.touchX;
                out.tapY = in.touchY;
                // Where a touch STARTS decides what it is for its whole life.
                state.touch = touchHit(touchLayout(set.leftHanded), in.touchX, in.touchY);
                // USE, MENU, RELOAD and a weapon's slot are presses, not
                // holds: they happen on the frame the stylus lands on them.
                out.usePressed = state.touch == kTouchUse;
                out.menuPressed = state.touch == kTouchMenu;
                out.reloadPressed = out.reloadPressed || state.touch == kTouchReload;
                out.lampPressed = out.lampPressed || state.touch == kTouchLamp;
                if (state.touch >= kTouchSlot)
                    out.weaponPick = state.touch - kTouchSlot + 1;
                // (The button on the screen is held to crouch; touched while
                // crouching by the other way, it is the way to stand.)
                if (state.touch == kTouchDuck)
                    state.crouched = false;
                state.jumping = false;
                state.strayed = false;
                state.downBlank = in.blank;
                state.downX = in.touchX;
                state.downY = in.touchY;
                if (state.touch == kTouchAim)
                {
                    // The second of two taps close together, in time and in
                    // place, is a jump.
                    const int dx = in.touchX - state.lastTapX, dy = in.touchY - state.lastTapY;
                    state.jumping = in.blank - state.lastTapBlank <= kDoubleTapBlanks && dx > -kDoubleTapReach &&
                                    dx < kDoubleTapReach && dy > -kDoubleTapReach && dy < kDoubleTapReach;
                    // (A third tap is not a second double tap with the
                    // second.)
                    state.lastTapBlank = state.jumping ? in.blank - 1000 : in.blank;
                    state.lastTapX = in.touchX;
                    state.lastTapY = in.touchY;
                }
            }
            else if (state.touch == kTouchAim)
            {
                // The first frame of a touch only set the reference point:
                // otherwise putting the stylus down would jerk the view by
                // however far it landed from where it last lifted.
                dragX = in.touchX - state.lastX;
                dragY = in.touchY - state.lastY;
            }
            if (state.touch == kTouchAim)
            {
                stickX = in.touchX - state.downX;
                stickY = in.touchY - state.downY;
                if (stickX <= -kTapReach || stickX >= kTapReach || stickY <= -kTapReach || stickY >= kTapReach)
                    state.strayed = true;
            }
            state.lastX = in.touchX;
            state.lastY = in.touchY;
            state.touching = true;
        }
        else
        {
            // Lifted: a touch of the aim pad that was short and went nowhere
            // was a tap, and in the MODERN layout a tap is USE. (Not the
            // second of two: that one was a jump.)
            if (modern && state.touching && state.touch == kTouchAim && !state.jumping && !state.strayed &&
                in.blank - state.downBlank <= kTapBlanks)
                out.usePressed = true;
            state.touching = false;
            state.touch = kTouchAim;
            state.jumping = false;
        }

        // The two sticks, an axis at a time.
        std::uint32_t blanks = in.blank - state.lastBlank;
        blanks = blanks < 1 ? 1 : (blanks > 4 ? 4 : blanks);
        if (set.padTurns)
        {
            out.moveSide = stickX >= kStickDeadZone ? 1 : (stickX <= -kStickDeadZone ? -1 : 0);
            out.lookDx = padSide * kPadTurnPixels * static_cast<int>(blanks);
        }
        else
        {
            out.moveSide = padSide;
            out.lookDx = dragX;
        }
        if (set.padLooks)
        {
            // (Up the screen is forward; and the pad's up looks up, as a
            // stylus pushed up does.)
            out.moveForward = stickY <= -kStickDeadZone ? 1 : (stickY >= kStickDeadZone ? -1 : 0);
            out.lookDy = -padForward * kPadLookPixels * static_cast<int>(blanks);
        }
        else
        {
            out.moveForward = padForward;
            out.lookDy = dragY;
        }

        out.jump = state.jumping || state.touch == kTouchJump || faceJump;
        out.duck = state.touch == kTouchDuck || state.crouched;
        out.touchOn = state.touching ? state.touch : kTouchAim;
        state.lastHeld = held;
        state.lastBlank = in.blank;
        return out;
    }

    /// What each button does, in a few short lines, for the options to
    /// show: `line` from 0; null past the last.
    inline const char *controlsLegend(const ControlSettings &set, int line)
    {
        const bool modern = set.scheme == kControlsModern;
        if (!modern)
        {
            switch (line)
            {
            case 0: return set.leftHanded ? "R fire    L second fire" : "L fire    R second fire";
            case 1: return "A B X Y as the D-pad";
            case 2: return "double tap: jump";
            default: return nullptr;
            }
        }
        if (!set.leftHanded)
        {
            switch (line)
            {
            case 0: return "L fire    R second fire";
            case 1: return "X Y weapon   B reload   A jump";
            case 2: return "R + down crouch   R + up stand";
            case 3: return "tap: use   double tap: jump";
            default: return nullptr;
            }
        }
        switch (line)
        {
        case 0: return "R fire    L second fire";
        case 1: return "A B X Y move   up/left weapon";
        case 2: return "down reload   right jump";
        case 3: return "L + B crouch   L + X stand";
        case 4: return "tap: use   double tap: jump";
        default: return nullptr;
        }
    }
} // namespace hldata
