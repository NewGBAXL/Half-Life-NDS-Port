#include "render/HudDraw.hpp"
#include "render/WorldDraw.hpp"

#include "render/EffectDraw.hpp"
#include "render/ModelDraw.hpp"

#include "Global.hpp"

#include <cstdlib>
#include <cstring>

#include <nds.h>

#include <NEAMain.h>

#include "DebugLog.hpp"
#include "HlPlaneMath.hpp"
#include "data/LevelFile.hpp"
#include "data/TextureCache.hpp"
#include "audio/Sound.hpp"
#include "render/Camera.hpp"
#include "render/WorldDrawParts.hpp"

// THIS FILE IS IN ITCM (the name says so to the linker). Keep it to the frame's
// inner loops: no standard-library containers, no strings, nothing that would
// drag template code into the 32 KB.

namespace hlrt
{
    WorldDraw worldDraw;

    using namespace hldata;
    using namespace worldparts;

    void WorldDraw::walk()
    {
        const LevelView &view = level.view;
        const std::uint16_t frame = visFrame_;
        const CullPlane *planes = camera.planes;
        const fx *eye = camera.pos;

        std::memset(chosen_, 0, chosenWords_ * 4);
        int polygons = 0;
        int vertices = 0;
        int surfaces = 0;
        int dropped = 0, droppedPolygons = 0, droppedVertices = 0;
        // Under pressure, what is small and far is left out (WorldDraw.hpp):
        // how far in front of the eye a surface is, against its size.
        const int detail = detailRatio();
        if (detail != 0)
            std::memset(skipped_, 0, chosenWords_ * 4);
        const std::int32_t *forward = camera.forward;
        const std::int32_t eyeAt[3] = {fxToInt(eye[0]), fxToInt(eye[1]), fxToInt(eye[2])};
        stats_.cutDistance = 0;
        // The models have been costed already (ModelDraw::plan): the world
        // has what they leave.
        const int polygonBudget = this->polygonBudget() - modelPolygons_;
        const int vertexBudget = this->vertexBudget() - modelVertices_;

        int sp = 0;
        walkStack[sp++] = static_cast<std::int16_t>(view.models[0].headNode);
        while (sp > 0)
        {
            const std::int32_t index = walkStack[--sp];
            if (index >= 0)
            {
                if (nodeFrame_[index] != frame)
                    continue;
                const Node &node = view.nodes[index];
                if (boxOutside(planes, node.mins, node.maxs))
                    continue;
                const fx d = planeDistance(view.planes[node.plane], view.planeTypes[node.plane], eye);
                const int nearSide = d >= 0 ? 0 : 1;
                // Far first, so the near side is popped first.
                if (sp < kWalkStack - 2)
                    walkStack[sp++] = node.children[nearSide ^ 1];
                walkStack[sp++] = node.children[nearSide];
                continue;
            }

            const int leafIndex = -1 - index;
            if (leafFrame_[leafIndex] != frame)
                continue;
            const Leaf &leaf = view.leaves[leafIndex];
            if (boxOutside(planes, leaf.mins, leaf.maxs))
                continue;

            const std::uint16_t *mark = view.marks + leaf.firstMark;
            for (int m = leaf.markCount; m > 0; --m, ++mark)
            {
                const std::uint32_t s = *mark;
                std::uint32_t &word = chosen_[s >> 5];
                const std::uint32_t bit = 1u << (s & 31);
                if (word & bit)
                    continue; // already taken through another leaf

                const Surface &surface = view.surfaces[s];
                const std::uint32_t planeIndex = surface.planeRef & kSurfacePlaneMask;
                const fx side =
                    planeDistance(view.planes[planeIndex], view.planeTypes[planeIndex], eye);
                // A face on the plane's front is seen from in front; the back
                // flag turns that round.
                if ((surface.planeRef & kSurfacePlaneBack) ? side >= 0 : side <= 0)
                    continue;
                if (sphereOutside(planes, surface.center, surface.radius))
                    continue;
                if (detail != 0)
                {
                    const std::int32_t depth = ((surface.center[0] - eyeAt[0]) * forward[0] +
                                                (surface.center[1] - eyeAt[1]) * forward[1] +
                                                (surface.center[2] - eyeAt[2]) * forward[2]) >>
                                               12;
                    if (depth > static_cast<std::int32_t>(surface.radius) * detail)
                    {
                        // (Counted once, though it is met in every leaf it
                        // is part of.)
                        std::uint32_t &seen = skipped_[s >> 5];
                        if (!(seen & bit))
                        {
                            seen |= bit;
                            ++dropped;
                            droppedPolygons += surface.polygons;
                            droppedVertices += surface.vertices;
                        }
                        continue;
                    }
                }

                if (polygons + surface.polygons > polygonBudget ||
                    vertices + surface.vertices > vertexBudget)
                {
                    // Out of room, and everything still on the stack is farther
                    // away than this: stop here.
                    stats_.budgetHit = true;
                    // How far off that is: where the fog has to be whole.
                    const std::int32_t depth = ((surface.center[0] - eyeAt[0]) * forward[0] +
                                                (surface.center[1] - eyeAt[1]) * forward[1] +
                                                (surface.center[2] - eyeAt[2]) * forward[2]) >>
                                               12;
                    stats_.cutDistance = depth - surface.radius;
                    sp = 0;
                    break;
                }
                word |= bit;
                polygons += surface.polygons;
                vertices += surface.vertices;
                ++surfaces;
            }
        }
        stats_.surfaces = surfaces;
        stats_.dropped = dropped;
        stats_.droppedPolygons = droppedPolygons;
        stats_.droppedVertices = droppedVertices;
        stats_.polygons = polygons;
        stats_.vertices = vertices;
    }

    namespace
    {
        /// Start the DSi's own DMA feeding `words` words to the geometry
        /// engine, as fast as it asks for them, and come straight back. (As
        /// NEA's NEA_DisplayListDrawNDMA_GFX_FIFO does it, which waits.)
        inline void feed(const std::uint32_t *from, std::uint32_t words)
        {
            DC_FlushRange(from, words * 4);
            REG_NDMA_SRC(0) = reinterpret_cast<std::uint32_t>(from);
            REG_NDMA_DEST(0) = reinterpret_cast<std::uint32_t>(&GFX_FIFO);
            REG_NDMA_LENGTH(0) = words;
            REG_NDMA_BLENGTH(0) = 112;
            REG_NDMA_CR(0) = NDMA_ENABLE | NDMA_START_FIFO | NDMA_SRC_INC | NDMA_DST_FIX;
        }
    } // namespace

    void WorldDraw::emitList()
    {
        const LevelView &view = level.view;
        std::uint32_t bound = 0xFFFFFFFFu;
        // `sent` .. `at` is put together but not yet handed over. It is
        // handed over whenever the DMA has finished with the piece before:
        // the geometry engine is being fed while the rest is put together.
        std::uint32_t *at = list_;
        const std::uint32_t *sent = list_;
        const std::uint32_t *const end = list_ + kListWords;
        constexpr int kPiece = 512; // words worth starting the DMA for

        for (std::uint32_t w = 0; w < chosenWords_; ++w)
        {
            std::uint32_t bits = chosen_[w];
            while (bits != 0)
            {
                const int b = __builtin_ctz(bits);
                bits &= bits - 1;
                const Surface &surface = view.surfaces[(w << 5) + b];
                if (at + surface.gxWords + 4 > end)
                {
                    // No room left (it does not happen: see kListWords): all
                    // of it goes, and the list is begun again.
                    while (ndmaBusy(0))
                    {
                    }
                    if (at != sent)
                        feed(sent, static_cast<std::uint32_t>(at - sent));
                    while (ndmaBusy(0))
                    {
                    }
                    at = list_;
                    sent = list_;
                    if (surface.gxWords + 4 > kListWords)
                        continue;
                }
                if (surface.texture != bound)
                {
                    // What NEA_MaterialUse writes to registers, as commands.
                    bound = surface.texture;
                    const std::uint32_t *bind = textureCache.bind(textureCache.shown(bound));
                    *at++ = FIFO_COMMAND_PACK(FIFO_PAL_FORMAT, FIFO_COLOR, FIFO_TEX_FORMAT, FIFO_NOP);
                    *at++ = bind[1];
                    *at++ = 0x7FFF;
                    *at++ = bind[0];
                }
                std::memcpy(at, view.gxStream + surface.gxOfs, surface.gxWords * 4u);
                at += surface.gxWords;
                if (at - sent >= kPiece && !ndmaBusy(0))
                {
                    feed(sent, static_cast<std::uint32_t>(at - sent));
                    sent = at;
                }
            }
        }
        if (at != sent)
        {
            while (ndmaBusy(0))
            {
            }
            feed(sent, static_cast<std::uint32_t>(at - sent));
        }
        listSent_ = true;
    }

    void WorldDraw::finishList()
    {
        if (!listSent_)
            return;
        // Nothing else may be written to the geometry engine until the last
        // of the list has gone in.
        while (ndmaBusy(0))
        {
        }
        listSent_ = false;
    }

    void WorldDraw::emit()
    {
        // On a DSi, by display list. (Not while its DSP is in use: a DMA to
        // the geometry engine and the DSP's own traffic do not get on --
        // NEA's notes on its display lists -- and the DSP is an option.)
        if (list_ != nullptr && !sound.extended())
        {
            emitList();
            return;
        }
        const LevelView &view = level.view;
        // Surfaces are stored sorted by texture, so walking the bits in order
        // changes texture as seldom as it can.
        std::uint32_t bound = 0xFFFFFFFFu;

        for (std::uint32_t w = 0; w < chosenWords_; ++w)
        {
            std::uint32_t bits = chosen_[w];
            while (bits != 0)
            {
                const int b = __builtin_ctz(bits);
                bits &= bits - 1;
                const Surface &surface = view.surfaces[(w << 5) + b];
                if (surface.texture != bound)
                {
                    bound = surface.texture;
                    // (Whichever picture of it is due: TextureCache.)
                    const NEA_Material *material = textureCache.material(textureCache.shown(bound));
                    if (material != nullptr)
                        NEA_MaterialUse(material);
                    else
                        GFX_TEX_FORMAT = 0; // not on the card: baked light alone
                }
                const std::uint32_t *word = view.gxStream + surface.gxOfs;
                // The FIFO takes packed command words as they are; a write
                // simply waits while it is full.
                for (int n = surface.gxWords; n > 0; --n)
                    GFX_FIFO = *word++;
            }
        }
    }

    // Liquids sway: their textures' coordinates go through the texture
    // matrix (TextureCache asks for that), and the matrix slides them round
    // a small circle, a couple of texels across, every four seconds. In the
    // matrix a texel is 16: the hardware feeds it a sixteenth where a vertex
    // has a one.
    void WorldDraw::sway()
    {
        const auto phase = static_cast<std::uint16_t>(Global::vblanks() * 273u);
        fx sine, cosine;
        fxSinCos(phase, sine, cosine);
        glMatrixMode(GL_TEXTURE);
        glLoadIdentity();
        glTranslatef32(sine << 5, cosine << 5, 0);
        glMatrixMode(GL_MODELVIEW);
    }

    void WorldDraw::draw()
    {
        if (!level.loaded() || chosen_ == nullptr)
            return;
        stats_.budgetHit = false;
        const int leaf = findLeaf(level.view, camera.pos);
        stats_.leaf = leaf;
        if (leaf != visLeaf_)
            markVisible(leaf);

        // The sky, behind everything, where any of it can be seen from here.
        if (level.view.header->skyTexture != 0 && leaf >= 0 &&
            (level.view.leaves[leaf].flags & kLeafSeesSky))
            drawSky();

        textureCache.frame();
        sway();
        {
            const DebugLog::Perf::Scope scope(DebugLog::Section::Walk);
            modelDraw.plan(modelPolygons_, modelVertices_);
            // (What is only seen -- sprites, beams, bursts -- is costed
            // with the models: the world has what both leave.)
            effectDraw.gather(modelPolygons_, modelVertices_);
            // (... and the head-up display, which is drawn over it all.)
            modelPolygons_ += HudDraw::kPolygons + (hudDraw.lampAlight() ? HudDraw::kLampPolygons : 0);
            modelVertices_ += HudDraw::kVertices + (hudDraw.lampAlight() ? HudDraw::kLampVertices : 0);
            // (... and the marks and small things there are: nothing, with
            // none.)
            effectDraw.reserve(modelPolygons_, modelVertices_);
            walk();
            // ... and the models' share is counted with the world's from here
            // on, so that the brushes are held to what is really left.
            stats_.polygons += modelPolygons_;
            stats_.vertices += modelVertices_;
        }
        {
            const DebugLog::Perf::Scope scope(DebugLog::Section::Emit);
            emit();
            // While a display list is on its way, what does not need the
            // geometry engine: where the models' bones are.
            modelDraw.prepare();
            finishList();
            // The actors, between the world and its see-through brushes: they
            // are opaque, and glass is drawn over them.
            stats_.models = 0;
            stats_.modelsCut = 0;
            modelDraw.draw(stats_.models, stats_.modelsCut);
            drawBrushes(); // (and the effects, in among the glass)
            modelDraw.drawHeld();
        }

        // The usage registers are only meaningful once the geometry engine has
        // worked through everything sent. It keeps pace with the loop above, so
        // this wait is a few cycles.
        while (GFX_STATUS & BIT(27))
        {
        }
        stats_.hwPolygons = GFX_POLYGON_RAM_USAGE;
        stats_.hwVertices = GFX_VERTEX_RAM_USAGE;
    }
} // namespace hlrt
