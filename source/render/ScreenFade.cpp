#include "global_header.hpp"

#include "render/ScreenFade.hpp"

namespace hlrt
{
    ScreenFade screenFade;

    void ScreenFade::apply()
    {
        const int dim = dim_ ? 7 : 0;
        const int level = curtain_ ? 16 : (fade_ > dim ? fade_ : dim);
        if (level == shown_)
            return;
        shown_ = level;
        setBrightness(1, -level);
    }

    void ScreenFade::setFade(int level)
    {
        fade_ = level < 0 ? 0 : (level > 16 ? 16 : level);
        apply();
    }

    void ScreenFade::setDim(bool under)
    {
        dim_ = under;
        apply();
    }

    void ScreenFade::curtainDown()
    {
        // Between two pictures, not part of the way down one: the top of
        // the old picture over black is a tear, if only for a frame.
        if (!curtain_)
            swiWaitForVBlank();
        curtain_ = true;
        wait_ = 0;
        apply();
    }

    void ScreenFade::curtainUp(int frames)
    {
        wait_ = frames;
        if (frames <= 0)
        {
            if (curtain_)
                swiWaitForVBlank(); // (as it came down: between two pictures)
            curtain_ = false;
            apply();
        }
    }

    void ScreenFade::tick()
    {
        if (!curtain_ || wait_ <= 0 || --wait_ > 0)
            return;
        curtain_ = false;
        apply();
    }
} // namespace hlrt
