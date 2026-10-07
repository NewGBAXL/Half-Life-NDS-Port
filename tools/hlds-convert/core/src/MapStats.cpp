#include "hlds/MapStats.hpp"

#include <algorithm>

#include "hlds/EntityParser.hpp"

namespace hlds
{
    bool isUndrawnTexture(std::string_view lowerName)
    {
        static constexpr std::string_view kNames[] = {
            "sky", "aaatrigger", "clip", "origin", "null", "hint", "skip", "bevel"};
        for (std::string_view name : kNames)
        {
            if (lowerName == name)
                return true;
        }
        return false;
    }

    FaceCost faceCost(int edgeCount)
    {
        FaceCost cost;
        cost.triangles = edgeCount - 2;
        const int quads = cost.triangles / 2;
        const int odd = cost.triangles % 2;
        cost.polygons = quads + odd;
        cost.vertices = quads * 4 + odd * 3;
        return cost;
    }

    namespace
    {
        bool isDrawn(const BspFile &bsp, const BspFace &face)
        {
            const BspTexture &tex = bsp.textures[bsp.texInfos[face.texInfo].miptex];
            return !tex.missing && !isUndrawnTexture(tex.name);
        }

        void add(FaceCost &sum, const FaceCost &cost)
        {
            sum.triangles += cost.triangles;
            sum.polygons += cost.polygons;
            sum.vertices += cost.vertices;
        }
    } // namespace

    MapStats computeMapStats(const BspFile &bsp)
    {
        MapStats stats;
        const BspModel &world = bsp.models[0];
        for (int i = 0; i < 3; ++i)
        {
            stats.mins[i] = world.mins[i];
            stats.maxs[i] = world.maxs[i];
        }

        std::vector<char> drawn(bsp.faces.size(), 0);
        std::vector<char> textureUsed(bsp.textures.size(), 0);
        for (std::size_t f = 0; f < bsp.faces.size(); ++f)
        {
            drawn[f] = isDrawn(bsp, bsp.faces[f]) ? 1 : 0;
            if (drawn[f])
                textureUsed[bsp.texInfos[bsp.faces[f].texInfo].miptex] = 1;
        }

        stats.worldFaces = world.faceCount;
        for (int f = world.firstFace; f < world.firstFace + world.faceCount; ++f)
        {
            if (!drawn[f])
                continue;
            ++stats.drawnWorldFaces;
            add(stats.worldCost, faceCost(bsp.faces[f].edgeCount));
        }
        for (std::size_t m = 1; m < bsp.models.size(); ++m)
        {
            const BspModel &model = bsp.models[m];
            for (int f = model.firstFace; f < model.firstFace + model.faceCount; ++f)
            {
                if (drawn[f])
                    ++stats.brushModelFaces;
            }
        }

        // Per leaf: every face marked by any leaf its PVS row lists, each
        // counted once. `stamp` is the usual trick for "once" without clearing
        // a set per leaf.
        stats.visLeaves = bsp.visLeafCount();
        std::vector<int> stamp(bsp.faces.size(), 0);
        std::vector<int> perLeaf;
        perLeaf.reserve(stats.visLeaves);
        for (int leaf = 1; leaf <= stats.visLeaves; ++leaf)
        {
            const std::vector<std::uint8_t> row = bsp.decompressVis(leaf);
            LeafLoad load;
            load.leaf = leaf;
            for (int other = 1; other <= stats.visLeaves; ++other)
            {
                if (!(row[(other - 1) >> 3] & (1 << ((other - 1) & 7))))
                    continue;
                const BspLeaf &seen = bsp.leaves[other];
                for (int m = 0; m < seen.markSurfaceCount; ++m)
                {
                    const int f = bsp.markSurfaces[seen.firstMarkSurface + m];
                    if (stamp[f] == leaf || !drawn[f])
                        continue;
                    stamp[f] = leaf;
                    ++load.faces;
                    add(load.cost, faceCost(bsp.faces[f].edgeCount));
                }
            }
            perLeaf.push_back(load.cost.polygons);
            if (load.cost.polygons > stats.worst.cost.polygons)
                stats.worst = load;

            const std::size_t bucket = load.cost.polygons / MapStats::kBucket;
            if (stats.histogram.size() <= bucket)
                stats.histogram.resize(bucket + 1, 0);
            ++stats.histogram[bucket];
        }
        if (!perLeaf.empty())
        {
            std::sort(perLeaf.begin(), perLeaf.end());
            stats.medianPolygons = perLeaf[perLeaf.size() / 2];
            stats.p90Polygons = perLeaf[(perLeaf.size() * 9) / 10];
        }

        stats.textureCount = static_cast<int>(bsp.textures.size());
        for (std::size_t t = 0; t < bsp.textures.size(); ++t)
        {
            if (bsp.textures[t].embedded)
                ++stats.embeddedTextures;
            if (textureUsed[t])
                ++stats.usedTextures;
        }

        std::vector<Entity> entities;
        std::string error;
        if (parseEntities(bsp.entities, entities, error))
        {
            for (const Entity &entity : entities)
            {
                ++stats.classCounts[std::string(entity.className())];
            }
        }
        return stats;
    }
} // namespace hlds
