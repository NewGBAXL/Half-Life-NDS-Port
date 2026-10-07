#include "hlds/PathNodes.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

#include "HlMonsters.hpp"

namespace hlds
{
    using namespace hldata;

    void cookPathNodes(const std::vector<Entity> &entities, CookedLevel &level)
    {
        level.pathNodes.clear();
        level.pathNext.clear();
        if (level.models.empty())
            return;

        MoveWorld world;
        world.standing = Hull{level.clipNodes.data(), level.planes.data(), level.planeTypes.data(),
                              level.models[0].clipStanding};
        world.crouched = Hull{level.clipNodes.data(), level.planes.data(), level.planeTypes.data(),
                              level.models[0].clipCrouched};

        for (const Entity &entity : entities)
        {
            if (entity.className() != "info_node")
                continue;
            double origin[3] = {0, 0, 0};
            entity.vec3("origin", origin);
            // Onto the floor under it, as something standing there would be.
            ActorState probe;
            for (int a = 0; a < 3; ++a)
                probe.pos[a] = fxFromDouble(origin[a]);
            if (!actorToFloor(world, probe, fxFromInt(8), fxFromInt(256)))
                continue; // in a wall, or over nothing
            if (level.pathNodes.size() >= static_cast<std::size_t>(kMaxPathNodes))
                break;
            PathNode node;
            for (int a = 0; a < 3; ++a)
                node.pos[a] = probe.pos[a];
            level.pathNodes.push_back(node);
        }
        const int count = static_cast<int>(level.pathNodes.size());
        if (count == 0)
            return;

        // Which can be walked between, both ways, and how far that is.
        const float kNone = std::numeric_limits<float>::infinity();
        std::vector<float> far(static_cast<std::size_t>(count) * count, kNone);
        std::vector<std::uint8_t> next(static_cast<std::size_t>(count) * count, kNoPathNode);
        for (int a = 0; a < count; ++a)
        {
            far[a * count + a] = 0;
            next[a * count + a] = static_cast<std::uint8_t>(a);
            for (int b = a + 1; b < count; ++b)
            {
                const fx *p = level.pathNodes[a].pos;
                const fx *q = level.pathNodes[b].pos;
                const double d[3] = {(q[0] - p[0]) / 4096.0, (q[1] - p[1]) / 4096.0,
                                     (q[2] - p[2]) / 4096.0};
                const double length = std::sqrt(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
                if (length > 1024)
                    continue;
                const int steps = static_cast<int>(length / 16) + 4;
                if (!walkClear(world, nullptr, -1, false, p, q, steps) ||
                    !walkClear(world, nullptr, -1, false, q, p, steps))
                    continue;
                far[a * count + b] = far[b * count + a] = static_cast<float>(length);
                next[a * count + b] = static_cast<std::uint8_t>(b);
                next[b * count + a] = static_cast<std::uint8_t>(a);
            }
        }
        // The shortest way between every two (Floyd and Warshall's sum),
        // keeping the first step of each.
        for (int k = 0; k < count; ++k)
        {
            for (int a = 0; a < count; ++a)
            {
                const float ak = far[a * count + k];
                if (ak == kNone)
                    continue;
                for (int b = 0; b < count; ++b)
                {
                    const float through = ak + far[k * count + b];
                    if (through < far[a * count + b])
                    {
                        far[a * count + b] = through;
                        next[a * count + b] = next[a * count + k];
                    }
                }
            }
        }
        level.pathNext = std::move(next);
    }
} // namespace hlds
