#pragma once

// The map's path nodes, and the way from each to each.
//
// The game's maps mark where a monster may stand with info_node entities and
// leave the engine to work out, when the map is first run, which can be
// walked between. That is done here instead, with the very walk test the DS
// uses to move a monster (HlMonsters.hpp), and then carried to its end: for
// every pair of nodes, the next node to make for. The console looks its way
// up; it never searches.

#include <vector>

#include "hlds/EntityParser.hpp"
#include "hlds/LevelWriter.hpp"

namespace hlds
{
    /// Fills `level.pathNodes` and `level.pathNext` from the map's
    /// info_nodes. The level's planes, clip nodes and world model must be in
    /// place. A map with none, or with more than the format holds, gets none.
    void cookPathNodes(const std::vector<Entity> &entities, CookedLevel &level);
} // namespace hlds
