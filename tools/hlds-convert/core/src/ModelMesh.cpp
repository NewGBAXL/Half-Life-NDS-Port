#include "hlds/ModelMesh.hpp"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstdlib>
#include <cstdio>
#include <map>

#include "meshoptimizer/meshoptimizer.h"

namespace hlds
{
    namespace
    {
        using Strips = std::vector<std::vector<unsigned int>>;

        std::size_t costOf(const Strips &strips)
        {
            std::size_t total = 0;
            for (const auto &strip : strips)
                total += strip.size();
            return total;
        }

        /// meshoptimizer's strips: its order for strips, then its strips,
        /// which come as one run with a mark between them and, where a strip
        /// has to turn, a vertex sent twice. The DS has separate strips and
        /// pays for every vertex: the run is cut at the marks, and cut again
        /// at each turn.
        Strips stripsByMeshopt(const std::vector<unsigned int> &indices, std::size_t vertexCount)
        {
            Strips out;
            if (indices.empty())
                return out;
            std::vector<unsigned int> ordered(indices.size());
            meshopt_optimizeVertexCacheStrip(ordered.data(), indices.data(), indices.size(), vertexCount);
            std::vector<unsigned int> run(meshopt_stripifyBound(ordered.size()));
            const unsigned int kRestart = ~0u;
            run.resize(meshopt_stripify(run.data(), ordered.data(), ordered.size(), vertexCount, kRestart));
            std::vector<unsigned int> strip;
            auto close = [&]()
            {
                if (strip.size() >= 3)
                    out.push_back(strip);
                strip.clear();
            };
            for (unsigned int index : run)
            {
                if (index == kRestart)
                {
                    close();
                    continue;
                }
                strip.push_back(index);
                const std::size_t n = strip.size();
                if (n >= 3 && (strip[n - 1] == strip[n - 2] || strip[n - 1] == strip[n - 3] || strip[n - 2] == strip[n - 3]))
                {
                    // A turn (a b a ...): the strip ends before it, and the
                    // next begins with the two it goes on from.
                    const unsigned int b = strip[n - 2], a = strip[n - 1];
                    strip.pop_back();
                    close();
                    strip = {b, a};
                }
            }
            close();
            return out;
        }

        /// The plain way: each strip grown forward from a triangle through
        /// whichever neighbour shares the edge, the best of its three
        /// starts kept.
        Strips stripsGreedy(const std::vector<unsigned int> &indices)
        {
            Strips out;
            const std::size_t count = indices.size() / 3;
            std::map<std::pair<unsigned int, unsigned int>, std::pair<int, int>> edges;
            for (std::size_t t = 0; t < count; ++t)
            {
                for (int e = 0; e < 3; ++e)
                    edges.insert({{indices[t * 3 + e], indices[t * 3 + (e + 1) % 3]},
                                  {static_cast<int>(t), (e + 2) % 3}});
            }
            std::vector<char> used(count, 0);
            auto grow = [&](int start, int rotation, std::vector<int> &members)
            {
                std::vector<unsigned int> strip = {indices[start * 3 + rotation], indices[start * 3 + (rotation + 1) % 3],
                                                   indices[start * 3 + (rotation + 2) % 3]};
                members = {start};
                used[start] = 1;
                for (;;)
                {
                    const std::size_t i = strip.size() - 2;
                    const auto found = (i & 1) ? edges.find({strip[i + 1], strip[i]}) : edges.find({strip[i], strip[i + 1]});
                    if (found == edges.end() || used[found->second.first])
                        break;
                    used[found->second.first] = 1;
                    members.push_back(found->second.first);
                    strip.push_back(indices[found->second.first * 3 + found->second.second]);
                }
                for (int member : members)
                    used[member] = 0;
                return strip;
            };
            for (std::size_t t = 0; t < count; ++t)
            {
                if (used[t])
                    continue;
                std::vector<int> members, others;
                std::vector<unsigned int> best = grow(static_cast<int>(t), 0, members);
                for (int rotation = 1; rotation < 3; ++rotation)
                {
                    std::vector<unsigned int> other = grow(static_cast<int>(t), rotation, others);
                    if (other.size() > best.size())
                    {
                        best = std::move(other);
                        members = others;
                    }
                }
                for (int member : members)
                    used[member] = 1;
                out.push_back(std::move(best));
            }
            return out;
        }

        Strips bestStrips(const std::vector<unsigned int> &indices, std::size_t vertexCount)
        {
            Strips a = stripsByMeshopt(indices, vertexCount);
            Strips b = stripsGreedy(indices);
            return costOf(a) < costOf(b) || (costOf(a) == costOf(b) && a.size() <= b.size()) ? a : b;
        }

        /// The corners a set of triangles is drawn with, numbered: one
        /// number for each distinct corner as `same` tells them apart.
        struct Numbered
        {
            std::vector<MdlCorner> corners;                 ///< by number
            std::vector<int> meshOf;                        ///< ... and the mesh each is of
            std::vector<std::vector<unsigned int>> indices; ///< by mesh: three numbers a triangle
        };
    } // namespace

    namespace
    {
        /// How far `p` is from the triangle a b c.
        double toTriangle(const double *p, const double *a, const double *b, const double *c)
        {
            auto sub = [](const double *x, const double *y, double *o)
            { o[0] = x[0] - y[0], o[1] = x[1] - y[1], o[2] = x[2] - y[2]; };
            auto dot = [](const double *x, const double *y) { return x[0] * y[0] + x[1] * y[1] + x[2] * y[2]; };
            double ab[3], ac[3], ap[3];
            sub(b, a, ab), sub(c, a, ac), sub(p, a, ap);
            auto at = [&](double v, double w)
            {
                double q[3];
                for (int k = 0; k < 3; ++k)
                    q[k] = a[k] + ab[k] * v + ac[k] * w - p[k];
                return std::sqrt(dot(q, q));
            };
            const double d1 = dot(ab, ap), d2 = dot(ac, ap);
            if (d1 <= 0 && d2 <= 0)
                return at(0, 0);
            double bp[3];
            sub(p, b, bp);
            const double d3 = dot(ab, bp), d4 = dot(ac, bp);
            if (d3 >= 0 && d4 <= d3)
                return at(1, 0);
            const double vc = d1 * d4 - d3 * d2;
            if (vc <= 0 && d1 >= 0 && d3 <= 0)
                return at(d1 / (d1 - d3), 0);
            double cp[3];
            sub(p, c, cp);
            const double d5 = dot(ab, cp), d6 = dot(ac, cp);
            if (d6 >= 0 && d5 <= d6)
                return at(0, 1);
            const double vb = d5 * d2 - d1 * d6;
            if (vb <= 0 && d2 >= 0 && d6 <= 0)
                return at(0, d2 / (d2 - d6));
            const double va = d3 * d6 - d5 * d4;
            if (va <= 0 && (d4 - d3) >= 0 && (d5 - d6) >= 0)
            {
                const double w = (d4 - d3) / ((d4 - d3) + (d5 - d6));
                return at(1 - w, w);
            }
            const double denom = 1.0 / (va + vb + vc);
            return at(vb * denom, vc * denom);
        }
    } // namespace

    double meshError(const std::vector<MeshTriangle> &source, const std::vector<MeshTriangle> &thin,
                     const std::vector<std::array<double, 3>> &rest)
    {
        std::vector<char> used(rest.size(), 0);
        for (const MeshTriangle &tri : source)
        {
            for (const MdlCorner &corner : tri.c)
                used[corner.vertex] = 1;
        }
        std::vector<double> off;
        for (std::size_t v = 0; v < rest.size(); ++v)
        {
            if (!used[v])
                continue;
            double nearest = 1e30;
            for (const MeshTriangle &tri : thin)
                nearest = std::min(nearest, toTriangle(rest[v].data(), rest[tri.c[0].vertex].data(),
                                                       rest[tri.c[1].vertex].data(), rest[tri.c[2].vertex].data()));
            off.push_back(thin.empty() ? 0 : nearest);
        }
        if (off.empty())
            return 0;
        std::sort(off.begin(), off.end());
        // (Not the very worst: one stray vertex is not what is seen. But
        // nearly: an arm gone thin is a few in a hundred.)
        return off[(off.size() - 1) * 97 / 100];
    }

    std::vector<MeshTriangle> meshTriangles(const MdlSubModel &model)
    {
        std::vector<MeshTriangle> out;
        for (std::size_t m = 0; m < model.meshes.size(); ++m)
        {
            for (const MdlStrip &strip : model.meshes[m].strips)
            {
                const int n = static_cast<int>(strip.corners.size());
                for (int i = 0; i + 2 < n; ++i)
                {
                    MeshTriangle tri;
                    tri.mesh = static_cast<int>(m);
                    if (strip.fan)
                        tri.c[0] = strip.corners[0], tri.c[1] = strip.corners[i + 1], tri.c[2] = strip.corners[i + 2];
                    else if (i & 1)
                        tri.c[0] = strip.corners[i + 1], tri.c[1] = strip.corners[i], tri.c[2] = strip.corners[i + 2];
                    else
                        tri.c[0] = strip.corners[i], tri.c[1] = strip.corners[i + 1], tri.c[2] = strip.corners[i + 2];
                    if (tri.c[0].vertex != tri.c[1].vertex && tri.c[1].vertex != tri.c[2].vertex &&
                        tri.c[0].vertex != tri.c[2].vertex)
                        out.push_back(tri);
                }
            }
        }
        return out;
    }

    int sentVertices(const MdlSubModel &model)
    {
        int total = 0;
        for (const MdlMesh &mesh : model.meshes)
        {
            for (const MdlStrip &strip : mesh.strips)
            {
                const int n = static_cast<int>(strip.corners.size());
                if (n >= 3)
                    total += strip.fan ? (n - 2) * 3 : n;
            }
        }
        return total;
    }

    namespace
    {
        /// The meshes of `model` with these strips in place of their own.
        MdlSubModel withStrips(const MdlSubModel &model, const Numbered &numbered,
                               const std::vector<Strips> &stripsOf)
        {
            MdlSubModel out = model;
            for (std::size_t m = 0; m < out.meshes.size(); ++m)
            {
                MdlMesh &mesh = out.meshes[m];
                mesh.strips.clear();
                mesh.triangles = 0;
                Strips strips = stripsOf[m];
                // Strips that begin on the same bone follow one another:
                // the console changes matrix less often.
                std::stable_sort(strips.begin(), strips.end(),
                                 [&](const std::vector<unsigned int> &a, const std::vector<unsigned int> &b)
                                 {
                                     return model.vertexBone[numbered.corners[a[0]].vertex] <
                                            model.vertexBone[numbered.corners[b[0]].vertex];
                                 });
                for (const auto &indices : strips)
                {
                    MdlStrip strip;
                    for (unsigned int index : indices)
                        strip.corners.push_back(numbered.corners[index]);
                    mesh.triangles += static_cast<int>(strip.corners.size()) - 2;
                    mesh.strips.push_back(std::move(strip));
                }
            }
            return out;
        }
    } // namespace

    MdlSubModel restripped(const MdlSubModel &model)
    {
        // Corner for corner: the same vertex, normal and texel.
        Numbered numbered;
        numbered.indices.resize(model.meshes.size());
        std::map<std::array<int, 5>, unsigned int> known;
        for (const MeshTriangle &tri : meshTriangles(model))
        {
            for (const MdlCorner &corner : tri.c)
            {
                const std::array<int, 5> key = {tri.mesh, corner.vertex, corner.normal, corner.s, corner.t};
                auto found = known.find(key);
                if (found == known.end())
                {
                    found = known.insert({key, static_cast<unsigned int>(numbered.corners.size())}).first;
                    numbered.corners.push_back(corner);
                    numbered.meshOf.push_back(tri.mesh);
                }
                numbered.indices[tri.mesh].push_back(found->second);
            }
        }
        std::vector<Strips> stripsOf(model.meshes.size());
        for (std::size_t m = 0; m < model.meshes.size(); ++m)
            stripsOf[m] = bestStrips(numbered.indices[m], numbered.corners.size());
        const MdlSubModel made = withStrips(model, numbered, stripsOf);
        // (Its own strips, if they were as good: nothing is changed for the
        // sake of changing it.)
        return sentVertices(made) < sentVertices(model) ? made : model;
    }

    MeshInput meshInput(const MdlFile &mdl, const MdlSubModel &model,
                        const std::vector<std::array<double, 12>> &standing,
                        std::vector<std::array<double, 3>> &rest)
    {
        MeshInput input;
        input.model = &model;
        rest.assign(model.vertices.size(), {0, 0, 0});
        for (std::size_t v = 0; v < rest.size(); ++v)
        {
            const auto &bone = standing[model.vertexBone[v]];
            const auto &at = model.vertices[v];
            for (int a = 0; a < 3; ++a)
                rest[v][a] = bone[a * 4] * at[0] + bone[a * 4 + 1] * at[1] + bone[a * 4 + 2] * at[2] + bone[a * 4 + 3];
        }
        input.rest = &rest;
        input.normals.assign(model.normals.size(), {0, 0, 0});
        for (std::size_t n = 0; n < model.normals.size(); ++n)
        {
            const auto &bone = standing[model.normalBone[n]];
            const auto &at = model.normals[n];
            for (int a = 0; a < 3; ++a)
                input.normals[n][a] = bone[a * 4] * at[0] + bone[a * 4 + 1] * at[1] + bone[a * 4 + 2] * at[2];
        }
        const std::vector<int> none;
        const std::vector<int> &skin = mdl.skins.empty() ? none : mdl.skins[0];
        for (const MdlMesh &mesh : model.meshes)
        {
            const int texture =
                mesh.skinRef >= 0 && mesh.skinRef < static_cast<int>(skin.size()) ? skin[mesh.skinRef] : -1;
            if (texture >= 0 && texture < static_cast<int>(mdl.textures.size()) && mdl.textures[texture].width > 0 &&
                mdl.textures[texture].height > 0)
                input.texel.push_back({1.0 / mdl.textures[texture].width, 1.0 / mdl.textures[texture].height});
            else
                input.texel.push_back({0, 0});
        }
        return input;
    }

    std::vector<MeshLevel> meshLevels(const MeshInput &input, const std::vector<int> &targets)
    {
        const MdlSubModel &model = *input.model;
        const std::vector<std::array<double, 3>> &rest = *input.rest;
        std::vector<MeshLevel> out;

        // Vertices that are the same place are one place here, whatever
        // their index (the game repeats a vertex where two bones meet).
        std::vector<int> place(model.vertices.size());
        {
            std::map<std::array<long, 3>, int> first;
            for (std::size_t v = 0; v < model.vertices.size(); ++v)
            {
                const std::array<long, 3> key = {std::lround(rest[v][0] * 64), std::lround(rest[v][1] * 64),
                                                 std::lround(rest[v][2] * 64)};
                place[v] = first.insert({key, static_cast<int>(v)}).first->second;
            }
        }

        // The wedges: a place, on a bone, in a mesh, at a texel. (Not its
        // normal: two corners that differ only in that are one wedge, with
        // the first's. A vertex with one wedge too many cannot be moved.)
        Numbered numbered;
        std::vector<int> placeOf;
        std::vector<unsigned int> indices; // all meshes: the simplifier sees one surface
        {
            std::map<std::array<int, 5>, unsigned int> known;
            for (const MeshTriangle &tri : meshTriangles(model))
            {
                for (const MdlCorner &corner : tri.c)
                {
                    const std::array<int, 5> key = {place[corner.vertex], model.vertexBone[corner.vertex], tri.mesh,
                                                    corner.s, corner.t};
                    auto found = known.find(key);
                    if (found == known.end())
                    {
                        found = known.insert({key, static_cast<unsigned int>(numbered.corners.size())}).first;
                        numbered.corners.push_back(corner);
                        numbered.meshOf.push_back(tri.mesh);
                        placeOf.push_back(place[corner.vertex]);
                    }
                    indices.push_back(found->second);
                }
            }
        }
        const std::size_t wedges = numbered.corners.size();

        // What the simplifier weighs: where (exactly the same numbers for
        // wedges of one place, which is how it knows them for one), and
        // beside that the texel as a part of its texture, and the normal.
        constexpr int kAttributes = 5;
        // (Weighed lightly: on models this small the shape is what is seen
        // first. With these the first level is nearer the model's own shape
        // than the plain thinning this replaced, by a tenth, over the
        // people and monsters it was measured on; heavier, and it is not.)
        constexpr float kTexelWeight = 1.0f, kNormalWeight = 0.2f;
        const float weights[kAttributes] = {kTexelWeight, kTexelWeight, kNormalWeight, kNormalWeight, kNormalWeight};
        std::vector<float> positions(wedges * 3), attributes(wedges * kAttributes, 0.0f);
        std::vector<unsigned char> lock(wedges, 0), protect(wedges, 0);
        {
            // A place with more than one bone is a joint: its vertices are
            // the last to go. A place with more than one texel, or in more
            // than one mesh, is a seam of the skin.
            std::map<int, std::vector<unsigned int>> at;
            for (unsigned int w = 0; w < wedges; ++w)
                at[placeOf[w]].push_back(w);
            for (unsigned int w = 0; w < wedges; ++w)
            {
                const MdlCorner &corner = numbered.corners[w];
                const int mesh = numbered.meshOf[w];
                for (int k = 0; k < 3; ++k)
                    positions[w * 3 + k] = static_cast<float>(rest[placeOf[w]][k]);
                const std::array<double, 2> texel =
                    static_cast<std::size_t>(mesh) < input.texel.size() ? input.texel[mesh] : std::array<double, 2>{0, 0};
                attributes[w * kAttributes + 0] = static_cast<float>(corner.s * texel[0]);
                attributes[w * kAttributes + 1] = static_cast<float>(corner.t * texel[1]);
                if (static_cast<std::size_t>(corner.normal) < input.normals.size())
                {
                    for (int k = 0; k < 3; ++k)
                        attributes[w * kAttributes + 2 + k] = static_cast<float>(input.normals[corner.normal][k]);
                }
                for (unsigned int other : at[placeOf[w]])
                {
                    const MdlCorner &theirs = numbered.corners[other];
                    if (model.vertexBone[theirs.vertex] != model.vertexBone[corner.vertex])
                        lock[w] |= meshopt_SimplifyVertex_Priority;
                    if (numbered.meshOf[other] != mesh || theirs.s != corner.s || theirs.t != corner.t)
                        protect[w] = meshopt_SimplifyVertex_Protect;
                }
            }
        }

        std::vector<unsigned int> current = indices;
        const std::vector<MeshTriangle> source = meshTriangles(model);
        for (int target : targets)
        {
            const std::size_t want = static_cast<std::size_t>(std::max(target, 0)) * 3;
            if (want < current.size())
            {
                // Errors in the game's units; triangles of an even size
                // rather than slivers, which bend badly at a joint.
                const unsigned int options = meshopt_SimplifyErrorAbsolute | meshopt_SimplifyRegularizeLight;
                std::vector<unsigned int> made(current.size());
                float moved = 0;
                made.resize(meshopt_simplifyWithAttributes(made.data(), current.data(), current.size(), positions.data(),
                                                           wedges, sizeof(float) * 3, attributes.data(),
                                                           sizeof(float) * kAttributes, weights, kAttributes, lock.data(),
                                                           want, FLT_MAX, options, &moved));
                if (made.size() > want + want / 8)
                {
                    // Stuck short of it: where a seam, a joint and an edge
                    // meet at one place, that place is held. Let those go
                    // too, keeping only the seams of the skin themselves.
                    std::vector<unsigned char> loose(wedges);
                    for (unsigned int w = 0; w < wedges; ++w)
                        loose[w] = static_cast<unsigned char>(lock[w] | protect[w]);
                    std::vector<unsigned int> again(current.size());
                    float movedAgain = 0;
                    again.resize(meshopt_simplifyWithAttributes(
                        again.data(), current.data(), current.size(), positions.data(), wedges, sizeof(float) * 3,
                        attributes.data(), sizeof(float) * kAttributes, weights, kAttributes, loose.data(), want, FLT_MAX,
                        options | meshopt_SimplifyPermissive, &movedAgain));
                    if (again.size() < made.size())
                    {
                        made = std::move(again);
                        moved = movedAgain;
                    }
                }
                current = std::move(made);
                (void)moved;
            }

            // The level's triangles, each in the mesh its corners are of.
            // (A triangle whose corners ended up in different meshes has no
            // texture to be drawn with: there are none, unless the
            // simplifier was let cross a seam; then it is left out.)
            Numbered level = numbered;
            for (auto &list : level.indices)
                list.clear();
            level.indices.resize(model.meshes.size());
            int triangles = 0;
            for (std::size_t i = 0; i + 2 < current.size(); i += 3)
            {
                const int mesh = numbered.meshOf[current[i]];
                if (numbered.meshOf[current[i + 1]] != mesh || numbered.meshOf[current[i + 2]] != mesh)
                    continue;
                level.indices[mesh].insert(level.indices[mesh].end(), {current[i], current[i + 1], current[i + 2]});
                ++triangles;
            }
            std::vector<Strips> stripsOf(model.meshes.size());
            for (std::size_t m = 0; m < model.meshes.size(); ++m)
                stripsOf[m] = bestStrips(level.indices[m], wedges);
            MeshLevel one;
            one.model = withStrips(model, level, stripsOf);
            one.triangles = triangles;
            one.error = current.size() == indices.size() ? 0 : meshError(source, meshTriangles(one.model), rest);
            out.push_back(std::move(one));
        }
        return out;
    }

    void meshCoverage(const std::vector<MeshTriangle> &triangles, const std::vector<std::array<double, 3>> &rest,
                      float out[3])
    {
        std::vector<float> positions(rest.size() * 3);
        for (std::size_t v = 0; v < rest.size(); ++v)
        {
            for (int k = 0; k < 3; ++k)
                positions[v * 3 + k] = static_cast<float>(rest[v][k]);
        }
        std::vector<unsigned int> indices;
        for (const MeshTriangle &tri : triangles)
        {
            for (const MdlCorner &corner : tri.c)
                indices.push_back(static_cast<unsigned int>(corner.vertex));
        }
        out[0] = out[1] = out[2] = 0;
        if (indices.empty())
            return;
        const meshopt_CoverageStatistics stats =
            meshopt_analyzeCoverage(indices.data(), indices.size(), positions.data(), rest.size(), sizeof(float) * 3);
        for (int k = 0; k < 3; ++k)
            out[k] = stats.coverage[k];
    }
} // namespace hlds
