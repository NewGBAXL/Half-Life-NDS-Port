#include "global_header.hpp"

#include "game/Think.hpp"

#include <cstdlib>

#include <nds.h>
#include <nds/cothread.h>

#include <NEAMain.h>
#include <NEAThread.h>

#include "HlThink.hpp"

namespace hlrt
{
    Think think;

    namespace
    {
        hldata::ThinkQueue queue;
        hldata::LogicWorld *world = nullptr; // the map's, while there is one
        volatile bool busy = false;          // a question is being worked out
        cothread_t mainThread = -1;
        NEA_Task *task = nullptr;

        // Where long work stops for the frame: only the worker, and not
        // while a map is being left (then it is finished without stopping).
        void pause()
        {
            if (!queue.abort && cothread_get_current() != mainThread)
                NEA_TaskYield();
        }

        // The worker: a question at a time while there are any; otherwise
        // asleep until the next frame's wait.
        int work(void *)
        {
            while (!NEA_TaskShouldStop())
            {
                if (world != nullptr && !queue.abort)
                {
                    busy = true;
                    const bool did = hldata::thinkRunOne(*world);
                    busy = false;
                    if (did)
                    {
                        NEA_TaskYield();
                        continue;
                    }
                }
                cothread_yield_irq(IRQ_VBLANK);
            }
            return 0;
        }
    } // namespace

    void Think::start()
    {
        if (started_)
            return;
        // One worker: what it does is all reckoning, nothing from the card.
        // (A quarter of a frame at most, of the time the frame leaves.)
        if (NEA_ThreadSystemReset(1, 16 * 1024) != 0)
            return;
        NEA_ThreadSetFrameBudget(64);
        mainThread = cothread_get_current();
        hldata::thinkYield = pause;
        task = NEA_TaskSubmit(work, nullptr, nullptr);
        started_ = task != nullptr;
    }

    void Think::attach(hldata::LogicWorld &w)
    {
        detach();
        if (w.actors == nullptr || w.actors->count <= 0)
            return;
        queue.slots = static_cast<hldata::ThinkSlot *>(
            std::calloc(static_cast<std::size_t>(w.actors->count), sizeof(hldata::ThinkSlot)));
        if (queue.slots == nullptr)
            return;
        queue.count = w.actors->count;
        hldata::thinkClear(queue);
        queue.deferred = started_;
        queue.abort = false;
        w.actors->think = &queue;
        world = &w;
    }

    void Think::detach()
    {
        // (Whatever is half worked out is finished now -- it no longer stops
        // for the frame -- before what it reads goes.)
        queue.abort = true;
        while (busy)
            cothread_yield();
        if (world != nullptr && world->actors != nullptr)
            world->actors->think = nullptr;
        world = nullptr;
        std::free(queue.slots);
        queue.slots = nullptr;
        queue.count = 0;
        queue.deferred = false;
        queue.abort = false;
    }
} // namespace hlrt
