#pragma once

// A model's meshes, made fewer and made into strips: with meshoptimizer
// (thirdparty/meshoptimizer).
//
// FEWER TRIANGLES, at several levels of detail. A sub-model's strips and
// fans are taken apart into triangles over "wedges" -- one for each distinct
// (place with the model standing, bone, texture, texel) -- and simplified
// with the texture coordinates and normals weighed in the error beside the
// shape. The simplifier only ever keeps vertices the model has: every one
// that is left is on its own bone, at its own place, with its own texel, so
// the skinning stays exactly as rigid as it was and no seam of the texture
// opens. Each level is made from the one before, and says how far it is from
// the model as it was, in the game's units: that is what the console picks a
// level by (ModelDraw).
//
// FEWER VERTICES SENT. The DS has no index buffers: a vertex costs each time
// it is sent, so a triangle in a long strip costs one and a loose one three.
// Every model's meshes -- thinned or not -- are made into strips afresh, more
// than one way, and whichever way sends the fewest vertices is kept.

#include <array>
#include <vector>

#include "hlds/MdlFile.hpp"

namespace hlds
{
    struct MeshTriangle
    {
        MdlCorner c[3];
        int mesh = 0;
    };

    /// A sub-model's strips and fans as triangles (those with no area left
    /// out).
    std::vector<MeshTriangle> meshTriangles(const MdlSubModel &model);

    /// How many vertices the console is sent for it: a strip's corners; three
    /// for each triangle of a fan, which the hardware has not got.
    int sentVertices(const MdlSubModel &model);

    /// What the simplifier is told about a sub-model besides its shape.
    struct MeshInput
    {
        const MdlSubModel *model = nullptr;
        /// Where each vertex is with the model standing (model space).
        const std::vector<std::array<double, 3>> *rest = nullptr;
        /// Each normal, likewise turned to model space.
        std::vector<std::array<double, 3>> normals;
        /// For each mesh: one over its texture's width and height (0: none).
        std::vector<std::array<double, 2>> texel;
    };

    /// ... for `model` of `mdl`, with every bone's matrix as the model
    /// stands (mdlBoneMatrices). `rest` is filled, and must outlive the
    /// input.
    MeshInput meshInput(const MdlFile &mdl, const MdlSubModel &model,
                        const std::vector<std::array<double, 12>> &standing,
                        std::vector<std::array<double, 3>> &rest);

    struct MeshLevel
    {
        MdlSubModel model; ///< the same vertices and normals; new strips
        int triangles = 0;
        /// How far it is from the sub-model as it was: the distance, in
        /// units, that all but three in a hundred of the model's own
        /// vertices are within of this level's surface (with the model
        /// standing).
        double error = 0;
    };

    /// One level for each of `targets` (triangles, falling), each made from
    /// the one before. A target at or over what there is gives the mesh as
    /// it is (in new strips); one that cannot be reached gives as near as
    /// could be got.
    std::vector<MeshLevel> meshLevels(const MeshInput &input, const std::vector<int> &targets);

    /// `model` with the same triangles, corner for corner, in whichever
    /// strips send the fewest vertices (its own, if nothing does better).
    MdlSubModel restripped(const MdlSubModel &model);

    /// How much of each of three views (along x, y and z) the triangles
    /// cover, with the model standing: for telling how much of a model's
    /// outline a thinning has cost.
    void meshCoverage(const std::vector<MeshTriangle> &triangles, const std::vector<std::array<double, 3>> &rest,
                      float out[3]);

    /// That distance, for any thinning of `source`.
    double meshError(const std::vector<MeshTriangle> &source, const std::vector<MeshTriangle> &thin,
                     const std::vector<std::array<double, 3>> &rest);
} // namespace hlds
