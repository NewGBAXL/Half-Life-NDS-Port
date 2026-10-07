#pragma once

// The hull trace, as the copy of it that runs from fast memory.

#include "HlMovers.hpp"

namespace hlrt
{
    /// hldata::traceAll, from ITCM. Game puts this in MoveWorld::trace, so
    /// that the player's movement -- which is not in fast memory -- traces
    /// through it.
    hldata::Trace traceFast(const hldata::MoveWorld &world, bool crouched, const hldata::fx *start,
                            const hldata::fx *end);
} // namespace hlrt
