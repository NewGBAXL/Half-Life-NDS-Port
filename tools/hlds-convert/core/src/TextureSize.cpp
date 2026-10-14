#include "hlds/TextureSize.hpp"

#include <algorithm>

namespace hlds
{
    namespace
    {
        int nearestPowerOfTwo(std::uint32_t value)
        {
            int p = 1;
            // Step up while the value is at least half way (in ratio) to the
            // next power: p * sqrt(2). Compared squared to stay in integers.
            while (static_cast<std::uint64_t>(value) * value >=
                   2ull * static_cast<std::uint64_t>(p) * p)
            {
                p *= 2;
            }
            return p;
        }
    } // namespace

    TextureSize defaultTextureSize(std::uint32_t sourceWidth, std::uint32_t sourceHeight)
    {
        TextureSize size;
        size.width = static_cast<std::uint16_t>(std::clamp(
            nearestPowerOfTwo(std::max<std::uint32_t>(sourceWidth / 2, 1)), kMinTextureSide,
            kMaxTextureSide));
        size.height = static_cast<std::uint16_t>(std::clamp(
            nearestPowerOfTwo(std::max<std::uint32_t>(sourceHeight / 2, 1)), kMinTextureSide,
            kMaxTextureSide));
        return size;
    }

    TextureSize streamedTextureSize(std::uint32_t sourceWidth, std::uint32_t sourceHeight)
    {
        TextureSize size;
        size.width = static_cast<std::uint16_t>(
            std::clamp(nearestPowerOfTwo(std::max<std::uint32_t>(sourceWidth, 1)), kMinTextureSide, kMaxTextureSide));
        size.height = static_cast<std::uint16_t>(
            std::clamp(nearestPowerOfTwo(std::max<std::uint32_t>(sourceHeight, 1)), kMinTextureSide, kMaxTextureSide));
        return size;
    }
} // namespace hlds
