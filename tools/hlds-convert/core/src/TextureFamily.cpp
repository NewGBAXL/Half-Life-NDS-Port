#include "hlds/TextureFamily.hpp"

#include <cmath>
#include <string>

namespace hlds
{
    namespace
    {
        /// The map's texture called `name`, or -1.
        int find(const BspFile &bsp, const std::string &name)
        {
            for (std::size_t t = 0; t < bsp.textures.size(); ++t)
            {
                if (!bsp.textures[t].missing && bsp.textures[t].name == name)
                    return static_cast<int>(t);
            }
            return -1;
        }

        /// Those of `stem` with each of `count` marks from `first` in front
        /// (and `lead` before that), for as long as the run is unbroken.
        std::vector<int> run(const BspFile &bsp, char lead, char first, int count, const std::string &stem)
        {
            std::vector<int> found;
            for (int n = 0; n < count; ++n)
            {
                const int t = find(bsp, std::string(1, lead) + static_cast<char>(first + n) + stem);
                if (t < 0)
                    break;
                found.push_back(t);
            }
            return found;
        }
    } // namespace

    TextureFamily textureFamily(const BspFile &bsp, int miptex)
    {
        TextureFamily family;
        const std::string &name = bsp.textures[miptex].name;
        if (name.size() < 3 || name[0] != '+')
        {
            family.frames = {miptex};
            return family;
        }
        const std::string stem = name.substr(2);
        if (name[1] >= '0' && name[1] <= '9')
        {
            family.frames = run(bsp, '+', '0', 10, stem);
            family.other = run(bsp, '+', 'a', 10, stem);
        }
        else if (name[1] >= 'a' && name[1] <= 'j')
        {
            family.frames = run(bsp, '+', 'a', 10, stem);
        }
        // (A cycle that does not start at its first picture, or a name that
        // is no cycle's: the texture as it is.)
        if (family.frames.empty())
        {
            family.frames = {miptex};
            family.other.clear();
        }
        return family;
    }

    namespace
    {
        bool randomTiling = true;
    }
    void setRandomTiling(bool on)
    {
        randomTiling = on;
    }

    int wornTexture(const BspFile &bsp, const BspFace &face)
    {
        const BspTexInfo &info = bsp.texInfos[face.texInfo];
        const BspTexture &tex = bsp.textures[info.miptex];
        if (!randomTiling || tex.missing || tex.name.size() < 3 || tex.name[0] != '-' || tex.name[1] < '0' || tex.name[1] > '9' ||
            tex.width == 0 || tex.height == 0)
            return info.miptex;
        const std::vector<int> variants = run(bsp, '-', '0', 10, tex.name.substr(2));
        if (variants.size() < 2)
            return info.miptex;
        // Where the face starts in the texture's own space, in whole tiles
        // (the game goes by the same: its "texture mins").
        double least[2] = {0, 0};
        for (int i = 0; i < face.edgeCount; ++i)
        {
            const float *p = bsp.faceVertex(face, i).point;
            for (int k = 0; k < 2; ++k)
            {
                const double at = double(p[0]) * info.vecs[k][0] + double(p[1]) * info.vecs[k][1] +
                                  double(p[2]) * info.vecs[k][2] + info.vecs[k][3];
                least[k] = i == 0 || at < least[k] ? at : least[k];
            }
        }
        const auto tileS = static_cast<long long>(std::floor(std::floor(least[0] / 16.0) * 16.0 / tex.width));
        const auto tileT = static_cast<long long>(std::floor(std::floor(least[1] / 16.0) * 16.0 / tex.height));
        // (The game looks the pair up in a table of the C library's random
        // numbers. Any even scatter does: this is one.)
        std::uint32_t h = static_cast<std::uint32_t>(tileS) * 0x9E3779B1u ^ static_cast<std::uint32_t>(tileT) * 0x85EBCA77u;
        h ^= h >> 15;
        h *= 0x2C1B3C6Du;
        h ^= h >> 12;
        return variants[h % variants.size()];
    }
} // namespace hlds
