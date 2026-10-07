// Copies of the trace of this file's own: see HL_HOT in HlFixed.hpp.
#define HL_HOT [[maybe_unused]] static

#include "game/Trace.hpp"

// IN ITCM, AS ARM CODE (the file name says both): the hull trace, and nothing
// else. Everything that moves asks "how far can this box go from here to
// there" many times a frame, and each answer is a walk down a tree with a
// 64-bit multiply at every node.

namespace hlrt
{
    hldata::Trace traceFast(const hldata::MoveWorld &world, bool crouched, const hldata::fx *start,
                            const hldata::fx *end)
    {
        return hldata::traceAll(world, crouched, start, end);
    }
} // namespace hlrt
