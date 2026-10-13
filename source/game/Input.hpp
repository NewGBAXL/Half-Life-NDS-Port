#pragma once

// The controls: what the buttons and the stylus mean is in
// shared/HlControls.hpp (two layouts, either hand, and which of the pad and
// the stylus turns and which moves); where things are on the touch screen is
// in shared/HlTouchLayout.hpp. This reads the hardware and asks them.
//
// poll() is called once per frame and leaves the result in plain fields, so
// nothing else touches the hardware's input registers.

#include <cstdint>

#include "HlControls.hpp"

namespace hlrt
{
    class Input : public hldata::ControlsOut
    {
    public:
        void poll();

    private:
        hldata::ControlsState state_;
    };

    extern Input input;
} // namespace hlrt
