#include "hlds/BspFile.hpp"

#include <algorithm>
#include <bit>
#include <cctype>
#include <cstring>
#include <fstream>

// The lump structs are memcpy'd out of the file as they are.
static_assert(std::endian::native == std::endian::little,
              "hlds-convert reads little-endian files with memcpy");

namespace hlds
{
    namespace
    {
        enum Lump
        {
            kLumpEntities,
            kLumpPlanes,
            kLumpTextures,
            kLumpVertices,
            kLumpVisibility,
            kLumpNodes,
            kLumpTexInfo,
            kLumpFaces,
            kLumpLighting,
            kLumpClipNodes,
            kLumpLeaves,
            kLumpMarkSurfaces,
            kLumpEdges,
            kLumpSurfEdges,
            kLumpModels,
            kLumpCount
        };

        const char *const kLumpNames[kLumpCount] = {
            "entities", "planes", "textures", "vertices", "visibility",
            "nodes", "texinfo", "faces", "lighting", "clipnodes",
            "leaves", "marksurfaces", "edges", "surfedges", "models"};

#pragma pack(push, 1)
        struct LumpDir
        {
            std::int32_t offset;
            std::int32_t length;
        };

        struct Header
        {
            std::int32_t version;
            LumpDir lumps[kLumpCount];
        };

        struct MipTex
        {
            char name[16];
            std::uint32_t width;
            std::uint32_t height;
            std::uint32_t offsets[4];
        };
#pragma pack(pop)
        static_assert(sizeof(Header) == 124);
        static_assert(sizeof(MipTex) == 40);

        template <class T>
        bool readLump(const std::vector<std::uint8_t> &bytes, const Header &header, Lump lump,
                      std::vector<T> &out, std::string &error)
        {
            const LumpDir &dir = header.lumps[lump];
            if (dir.length % sizeof(T) != 0)
            {
                error = std::string("the ") + kLumpNames[lump] +
                        " lump is not a whole number of records";
                return false;
            }
            out.resize(dir.length / sizeof(T));
            if (!out.empty())
            {
                std::memcpy(out.data(), bytes.data() + dir.offset, dir.length);
            }
            return true;
        }

        std::string lowerName(const char *raw, std::size_t maxLen)
        {
            std::string name;
            for (std::size_t i = 0; i < maxLen && raw[i] != '\0'; ++i)
            {
                name.push_back(static_cast<char>(
                    std::tolower(static_cast<unsigned char>(raw[i]))));
            }
            return name;
        }

        bool readTextures(const std::vector<std::uint8_t> &bytes, const LumpDir &dir,
                          std::vector<BspTexture> &out, std::string &error)
        {
            out.clear();
            if (dir.length == 0)
            {
                return true;
            }
            const std::uint8_t *base = bytes.data() + dir.offset;
            const std::size_t size = static_cast<std::size_t>(dir.length);
            if (size < 4)
            {
                error = "the textures lump is truncated";
                return false;
            }
            std::int32_t count = 0;
            std::memcpy(&count, base, 4);
            if (count < 0 || 4 + static_cast<std::size_t>(count) * 4 > size)
            {
                error = "the textures lump has a bad count";
                return false;
            }
            out.resize(count);
            for (std::int32_t i = 0; i < count; ++i)
            {
                BspTexture &tex = out[i];
                std::int32_t ofs = 0;
                std::memcpy(&ofs, base + 4 + i * 4, 4);
                if (ofs < 0)
                {
                    tex.missing = true;
                    continue;
                }
                if (static_cast<std::size_t>(ofs) + sizeof(MipTex) > size)
                {
                    error = "texture " + std::to_string(i) + " lies outside the textures lump";
                    return false;
                }
                MipTex mip;
                std::memcpy(&mip, base + ofs, sizeof(mip));
                tex.name = lowerName(mip.name, sizeof(mip.name));
                tex.width = mip.width;
                tex.height = mip.height;
                if (tex.width == 0 || tex.height == 0 || tex.width > 4096 || tex.height > 4096)
                {
                    error = "texture '" + tex.name + "' has an impossible size";
                    return false;
                }
                if (mip.offsets[0] == 0)
                {
                    continue; // named only: the pixels are in a WAD
                }

                // Four mips, each a quarter of the one before, then a 16-bit
                // colour count and the palette.
                const std::size_t pixels = static_cast<std::size_t>(tex.width) * tex.height;
                const std::size_t mipBytes =
                    pixels + pixels / 4 + pixels / 16 + pixels / 64;
                const std::size_t palOfs = ofs + sizeof(MipTex) + mipBytes + 2;
                if (ofs + mip.offsets[0] + pixels > size || palOfs + 768 > size)
                {
                    error = "texture '" + tex.name + "' is truncated";
                    return false;
                }
                tex.embedded = true;
                tex.pixels.assign(base + ofs + mip.offsets[0],
                                  base + ofs + mip.offsets[0] + pixels);
                tex.palette.assign(base + palOfs, base + palOfs + 768);
            }
            return true;
        }
    } // namespace

    bool BspFile::load(const std::filesystem::path &path, std::string &error)
    {
        std::ifstream in(path, std::ios::binary);
        if (!in)
        {
            error = "cannot open " + path.string();
            return false;
        }
        const std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(in)),
                                              std::istreambuf_iterator<char>());
        return parse(bytes, error);
    }

    bool BspFile::parse(const std::vector<std::uint8_t> &bytes, std::string &error)
    {
        *this = BspFile();

        if (bytes.size() < sizeof(Header))
        {
            error = "not a BSP: the file is smaller than a header";
            return false;
        }
        Header header;
        std::memcpy(&header, bytes.data(), sizeof(header));
        if (header.version != kBspVersion)
        {
            error = "not a Half-Life BSP: version " + std::to_string(header.version) +
                    ", expected 30";
            return false;
        }
        for (int i = 0; i < kLumpCount; ++i)
        {
            const LumpDir &dir = header.lumps[i];
            if (dir.offset < 0 || dir.length < 0 ||
                static_cast<std::size_t>(dir.offset) + static_cast<std::size_t>(dir.length) >
                    bytes.size())
            {
                error = std::string("the ") + kLumpNames[i] + " lump lies outside the file";
                return false;
            }
        }

        // Blue Shift ships its maps with the first two lumps swapped. Entity
        // text starts with '{'; a plane record essentially never does.
        {
            const LumpDir &ent = header.lumps[kLumpEntities];
            const LumpDir &pln = header.lumps[kLumpPlanes];
            const bool entLooksText = ent.length > 0 && bytes[ent.offset] == '{';
            const bool plnLooksText = pln.length > 0 && bytes[pln.offset] == '{';
            if (!entLooksText && plnLooksText)
            {
                std::swap(header.lumps[kLumpEntities], header.lumps[kLumpPlanes]);
            }
        }

        {
            const LumpDir &dir = header.lumps[kLumpEntities];
            entities.assign(reinterpret_cast<const char *>(bytes.data()) + dir.offset,
                            static_cast<std::size_t>(dir.length));
            const std::size_t nul = entities.find('\0');
            if (nul != std::string::npos)
            {
                entities.resize(nul);
            }
        }

        if (!readLump(bytes, header, kLumpPlanes, planes, error) ||
            !readLump(bytes, header, kLumpVertices, vertices, error) ||
            !readLump(bytes, header, kLumpVisibility, visibility, error) ||
            !readLump(bytes, header, kLumpNodes, nodes, error) ||
            !readLump(bytes, header, kLumpTexInfo, texInfos, error) ||
            !readLump(bytes, header, kLumpFaces, faces, error) ||
            !readLump(bytes, header, kLumpLighting, lighting, error) ||
            !readLump(bytes, header, kLumpClipNodes, clipNodes, error) ||
            !readLump(bytes, header, kLumpLeaves, leaves, error) ||
            !readLump(bytes, header, kLumpMarkSurfaces, markSurfaces, error) ||
            !readLump(bytes, header, kLumpEdges, edges, error) ||
            !readLump(bytes, header, kLumpSurfEdges, surfEdges, error) ||
            !readLump(bytes, header, kLumpModels, models, error) ||
            !readTextures(bytes, header.lumps[kLumpTextures], textures, error))
        {
            *this = BspFile();
            return false;
        }

        // --- Cross-lump validation. One message per kind of fault is enough:
        // a map that fails here is not going to be cooked either way.
        auto fail = [&](const std::string &what)
        {
            error = what;
            *this = BspFile();
            return false;
        };

        if (models.empty())
            return fail("the map has no models");
        if (leaves.empty())
            return fail("the map has no leaves");

        const auto planeCount = static_cast<std::int64_t>(planes.size());
        const auto nodeCount = static_cast<std::int64_t>(nodes.size());
        const auto leafCount = static_cast<std::int64_t>(leaves.size());
        const auto faceCount = static_cast<std::int64_t>(faces.size());
        const auto clipCount = static_cast<std::int64_t>(clipNodes.size());

        for (const BspEdge &edge : edges)
        {
            if (edge.v[0] >= vertices.size() || edge.v[1] >= vertices.size())
                return fail("an edge names a vertex that does not exist");
        }
        for (std::int32_t se : surfEdges)
        {
            const std::int64_t idx = se < 0 ? -static_cast<std::int64_t>(se) : se;
            if (idx >= static_cast<std::int64_t>(edges.size()))
                return fail("a surfedge names an edge that does not exist");
        }
        for (const BspTexInfo &ti : texInfos)
        {
            if (ti.miptex < 0 || ti.miptex >= static_cast<std::int64_t>(textures.size()))
                return fail("a texinfo names a texture that does not exist");
        }
        for (const BspFace &face : faces)
        {
            if (face.plane >= planeCount)
                return fail("a face names a plane that does not exist");
            if (face.texInfo < 0 || face.texInfo >= static_cast<std::int64_t>(texInfos.size()))
                return fail("a face names a texinfo that does not exist");
            if (face.edgeCount < 3)
                return fail("a face has fewer than three edges");
            if (face.firstEdge < 0 ||
                static_cast<std::int64_t>(face.firstEdge) + face.edgeCount >
                    static_cast<std::int64_t>(surfEdges.size()))
                return fail("a face's edges lie outside the surfedge lump");
            if (face.lightOfs >= static_cast<std::int64_t>(lighting.size()))
                return fail("a face's lightmap lies outside the lighting lump");
        }
        for (std::uint16_t mark : markSurfaces)
        {
            if (mark >= faceCount)
                return fail("a marksurface names a face that does not exist");
        }
        for (const BspNode &node : nodes)
        {
            if (node.plane < 0 || node.plane >= planeCount)
                return fail("a node names a plane that does not exist");
            for (std::int16_t child : node.children)
            {
                const bool ok = child >= 0 ? child < nodeCount : (-1 - child) < leafCount;
                if (!ok)
                    return fail("a node has a child that does not exist");
            }
            if (static_cast<std::int64_t>(node.firstFace) + node.faceCount > faceCount)
                return fail("a node's faces lie outside the face lump");
        }
        for (const BspLeaf &leaf : leaves)
        {
            if (static_cast<std::size_t>(leaf.firstMarkSurface) + leaf.markSurfaceCount >
                markSurfaces.size())
                return fail("a leaf's marksurfaces lie outside the lump");
            if (leaf.visOfs >= static_cast<std::int64_t>(visibility.size()))
                return fail("a leaf's visibility row lies outside the lump");
        }
        for (const BspClipNode &clip : clipNodes)
        {
            if (clip.plane < 0 || clip.plane >= planeCount)
                return fail("a clipnode names a plane that does not exist");
            for (std::int16_t child : clip.children)
            {
                if (child >= clipCount)
                    return fail("a clipnode has a child that does not exist");
            }
        }
        for (const BspModel &model : models)
        {
            if (model.firstFace < 0 || model.faceCount < 0 ||
                static_cast<std::int64_t>(model.firstFace) + model.faceCount > faceCount)
                return fail("a model's faces lie outside the face lump");
            // Hull 0 is the render tree. A model with no faces at all (a trigger
            // brush) may legitimately point at a leaf instead of a node.
            const std::int32_t h0 = model.headNode[0];
            if (h0 >= nodeCount || (h0 < 0 && (-1 - h0) >= leafCount))
                return fail("a model's render tree root does not exist");
            // headNode[1..3] are NOT checked, on purpose. The ZHLT compilers
            // write a root one past the last clip node for a model that has no
            // hull at all, and retail maps (c2a2a, c3a2c) ship that way. The
            // engine reads it as "no hull"; so does clipHullRoot(), which is
            // the only way the rest of the converter may read these.
        }
        if (models[0].visLeafs < 0 || models[0].visLeafs >= leafCount)
            return fail("the world model's visible leaf count is impossible");

        return true;
    }

    const BspVertex &BspFile::faceVertex(const BspFace &face, int i) const
    {
        const std::int32_t se = surfEdges[face.firstEdge + i];
        return se >= 0 ? vertices[edges[se].v[0]] : vertices[edges[-se].v[1]];
    }

    std::int32_t BspFile::clipHullRoot(int model, int hull) const
    {
        const std::int32_t root = models[model].headNode[hull];
        if (root < 0)
            return root; // already a contents value
        if (root >= static_cast<std::int64_t>(clipNodes.size()))
            return kContentsEmpty;
        return root;
    }

    int BspFile::visLeafCount() const
    {
        return models.empty() ? 0 : models[0].visLeafs;
    }

    std::vector<std::uint8_t> BspFile::decompressVis(int leaf) const
    {
        const int rowBytes = (visLeafCount() + 7) / 8;
        std::vector<std::uint8_t> row(rowBytes, 0);
        const std::int32_t ofs = leaves[leaf].visOfs;
        if (ofs < 0 || visibility.empty())
        {
            std::fill(row.begin(), row.end(), 0xFF);
            return row;
        }

        // A non-zero byte is literal; a zero is followed by a count of zero
        // bytes. A row that runs off the end of the lump leaves the rest
        // invisible rather than reading past it.
        std::size_t in = static_cast<std::size_t>(ofs);
        int out = 0;
        while (out < rowBytes && in < visibility.size())
        {
            const std::uint8_t b = visibility[in++];
            if (b != 0)
            {
                row[out++] = b;
                continue;
            }
            if (in >= visibility.size())
                break;
            int run = visibility[in++];
            while (run-- > 0 && out < rowBytes)
            {
                row[out++] = 0;
            }
        }
        return row;
    }
} // namespace hlds
