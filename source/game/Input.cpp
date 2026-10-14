#include "game/Input.hpp"

#include <nds.h>

#include "Global.hpp"
#include "Settings.hpp"

namespace hlrt
{
    Input input;

    // (HlControls.hpp names the buttons by the bits of the key register.)
    static_assert(hldata::kButtonA == KEY_A && hldata::kButtonB == KEY_B && hldata::kButtonX == KEY_X &&
                  hldata::kButtonY == KEY_Y && hldata::kButtonL == KEY_L && hldata::kButtonR == KEY_R &&
                  hldata::kButtonUp == KEY_UP && hldata::kButtonDown == KEY_DOWN &&
                  hldata::kButtonLeft == KEY_LEFT && hldata::kButtonRight == KEY_RIGHT &&
                  hldata::kButtonSelect == KEY_SELECT && hldata::kButtonStart == KEY_START);

    HL_ARM void Input::poll()
    {
        scanKeys();
        // What is HELD, and never keysDown():
        // keysDown() only reports what changed since the
        // LAST scan, whoever made it. The edges are worked out from this.
        const std::uint32_t held = keysHeld();
        hldata::ControlsIn in;
        in.held = held;
        in.blank = Global::vblanks();
        if (held & KEY_TOUCH)
        {
            touchPosition touch;
            touchRead(&touch);
            in.touching = true;
            in.touchX = touch.px;
            in.touchY = touch.py;
        }
        static_cast<hldata::ControlsOut &>(*this) = hldata::controlsStep(settings.controls(), in, state_);
    }
} // namespace hlrt
