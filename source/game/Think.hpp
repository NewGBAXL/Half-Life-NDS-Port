#pragma once

// The actors' long thinking outside the frame (shared/HlThink.hpp): a worker
// of the 3D engine's (NEAThread.h) that answers their questions while the
// frame waits for the screen, stopping now and then so as not to hold the
// next frame up. Until start() it is not there, and every question is
// answered as it is asked, as before.

#include "HlActors.hpp"
#include "HlLogic.hpp"

namespace hlrt
{
    class Think
    {
    public:
        /// Once, after the 3D engine is up: the worker.
        void start();
        /// A map is set up: its actors ask, the worker answers.
        void attach(hldata::LogicWorld &w);
        /// The map is being left: what is being worked out is finished at
        /// once and dropped, and nothing more is asked of the worker.
        void detach();
        bool running() const { return started_; }

    private:
        bool started_ = false;
    };

    extern Think think;
} // namespace hlrt
