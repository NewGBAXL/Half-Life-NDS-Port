#include "HlSound.hpp"
#include "hlds/SoundCook.hpp"
#include "hlds/PathNodes.hpp"
#include "hlds/Cooker.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <map>
#include <numeric>
#include <tuple>

#include "hlds/BrushInfo.hpp"
#include "hlds/EntityCook.hpp"
#include "hlds/EntityParser.hpp"
#include "hlds/FaceBuilder.hpp"
#include "hlds/MapStats.hpp"
#include "hlds/MeshOptimize.hpp"
#include "hlds/TextureCook.hpp"

namespace hlds
{
    using namespace hldata;

    namespace
    {
        constexpr int kNone = -1;

        /// Hands out new, dense indices to the planes something actually uses.
        class PlaneTable
        {
        public:
            explicit PlaneTable(const BspFile &bsp) : bsp_(bsp), remap_(bsp.planes.size(), kNone) {}

            std::uint16_t use(int bspPlane)
            {
                if (remap_[bspPlane] == kNone)
                {
                    remap_[bspPlane] = static_cast<int>(planes.size());
                    const BspPlane &src = bsp_.planes[bspPlane];
                    Plane plane{};
                    std::uint8_t type = kPlaneOther;
                    for (int a = 0; a < 3; ++a)
                    {
                        plane.normal[a] = normalFromDouble(src.normal[a]);
                        // "Axial" lets the DS skip three multiplies, so it has
                        // to be exactly true and not merely what the file says.
                        if (src.normal[a] == 1.0f && src.normal[(a + 1) % 3] == 0.0f &&
                            src.normal[(a + 2) % 3] == 0.0f)
                            type = static_cast<std::uint8_t>(a);
                    }
                    plane.dist = fxFromDouble(src.dist);
                    planes.push_back(plane);
                    types.push_back(type);
                }
                return static_cast<std::uint16_t>(remap_[bspPlane]);
            }

            std::vector<Plane> planes;
            std::vector<std::uint8_t> types;

        private:
            const BspFile &bsp_;
            std::vector<int> remap_;
        };

        std::int16_t clampI16(double v)
        {
            return static_cast<std::int16_t>(std::clamp(v, -32768.0, 32767.0));
        }

        /// Copy the clip nodes reachable from `root` into `out`, renumbered, and
        /// return the new root. Shared subtrees are copied once.
        std::int32_t compactClipHull(const BspFile &bsp, std::int32_t root, PlaneTable &planes,
                                     std::vector<int> &remap, std::vector<ClipNode> &out)
        {
            if (root < 0)
                return root; // a contents value: BspFile::clipHullRoot settled it

            // Iterative, parents before children: a hull can be tens of
            // thousands of nodes deep in the worst case.
            std::vector<std::int32_t> pending = {root};
            std::vector<std::int32_t> order;
            while (!pending.empty())
            {
                const std::int32_t node = pending.back();
                pending.pop_back();
                if (remap[node] != kNone)
                    continue;
                remap[node] = static_cast<int>(out.size()) + static_cast<int>(order.size());
                order.push_back(node);
                for (std::int16_t child : bsp.clipNodes[node].children)
                {
                    if (child >= 0 && remap[child] == kNone)
                        pending.push_back(child);
                }
            }
            // A node pushed twice before being visited would have been skipped
            // above, so `order` holds each node once, in the order it was
            // numbered.
            for (std::int32_t node : order)
            {
                const BspClipNode &src = bsp.clipNodes[node];
                ClipNode clip{};
                clip.plane = planes.use(src.plane);
                for (int c = 0; c < 2; ++c)
                {
                    clip.children[c] = src.children[c] >= 0
                                           ? static_cast<std::int16_t>(remap[src.children[c]])
                                           : src.children[c];
                }
                out.push_back(clip);
            }
            return remap[root];
        }

        void cookEntities(const std::vector<Entity> &entities, CookedLevel &level)
        {
            for (const Entity &entity : entities)
            {
                const std::string_view cls = entity.className();
                EntityRecord record{};
                if (cls == "worldspawn")
                    record.classId = static_cast<std::uint16_t>(EntityClass::Worldspawn);
                else if (cls == "info_player_start")
                    record.classId = static_cast<std::uint16_t>(EntityClass::InfoPlayerStart);
                else
                    continue; // everything else arrives with the milestone that runs it

                double v[3];
                if (entity.vec3("origin", v))
                {
                    for (int a = 0; a < 3; ++a)
                        record.origin[a] = fxFromDouble(v[a]);
                }
                if (entity.vec3("angles", v))
                {
                    for (int a = 0; a < 3; ++a)
                        record.angles[a] = angleFromDegrees(v[a]);
                }
                else
                {
                    // The older single-number form: a yaw.
                    const std::string yaw(entity.value("angle", "0"));
                    record.angles[1] = angleFromDegrees(std::atof(yaw.c_str()));
                }
                level.entities.push_back(record);
            }
        }
    } // namespace

    bool cookLevel(const BspFile &bsp, const CookOptions &options, const TexturePlan &plan,
                   CookedLevel &level, CookReport &report, std::string &error)
    {
        level = CookedLevel();
        report = CookReport();

        std::vector<Entity> entities;
        if (!parseEntities(bsp.entities, entities, error))
            return false;

        // The cooked format indexes these with 16 bits.
        if (bsp.faces.size() > 0xFFFF || bsp.nodes.size() > 0x7FFF ||
            bsp.leaves.size() > 0x7FFF || bsp.clipNodes.size() > 0x7FFF ||
            bsp.models.size() > 0xFFFF)
        {
            error = "the map is too large for the cooked level format";
            return false;
        }

        const BspModel &world = bsp.models[0];
        for (int a = 0; a < 3; ++a)
        {
            level.mins[a] = clampI16(std::floor(world.mins[a]));
            level.maxs[a] = clampI16(std::ceil(world.maxs[a]));
        }
        level.visLeafCount = static_cast<std::uint16_t>(bsp.visLeafCount());

        PlaneTable planes(bsp);
        const LightBake light(bsp, entities, options.lightGamma);

        const std::vector<BrushInfo> brushes = analyseBrushes(bsp, entities);
        const std::vector<const BrushInfo *> brushOwner = brushOfModel(bsp, brushes);

        // --- Textures: only the ones a drawn face names, in the form it
        // wears them. A '{' texture can be in the table twice.
        std::map<std::pair<int, bool>, int> textureRemap;
        auto sizeOf = [&](int bspTexture, bool a3i5)
        {
            const std::vector<TextureSize> &sizes = a3i5 ? plan.alphaSizes : plan.sizes;
            const BspTexture &src = bsp.textures[bspTexture];
            return sizes.empty() ? defaultTextureSize(src.width, src.height) : sizes[bspTexture];
        };
        auto useTexture = [&](int bspTexture, bool a3i5)
        {
            const auto key = std::make_pair(bspTexture, a3i5);
            const auto found = textureRemap.find(key);
            if (found != textureRemap.end())
                return static_cast<std::uint16_t>(found->second);
            const BspTexture &src = bsp.textures[bspTexture];
            const TextureSize size = sizeOf(bspTexture, a3i5);
            TextureRef ref{};
            std::strncpy(ref.name, src.name.c_str(), sizeof(ref.name) - 1);
            ref.width = size.width;
            ref.height = size.height;
            ref.sourceWidth = static_cast<std::uint16_t>(src.width);
            ref.sourceHeight = static_cast<std::uint16_t>(src.height);
            if (!src.name.empty() && src.name[0] == '{')
                ref.flags |= kTextureColourKey;
            if (a3i5)
                ref.flags |= kTextureA3I5;
            // The game's own rule for what is drawn as liquid: the name.
            if (!src.name.empty() && (src.name[0] == '!' || src.name.rfind("water", 0) == 0))
                ref.flags |= kTextureLiquid;
            textureRemap[key] = static_cast<int>(level.textures.size());
            level.textures.push_back(ref);
            return static_cast<std::uint16_t>(level.textures.size() - 1);
        };

        // --- Surfaces. Each model's drawn faces become mesh faces, are merged
        // and thinned where that is provably invisible, and are then grouped
        // by plane and texture: one group, one surface. Groups are stored
        // sorted by texture, so that walking surfaces in index order changes
        // texture as rarely as possible. The world comes first because it is
        // model 0.
        std::vector<int> surfaceOfFace(bsp.faces.size(), kNone);
        bool clampedTexCoords = false;

        // Faces may share a group only with the same texture MAPPING, which is
        // more than the same texture: two texinfos are one mapping when their
        // vectors and texture agree.
        std::vector<int> mappingOf(bsp.texInfos.size());
        for (std::size_t i = 0; i < bsp.texInfos.size(); ++i)
        {
            mappingOf[i] = static_cast<int>(i);
            for (std::size_t j = 0; j < i; ++j)
            {
                if (bsp.texInfos[j].miptex == bsp.texInfos[i].miptex &&
                    std::memcmp(bsp.texInfos[j].vecs, bsp.texInfos[i].vecs,
                                sizeof(bsp.texInfos[i].vecs)) == 0)
                {
                    mappingOf[i] = mappingOf[j];
                    break;
                }
            }
        }

        for (std::size_t m = 0; m < bsp.models.size(); ++m)
        {
            const BspModel &src = bsp.models[m];

            // The world is drawn as it is. Any other model is drawn the way
            // the entity that owns it says -- and not at all if that entity is
            // never visible, or if nothing owns it.
            const BrushInfo *brush = brushOwner[m];
            const bool modelDrawn = m == 0 || (brush != nullptr && brush->drawn());
            const bool flatColour =
                brush != nullptr && brush->renderClass == RenderClass::Colour;
            const bool fullBright = brush != nullptr && brush->fullBright();

            std::vector<MeshFace> meshFaces;
            std::map<std::tuple<int, int, int>, int> keys; // (plane, side, mapping) -> key
            std::vector<int> keyPlane, keySide, keyTexture;
            std::vector<char> keyAlpha;
            for (int f = src.firstFace; f < src.firstFace + src.faceCount; ++f)
            {
                const BspFace &face = bsp.faces[f];
                const int bspTexture = bsp.texInfos[face.texInfo].miptex;
                const BspTexture &tex = bsp.textures[bspTexture];
                const bool holes = brush != nullptr && brush->cutsHoles() && !tex.name.empty() &&
                                   tex.name[0] == '{';
                const bool planned =
                    flatColour ||
                    (holes ? plan.alphaSizes : plan.sizes).empty() ||
                    (holes ? plan.alphaSizes : plan.sizes)[bspTexture].width != 0;
                if (!modelDrawn || tex.missing || isUndrawnTexture(tex.name) || !planned)
                {
                    ++report.droppedFaces;
                    continue;
                }

                MeshFace mesh;
                mesh.sources = {f};
                const auto keyId = std::make_tuple(static_cast<int>(face.plane),
                                                   face.side != 0 ? 1 : 0,
                                                   mappingOf[face.texInfo]);
                auto found = keys.find(keyId);
                if (found == keys.end())
                {
                    found = keys.insert({keyId, static_cast<int>(keyPlane.size())}).first;
                    keyPlane.push_back(face.plane);
                    keySide.push_back(face.side != 0 ? 1 : 0);
                    keyTexture.push_back(bspTexture);
                    keyAlpha.push_back(holes ? 1 : 0);
                }
                mesh.key = found->second;
                const BspPlane &plane = bsp.planes[face.plane];
                for (int a = 0; a < 3; ++a)
                    mesh.normal[a] = face.side != 0 ? -plane.normal[a] : plane.normal[a];

                mesh.points.resize(face.edgeCount);
                for (int i = 0; i < face.edgeCount; ++i)
                {
                    FacePoint &p = mesh.points[i];
                    const float *pos = bsp.faceVertex(face, i).point;
                    for (int a = 0; a < 3; ++a)
                        p.pos[a] = pos[a];
                    light.texCoords(face, i, p.s, p.t);
                    // Blended and coloured brushes are not lit: the engine
                    // draws them at full brightness, or in their own colour.
                    const Rgb c = fullBright ? Rgb{255, 255, 255} : light.sample(face, p.s, p.t);
                    p.colour = flatColour ? brush->colour : rgb555(c.r, c.g, c.b);
                }
                const FaceCost baseline = faceCost(face.edgeCount);
                ++report.faces;
                report.facePolygons += baseline.polygons;
                report.faceVertices += baseline.vertices;
                meshFaces.push_back(std::move(mesh));
            }

            report.weldedVertices += weldTJunctions(meshFaces);

            if (options.guardedMerges)
            {
                OptimizeReport optimized;
                optimizeFaces(meshFaces, optimized);
                report.merges += optimized.merges;
                report.removedVertices += optimized.removedVertices;
            }

            std::vector<std::vector<int>> groups = groupFaces(meshFaces);
            std::stable_sort(groups.begin(), groups.end(),
                             [&](const std::vector<int> &a, const std::vector<int> &b)
            {
                const int ka = meshFaces[a[0]].key;
                const int kb = meshFaces[b[0]].key;
                if (keyTexture[ka] != keyTexture[kb])
                    return keyTexture[ka] < keyTexture[kb];
                return keyPlane[ka] < keyPlane[kb];
            });

            Model model{};
            for (int a = 0; a < 3; ++a)
            {
                model.mins[a] = clampI16(std::floor(src.mins[a]));
                model.maxs[a] = clampI16(std::ceil(src.maxs[a]));
                model.origin[a] = fxFromDouble(src.origin[a]);
            }
            model.headNode = src.headNode[0];
            model.firstSurface = static_cast<std::uint16_t>(level.surfaces.size());
            model.surfaceCount = static_cast<std::uint16_t>(groups.size());
            level.models.push_back(model);

            for (const std::vector<int> &group : groups)
            {
                const int key = meshFaces[group[0]].key;
                const int bspTexture = keyTexture[key];
                const bool a3i5 = keyAlpha[key] != 0;
                const BspTexture &tex = bsp.textures[bspTexture];
                const std::uint16_t texture = flatColour ? kNoTexture : useTexture(bspTexture, a3i5);

                std::vector<const MeshFace *> members;
                for (int index : group)
                    members.push_back(&meshFaces[index]);
                const BuiltSurface built = buildGroupSurface(
                    members, sizeOf(bspTexture, a3i5), tex.width, tex.height);
                // What was sent must cover the faces it was made from. Held
                // group by group, on the grid, and one way only: a face a
                // fraction of a unit wide can fold over itself when snapped
                // and come out as slightly MORE than its outline encloses,
                // which nobody will see; less is a piece left out.
                double want = 0;
                for (const MeshFace *face : members)
                {
                    double sum[3] = {0, 0, 0};
                    auto grid = [](double v) { return std::round(v * 4) / 4; };
                    const std::size_t n = face->points.size();
                    for (std::size_t k = 1; k + 1 < n; ++k)
                    {
                        double e1[3], e2[3];
                        for (int a = 0; a < 3; ++a)
                        {
                            e1[a] = grid(face->points[k].pos[a]) - grid(face->points[0].pos[a]);
                            e2[a] = grid(face->points[k + 1].pos[a]) - grid(face->points[0].pos[a]);
                        }
                        sum[0] += e1[1] * e2[2] - e1[2] * e2[1];
                        sum[1] += e1[2] * e2[0] - e1[0] * e2[2];
                        sum[2] += e1[0] * e2[1] - e1[1] * e2[0];
                    }
                    const double area =
                        0.5 * std::sqrt(sum[0] * sum[0] + sum[1] * sum[1] + sum[2] * sum[2]);
                    // A face about half a unit wide or less is not held to
                    // account: on the grid it is a line with a kink in it, and
                    // whether the kink is drawn makes no picture.
                    double around = 0;
                    for (std::size_t k = 0; k < n; ++k)
                    {
                        double d2 = 0;
                        for (int a = 0; a < 3; ++a)
                        {
                            const double d = grid(face->points[(k + 1) % n].pos[a]) -
                                             grid(face->points[k].pos[a]);
                            d2 += d * d;
                        }
                        around += std::sqrt(d2);
                    }
                    if (area >= 0.3 * around)
                        want += area;
                }
                report.sourceArea += want;
                if (want - built.area > 0.5)
                    report.lostArea += want - built.area;
                if (built.outOfRange)
                {
                    error = "the map reaches beyond +-8191 units, which a DS vertex cannot hold";
                    return false;
                }
                if (built.polygons > 0xFFFF || built.vertices > 0xFFFF ||
                    built.words.size() > 0xFFFF || level.surfaces.size() >= 0xFFFF)
                {
                    error = "a group of faces is too large for one surface";
                    return false;
                }
                clampedTexCoords = clampedTexCoords || built.texCoordsClamped;

                Surface surface{};
                surface.gxOfs = static_cast<std::uint32_t>(level.gxStream.size());
                surface.gxWords = static_cast<std::uint16_t>(built.words.size());
                surface.planeRef = planes.use(keyPlane[key]);
                if (keySide[key] != 0)
                    surface.planeRef |= kSurfacePlaneBack;
                for (int a = 0; a < 3; ++a)
                    surface.center[a] = built.center[a];
                surface.radius = built.radius;
                surface.texture = texture;
                surface.polygons = static_cast<std::uint16_t>(built.polygons);
                surface.vertices = static_cast<std::uint16_t>(built.vertices);

                for (const MeshFace *member : members)
                {
                    for (int f : member->sources)
                        surfaceOfFace[f] = static_cast<int>(level.surfaces.size());
                }
                level.surfaces.push_back(surface);
                level.gxStream.insert(level.gxStream.end(), built.words.begin(),
                                      built.words.end());
                report.polygons += built.polygons;
                report.vertices += built.vertices;
            }
        }
        report.surfaces = static_cast<int>(level.surfaces.size());
        if (report.lostArea > 4.0)
        {
            report.warnings.push_back("the surfaces do not cover the map's faces (" +
                                      std::to_string(static_cast<long>(report.lostArea)) +
                                      " square units missing); part of it will not be drawn");
        }
        if (planes.planes.size() > kSurfacePlaneMask)
        {
            error = "the map uses more planes than a surface can name";
            return false;
        }
        if (clampedTexCoords)
            report.warnings.push_back("a face spans more texels than the DS can address; "
                                      "its texture will be distorted");

        // --- The render tree, with parent links for marking visible branches.
        level.nodes.resize(bsp.nodes.size());
        std::vector<std::uint16_t> leafParent(bsp.leaves.size(), kNoParent);
        for (std::size_t i = 0; i < bsp.nodes.size(); ++i)
            level.nodes[i].parent = kNoParent;
        for (std::size_t i = 0; i < bsp.nodes.size(); ++i)
        {
            const BspNode &src = bsp.nodes[i];
            Node &node = level.nodes[i];
            node.plane = planes.use(src.plane);
            for (int c = 0; c < 2; ++c)
            {
                node.children[c] = src.children[c];
                if (src.children[c] >= 0)
                    level.nodes[src.children[c]].parent = static_cast<std::uint16_t>(i);
                else
                    leafParent[-1 - src.children[c]] = static_cast<std::uint16_t>(i);
            }
            for (int a = 0; a < 3; ++a)
            {
                node.mins[a] = src.mins[a];
                node.maxs[a] = src.maxs[a];
            }
        }

        // --- Leaves and their marks, with undrawn faces dropped from the lists.
        level.leaves.resize(bsp.leaves.size());
        std::vector<char> leafHasSky(bsp.leaves.size(), 0);
        for (std::size_t i = 0; i < bsp.leaves.size(); ++i)
        {
            const BspLeaf &src = bsp.leaves[i];
            Leaf &leaf = level.leaves[i];
            // Leaf 0 is all the solid there is. The compiler leaves its row at
            // offset 0, which is some other leaf's; an eye that ends up in a
            // wall should see everything rather than that.
            leaf.visOfs = (src.visOfs < 0 || i == 0) ? -1 : src.visOfs;
            for (int a = 0; a < 3; ++a)
            {
                leaf.mins[a] = src.mins[a];
                leaf.maxs[a] = src.maxs[a];
            }
            leaf.firstMark = static_cast<std::uint16_t>(level.marks.size());
            // Several of a leaf's faces may now be one surface: list it once.
            for (int k = 0; k < src.markSurfaceCount; ++k)
            {
                const int faceIndex = bsp.markSurfaces[src.firstMarkSurface + k];
                // A face of sky is not drawn, but it is why the sky box is:
                // remembered here, spread to every leaf that can see this one
                // further down.
                if (bsp.textures[bsp.texInfos[bsp.faces[faceIndex].texInfo].miptex].name == "sky")
                    leafHasSky[i] = 1;
                const int surface = surfaceOfFace[faceIndex];
                if (surface == kNone)
                    continue;
                const auto mark = static_cast<std::uint16_t>(surface);
                if (std::find(level.marks.begin() + leaf.firstMark, level.marks.end(), mark) ==
                    level.marks.end())
                    level.marks.push_back(mark);
            }
            leaf.markCount =
                static_cast<std::uint16_t>(level.marks.size() - leaf.firstMark);
            leaf.parent = leafParent[i];
            leaf.contents = static_cast<std::int8_t>(std::clamp(src.contents, -128, 127));
        }
        if (level.marks.size() > 0xFFFF)
        {
            error = "the map has more leaf-to-surface marks than the cooked format holds";
            return false;
        }
        level.vis = bsp.visibility;

        // --- Clip hulls 1 (standing) and 3 (crouched). Hull 2 is for large
        // monsters and is not carried until something needs it.
        std::vector<int> clipRemap(bsp.clipNodes.size(), kNone);
        for (std::size_t m = 0; m < bsp.models.size(); ++m)
        {
            level.models[m].clipStanding = compactClipHull(
                bsp, bsp.clipHullRoot(static_cast<int>(m), 1), planes, clipRemap, level.clipNodes);
            level.models[m].clipCrouched = compactClipHull(
                bsp, bsp.clipHullRoot(static_cast<int>(m), 3), planes, clipRemap, level.clipNodes);
        }
        report.droppedClipNodes =
            static_cast<int>(bsp.clipNodes.size() - level.clipNodes.size());

        // --- Brush records: every brush entity that is seen or is in the way.
        // A volume that is neither -- a trigger, a ladder -- has nothing for
        // the renderer or the collision to know yet.
        std::vector<int> brushRecordOfEntity(entities.size(), -1);
        std::vector<std::array<double, 3>> restLo, restHi; // each brush's box where it was built
        for (const BrushInfo &info : brushes)
        {
            if (!info.drawn() && !info.solid)
                continue;
            brushRecordOfEntity[info.entity] = static_cast<int>(level.brushes.size());
            const BspModel &src = bsp.models[info.model];
            Brush record{};
            record.model = static_cast<std::uint16_t>(info.model);
            record.renderClass = static_cast<std::uint8_t>(info.renderClass);
            record.alpha = static_cast<std::uint8_t>(info.alpha);
            record.colour = info.colour;
            record.flags = static_cast<std::uint8_t>((info.solid ? kBrushSolid : 0) |
                                                     (info.hasPivot ? kBrushHasPivot : 0) |
                                                     (info.startsOff ? kBrushStartsOff : 0));
            record.logic = kNoLogic;
            // A func_water is a door whose "skin" says what it is full of
            // (-3 water, -4 slime, -5 lava).
            if (info.className == "func_water")
            {
                const int skin = std::atoi(std::string(entities[info.entity].value("skin")).c_str());
                if (skin <= kContentsWater && skin >= kContentsLava)
                    record.kind = static_cast<std::uint8_t>(static_cast<std::int8_t>(skin));
            }
            double lo[3], hi[3];
            for (int a = 0; a < 3; ++a)
            {
                record.origin[a] = fxFromDouble(info.origin[a]);
                lo[a] = src.mins[a] + info.origin[a];
                hi[a] = src.maxs[a] + info.origin[a];
                record.mins[a] = clampI16(std::floor(lo[a]));
                record.maxs[a] = clampI16(std::ceil(hi[a]));
            }

            restLo.push_back({lo[0], lo[1], lo[2]});
            restHi.push_back({hi[0], hi[1], hi[2]});
            level.brushes.push_back(record);
            ++report.brushes;
            if (!info.drawn())
                ++report.hiddenBrushes;
        }

        // --- Logic: the entities that do something, and which brush each
        // one drives.
        {
            LogicCook logic;
            cookLogic(bsp, entities, brushRecordOfEntity, options.models, logic, options.sentences);
            if (logic.records.size() > 0x7FFF || logic.aux.size() > 0xFFFF)
            {
                error = "the map has more logic entities than the cooked format holds";
                return false;
            }
            for (std::size_t e = 0; e < entities.size(); ++e)
            {
                if (brushRecordOfEntity[e] >= 0 && logic.recordOfEntity[e] >= 0)
                    level.brushes[brushRecordOfEntity[e]].logic =
                        static_cast<std::uint16_t>(logic.recordOfEntity[e]);
            }
            // A brush that turns about anything but the vertical is boxed as
            // a ball round its pivot, here and on the DS.
            for (const LogicRecord &rec : logic.records)
            {
                const bool turns = rec.kind == static_cast<std::uint8_t>(LogicKind::DoorRotating) ||
                                   rec.kind == static_cast<std::uint8_t>(LogicKind::Rotating);
                if (turns && rec.brush != kNoBrush && rec.extra[0] != kYaw)
                    level.brushes[rec.brush].flags |= kBrushTumbles;
                // A track train goes wherever its track does, and into the
                // next map; a platform that turns as it sinks has a pivot.
                if (rec.kind == static_cast<std::uint8_t>(LogicKind::TrackTrain) &&
                    rec.brush != kNoBrush)
                    level.brushes[rec.brush].flags |= kBrushRoams | kBrushHasPivot;
                // A box goes wherever it is pushed.
                if (rec.kind == static_cast<std::uint8_t>(LogicKind::Pushable) &&
                    rec.brush != kNoBrush)
                    level.brushes[rec.brush].flags |= kBrushRoams;
                if (rec.kind == static_cast<std::uint8_t>(LogicKind::TrackChange) &&
                    rec.brush != kNoBrush)
                    level.brushes[rec.brush].flags |= kBrushRoams | kBrushHasPivot;
            }
            level.logic = std::move(logic.records);
            level.logicAux = std::move(logic.aux);
            level.corners = std::move(logic.corners);
            level.nameHashes = std::move(logic.nameHashes);
            level.landmarks = std::move(logic.landmarks);
            level.strings = std::move(logic.strings);
            level.modelRefs = std::move(logic.modelRefs);
            for (std::size_t m = 0; m < options.models.size(); ++m)
            {
                if (!options.viewModel.empty() && options.models[m] == options.viewModel)
                    level.viewModel = static_cast<std::uint16_t>(m + 1);
            }
            level.actors = std::move(logic.actors);
            // Each sound under the name it has at the usual rate; whoever
            // cooks the files may change that (a map short of room has some
            // at half the rate, and the name says so).
            for (std::size_t n = 0; n < logic.sounds.size(); ++n)
            {
                SoundRef ref{};
                if (!logic.sounds[n].empty())
                {
                    const std::string name =
                        soundCardName(logic.sounds[n], kSoundRate, logic.soundLoops[n]);
                    std::strncpy(ref.name, name.c_str(), sizeof(ref.name) - 1);
                }
                level.sounds.push_back(ref);
            }
            level.soundPaths = std::move(logic.sounds);
            level.soundLoops = std::move(logic.soundLoops);
            level.logicSounds = std::move(logic.logicSounds);
            level.sentences = std::move(logic.sentences);
            level.sentenceWords = std::move(logic.sentenceWords);
            level.rooms = std::move(logic.rooms);

            // --- Where each brush can be seen from: the world leaves its box
            // touches, found by pushing the box down the world's tree. For a
            // brush that moves, the box is everywhere it ever is -- a door at
            // both ends of its travel, a train at every corner of its route --
            // or a lift would vanish the moment it left the room it was built
            // in.
            for (std::size_t b = 0; b < level.brushes.size(); ++b)
            {
                Brush &record = level.brushes[b];
                double lo[3] = {restLo[b][0], restLo[b][1], restLo[b][2]};
                double hi[3] = {restHi[b][0], restHi[b][1], restHi[b][2]};
                auto include = [&](const double *shift)
                {
                    for (int a = 0; a < 3; ++a)
                    {
                        lo[a] = std::min(lo[a], restLo[b][a] + shift[a]);
                        hi[a] = std::max(hi[a], restHi[b][a] + shift[a]);
                    }
                };
                if (record.logic != kNoLogic)
                {
                    const LogicRecord &rec = level.logic[record.logic];
                    if (rec.kind == static_cast<std::uint8_t>(LogicKind::Train))
                    {
                        // Follow the route, once round at most.
                        std::uint16_t corner = rec.firstAux;
                        for (std::size_t steps = 0;
                             corner != kNoCorner && steps <= level.corners.size(); ++steps)
                        {
                            double shift[3];
                            for (int a = 0; a < 3; ++a)
                                shift[a] = (level.corners[corner].pos[a] - rec.move[a]) / 4096.0;
                            include(shift);
                            corner = level.corners[corner].next;
                        }
                    }
                    else if (rec.kind == static_cast<std::uint8_t>(LogicKind::DoorRotating) ||
                             rec.kind == static_cast<std::uint8_t>(LogicKind::Rotating))
                    {
                        // It sweeps a circle round its hinge -- a ball, if the
                        // hinge is not vertical: the box of that.
                        const bool tumbles = (record.flags & kBrushTumbles) != 0;
                        double reach = 0;
                        for (int corner = 0; corner < 8; ++corner)
                        {
                            const double x = ((corner & 1) ? restHi[b][0] : restLo[b][0]) -
                                             record.origin[0] / 4096.0;
                            const double y = ((corner & 2) ? restHi[b][1] : restLo[b][1]) -
                                             record.origin[1] / 4096.0;
                            const double z = tumbles ? ((corner & 4) ? restHi[b][2] : restLo[b][2]) -
                                                           record.origin[2] / 4096.0
                                                     : 0;
                            reach = std::max(reach, std::sqrt(x * x + y * y + z * z));
                        }
                        for (int a = 0; a < (tumbles ? 3 : 2); ++a)
                        {
                            lo[a] = std::min(lo[a], record.origin[a] / 4096.0 - reach);
                            hi[a] = std::max(hi[a], record.origin[a] / 4096.0 + reach);
                        }
                    }
                    else
                    {
                        const double shift[3] = {rec.move[0] / 4096.0, rec.move[1] / 4096.0,
                                                 rec.move[2] / 4096.0};
                        include(shift);
                    }
                }

                // A unit of slack: a brush flush against a wall must still
                // count as being in the room in front of it.
                record.firstLeaf = static_cast<std::uint16_t>(level.brushLeaves.size());
                std::vector<int> pending = {world.headNode[0]};
                while (!pending.empty())
                {
                    const int index = pending.back();
                    pending.pop_back();
                    if (index < 0)
                    {
                        const int leaf = -1 - index;
                        if (leaf != 0 && leaf <= level.visLeafCount &&
                            bsp.leaves[leaf].contents != kContentsSolid)
                            level.brushLeaves.push_back(static_cast<std::uint16_t>(leaf));
                        continue;
                    }
                    const BspNode &node = bsp.nodes[index];
                    const BspPlane &plane = bsp.planes[node.plane];
                    // The box's nearest and farthest corners along the normal.
                    double nearest = -plane.dist, farthest = -plane.dist;
                    for (int a = 0; a < 3; ++a)
                    {
                        const double n = plane.normal[a];
                        nearest += n * (n >= 0 ? lo[a] - 1 : hi[a] + 1);
                        farthest += n * (n >= 0 ? hi[a] + 1 : lo[a] - 1);
                    }
                    if (farthest >= 0)
                        pending.push_back(node.children[0]);
                    if (nearest <= 0)
                        pending.push_back(node.children[1]);
                }
                if (level.brushLeaves.size() > 0xFFFF)
                {
                    error = "the map's brush entities touch more leaves than the format holds";
                    return false;
                }
                record.leafCount =
                    static_cast<std::uint16_t>(level.brushLeaves.size() - record.firstLeaf);
            }
            report.logicRecords = static_cast<int>(level.logic.size());
            report.unsupported = std::move(logic.unsupported);
        }

        level.planes = std::move(planes.planes);
        level.planeTypes = std::move(planes.types);
        // (Now that the planes are the level's: the walk test reads them.)
        cookPathNodes(entities, level);
        report.droppedPlanes = static_cast<int>(bsp.planes.size() - level.planes.size());

        cookEntities(entities, level);
        level.entityText = bsp.entities;

        // --- The sky: six more textures, which no surface uses.
        if (!options.skyName.empty())
        {
            level.skyTexture = static_cast<std::uint16_t>(level.textures.size() + 1);
            for (const char *side : kSkySides)
            {
                TextureRef ref{};
                const std::string name = options.skyName + side;
                std::strncpy(ref.name, name.c_str(), sizeof(ref.name) - 1);
                ref.width = ref.height = kSkySize;
                ref.sourceWidth = ref.sourceHeight = 256;
                ref.flags = kTextureSky;
                level.textures.push_back(ref);
            }
        }

        // --- The worst a single leaf's PVS can ask for, in the cooked
        // surfaces' own costs. This is the number the runtime's choice between
        // single-pass and two-pass rendering starts from.
        {
            std::vector<int> stamp(level.surfaces.size(), 0);
            int worstPolygons = 0;
            int worstVertices = 0;
            for (int leaf = 1; leaf <= level.visLeafCount; ++leaf)
            {
                const std::vector<std::uint8_t> row = bsp.decompressVis(leaf);
                int polygons = 0;
                int vertices = 0;
                for (int other = 1; other <= level.visLeafCount; ++other)
                {
                    if (!(row[(other - 1) >> 3] & (1 << ((other - 1) & 7))))
                        continue;
                    const Leaf &seen = level.leaves[other];
                    for (int k = 0; k < seen.markCount; ++k)
                    {
                        const int s = level.marks[seen.firstMark + k];
                        if (stamp[s] == leaf)
                            continue;
                        stamp[s] = leaf;
                        polygons += level.surfaces[s].polygons;
                        vertices += level.surfaces[s].vertices;
                    }
                }
                // And whether any of what it sees is sky.
                bool sky = leafHasSky[leaf] != 0;
                for (int other = 1; other <= level.visLeafCount && !sky; ++other)
                    sky = (row[(other - 1) >> 3] & (1 << ((other - 1) & 7))) &&
                          leafHasSky[other] != 0;
                if (sky)
                    level.leaves[leaf].flags |= kLeafSeesSky;
                worstPolygons = std::max(worstPolygons, polygons);
                worstVertices = std::max(worstVertices, vertices);
            }
            level.worstPolygons = static_cast<std::uint16_t>(std::min(worstPolygons, 0xFFFF));
            level.worstVertices = static_cast<std::uint16_t>(std::min(worstVertices, 0xFFFF));
        }
        return true;
    }
} // namespace hlds
