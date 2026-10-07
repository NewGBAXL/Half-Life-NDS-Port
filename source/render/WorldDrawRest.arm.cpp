#include "render/WorldDraw.hpp"

#include <cstdlib>
#include <malloc.h>
#include <cstring>

#include <nds.h>

#include <NEAMain.h>

#include "DebugLog.hpp"
#include "Global.hpp"
#include "HlPlaneMath.hpp"
#include "data/LevelFile.hpp"
#include "data/TextureCache.hpp"
#include "render/Camera.hpp"
#include "render/ModelDraw.hpp"
#include "render/WorldDrawParts.hpp"

// The parts of the world renderer that do NOT run from fast memory. ARM code
// still (the file name), for the 64-bit multiplies; but what is here runs a
// few times a frame, or once a map, and the instruction cache is enough for
// it. The fast memory is 32 KB and is kept for the two loops that run
// thousands of times a frame: the walk and the emit (WorldDraw.itcm.arm.cpp).

namespace hlrt
{
    using namespace hldata;
    using namespace worldparts;

    bool WorldDraw::create()
    {
        destroy();
        const LevelView &view = level.view;
        chosenWords_ = (view.surfaceCount + 31) / 32;
        // calloc: a zero stamp never matches visFrame_, which starts at 1.
        nodeFrame_ = static_cast<std::uint16_t *>(std::calloc(view.nodeCount + 1, 2));
        leafFrame_ = static_cast<std::uint16_t *>(std::calloc(view.leafCount + 1, 2));
        chosen_ = static_cast<std::uint32_t *>(std::calloc(chosenWords_ + 1, 4));
        skipped_ = static_cast<std::uint32_t *>(std::calloc(chosenWords_ + 1, 4));
        if (nodeFrame_ == nullptr || leafFrame_ == nullptr || chosen_ == nullptr || skipped_ == nullptr)
        {
            destroy();
            return false;
        }
        // (Not having it is not an error: the surfaces are then written one
        // word at a time, as on a DS. On cache lines of its own: it is
        // flushed in pieces.)
        if (isDSiMode())
            list_ = static_cast<std::uint32_t *>(memalign(32, kListWords * 4));
        visFrame_ = 0;
        visLeaf_ = -1;
        return true;
    }

    void WorldDraw::destroy()
    {
        std::free(nodeFrame_);
        std::free(leafFrame_);
        std::free(chosen_);
        std::free(skipped_);
        skipped_ = nullptr;
        std::free(list_);
        list_ = nullptr;
        brushState_ = nullptr;
        nodeFrame_ = nullptr;
        leafFrame_ = nullptr;
        chosen_ = nullptr;
        chosenWords_ = 0;
        pressure_ = 0;
        cutPasses_ = easyPasses_ = sinceRelax_ = 0;
        relaxWait_ = kRelaxWait;
        fogEnd_ = kNoFog;
        fogOn_ = false;
    }

    void WorldDraw::markVisible(int leaf)
    {
        const LevelView &view = level.view;
        visLeaf_ = leaf;
        if (++visFrame_ == 0)
        {
            // The stamp wrapped: old stamps could now match by accident.
            std::memset(nodeFrame_, 0, view.nodeCount * 2);
            std::memset(leafFrame_, 0, view.leafCount * 2);
            visFrame_ = 1;
        }
        const std::uint16_t frame = visFrame_;

        auto stamp = [&](int visible)
        {
            leafFrame_[visible] = frame;
            // Up the tree until a node some earlier leaf already stamped.
            std::uint16_t node = view.leaves[visible].parent;
            while (node != kNoParent && nodeFrame_[node] != frame)
            {
                nodeFrame_[node] = frame;
                node = view.nodes[node].parent;
            }
        };

        const int count = view.header->visLeafCount;
        const std::int32_t ofs = view.leaves[leaf].visOfs;
        if (ofs < 0)
        {
            // No row: outside the world (or an unvised map). Everything shows,
            // and the budget does the rest.
            for (int i = 1; i <= count; ++i)
                stamp(i);
            return;
        }

        // The BSP's own run-length rows: a non-zero byte is eight leaves, a
        // zero byte is followed by how many zero bytes it stands for.
        const std::uint8_t *in = view.vis + ofs;
        const std::uint8_t *end = view.vis + view.visSize;
        int index = 1; // bit 0 of the first byte is leaf 1
        while (index <= count && in < end)
        {
            const std::uint8_t bits = *in++;
            if (bits == 0)
            {
                if (in >= end)
                    break;
                index += 8 * *in++;
                continue;
            }
            for (int b = 0; b < 8 && index + b <= count; ++b)
            {
                if (bits & (1 << b))
                    stamp(index + b);
            }
            index += 8;
        }
    }

    void WorldDraw::drawBrush(std::uint32_t index, int polygonId)
    {
        const LevelView &view = level.view;
        const Brush &brush = view.brushes[index];
        const Model &model = view.models[brush.model];
        const fx *pos = brushState_ != nullptr ? brushState_[index].pos : brush.origin;

        // The eye in the brush's own space: its surfaces and their planes
        // never move, the brush does.
        static const std::uint16_t kNotTurned[3] = {0, 0, 0};
        const std::uint16_t *angles =
            brushState_ != nullptr ? brushState_[index].angles : kNotTurned;
        const bool turned = angles[0] != 0 || angles[1] != 0 || angles[2] != 0;
        fx eye[3] = {camera.pos[0] - pos[0], camera.pos[1] - pos[1], camera.pos[2] - pos[2]};
        std::int32_t axes[9]; // Q30
        if (turned)
        {
            // ... and turned back the way the brush is turned: by the
            // transpose of its matrix.
            anglesToAxes(angles, axes);
            const fx d[3] = {eye[0], eye[1], eye[2]};
            for (int j = 0; j < 3; ++j)
            {
                eye[j] = static_cast<fx>((static_cast<std::int64_t>(d[0]) * axes[j] +
                                          static_cast<std::int64_t>(d[1]) * axes[3 + j] +
                                          static_cast<std::int64_t>(d[2]) * axes[6 + j]) >>
                                         kNormalShift);
            }
        }

        switch (static_cast<RenderClass>(brush.renderClass))
        {
        case RenderClass::Cutout:
            // Full alpha, but the A3I5 texture makes the hardware treat the
            // polygon as see-through: without the depth-update bit the solid
            // parts of a grate would not hide what is drawn behind it later.
            NEA_PolyFormat(31, polygonId, static_cast<NEA_LightEnum>(0), kCull,
                           static_cast<NEA_OtherFormatEnum>(polygonMode() |
                                                            NEA_TRANS_DEPTH_UPDATE));
            break;
        case RenderClass::Blend:
        case RenderClass::Colour:
            NEA_PolyFormat(brush.alpha, polygonId, static_cast<NEA_LightEnum>(0), kCull,
                           polygonMode());
            break;
        default:
            NEA_PolyFormat(31, 0, static_cast<NEA_LightEnum>(0), kCull, polygonMode());
            break;
        }

        // Vertices are quarter units and positions are Q12 units: >> 10.
        const bool moved = pos[0] != 0 || pos[1] != 0 || pos[2] != 0 || turned;
        if (moved)
        {
            glPushMatrix();
            glTranslatef32(pos[0] >> 10, pos[1] >> 10, pos[2] >> 10);
            if (turned)
            {
                // The same matrix the collision uses, so that what is seen is
                // what is walked into. The hardware multiplies a ROW vector
                // by its matrices, which makes the one to load the transpose.
                // In Half-Life's axes: the swap to the hardware's is in the
                // view matrix, above this.
                m3x3 turn;
                for (int row = 0; row < 3; ++row)
                {
                    for (int column = 0; column < 3; ++column)
                        turn.m[row * 3 + column] = axes[column * 3 + row] >> (kNormalShift - 12);
                }
                glMultMatrix3x3(&turn);
            }
        }

        std::uint32_t bound = 0xFFFFFFFEu;
        const Surface *surface = view.surfaces + model.firstSurface;
        for (int n = model.surfaceCount; n > 0; --n, ++surface)
        {
            const std::uint32_t planeIndex = surface->planeRef & kSurfacePlaneMask;
            const fx side = planeDistance(view.planes[planeIndex], view.planeTypes[planeIndex], eye);
            if ((surface->planeRef & kSurfacePlaneBack) ? side >= 0 : side <= 0)
                continue;
            if (stats_.polygons + surface->polygons > polygonBudget() ||
                stats_.vertices + surface->vertices > vertexBudget())
            {
                stats_.budgetHit = true;
                break;
            }
            if (surface->texture != bound)
            {
                bound = surface->texture;
                const NEA_Material *material =
                    bound == kNoTexture ? nullptr : textureCache.material(bound);
                if (material != nullptr)
                    NEA_MaterialUse(material);
                else
                    GFX_TEX_FORMAT = 0;
            }
            const std::uint32_t *word = view.gxStream + surface->gxOfs;
            for (int k = surface->gxWords; k > 0; --k)
                GFX_FIFO = *word++;
            stats_.polygons += surface->polygons;
            stats_.vertices += surface->vertices;
            ++stats_.surfaces;
        }

        if (moved)
            glPopMatrix(1);
    }

    void WorldDraw::drawBrushes()
    {
        const LevelView &view = level.view;
        const std::uint16_t frame = visFrame_;

        // See-through brushes wait here to be sent far to near. A room with
        // more of them in view at once than this drops the nearest ones.
        constexpr int kMaxLater = 64;
        static std::uint16_t laterIndex[kMaxLater];
        static std::int32_t laterDistance[kMaxLater];
        int later = 0;
        int drawn = 0;

        for (std::uint32_t i = 0; i < view.brushCount; ++i)
        {
            const Brush &brush = view.brushes[i];
            const bool active = brushState_ != nullptr ? brushState_[i].active
                                                       : (brush.flags & kBrushStartsOff) == 0;
            const fx *pos = brushState_ != nullptr ? brushState_[i].pos : brush.origin;
            if (!active || brush.renderClass == static_cast<std::uint8_t>(RenderClass::Hidden))
                continue;

            // In a leaf the eye can see? A brush that roams is not asked: it
            // is wherever its track has taken it, and the view test below is
            // the only one it gets.
            const std::uint16_t *leaf = view.brushLeaves + brush.firstLeaf;
            bool visible = (brush.flags & kBrushRoams) != 0;
            for (int n = brush.leafCount; n > 0 && !visible; --n, ++leaf)
                visible = leafFrame_[*leaf] == frame;
            if (!visible)
                continue;

            // Its box, moved by however far the brush is from where it was
            // built, against the view. A brush that turns is given the box of
            // the whole circle it sweeps.
            std::int16_t mins[3], maxs[3];
            std::int32_t centre[3];
            std::int32_t reach = 0;
            const bool pivots = (brush.flags & kBrushHasPivot) != 0;
            const bool tumbles = pivots && (brush.flags & kBrushTumbles) != 0;
            if (pivots)
            {
                for (int corner = 0; corner < 8; ++corner)
                {
                    const std::int32_t x =
                        ((corner & 1) ? brush.maxs[0] : brush.mins[0]) - fxToInt(brush.origin[0]);
                    const std::int32_t y =
                        ((corner & 2) ? brush.maxs[1] : brush.mins[1]) - fxToInt(brush.origin[1]);
                    const std::int32_t z =
                        tumbles ? ((corner & 4) ? brush.maxs[2] : brush.mins[2]) -
                                      fxToInt(brush.origin[2])
                                : 0;
                    // An over-estimate, and no square root.
                    const std::int32_t far =
                        (x < 0 ? -x : x) + (y < 0 ? -y : y) + (z < 0 ? -z : z);
                    if (far > reach)
                        reach = far;
                }
            }
            for (int a = 0; a < 3; ++a)
            {
                const std::int32_t shift = fxToInt(pos[a] - brush.origin[a]);
                mins[a] = static_cast<std::int16_t>(brush.mins[a] + shift);
                maxs[a] = static_cast<std::int16_t>(brush.maxs[a] + shift);
                if (pivots && (a < 2 || tumbles))
                {
                    mins[a] = static_cast<std::int16_t>(fxToInt(pos[a]) - reach);
                    maxs[a] = static_cast<std::int16_t>(fxToInt(pos[a]) + reach);
                }
                centre[a] = (mins[a] + maxs[a]) / 2 - fxToInt(camera.pos[a]);
            }
            if (boxOutside(camera.planes, mins, maxs))
                continue;
            // Under pressure, the small and far are left out, as with the
            // world's surfaces (WorldDraw.hpp).
            if (const int detail = detailRatio())
            {
                std::int32_t size = 0;
                for (int a = 0; a < 3; ++a)
                    size = size > maxs[a] - mins[a] ? size : maxs[a] - mins[a];
                const std::int32_t depth = (centre[0] * camera.forward[0] + centre[1] * camera.forward[1] +
                                            centre[2] * camera.forward[2]) >>
                                           12;
                if (depth > (size >> 1) * detail)
                {
                    ++stats_.dropped;
                    continue;
                }
            }

            if (brush.renderClass == static_cast<std::uint8_t>(RenderClass::Opaque))
            {
                drawBrush(i, 0);
                ++drawn;
                continue;
            }
            if (later == kMaxLater)
                continue;
            // Insert by distance, farthest first. Halved coordinates keep the
            // squares inside 32 bits across the whole map.
            const std::int32_t distance = (centre[0] >> 1) * (centre[0] >> 1) +
                                          (centre[1] >> 1) * (centre[1] >> 1) +
                                          (centre[2] >> 1) * (centre[2] >> 1);
            int at = later++;
            while (at > 0 && laterDistance[at - 1] < distance)
            {
                laterIndex[at] = laterIndex[at - 1];
                laterDistance[at] = laterDistance[at - 1];
                --at;
            }
            laterIndex[at] = static_cast<std::uint16_t>(i);
            laterDistance[at] = distance;
        }

        // Each see-through brush gets a polygon ID of its own (1..63): the
        // hardware will not blend two polygons of the same ID over each other,
        // which is what keeps the back of a glass box from showing through its
        // front as a darker patch.
        for (int k = 0; k < later; ++k)
        {
            drawBrush(laterIndex[k], 1 + (k % 63));
            ++drawn;
        }
        stats_.brushes = drawn;
    }

    bool WorldDraw::ballVisible(const std::int16_t *centre, int radius) const
    {
        if (!level.loaded() || leafFrame_ == nullptr)
            return false;
        const fx at[3] = {fxFromInt(centre[0]), fxFromInt(centre[1]), fxFromInt(centre[2])};
        const int leaf = findLeaf(level.view, at);
        // In solid (leaf 0) it cannot be judged by leaf: an actor standing
        // half in a wall is still drawn if the view test lets it be.
        if (leaf != 0 && leafFrame_[leaf] != visFrame_)
            return false;
        return !sphereOutside(camera.planes, centre, radius);
    }

    bool WorldDraw::spend(int polygons, int vertices)
    {
        if (stats_.polygons + polygons > polygonBudget() || stats_.vertices + vertices > vertexBudget())
            return false;
        stats_.polygons += polygons;
        stats_.vertices += vertices;
        return true;
    }

    void WorldDraw::drawSky()
    {
        // A box round the eye, one picture a side, that has to come out
        // BEHIND the whole world. Depth here is distance (W-buffering), so
        // the box is simply made big: its sides are further away than
        // anything the walk lets through (Camera::kFarUnits). That puts it
        // beyond the hardware's far plane too, which would clip it away; so
        // it is drawn through a projection whose z row is the w row's
        // values, a hair short -- every point of it then sits just inside
        // the far plane as far as clipping goes, while its depth stays its
        // distance. The projection is read back because it is NEA's, and in
        // two-pass mode is one half's.
        const LevelView &view = level.view;
        const unsigned first = view.header->skyTexture - 1u;

        int projection[16];
        glGetFixed(GL_GET_MATRIX_PROJECTION, projection);
        m4x4 flat;
        for (int row = 0; row < 4; ++row)
        {
            for (int column = 0; column < 4; ++column)
                flat.m[row * 4 + column] = projection[row * 4 + column];
            flat.m[row * 4 + 2] = static_cast<int>(
                (static_cast<std::int64_t>(projection[row * 4 + 3]) * 4095) >> 12);
        }
        glMatrixMode(GL_PROJECTION);
        glPushMatrix();
        glLoadMatrix4x4(&flat);
        glMatrixMode(GL_MODELVIEW);
        glPushMatrix();
        glTranslatef32(camera.pos[0] >> 10, camera.pos[1] >> 10, camera.pos[2] >> 10);
        // The box is built 64 units to a side from the eye (what fits a
        // vertex) and scaled to half as far again as the cull distance.
        constexpr int kSkyScale = ((Camera::kFarUnits * 3 / 2) / 64) << 12;
        glScalef32(kSkyScale, kSkyScale, kSkyScale);
        NEA_PolyFormat(31, 0, static_cast<NEA_LightEnum>(0), NEA_CULL_NONE, kPolygonMode);

        // The game's own table of which way each side faces: for a point
        // (s, t) of a side's picture, each of x, y, z is plus or minus one of
        // s, t and 1 (as 1, 2, 3).
        static const std::int8_t kAxes[6][3] = {{3, -1, 2}, {-3, 1, 2},  {1, 3, 2},
                                                {-1, -3, 2}, {-2, -1, 3}, {2, -1, -3}};
        static const std::int8_t kCorner[4][2] = {{-1, -1}, {-1, 1}, {1, 1}, {1, -1}};
        constexpr int kHalf = 64 << 2;                    // 64 units, in vertex units
        constexpr int kTexels = hldata::kSkySide << 4;    // the picture's side, 12.4
        for (int side = 0; side < 6; ++side)
        {
            // ... and its table of which picture goes on which side: the
            // second and third are the other way round from their names.
            static const std::uint8_t kPicture[6] = {0, 2, 1, 3, 4, 5};
            const NEA_Material *material = textureCache.material(first + kPicture[side]);
            if (material == nullptr)
                continue;
            NEA_MaterialUse(material);
            GFX_COLOR = 0x7FFF;
            glBegin(GL_QUADS);
            for (int corner = 0; corner < 4; ++corner)
            {
                const int b[3] = {kCorner[corner][0], kCorner[corner][1], 1};
                int v[3];
                for (int a = 0; a < 3; ++a)
                {
                    const int k = kAxes[side][a];
                    v[a] = (k < 0 ? -b[-k - 1] : b[k - 1]) * kHalf;
                }
                // s runs across the picture, t up it: the picture's own
                // rows run down.
                const int s = (kCorner[corner][0] + 1) * kTexels / 2;
                const int t = (1 - kCorner[corner][1]) * kTexels / 2;
                GFX_TEX_COORD = TEXTURE_PACK(s, t);
                glVertex3v16(static_cast<v16>(v[0]), static_cast<v16>(v[1]),
                             static_cast<v16>(v[2]));
            }
            glEnd();
        }

        glPopMatrix(1);
        glMatrixMode(GL_PROJECTION);
        glPopMatrix(1);
        glMatrixMode(GL_MODELVIEW);
        NEA_PolyFormat(31, 0, static_cast<NEA_LightEnum>(0), kCull, polygonMode());
    }


    void WorldDraw::applyFog()
    {
        if (!fogOn_)
        {
            NEA_FogDisable();
            return;
        }
        // The hardware's fog goes by depth, in steps: with the depth buffer
        // holding distance (W), one of its units is kFogUnit of the map's.
        // Its table of 32 densities covers a band of 512 of those units, or
        // 1024, 2048, 4096; before the band there is none, past it the
        // last of the table. A straight ramp over the widest band that is
        // not more than the far half of what can be seen, ending where the
        // fog is to be whole.
        constexpr int kFogUnit = 16;
        int shift = 10;
        int band = 512;
        while (shift > 7 && band * 2 <= fogEnd_ / 2)
        {
            band *= 2;
            --shift;
        }
        int offset = (fogEnd_ - band) / kFogUnit;
        offset = offset < 0 ? 0 : (offset > 0x7FFF ? 0x7FFF : offset);
        GFX_CONTROL = (GFX_CONTROL & ~(15 << 8)) | GL_FOG | (shift << 8);
        // (The colour of what is behind everything: Global::InitVideo.)
        GFX_FOG_COLOR = RGB15(0, 0, 0) | (31 << 16);
        GFX_FOG_OFFSET = offset;
        // (The first entry has to be none: the hardware's rule.)
        for (int i = 0; i < 32; ++i)
            GFX_FOG_TABLE[i] = i == 31 ? 127 : i * 4;
    }

    void WorldDraw::govern(bool overflow, bool underflow, int lines, bool slow)
    {
        if (slow)
        {
            quickPasses_ = 0;
            if (spareQuarters_ > 0)
                --spareQuarters_;
        }
        else if (++quickPasses_ >= 60)
        {
            quickPasses_ = 0;
            if (spareQuarters_ < 4)
                ++spareQuarters_;
        }

        // Pressure: up after a few passes on end that ran out of room, down
        // after a couple of seconds of passes that had room for everything.
        // (If it has to go straight back up, it is let down less readily
        // the next time: a view on the edge is not to flicker.)
        ++sinceRelax_;
        if (stats_.budgetHit)
        {
            easyPasses_ = 0;
            if (++cutPasses_ >= 6)
            {
                cutPasses_ = 0;
                if (pressure_ < 3)
                {
                    ++pressure_;
                    if (sinceRelax_ < 600 && relaxWait_ < kRelaxWait * 16)
                        relaxWait_ *= 2;
                }
            }
        }
        else
        {
            cutPasses_ = 0;
            // (Room for what is being left out as well: otherwise letting
            // it back in would only cut the view again.)
            const bool room =
                stats_.polygons + stats_.droppedPolygons < polygonBudget() * 15 / 16 &&
                stats_.vertices + stats_.droppedVertices < vertexBudget() * 15 / 16;
            if (pressure_ > 0 && room)
            {
                if (++easyPasses_ >= relaxWait_)
                {
                    easyPasses_ = 0;
                    sinceRelax_ = 0;
                    --pressure_;
                }
            }
            else
            {
                easyPasses_ = 0;
            }
        }
        // The fog: whole a little short of where the view is cut off. In at
        // once (a quarter of the way each pass), out slowly; with nothing
        // cut off it goes out until it is no fog at all.
        {
            int want = kNoFog;
            if (stats_.budgetHit)
            {
                want = stats_.cutDistance * 7 / 8;
                want = want < 192 ? 192 : (want > kNoFog ? kNoFog : want);
            }
            if (want < fogEnd_)
                fogEnd_ -= (fogEnd_ - want + 3) / 4;
            else if (want > fogEnd_)
                fogEnd_ += (fogEnd_ >> 7) + 1;
            if (fogEnd_ > kNoFog)
                fogEnd_ = kNoFog;
            fogOn_ = fogEnd_ < kNoFog;
        }

        // How much is taken at once, the most that is ever held back, and
        // the lead (of 46 lines) under which a pass counts as too heavy
        // although it was finished.
        constexpr int kStep = 120;
        constexpr int kMost = 900;
        constexpr int kLowLines = 6;
        constexpr int kEasyLines = 20;
        if (overflow || stats_.hwPolygons > 2040 || stats_.hwVertices > 6100)
            held_ += kStep * 2;
        else if (underflow)
            held_ += kStep;
        else if (lines < kLowLines)
            held_ += kStep / 4;
        else if (held_ > 0 && lines >= kEasyLines && stats_.hwPolygons < 1900)
            held_ -= 2; // a second and more for each step taken
        if (held_ > kMost)
            held_ = kMost;
        if (held_ < 0)
            held_ = 0;

        // What was counted and not used. The lesser of the last two passes
        // of this half, and three quarters of that: the view may be turning
        // towards somewhere with less off the screen.
        constexpr int kMostSparePolygons = 600;
        constexpr int kMostSpareVertices = 1500;
        int polygons = stats_.polygons - stats_.hwPolygons;
        int vertices = stats_.vertices - stats_.hwVertices;
        polygons = polygons < 0 ? 0 : polygons;
        vertices = vertices < 0 ? 0 : vertices;
        const int p = polygons < unusedPolygons_[pass_] ? polygons : unusedPolygons_[pass_];
        const int v = vertices < unusedVertices_[pass_] ? vertices : unusedVertices_[pass_];
        unusedPolygons_[pass_] = polygons;
        unusedVertices_[pass_] = vertices;
        sparePolygons_[pass_] = p * 3 / 4 > kMostSparePolygons ? kMostSparePolygons : p * 3 / 4;
        spareVertices_[pass_] = v * 3 / 4 > kMostSpareVertices ? kMostSpareVertices : v * 3 / 4;
        sparePolygons_[pass_] = sparePolygons_[pass_] * spareQuarters_ / 4;
        spareVertices_[pass_] = spareVertices_[pass_] * spareQuarters_ / 4;
        // None of it while the hardware is complaining.
        if (held_ > 0)
            sparePolygons_[pass_] = spareVertices_[pass_] = 0;
    }
} // namespace hlrt
