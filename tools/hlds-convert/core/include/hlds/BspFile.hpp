#pragma once

// A GoldSrc level (BSP version 30), read into memory and VALIDATED.
//
// Validated is the point of this class. Every index a lump holds into another
// lump -- a face's edges, an edge's vertices, a node's children, a leaf's
// marksurfaces -- is range-checked once in parse(), so the cooker behind it can
// index freely. A map that fails any check is refused with a message naming the
// lump; nothing downstream has to wonder.
//
// The structs are the on-disc layouts, packed, and are copied straight out of
// the file. That is only correct on a little-endian host, which BspFile.cpp
// asserts. Reference for the layouts: xash3d-fwgs common/bspfile.h.
//
// Floats are fine HERE: this is the host tool. Nothing in this header is
// compiled for the DS.

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include "HlLevel.hpp"

namespace hlds
{
    inline constexpr std::int32_t kBspVersion = 30;
    inline constexpr int kBspHullCount = 4;

    // Leaf / clipnode contents. The values are the BSP's own and the cooked
    // level keeps them, so there is one definition, in the shared header.
    using hldata::kContentsEmpty;
    using hldata::kContentsLava;
    using hldata::kContentsSky;
    using hldata::kContentsSlime;
    using hldata::kContentsSolid;
    using hldata::kContentsWater;

    /// texinfo flag: the face has no lightmap (sky, water, tool textures).
    inline constexpr std::int32_t kTexSpecial = 1;

#pragma pack(push, 1)
    struct BspPlane
    {
        float normal[3];
        float dist;
        std::int32_t type; ///< 0..2 axial X/Y/Z, 3..5 nearest axis
    };

    struct BspVertex
    {
        float point[3];
    };

    struct BspNode
    {
        std::int32_t plane;
        std::int16_t children[2]; ///< negative: -(leaf + 1)
        std::int16_t mins[3];
        std::int16_t maxs[3];
        std::uint16_t firstFace;
        std::uint16_t faceCount;
    };

    struct BspTexInfo
    {
        float vecs[2][4]; ///< [s/t][xyz + offset], in texels
        std::int32_t miptex;
        std::int32_t flags;
    };

    struct BspFace
    {
        std::uint16_t plane;
        std::int16_t side; ///< non-zero: the face looks down the plane's back
        std::int32_t firstEdge;
        std::int16_t edgeCount;
        std::int16_t texInfo;
        std::uint8_t styles[4]; ///< 255 = unused
        std::int32_t lightOfs;  ///< -1 = no lightmap
    };

    struct BspClipNode
    {
        std::int32_t plane;
        std::int16_t children[2]; ///< negative: a kContents* value
    };

    struct BspLeaf
    {
        std::int32_t contents;
        std::int32_t visOfs; ///< -1 = no visibility row: everything is visible
        std::int16_t mins[3];
        std::int16_t maxs[3];
        std::uint16_t firstMarkSurface;
        std::uint16_t markSurfaceCount;
        std::uint8_t ambientLevel[4];
    };

    struct BspEdge
    {
        std::uint16_t v[2];
    };

    struct BspModel
    {
        float mins[3];
        float maxs[3];
        float origin[3];
        std::int32_t headNode[kBspHullCount];
        std::int32_t visLeafs; ///< leaves with a visibility row, not counting leaf 0
        std::int32_t firstFace;
        std::int32_t faceCount;
    };
#pragma pack(pop)

    static_assert(sizeof(BspPlane) == 20);
    static_assert(sizeof(BspVertex) == 12);
    static_assert(sizeof(BspNode) == 24);
    static_assert(sizeof(BspTexInfo) == 40);
    static_assert(sizeof(BspFace) == 20);
    static_assert(sizeof(BspClipNode) == 8);
    static_assert(sizeof(BspLeaf) == 28);
    static_assert(sizeof(BspEdge) == 4);
    static_assert(sizeof(BspModel) == 64);

    /// One entry of the texture lump. A map either carries the pixels itself or
    /// names a texture that lives in one of the WADs worldspawn lists.
    struct BspTexture
    {
        std::string name;          ///< lower-cased; empty when `missing`
        std::uint32_t width = 0;
        std::uint32_t height = 0;
        bool missing = false;      ///< the lump has a hole here (offset -1)
        bool embedded = false;     ///< pixels and palette below are filled in
        std::vector<std::uint8_t> pixels;  ///< width*height palette indices (mip 0)
        std::vector<std::uint8_t> palette; ///< 256 RGB triples
    };

    class BspFile
    {
    public:
        std::string entities; ///< the entity lump as text, without the trailing NUL
        std::vector<BspPlane> planes;
        std::vector<BspTexture> textures;
        std::vector<BspVertex> vertices;
        std::vector<std::uint8_t> visibility; ///< run-length compressed rows
        std::vector<BspNode> nodes;
        std::vector<BspTexInfo> texInfos;
        std::vector<BspFace> faces;
        std::vector<std::uint8_t> lighting; ///< RGB luxels
        std::vector<BspClipNode> clipNodes;
        std::vector<BspLeaf> leaves;
        std::vector<std::uint16_t> markSurfaces;
        std::vector<BspEdge> edges;
        std::vector<std::int32_t> surfEdges; ///< negative: the edge reversed
        std::vector<BspModel> models;

        bool load(const std::filesystem::path &path, std::string &error);
        /// Parse a level already in memory. On failure the object is left empty.
        bool parse(const std::vector<std::uint8_t> &bytes, std::string &error);

        /// The i-th vertex of a face, walking its surfedges. Indices are valid by
        /// construction once parse() has succeeded.
        const BspVertex &faceVertex(const BspFace &face, int i) const;

        /// The set of leaves visible from `leaf`, one bit per leaf, bit (n - 1)
        /// for leaf n -- leaf 0 is the shared solid leaf and has no bit. A leaf
        /// without a visibility row sees everything.
        std::vector<std::uint8_t> decompressVis(int leaf) const;

        /// The root of a model's clip hull (1..3): a clip node index, or a
        /// negative kContents* value when the whole hull is one thing. A model
        /// without a hull -- which the file encodes as a root past the end of
        /// the lump -- is all empty: nothing collides with it.
        std::int32_t clipHullRoot(int model, int hull) const;

        /// Leaves of the world model that have a visibility row.
        int visLeafCount() const;
    };
} // namespace hlds
