#include "hlds/NodGraph.hpp"

#include <cstring>

namespace hlds
{
    namespace
    {
        constexpr std::size_t kGraphBytes = 8396;
        constexpr std::size_t kNodeBytes = 88;
        constexpr std::size_t kLinkBytes = 24;
        constexpr std::size_t kDistBytes = 16;

        std::int32_t i32(const std::uint8_t *p)
        {
            return static_cast<std::int32_t>(p[0] | (p[1] << 8) | (p[2] << 16) | (std::uint32_t(p[3]) << 24));
        }
        float f32(const std::uint8_t *p)
        {
            float value;
            std::memcpy(&value, p, 4);
            return value;
        }
    } // namespace

    bool parseNodGraph(const std::vector<std::uint8_t> &bytes, NodGraph &out, std::string &error)
    {
        out = NodGraph();
        if (bytes.size() < 4 + kGraphBytes || i32(bytes.data()) != 16)
        {
            error = "not a version 16 node graph";
            return false;
        }
        const std::uint8_t *graph = bytes.data() + 4;
        const std::int64_t nodes = i32(graph + 24), links = i32(graph + 28), route = i32(graph + 32),
                           hash = i32(graph + 8384);
        // Every byte of the file is one of these, or the file is not what
        // it says it is.
        if (nodes < 0 || links < 0 || route < 0 || hash < 0 || nodes > 4096 || links > 65536 ||
            4 + kGraphBytes + nodes * kNodeBytes + links * kLinkBytes + nodes * kDistBytes + route + hash * 2 !=
                static_cast<std::int64_t>(bytes.size()))
        {
            error = "node graph of the wrong size for what it says it holds";
            return false;
        }
        const std::uint8_t *at = graph + kGraphBytes;
        for (std::int64_t n = 0; n < nodes; ++n, at += kNodeBytes)
        {
            NodNode node;
            for (int a = 0; a < 3; ++a)
                node.origin[a] = f32(at + a * 4);
            node.info = i32(at + 28);
            out.nodes.push_back(node);
        }
        for (std::int64_t l = 0; l < links; ++l, at += kLinkBytes)
        {
            NodLink link;
            link.from = i32(at);
            link.to = i32(at + 4);
            link.info = i32(at + 16);
            link.length = f32(at + 20);
            if (link.from < 0 || link.from >= nodes || link.to < 0 || link.to >= nodes)
            {
                error = "node graph link to a node it has not got";
                return false;
            }
            // "*12", in four bytes that need not end in a zero.
            if (at[12] == '*')
            {
                for (int k = 1; k < 4 && at[12 + k] >= '0' && at[12 + k] <= '9'; ++k)
                    link.model = link.model * 10 + (at[12 + k] - '0');
            }
            out.links.push_back(link);
        }
        return true;
    }
} // namespace hlds
