#include "hlds/ModelCook.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <map>

#include "HlLevelReader.hpp"

namespace hlds
{
    using namespace hldata;

    void mdlAngleQuaternion(const float *angles, double *quat)
    {
        const double sr = std::sin(angles[0] * 0.5), cr = std::cos(angles[0] * 0.5);
        const double sp = std::sin(angles[1] * 0.5), cp = std::cos(angles[1] * 0.5);
        const double sy = std::sin(angles[2] * 0.5), cy = std::cos(angles[2] * 0.5);
        quat[0] = sr * cp * cy - cr * sp * sy;
        quat[1] = cr * sp * cy + sr * cp * sy;
        quat[2] = cr * cp * sy - sr * sp * cy;
        quat[3] = cr * cp * cy + sr * sp * sy;
    }

    namespace
    {
        void quatMatrix(const double *q, double *m) // 3 x 3, row by row
        {
            m[0] = 1 - 2 * q[1] * q[1] - 2 * q[2] * q[2];
            m[1] = 2 * q[0] * q[1] - 2 * q[3] * q[2];
            m[2] = 2 * q[0] * q[2] + 2 * q[3] * q[1];
            m[3] = 2 * q[0] * q[1] + 2 * q[3] * q[2];
            m[4] = 1 - 2 * q[0] * q[0] - 2 * q[2] * q[2];
            m[5] = 2 * q[1] * q[2] - 2 * q[3] * q[0];
            m[6] = 2 * q[0] * q[2] - 2 * q[3] * q[1];
            m[7] = 2 * q[1] * q[2] + 2 * q[3] * q[0];
            m[8] = 1 - 2 * q[0] * q[0] - 2 * q[1] * q[1];
        }
    } // namespace

    void mdlBoneMatrices(const MdlFile &mdl, const MdlPose *poses,
                         std::vector<std::array<double, 12>> &out)
    {
        out.resize(mdl.bones.size());
        for (std::size_t b = 0; b < mdl.bones.size(); ++b)
        {
            double q[4], r[9];
            mdlAngleQuaternion(poses[b].rot, q);
            quatMatrix(q, r);
            std::array<double, 12> local = {r[0], r[1], r[2], poses[b].pos[0], r[3], r[4],
                                            r[5], poses[b].pos[1], r[6], r[7], r[8], poses[b].pos[2]};
            const int parent = mdl.bones[b].parent;
            if (parent < 0)
            {
                out[b] = local;
                continue;
            }
            const std::array<double, 12> &p = out[parent];
            for (int row = 0; row < 3; ++row)
            {
                for (int column = 0; column < 3; ++column)
                    out[b][row * 4 + column] = p[row * 4] * local[column] +
                                               p[row * 4 + 1] * local[4 + column] +
                                               p[row * 4 + 2] * local[8 + column];
                out[b][row * 4 + 3] = p[row * 4] * local[3] + p[row * 4 + 1] * local[7] +
                                      p[row * 4 + 2] * local[11] + p[row * 4 + 3];
            }
        }
    }

    namespace
    {
        std::string lower(std::string text)
        {
            for (char &c : text)
                c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            return text;
        }

        // ------------------------------------------------------------ the skin

        struct AtlasRect
        {
            int x = 0, y = 0, w = 4, h = 4; ///< where the texture's pixels are, in the atlas
            bool flat = false;              ///< chrome: one colour, every corner at its middle
        };

        /// Shelf-pack rectangles (already padded) into `width` x `height`.
        bool shelfPack(const std::vector<std::pair<int, int>> &sizes, int width, int height,
                       std::vector<std::pair<int, int>> &at)
        {
            std::vector<int> order(sizes.size());
            for (std::size_t i = 0; i < order.size(); ++i)
                order[i] = static_cast<int>(i);
            std::stable_sort(order.begin(), order.end(),
                             [&](int a, int b) { return sizes[a].second > sizes[b].second; });
            at.assign(sizes.size(), {0, 0});
            int x = 0, y = 0, shelf = 0;
            for (int i : order)
            {
                if (sizes[i].first > width)
                    return false;
                if (x + sizes[i].first > width)
                {
                    x = 0;
                    y += shelf;
                    shelf = 0;
                }
                if (y + sizes[i].second > height)
                    return false;
                at[i] = {x, y};
                x += sizes[i].first;
                shelf = std::max(shelf, sizes[i].second);
            }
            return true;
        }

        /// All of a model's textures in one image. Each is scaled by the same
        /// factor -- the largest that lets them all fit the smallest atlas
        /// that does them justice -- and given a border of its own edge
        /// pixels, so that neither the 4 x 4 blocks of the compression nor a
        /// coordinate that lands a hair outside pick up a neighbour.
        void buildAtlas(const MdlFile &mdl, RgbaImage &atlas, std::vector<AtlasRect> &rects,
                        int mostTexels)
        {
            rects.assign(mdl.textures.size(), AtlasRect());
            if (mdl.textures.empty())
                return;
            constexpr int kBorder = 2;
            static const int kSizes[][2] = {{32, 32},   {64, 32},   {64, 64},  {128, 64},
                                            {128, 128}, {256, 128}, {256, 256}};
            static const double kScales[] = {1.0, 0.75, 0.625, 0.5, 0.375, 0.3125, 0.25, 0.1875, 0.125};

            int width = 0, height = 0;
            std::vector<std::pair<int, int>> at;
            std::vector<std::pair<int, int>> inner;
            bool placed = false;
            // A smaller atlas wins if it holds the textures at half size or
            // better; the largest takes whatever scale fits.
            for (int pass = 0; pass < 2 && !placed; ++pass)
            {
                const int sizeCount = static_cast<int>(sizeof(kSizes) / sizeof(kSizes[0]));
                for (int n = 0; n < sizeCount; ++n)
                {
                    // The first time round from the smallest up; the second,
                    // when nothing held them at half size, from the largest
                    // allowed down.
                    const int *size = kSizes[pass == 0 ? n : sizeCount - 1 - n];
                    // (The smallest is always allowed.)
                    if (size[0] * size[1] > mostTexels && size[0] * size[1] > 32 * 32)
                    {
                        if (pass == 0)
                            break;
                        continue;
                    }
                    for (double scale : kScales)
                    {
                        if (pass == 0 && scale < 0.5)
                            break;
                        std::vector<std::pair<int, int>> padded;
                        inner.clear();
                        for (const MdlTexture &texture : mdl.textures)
                        {
                            const bool chrome = (texture.flags & kMdlTextureChrome) != 0;
                            int w = chrome ? 4 : std::max(4, static_cast<int>(std::lround(texture.width * scale)));
                            int h = chrome ? 4 : std::max(4, static_cast<int>(std::lround(texture.height * scale)));
                            inner.push_back({w, h});
                            // Whole 4 x 4 blocks, border included.
                            padded.push_back({(w + 2 * kBorder + 3) & ~3, (h + 2 * kBorder + 3) & ~3});
                        }
                        if (shelfPack(padded, size[0], size[1], at))
                        {
                            width = size[0];
                            height = size[1];
                            placed = true;
                            break;
                        }
                    }
                    if (placed)
                        break;
                }
            }
            if (!placed)
                return; // more textures than fit at an eighth: the model goes untextured

            atlas.width = width;
            atlas.height = height;
            atlas.rgba.assign(static_cast<std::size_t>(width) * height * 4, 0);
            for (std::size_t t = 0; t < mdl.textures.size(); ++t)
            {
                const MdlTexture &texture = mdl.textures[t];
                RgbaImage source;
                source.width = texture.width;
                source.height = texture.height;
                source.rgba.resize(texture.pixels.size() * 4);
                std::uint64_t sum[3] = {0, 0, 0};
                for (std::size_t i = 0; i < texture.pixels.size(); ++i)
                {
                    for (int c = 0; c < 3; ++c)
                    {
                        source.rgba[i * 4 + c] = texture.palette[texture.pixels[i] * 3 + c];
                        sum[c] += source.rgba[i * 4 + c];
                    }
                    source.rgba[i * 4 + 3] = 255;
                }
                AtlasRect &rect = rects[t];
                rect.w = inner[t].first;
                rect.h = inner[t].second;
                rect.x = at[t].first + kBorder;
                rect.y = at[t].second + kBorder;
                rect.flat = (texture.flags & kMdlTextureChrome) != 0;
                RgbaImage scaled = rect.flat ? RgbaImage() : resample(source, rect.w, rect.h);
                for (int y = -kBorder; y < rect.h + kBorder; ++y)
                {
                    for (int x = -kBorder; x < rect.w + kBorder; ++x)
                    {
                        std::uint8_t *to = atlas.at(rect.x + x, rect.y + y);
                        if (rect.flat)
                        {
                            // Chrome is a reflection the DS does not do: its
                            // average colour stands in.
                            for (int c = 0; c < 3; ++c)
                                to[c] = static_cast<std::uint8_t>(sum[c] / std::max<std::size_t>(1, texture.pixels.size()));
                        }
                        else
                        {
                            const std::uint8_t *from = scaled.at(std::clamp(x, 0, rect.w - 1),
                                                                 std::clamp(y, 0, rect.h - 1));
                            to[0] = from[0], to[1] = from[1], to[2] = from[2];
                        }
                        to[3] = 255;
                    }
                }
            }
        }

        // ------------------------------------------------------------ streams

        struct Command
        {
            std::uint8_t code;
            std::uint8_t count;
            std::uint32_t params[2];
        };

        void pack(const std::vector<Command> &commands, std::vector<std::uint32_t> &words)
        {
            for (std::size_t i = 0; i < commands.size(); i += 4)
            {
                std::uint32_t head = 0;
                for (std::size_t k = 0; k < 4 && i + k < commands.size(); ++k)
                    head |= static_cast<std::uint32_t>(commands[i + k].code) << (8 * k);
                words.push_back(head);
                for (std::size_t k = 0; k < 4 && i + k < commands.size(); ++k)
                {
                    for (int p = 0; p < commands[i + k].count; ++p)
                        words.push_back(commands[i + k].params[p]);
                }
            }
        }

        /// One sub-model's meshes as batches of at most kModelBatchBones bones.
        void buildBatches(const MdlFile &mdl, const MdlSubModel &model, int bodyPart, int subModel,
                          const std::vector<AtlasRect> &rects, int vertexShift, CookedModel &out)
        {
            struct Open
            {
                std::vector<int> bones;
                std::vector<Command> commands;
                int polygons = 0;
                int vertices = 0;
            };
            Open batch;
            auto flush = [&]()
            {
                if (batch.commands.empty())
                    return;
                ModelBatch record{};
                record.gxOfs = static_cast<std::uint32_t>(out.gx.size());
                pack(batch.commands, out.gx);
                record.gxWords = static_cast<std::uint16_t>(out.gx.size() - record.gxOfs);
                record.polygons = static_cast<std::uint16_t>(batch.polygons);
                record.vertices = static_cast<std::uint16_t>(batch.vertices);
                record.bodyPart = static_cast<std::uint8_t>(bodyPart);
                record.subModel = static_cast<std::uint8_t>(subModel);
                record.boneCount = static_cast<std::uint8_t>(batch.bones.size());
                for (std::size_t i = 0; i < batch.bones.size(); ++i)
                    record.bones[i] = static_cast<std::uint8_t>(batch.bones[i]);
                out.batches.push_back(record);
                batch = Open();
            };

            const std::vector<int> &skin = mdl.skins.empty() ? std::vector<int>() : mdl.skins[0];
            for (const MdlMesh &mesh : model.meshes)
            {
                const int textureIndex =
                    mesh.skinRef >= 0 && mesh.skinRef < static_cast<int>(skin.size()) ? skin[mesh.skinRef] : -1;
                const AtlasRect rect = textureIndex >= 0 ? rects[textureIndex] : AtlasRect();
                const MdlTexture *texture = textureIndex >= 0 ? &mdl.textures[textureIndex] : nullptr;

                for (const MdlStrip &strip : mesh.strips)
                {
                    if (strip.corners.size() < 3)
                        continue;
                    // The bones this strip needs that the batch does not have.
                    std::vector<int> fresh;
                    for (const MdlCorner &corner : strip.corners)
                    {
                        const int bone = model.vertexBone[corner.vertex];
                        if (std::find(batch.bones.begin(), batch.bones.end(), bone) == batch.bones.end() &&
                            std::find(fresh.begin(), fresh.end(), bone) == fresh.end())
                            fresh.push_back(bone);
                    }
                    if (batch.bones.size() + fresh.size() > static_cast<std::size_t>(kModelBatchBones))
                    {
                        flush();
                        fresh.clear();
                        for (const MdlCorner &corner : strip.corners)
                        {
                            const int bone = model.vertexBone[corner.vertex];
                            if (std::find(fresh.begin(), fresh.end(), bone) == fresh.end())
                                fresh.push_back(bone);
                        }
                        // A strip on more bones than a batch holds does not
                        // happen in the game's models; it would be left out.
                        if (fresh.size() > static_cast<std::size_t>(kModelBatchBones))
                            continue;
                    }
                    batch.bones.insert(batch.bones.end(), fresh.begin(), fresh.end());

                    int currentBone = -1;
                    std::uint32_t currentNormal = 0xFFFFFFFFu;
                    auto emit = [&](const MdlCorner &corner)
                    {
                        const int bone = model.vertexBone[corner.vertex];
                        if (bone != currentBone)
                        {
                            currentBone = bone;
                            const auto slot = static_cast<std::uint32_t>(
                                std::find(batch.bones.begin(), batch.bones.end(), bone) -
                                batch.bones.begin() + kModelFirstSlot);
                            batch.commands.push_back(Command{kGxMtxRestore, 1, {slot, 0}});
                        }
                        const auto &n = model.normals[corner.normal];
                        const double length = std::sqrt(n[0] * n[0] + n[1] * n[1] + n[2] * n[2]);
                        std::uint32_t normal = 0;
                        for (int a = 0; a < 3; ++a)
                        {
                            const int v = static_cast<int>(
                                std::lround(std::clamp(length > 0 ? n[a] / length : 0.0, -1.0, 1.0) * 511));
                            normal |= static_cast<std::uint32_t>(v & 0x3FF) << (10 * a);
                        }
                        if (normal != currentNormal)
                        {
                            currentNormal = normal;
                            batch.commands.push_back(Command{kGxNormal, 1, {normal, 0}});
                        }
                        if (texture != nullptr)
                        {
                            double s = rect.x + rect.w * 0.5, t = rect.y + rect.h * 0.5;
                            if (!rect.flat)
                            {
                                s = rect.x + (corner.s + 0.5) * rect.w / texture->width;
                                t = rect.y + (corner.t + 0.5) * rect.h / texture->height;
                            }
                            const auto s16 = static_cast<std::int16_t>(std::lround(s * 16));
                            const auto t16 = static_cast<std::int16_t>(std::lround(t * 16));
                            batch.commands.push_back(
                                Command{kGxTexCoord, 1,
                                        {static_cast<std::uint16_t>(s16) |
                                             (static_cast<std::uint32_t>(static_cast<std::uint16_t>(t16)) << 16),
                                         0}});
                        }
                        const auto &v = model.vertices[corner.vertex];
                        std::uint16_t xyz[3];
                        for (int a = 0; a < 3; ++a)
                        {
                            const double raw = std::round(v[a] * (1 << kModelVertexShift) / (1 << vertexShift));
                            xyz[a] = static_cast<std::uint16_t>(
                                static_cast<std::int16_t>(std::clamp(raw, -32768.0, 32767.0)));
                        }
                        batch.commands.push_back(
                            Command{kGxVtx16, 2, {xyz[0] | (static_cast<std::uint32_t>(xyz[1]) << 16), xyz[2]}});
                        ++batch.vertices;
                    };

                    const int n = static_cast<int>(strip.corners.size());
                    if (strip.fan)
                    {
                        // The hardware has no fans: each triangle on its own.
                        batch.commands.push_back(Command{kGxBegin, 1, {0, 0}});
                        for (int i = 1; i + 1 < n; ++i)
                        {
                            emit(strip.corners[0]);
                            emit(strip.corners[i]);
                            emit(strip.corners[i + 1]);
                        }
                    }
                    else
                    {
                        batch.commands.push_back(Command{kGxBegin, 1, {2, 0}});
                        for (const MdlCorner &corner : strip.corners)
                            emit(corner);
                    }
                    batch.polygons += n - 2;
                }
            }
            flush();
        }

        // ------------------------------------------------------------ fewer triangles

        // A scientist is 740 triangles and the DS draws 2048 a frame, world
        // included. So a model's skin is thinned before it is cooked: edges
        // are collapsed, cheapest first, until it is down to its target.
        //
        // A collapse moves one end of an edge ONTO the other -- no new
        // vertices are made, so every vertex that is left is one of the
        // model's own, on its own bone, and the skinning stays exactly as
        // rigid as it was. The cost of a collapse is how far it moves the
        // surface (the sum of squared distances to the planes of the
        // triangles that met at the vertex that goes), measured with the
        // model standing in its first sequence. It is refused where it would
        // turn a triangle over, pull the open edge of a mesh inwards, or move
        // a vertex that sits on a seam of the texture.

        struct Quadric
        {
            double q[10] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0}; // a2 ab ac ad b2 bc bd c2 cd d2

            void addPlane(double a, double b, double c, double d, double weight)
            {
                const double v[10] = {a * a, a * b, a * c, a * d, b * b, b * c, b * d, c * c, c * d, d * d};
                for (int i = 0; i < 10; ++i)
                    q[i] += v[i] * weight;
            }
            void add(const Quadric &o)
            {
                for (int i = 0; i < 10; ++i)
                    q[i] += o.q[i];
            }
            double at(const double *p) const
            {
                return q[0] * p[0] * p[0] + 2 * q[1] * p[0] * p[1] + 2 * q[2] * p[0] * p[2] + 2 * q[3] * p[0] +
                       q[4] * p[1] * p[1] + 2 * q[5] * p[1] * p[2] + 2 * q[6] * p[1] + q[7] * p[2] * p[2] +
                       2 * q[8] * p[2] + q[9];
            }
        };

        struct Tri
        {
            MdlCorner c[3];
            int mesh = 0;
            bool alive = true;
        };

        void cross(const double *a, const double *b, const double *c, double *n)
        {
            const double e1[3] = {b[0] - a[0], b[1] - a[1], b[2] - a[2]};
            const double e2[3] = {c[0] - a[0], c[1] - a[1], c[2] - a[2]};
            n[0] = e1[1] * e2[2] - e1[2] * e2[1];
            n[1] = e1[2] * e2[0] - e1[0] * e2[2];
            n[2] = e1[0] * e2[1] - e1[1] * e2[0];
        }

        /// The triangles of `tris` that are left, as strips: each grown from
        /// a triangle through whichever neighbours share an edge with the
        /// same corners (place, normal and texture coordinate alike).
        void stripify(const std::vector<Tri> &tris, int mesh, std::vector<MdlStrip> &out)
        {
            using Key = std::array<int, 4>;
            auto keyOf = [](const MdlCorner &c) { return Key{c.vertex, c.normal, c.s, c.t}; };
            std::vector<int> mine;
            for (std::size_t i = 0; i < tris.size(); ++i)
            {
                if (tris[i].alive && tris[i].mesh == mesh)
                    mine.push_back(static_cast<int>(i));
            }
            // Directed edge -> the triangle that has it, and which corner is
            // the one not on it.
            std::map<std::pair<Key, Key>, std::pair<int, int>> edges;
            for (int t : mine)
            {
                for (int e = 0; e < 3; ++e)
                    edges.insert({{keyOf(tris[t].c[e]), keyOf(tris[t].c[(e + 1) % 3])}, {t, (e + 2) % 3}});
            }
            std::vector<char> used(tris.size(), 0);
            auto grow = [&](int start, int rotation, std::vector<char> &taken)
            {
                std::vector<MdlCorner> strip = {tris[start].c[rotation], tris[start].c[(rotation + 1) % 3],
                                                tris[start].c[(rotation + 2) % 3]};
                std::vector<int> members = {start};
                taken[start] = 1;
                for (;;)
                {
                    const std::size_t i = strip.size() - 2; // the triangle to add
                    const MdlCorner &p = strip[i], &q = strip[i + 1];
                    const auto found = (i & 1) ? edges.find({keyOf(q), keyOf(p)})
                                               : edges.find({keyOf(p), keyOf(q)});
                    if (found == edges.end() || taken[found->second.first])
                        break;
                    taken[found->second.first] = 1;
                    members.push_back(found->second.first);
                    strip.push_back(tris[found->second.first].c[found->second.second]);
                }
                for (int member : members)
                    taken[member] = 0;
                return std::make_pair(strip, members);
            };
            for (int t : mine)
            {
                if (used[t])
                    continue;
                auto best = grow(t, 0, used);
                for (int rotation = 1; rotation < 3; ++rotation)
                {
                    auto other = grow(t, rotation, used);
                    if (other.second.size() > best.second.size())
                        best = std::move(other);
                }
                for (int member : best.second)
                    used[member] = 1;
                MdlStrip strip;
                strip.corners = std::move(best.first);
                out.push_back(std::move(strip));
            }
        }

        /// `model` with its meshes thinned to about `target` triangles. `rest`
        /// is where each of its vertices is with the model standing (model
        /// space); the vertices and normals themselves are not touched.
        MdlSubModel thinned(const MdlSubModel &model, const std::vector<std::array<double, 3>> &rest,
                            int target)
        {
            std::vector<Tri> tris;
            for (std::size_t m = 0; m < model.meshes.size(); ++m)
            {
                for (const MdlStrip &strip : model.meshes[m].strips)
                {
                    const int n = static_cast<int>(strip.corners.size());
                    for (int i = 0; i + 2 < n; ++i)
                    {
                        Tri tri;
                        tri.mesh = static_cast<int>(m);
                        if (strip.fan)
                            tri.c[0] = strip.corners[0], tri.c[1] = strip.corners[i + 1], tri.c[2] = strip.corners[i + 2];
                        else if (i & 1)
                            tri.c[0] = strip.corners[i + 1], tri.c[1] = strip.corners[i], tri.c[2] = strip.corners[i + 2];
                        else
                            tri.c[0] = strip.corners[i], tri.c[1] = strip.corners[i + 1], tri.c[2] = strip.corners[i + 2];
                        if (tri.c[0].vertex != tri.c[1].vertex && tri.c[1].vertex != tri.c[2].vertex &&
                            tri.c[0].vertex != tri.c[2].vertex)
                            tris.push_back(tri);
                    }
                }
            }
            int alive = static_cast<int>(tris.size());
            const std::size_t vertexCount = model.vertices.size();

            // Vertices that are the same place are one vertex here, whatever
            // their index (the game repeats a vertex where two bones meet).
            std::vector<int> same(vertexCount);
            {
                std::map<std::array<long, 3>, int> first;
                for (std::size_t v = 0; v < vertexCount; ++v)
                {
                    const std::array<long, 3> key = {std::lround(rest[v][0] * 64), std::lround(rest[v][1] * 64),
                                                     std::lround(rest[v][2] * 64)};
                    same[v] = first.insert({key, static_cast<int>(v)}).first->second;
                }
            }
            auto place = [&](int v) { return same[v]; };

            std::vector<Quadric> quadric(vertexCount);
            std::vector<std::vector<int>> around(vertexCount); // by place: its triangles
            std::map<std::pair<int, int>, int> edgeUse;        // undirected, by place
            for (std::size_t t = 0; t < tris.size(); ++t)
            {
                double n[3];
                cross(rest[tris[t].c[0].vertex].data(), rest[tris[t].c[1].vertex].data(),
                      rest[tris[t].c[2].vertex].data(), n);
                const double length = std::sqrt(n[0] * n[0] + n[1] * n[1] + n[2] * n[2]);
                for (int k = 0; k < 3; ++k)
                {
                    const int v = place(tris[t].c[k].vertex);
                    around[v].push_back(static_cast<int>(t));
                    if (length > 0)
                    {
                        const double *p = rest[tris[t].c[0].vertex].data();
                        const double a = n[0] / length, b = n[1] / length, c = n[2] / length;
                        quadric[v].addPlane(a, b, c, -(a * p[0] + b * p[1] + c * p[2]), length);
                    }
                    const int w = place(tris[t].c[(k + 1) % 3].vertex);
                    ++edgeUse[{std::min(v, w), std::max(v, w)}];
                }
            }
            // An edge with a triangle on one side only is the rim of the
            // mesh (a cuff, a collar, where the head joins): a vertex on one
            // may only slide along the rim.
            std::vector<char> onRim(vertexCount, 0);
            for (const auto &edge : edgeUse)
            {
                if (edge.second == 1)
                    onRim[edge.first.first] = onRim[edge.first.second] = 1;
            }
            auto rimEdge = [&](int v, int u)
            {
                const auto found = edgeUse.find({std::min(v, u), std::max(v, u)});
                return found != edgeUse.end() && found->second == 1;
            };

            while (alive > target)
            {
                double bestCost = 1e300;
                int bestV = -1, bestU = -1;
                for (std::size_t v = 0; v < vertexCount; ++v)
                {
                    if (around[v].empty())
                        continue;
                    // The corners v is used with must agree on its texture
                    // coordinate within each mesh, or v is on a seam.
                    bool seam = false;
                    std::map<int, std::pair<int, int>> st;
                    std::vector<int> neighbours;
                    for (int t : around[v])
                    {
                        for (int k = 0; k < 3; ++k)
                        {
                            const MdlCorner &c = tris[t].c[k];
                            if (place(c.vertex) == static_cast<int>(v))
                            {
                                const auto found = st.insert({tris[t].mesh, {c.s, c.t}});
                                seam = seam || found.first->second != std::make_pair(c.s, c.t);
                            }
                            else if (std::find(neighbours.begin(), neighbours.end(), place(c.vertex)) ==
                                     neighbours.end())
                            {
                                neighbours.push_back(place(c.vertex));
                            }
                        }
                    }
                    if (seam)
                        continue;
                    for (int u : neighbours)
                    {
                        if (onRim[v] && !rimEdge(static_cast<int>(v), u))
                            continue;
                        Quadric both = quadric[v];
                        both.add(quadric[u]);
                        double cost = both.at(rest[u].data());
                        // Onto another bone costs more: that is where the
                        // model bends.
                        if (model.vertexBone[v] != model.vertexBone[u])
                            cost = cost * 4 + 1e-3;
                        if (cost >= bestCost)
                            continue;
                        // Every triangle that keeps going must still face the
                        // way it did, and u must have a corner of its own in
                        // that triangle's mesh to lend it.
                        bool ok = true;
                        for (int t : around[v])
                        {
                            const Tri &tri = tris[t];
                            bool hasU = false;
                            for (int k = 0; k < 3; ++k)
                                hasU = hasU || place(tri.c[k].vertex) == u;
                            if (hasU)
                                continue;
                            const double *p[3];
                            const double *moved[3];
                            for (int k = 0; k < 3; ++k)
                            {
                                p[k] = rest[tri.c[k].vertex].data();
                                moved[k] = place(tri.c[k].vertex) == static_cast<int>(v) ? rest[u].data() : p[k];
                            }
                            double before[3], after[3];
                            cross(p[0], p[1], p[2], before);
                            cross(moved[0], moved[1], moved[2], after);
                            const double dot = before[0] * after[0] + before[1] * after[1] + before[2] * after[2];
                            const double lb = std::sqrt(before[0] * before[0] + before[1] * before[1] + before[2] * before[2]);
                            const double la = std::sqrt(after[0] * after[0] + after[1] * after[1] + after[2] * after[2]);
                            if (la < 1e-6 || dot < 0.3 * lb * la)
                            {
                                ok = false;
                                break;
                            }
                            bool lends = false;
                            for (int o : around[u])
                            {
                                for (int k = 0; k < 3 && !lends; ++k)
                                    lends = tris[o].mesh == tri.mesh && place(tris[o].c[k].vertex) == u;
                            }
                            if (!lends)
                            {
                                ok = false;
                                break;
                            }
                        }
                        if (ok)
                        {
                            bestCost = cost;
                            bestV = static_cast<int>(v);
                            bestU = u;
                        }
                    }
                }
                if (bestV < 0)
                    break; // nothing more can go without doing harm

                // v onto u: the triangles with both are gone, the rest take
                // u's corner (of their own mesh, from a triangle that is on
                // the edge if there is one).
                const int v = bestV, u = bestU;
                for (int t : around[v])
                {
                    Tri &tri = tris[t];
                    if (!tri.alive)
                        continue;
                    bool hasU = false;
                    for (int k = 0; k < 3; ++k)
                        hasU = hasU || place(tri.c[k].vertex) == u;
                    if (hasU)
                    {
                        tri.alive = false;
                        --alive;
                        for (int k = 0; k < 3; ++k)
                        {
                            const int a = place(tri.c[k].vertex), b = place(tri.c[(k + 1) % 3].vertex);
                            --edgeUse[{std::min(a, b), std::max(a, b)}];
                        }
                        continue;
                    }
                    MdlCorner lend{};
                    bool found = false, onEdge = false;
                    for (int o : around[u])
                    {
                        if (tris[o].mesh != tri.mesh)
                            continue;
                        bool touchesV = false;
                        for (int k = 0; k < 3; ++k)
                            touchesV = touchesV || place(tris[o].c[k].vertex) == v;
                        for (int k = 0; k < 3; ++k)
                        {
                            if (place(tris[o].c[k].vertex) == u && (!found || (touchesV && !onEdge)))
                            {
                                lend = tris[o].c[k];
                                found = true;
                                onEdge = touchesV;
                            }
                        }
                    }
                    for (int k = 0; k < 3; ++k)
                    {
                        const int a = place(tri.c[k].vertex), b = place(tri.c[(k + 1) % 3].vertex);
                        --edgeUse[{std::min(a, b), std::max(a, b)}];
                    }
                    for (int k = 0; k < 3; ++k)
                    {
                        if (place(tri.c[k].vertex) == v)
                            tri.c[k] = lend;
                    }
                    for (int k = 0; k < 3; ++k)
                    {
                        const int a = place(tri.c[k].vertex), b = place(tri.c[(k + 1) % 3].vertex);
                        ++edgeUse[{std::min(a, b), std::max(a, b)}];
                    }
                    around[u].push_back(t);
                }
                around[v].clear();
                around[u].erase(std::remove_if(around[u].begin(), around[u].end(),
                                               [&](int t) { return !tris[t].alive; }),
                                around[u].end());
                quadric[u].add(quadric[v]);
                onRim[u] = onRim[u] || onRim[v];
            }

            MdlSubModel out = model;
            for (std::size_t m = 0; m < out.meshes.size(); ++m)
            {
                out.meshes[m].strips.clear();
                stripify(tris, static_cast<int>(m), out.meshes[m].strips);
                out.meshes[m].triangles = 0;
                for (const MdlStrip &strip : out.meshes[m].strips)
                    out.meshes[m].triangles += static_cast<int>(strip.corners.size()) - 2;
            }
            return out;
        }

        // ------------------------------------------------------------ animation

        struct BoneFrame
        {
            double quat[4];
            double pos[3];
        };

        /// Reconstruct frame `f` from keys at every `step` frames (and the
        /// last), the way the DS will: a straight blend, made unit again.
        BoneFrame blendKeys(const std::vector<BoneFrame> &frames, int step, int f)
        {
            const int last = static_cast<int>(frames.size()) - 1;
            const int keyCount = (last + step - 1) / step + 1;
            int index = std::min(f / step, keyCount - 2);
            const int from = index * step;
            const int to = index + 2 == keyCount ? last : from + step;
            const double s = to > from ? double(f - from) / (to - from) : 0;
            const BoneFrame &a = frames[from];
            const BoneFrame &b = frames[to];
            BoneFrame out;
            double dot = 0;
            for (int i = 0; i < 4; ++i)
                dot += a.quat[i] * b.quat[i];
            const double sign = dot < 0 ? -1 : 1;
            double length = 0;
            for (int i = 0; i < 4; ++i)
            {
                out.quat[i] = a.quat[i] + (sign * b.quat[i] - a.quat[i]) * s;
                length += out.quat[i] * out.quat[i];
            }
            length = std::sqrt(length);
            for (int i = 0; i < 4; ++i)
                out.quat[i] /= length > 0 ? length : 1;
            for (int i = 0; i < 3; ++i)
                out.pos[i] = a.pos[i] + (b.pos[i] - a.pos[i]) * s;
            return out;
        }

        ModelKey rotKey(const BoneFrame &frame)
        {
            // q and -q are the same turn: stored as the one whose w is not
            // negative, so that w can be left out.
            const double sign = frame.quat[3] < 0 ? -1 : 1;
            ModelKey key{};
            for (int i = 0; i < 3; ++i)
                key.v[i] = static_cast<std::int16_t>(
                    std::clamp(std::lround(sign * frame.quat[i] * (1 << kModelQuatShift)), -16384L, 16384L));
            return key;
        }

        ModelKey posKey(const BoneFrame &frame)
        {
            ModelKey key{};
            for (int i = 0; i < 3; ++i)
                key.v[i] = static_cast<std::int16_t>(
                    std::clamp(std::lround(frame.pos[i] * (1 << kModelPosShift)), -32768L, 32767L));
            return key;
        }
    } // namespace

    bool cookModel(const MdlFile &mdl, const std::vector<std::string> &clipNames, CookedModel &out,
                   ModelCookReport &report, std::string &error, int skinTexels)
    {
        out = CookedModel();
        report = ModelCookReport();
        if (mdl.bones.empty() || mdl.bones.size() > static_cast<std::size_t>(kModelMaxBones))
        {
            error = mdl.name + " has more bones than a cooked model holds";
            return false;
        }
        for (const MdlBone &bone : mdl.bones)
            out.parents.push_back(static_cast<std::int16_t>(bone.parent));

        // ---- how coarse the vertices must be to fit sixteen bits
        double furthest = 0;
        for (const MdlBodyPart &part : mdl.bodyParts)
        {
            for (const MdlSubModel &model : part.models)
            {
                for (const auto &v : model.vertices)
                    furthest = std::max({furthest, std::fabs(double(v[0])), std::fabs(double(v[1])),
                                         std::fabs(double(v[2]))});
            }
        }
        int vertexShift = 0;
        while (furthest * (1 << kModelVertexShift) / (1 << vertexShift) > 32000 && vertexShift < 6)
            ++vertexShift;

        // ---- the skin
        std::vector<AtlasRect> rects;
        buildAtlas(mdl, out.atlas, rects, skinTexels);
        const bool textured = out.atlas.width != 0;
        if (!textured)
            rects.assign(mdl.textures.size(), AtlasRect());

        // ---- how it stands: every bone's matrix in its first sequence
        std::vector<std::array<double, 12>> standing(mdl.bones.size(),
                                                     {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0});
        if (!mdl.sequences.empty())
        {
            std::vector<MdlPose> poses;
            if (!mdl.decode(0, poses, error))
                return false;
            mdlBoneMatrices(mdl, poses.data(), standing);
        }

        // ---- how many triangles it may keep
        int sourceTriangles = 0;
        for (const MdlSubModel *model : mdl.selectBody(0))
            sourceTriangles += model->triangleCount();
        report.sourceTriangles = sourceTriangles;
        const int allowed = sourceTriangles > kModelTriangleTarget
                                ? std::max(kModelTriangleTarget, sourceTriangles / 4)
                                : sourceTriangles;

        // ---- the meshes
        int worstPolygons = 0;
        for (std::size_t p = 0; p < mdl.bodyParts.size(); ++p)
        {
            const MdlBodyPart &part = mdl.bodyParts[p];
            if (part.models.empty())
                continue;
            ModelBodyPart record{};
            record.base = static_cast<std::uint16_t>(std::clamp(part.base, 1, 65535));
            record.count = static_cast<std::uint16_t>(part.models.size());
            const int index = static_cast<int>(out.bodyParts.size());
            out.bodyParts.push_back(record);
            int most = 0;
            for (std::size_t m = 0; m < part.models.size(); ++m)
            {
                const std::size_t before = out.batches.size();
                // Thinned first, every part in the same proportion.
                const MdlSubModel &source = part.models[m];
                std::vector<std::array<double, 3>> rest(source.vertices.size());
                for (std::size_t v = 0; v < rest.size(); ++v)
                {
                    const auto &bone = standing[source.vertexBone[v]];
                    const auto &at = source.vertices[v];
                    for (int a = 0; a < 3; ++a)
                        rest[v][a] = bone[a * 4] * at[0] + bone[a * 4 + 1] * at[1] + bone[a * 4 + 2] * at[2] +
                                     bone[a * 4 + 3];
                }
                const int had = source.triangleCount();
                const MdlSubModel cut =
                    allowed < sourceTriangles
                        ? thinned(source, rest,
                                  static_cast<int>(static_cast<long>(had) * allowed / sourceTriangles))
                        : source;
                if (m == static_cast<std::size_t>((0 / part.base) % part.models.size()))
                    report.triangles += cut.triangleCount();
                buildBatches(mdl, cut, index, static_cast<int>(m), rects, vertexShift, out);
                int polygons = 0;
                for (std::size_t b = before; b < out.batches.size(); ++b)
                    polygons += out.batches[b].polygons;
                most = std::max(most, polygons);
            }
            worstPolygons += most;
        }
        report.batches = static_cast<int>(out.batches.size());

        // ---- its size, from how it stands in its first sequence
        double lo[3] = {1e9, 1e9, 1e9}, hi[3] = {-1e9, -1e9, -1e9}, radius = 1;
        {
            const std::vector<std::array<double, 12>> &matrices = standing;
            for (const MdlBodyPart &part : mdl.bodyParts)
            {
                for (const MdlSubModel &model : part.models)
                {
                    for (std::size_t v = 0; v < model.vertices.size(); ++v)
                    {
                        const auto &m = matrices[model.vertexBone[v]];
                        const auto &p = model.vertices[v];
                        double w[3];
                        for (int a = 0; a < 3; ++a)
                        {
                            w[a] = m[a * 4] * p[0] + m[a * 4 + 1] * p[1] + m[a * 4 + 2] * p[2] + m[a * 4 + 3];
                            lo[a] = std::min(lo[a], w[a]);
                            hi[a] = std::max(hi[a], w[a]);
                        }
                        radius = std::max(radius, std::sqrt(w[0] * w[0] + w[1] * w[1] + w[2] * w[2]));
                    }
                }
            }
        }
        if (lo[0] > hi[0])
            lo[0] = lo[1] = lo[2] = hi[0] = hi[1] = hi[2] = 0;

        // ---- the clips
        std::vector<std::string> wanted;
        for (const std::string &name : clipNames)
        {
            const std::string label = lower(name);
            if (!label.empty() && std::find(wanted.begin(), wanted.end(), label) == wanted.end())
                wanted.push_back(label);
        }
        for (const std::string &label : wanted)
        {
            const int index = mdl.sequenceByName(label);
            if (index < 0)
            {
                report.missingClips.push_back(label);
                continue;
            }
            const MdlSequence &seq = mdl.sequences[index];
            std::vector<MdlPose> poses;
            if (!mdl.decode(index, poses, error))
                return false;
            const int boneCount = static_cast<int>(mdl.bones.size());
            const int frameCount = seq.frameCount;

            ModelClip clip{};
            clip.name = nameHash(label.c_str());
            clip.frameCount = static_cast<std::uint16_t>(frameCount);
            clip.fps = static_cast<std::uint16_t>(
                std::clamp(std::lround(std::max(0.01f, seq.fps) * 256), 1L, 65535L));
            clip.flags = (seq.flags & kMdlSequenceLooping) ? kClipLoops : 0;
            clip.firstTrack = static_cast<std::uint32_t>(out.tracks.size());
            for (int a = 0; a < 3; ++a)
                clip.travel[a] = fxFromDouble(seq.linearMovement[a]);
            clip.firstEvent = static_cast<std::uint16_t>(out.events.size());
            for (const MdlEvent &event : seq.events)
            {
                // 1003: fire the target named in the options. The others are
                // sounds and things for the monster's own code.
                if (event.event != 1003 || event.options.empty())
                    continue;
                ModelEvent record{};
                record.frame = static_cast<std::uint16_t>(std::clamp(event.frame, 0, frameCount - 1));
                record.target = nameHash(lower(event.options).c_str());
                out.events.push_back(record);
            }
            clip.eventCount = static_cast<std::uint16_t>(out.events.size() - clip.firstEvent);

            for (int b = 0; b < boneCount; ++b)
            {
                std::vector<BoneFrame> frames(frameCount);
                for (int f = 0; f < frameCount; ++f)
                {
                    const MdlPose &pose = poses[static_cast<std::size_t>(f) * boneCount + b];
                    mdlAngleQuaternion(pose.rot, frames[f].quat);
                    for (int a = 0; a < 3; ++a)
                        frames[f].pos[a] = pose.pos[a];
                    // The sequence's own travel is the entity's to make: the
                    // bone that carries it stays put on those axes.
                    if (b == seq.motionBone)
                    {
                        for (int a = 0; a < 3; ++a)
                        {
                            if (seq.motionType & (1 << a))
                                frames[f].pos[a] = 0;
                        }
                    }
                    // One turn, two quaternions: keep to the one nearer the
                    // frame before, so that blends go the short way.
                    if (f > 0)
                    {
                        double dot = 0;
                        for (int i = 0; i < 4; ++i)
                            dot += frames[f].quat[i] * frames[f - 1].quat[i];
                        if (dot < 0)
                        {
                            for (int i = 0; i < 4; ++i)
                                frames[f].quat[i] = -frames[f].quat[i];
                        }
                    }
                }
                if (b == 0 && frameCount > 1)
                {
                    clip.endOffset[0] = fxFromDouble(frames[frameCount - 1].pos[0] - frames[0].pos[0]);
                    clip.endOffset[1] = fxFromDouble(frames[frameCount - 1].pos[1] - frames[0].pos[1]);
                }

                // Turning and place are keyed apart, each at the largest
                // step that gives every frame back -- to within three quarters
                // of a degree, and
                // a quarter of a unit -- or as one key if it never changes.
                auto turnOff = [&](const BoneFrame &got, int f)
                {
                    double dot = 0;
                    for (int i = 0; i < 4; ++i)
                        dot += got.quat[i] * frames[f].quat[i];
                    return 2 * std::acos(std::min(1.0, std::fabs(dot))) * 180 / M_PI;
                };
                auto placeOff = [&](const BoneFrame &got, int f)
                {
                    double apart = 0;
                    for (int i = 0; i < 3; ++i)
                        apart = std::max(apart, std::fabs(got.pos[i] - frames[f].pos[i]));
                    return apart;
                };
                auto chooseStep = [&](bool rotation)
                {
                    bool still = true;
                    for (int f = 1; f < frameCount && still; ++f)
                        still = rotation ? turnOff(frames[0], f) <= 0.5 : placeOff(frames[0], f) <= 0.1;
                    if (still || frameCount == 1)
                        return 0; // one key
                    static const int kSteps[] = {16, 8, 6, 4, 3, 2};
                    for (int candidate : kSteps)
                    {
                        if (candidate >= frameCount)
                            continue;
                        bool good = true;
                        for (int f = 0; f < frameCount && good; ++f)
                        {
                            const BoneFrame got = blendKeys(frames, candidate, f);
                            good = rotation ? turnOff(got, f) <= 0.75 : placeOff(got, f) <= 0.25;
                        }
                        if (good)
                            return candidate;
                    }
                    return 1;
                };
                ModelTrack track{};
                const int last = frameCount - 1;
                const int rotStep = chooseStep(true);
                track.firstRot = static_cast<std::uint32_t>(out.keys.size());
                track.rotStep = static_cast<std::uint16_t>(std::max(1, rotStep));
                if (rotStep == 0)
                {
                    out.keys.push_back(rotKey(frames[0]));
                }
                else
                {
                    for (int f = 0; f < last; f += rotStep)
                        out.keys.push_back(rotKey(frames[f]));
                    out.keys.push_back(rotKey(frames[last]));
                }
                track.rotCount = static_cast<std::uint16_t>(out.keys.size() - track.firstRot);
                const int posStep = chooseStep(false);
                track.firstPos = static_cast<std::uint32_t>(out.keys.size());
                track.posStep = static_cast<std::uint16_t>(std::max(1, posStep));
                if (posStep == 0)
                {
                    out.keys.push_back(posKey(frames[0]));
                }
                else
                {
                    for (int f = 0; f < last; f += posStep)
                        out.keys.push_back(posKey(frames[f]));
                    out.keys.push_back(posKey(frames[last]));
                }
                track.posCount = static_cast<std::uint16_t>(out.keys.size() - track.firstPos);
                out.tracks.push_back(track);
            }
            report.sourceFrames += frameCount * boneCount;
            out.clips.push_back(clip);
        }
        report.clips = static_cast<int>(out.clips.size());
        report.keys = static_cast<int>(out.keys.size());

        ModelHeader &h = out.header;
        std::memcpy(h.magic, kModelMagic, 4);
        h.version = kModelVersion;
        h.boneCount = static_cast<std::uint16_t>(mdl.bones.size());
        h.batchCount = static_cast<std::uint16_t>(out.batches.size());
        h.clipCount = static_cast<std::uint16_t>(out.clips.size());
        h.eventCount = static_cast<std::uint16_t>(out.events.size());
        h.bodyPartCount = static_cast<std::uint16_t>(out.bodyParts.size());
        h.gxWordCount = static_cast<std::uint32_t>(out.gx.size());
        h.trackCount = static_cast<std::uint32_t>(out.tracks.size());
        h.keyCount = static_cast<std::uint32_t>(out.keys.size());
        h.atlasWidth = static_cast<std::uint16_t>(out.atlas.width);
        h.atlasHeight = static_cast<std::uint16_t>(out.atlas.height);
        h.vertexShift = static_cast<std::uint8_t>(vertexShift);
        for (int a = 0; a < 3; ++a)
        {
            h.mins[a] = static_cast<std::int16_t>(std::clamp(std::floor(lo[a]), -32768.0, 32767.0));
            h.maxs[a] = static_cast<std::int16_t>(std::clamp(std::ceil(hi[a]), -32768.0, 32767.0));
        }
        // Half as much again: an animation reaches further than the rest pose.
        h.radius = static_cast<std::uint16_t>(std::min(65535.0, std::ceil(radius * 1.5)));
        h.polygons = static_cast<std::uint16_t>(std::min(worstPolygons, 65535));
        return true;
    }

    bool writeModel(const CookedModel &model, std::vector<std::uint8_t> &out, std::string &error)
    {
        out.assign(sizeof(ModelHeader), 0);
        auto append = [&](const void *data, std::size_t bytes)
        {
            const auto *p = static_cast<const std::uint8_t *>(data);
            out.insert(out.end(), p, p + bytes);
            while (out.size() & 3)
                out.push_back(0);
        };
        append(model.parents.data(), model.parents.size() * 2);
        append(model.bodyParts.data(), model.bodyParts.size() * sizeof(ModelBodyPart));
        append(model.batches.data(), model.batches.size() * sizeof(ModelBatch));
        append(model.gx.data(), model.gx.size() * 4);
        append(model.clips.data(), model.clips.size() * sizeof(ModelClip));
        append(model.tracks.data(), model.tracks.size() * sizeof(ModelTrack));
        append(model.keys.data(), model.keys.size() * sizeof(ModelKey));
        append(model.events.data(), model.events.size() * sizeof(ModelEvent));

        ModelHeader header = model.header;
        header.fileSize = static_cast<std::uint32_t>(out.size());
        header.crc32 = crc32(out.data() + sizeof(ModelHeader),
                             static_cast<std::uint32_t>(out.size() - sizeof(ModelHeader)));
        std::memcpy(out.data(), &header, sizeof(header));

        ModelView view;
        if (!view.open(out.data(), static_cast<std::uint32_t>(out.size())))
        {
            error = "the cooked model does not pass the DS reader";
            out.clear();
            return false;
        }
        return true;
    }
} // namespace hlds
