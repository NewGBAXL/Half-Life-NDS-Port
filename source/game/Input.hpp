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
        /// Nothing is held (the menu is up over a game that goes on).
        void quiet() { static_cast<hldata::ControlsOut &>(*this) = hldata::ControlsOut(); }
        /// Whether this console has said how hard the stylus presses (some
        /// do not: a DSi in its own touch mode, an emulator). Seen once, it
        /// is known.
        static bool pressureSeen() { return pressureSeen_; }
        static void notePressure(int z1, int z2)
        {
            if (z1 > 0 && z2 > z1)
                pressureSeen_ = true;
        }

    private:
        static void fine(int rawX, int rawY, int z1, int z2, hldata::ControlsIn &in);
        hldata::ControlsState state_;
        static inline bool pressureSeen_ = false;
    };

    extern Input input;
} // namespace hlrt
