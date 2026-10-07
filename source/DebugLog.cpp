#include "DebugLog.hpp"

#ifdef HLDS_PROFILE

#include <nds.h>

namespace hlrt::DebugLog::Perf
{
    namespace
    {
        constexpr int kCount = static_cast<int>(Section::Count);
        std::uint32_t running[kCount];
        std::uint32_t latched[kCount];
    } // namespace

    void begin()
    {
        cpuStartTiming(0);
    }

    std::uint32_t now()
    {
        return cpuGetTiming();
    }

    void add(Section section, std::uint32_t ticks)
    {
        running[static_cast<int>(section)] += ticks;
    }

    std::uint32_t micros(Section section)
    {
        // The timer counts the 33.5 MHz bus clock.
        return (latched[static_cast<int>(section)] * 2u) / 67u;
    }

    void endFrame()
    {
        for (int i = 0; i < kCount; ++i)
        {
            latched[i] = running[i];
            running[i] = 0;
        }
    }
} // namespace hlrt::DebugLog::Perf

#endif
