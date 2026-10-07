#pragma once

// The player on the console: the shared movement code (HlPlayerMove.hpp) fed
// from the DS's input, and the camera placed at its eyes.
//
// The movement itself is not here. It lives in shared/ so the host tests run
// it; this class only gathers what it needs each frame -- the keys, the view
// direction, how much time has passed -- and is the one translation unit that
// instantiates it for the ARM9 (PlayerMove.itcm.arm.cpp).
//
// TIME STEP. The move is run once per rendered frame with the real time that
// frame took, in whole vblanks. Half-Life's movement is written for a variable
// step, and stepping with the frame keeps the view smooth at 60 fps without
// interpolating between ticks; the split gravity step keeps a jump the same
// height at 30.

#include "HlPlayerMove.hpp"

namespace hlrt
{
    class Player
    {
    public:
        hldata::PlayerState state;
        /// Free flight through walls, for looking at a map. Toggled by Select.
        bool noclip = false;

        /// Stand at the loaded level's player start.
        void spawn();
        /// A development aid: the track of music debug_start.txt asks for, or -1.
        int debugTrack = -1;

        /// Turn the view, move (against Game::world), and put the camera at the
        /// eyes. `vblanks` is how many sixtieths of a second this frame covers.
        /// Called by Game::frame, after the entity logic has run.
        void frame(int vblanks);

    private:
        void turn();
        void fly(int vblanks);
    };

    extern Player player;
} // namespace hlrt
