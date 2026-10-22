#pragma once

#include "EngineObject.hpp"

// The top screen's brightness, which three things want a say in: a fade the
// map has asked for, the dimness of being under water, and the curtain that
// is down while a map is loaded (the 3D picture left on the screen is drawn
// again every frame out of video memory, and video memory is being written
// over). One owner, so that none of them undoes another: the darkest wins.

namespace hlrt
{
    class ScreenFade
    {
    public:
        /// How far the map has the screen faded: 0 (not at all) to 16 (black).
        void setFade(int level);
        void setDim(bool under);
        /// Down: black, whatever else. `frames`, when it is let up: how
        /// many more pictures are to be handed to the hardware first (see
        /// tick()), so that the first thing seen is a whole new one.
        void curtainDown();
        void curtainUp(int frames = 0);
        /// Once for each picture handed to the hardware.
        void tick();
        bool curtained() const { return curtain_; }

    private:
        int fade_ = 0;
        bool dim_ = false;
        bool curtain_ = false;
        int wait_ = 0;
        int shown_ = 0;
        void apply();
    };

    HL_ENGINE_OBJECT(ScreenFade, screenFade);
} // namespace hlrt
