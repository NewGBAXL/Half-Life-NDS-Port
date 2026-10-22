#pragma once

// The last picture of the game, kept on the top screen while a map loads.
//
// What the screen shows when nothing is being drawn is the 3D layer, and the
// hardware goes on drawing that out of texture memory: which loads write over.
// Capture the screen first into bank D, then display it in front of the 3D layer
// until the new map's first picture is ready.

namespace hlrt
{
    class FrameFreeze
    {
    public:
        /// Keep what is on the screen now. Call while it is still whole:
        /// before anything of the map is let go. False if there is nothing
        /// to keep (then the caller shows something else).
        bool freeze();
        /// Back to the live picture. (Behind the curtain: what the 3D layer
        /// shows at that moment is not yet the new map.)
        void thaw();
        bool frozen() const { return frozen_; }

    private:
        bool frozen_ = false;
        bool twoPass_ = false; ///< frozen in two-pass: nothing was captured, nothing to put back
        unsigned long savedControl_ = 0;
        unsigned short savedBg0_ = 0;
    };

    extern FrameFreeze frameFreeze;
} // namespace hlrt
