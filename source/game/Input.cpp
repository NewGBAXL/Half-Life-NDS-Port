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

    // The stylus finer than a pixel, by the firmware's own calibration (two
    // points, raw and on the screen, for each axis); and how hard it is
    // pressed, as the touch controller's datasheet has it: the plate's
    // resistance is the raw X times (Z2 / Z1 - 1) -- the firmer, the less.
    // A reading the calibration cannot be trusted for (a DSi's own touch
    // mode reads otherwise), or that has no Z, is left at what it was: a
    // pixel, and no pressure.
    void Input::fine(int rawX, int rawY, int z1, int z2, hldata::ControlsIn &in)
    {
        const PERSONAL_DATA &data = *PersonalData;
        const int spanX = data.calX2 - data.calX1, spanY = data.calY2 - data.calY1;
        const int pxX = data.calX2px - data.calX1px, pxY = data.calY2px - data.calY1px;
        if (spanX != 0 && spanY != 0 && pxX != 0 && pxY != 0)
        {
            const int fx = ((rawX - data.calX1) * pxX * 16) / spanX + (data.calX1px - 1) * 16;
            const int fy = ((rawY - data.calY1) * pxY * 16) / spanY + (data.calY1px - 1) * 16;
            // (Within half a dozen pixels of where the console says it is.)
            if (fx >= 0 && fy >= 0 && fx / 16 - in.touchX < 6 && in.touchX - fx / 16 < 6 &&
                fy / 16 - in.touchY < 6 && in.touchY - fy / 16 < 6)
            {
                in.fineX = fx;
                in.fineY = fy;
            }
        }
        notePressure(z1, z2);
        if (z1 > 0 && z2 > z1 && rawX > 0)
        {
            const int pressure = static_cast<int>((static_cast<std::int64_t>(rawX) * (z2 - z1)) / z1);
            in.pressure = pressure > 0 ? pressure : 1;
        }
    }

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
            fine(touch.rawx, touch.rawy, touch.z1, touch.z2, in);
        }
        static_cast<hldata::ControlsOut &>(*this) = hldata::controlsStep(settings.controls(), in, state_);
    }
} // namespace hlrt
