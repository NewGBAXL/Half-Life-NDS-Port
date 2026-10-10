#pragma once

// The last picture of the game, kept on the top screen while a map loads.
//
// What the screen shows when nothing is being drawn depends on how the world
// is drawn. In the two-pass DMA mode it is a finished picture in main memory
// and stays as it is. In the other two the 3D layer is on the screen, all of
// it or half, and the hardware goes on drawing that out of texture memory:
// which a load writes over. So there the screen is captured first -- into
// bank D, which single-pass keeps free for it (Global::InitVideo), or into
// one of the two capture banks plain two-pass has anyway -- and the capture
// is shown in front of the 3D layer until the new map's first picture is
// ready.

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
        void hideSprites();
        bool frozen_ = false;
        unsigned long sprites_ = 0; ///< the sprite layer's bit, if it was on
        bool layered_ = false; ///< single-pass: the capture is on a layer of its own
        unsigned long savedControl_ = 0;
        unsigned short savedBg0_ = 0;
    };

    extern FrameFreeze frameFreeze;
} // namespace hlrt
