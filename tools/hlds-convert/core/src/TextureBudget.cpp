#include "hlds/TextureBudget.hpp"

#include "hlds/TextureFamily.hpp"

#include <algorithm>
#include <cmath>

#include "hlds/MapStats.hpp"

namespace hlds
{
    namespace
    {
        double faceArea(const BspFile &bsp, const BspFace &face)
        {
            const float *a = bsp.faceVertex(face, 0).point;
            double area = 0;
            for (int i = 1; i + 1 < face.edgeCount; ++i)
            {
                const float *b = bsp.faceVertex(face, i).point;
                const float *c = bsp.faceVertex(face, i + 1).point;
                const double e1[3] = {double(b[0]) - a[0], double(b[1]) - a[1], double(b[2]) - a[2]};
                const double e2[3] = {double(c[0]) - a[0], double(c[1]) - a[1], double(c[2]) - a[2]};
                const double n[3] = {e1[1] * e2[2] - e1[2] * e2[1], e1[2] * e2[0] - e1[0] * e2[2],
                                     e1[0] * e2[1] - e1[1] * e2[0]};
                area += 0.5 * std::sqrt(n[0] * n[0] + n[1] * n[1] + n[2] * n[2]);
            }
            return area;
        }

        /// A 4x4 block needs two to four colours of its own before it starts
        /// sharing, so the useful palette grows with the texel count.
        int coloursFor(int texels, int divisor)
        {
            return std::clamp(texels / divisor, 16, 512) & ~1;
        }
    } // namespace

    std::vector<char> findHiddenTextures(const BspFile &bsp, const WadSet &wads)
    {
        std::vector<char> hidden(bsp.textures.size(), 0);
        for (std::size_t t = 0; t < bsp.textures.size(); ++t)
        {
            const BspTexture &tex = bsp.textures[t];
            if (tex.missing || tex.name.empty() || tex.name[0] != '{')
                continue;
            IndexedImage wadImage;
            std::string ignored;
            const std::vector<std::uint8_t> *pixels = nullptr;
            if (tex.embedded)
                pixels = &tex.pixels;
            else if (wads.read(tex.name, wadImage, ignored))
                pixels = &wadImage.pixels;
            if (pixels == nullptr || pixels->empty())
                continue; // not found: reported as missing elsewhere
            hidden[t] = std::all_of(pixels->begin(), pixels->end(),
                                    [](std::uint8_t p) { return p == 255; })
                            ? 1
                            : 0;
        }
        return hidden;
    }

    TextureNeeds textureNeeds(const BspFile &bsp, const std::vector<BrushInfo> &brushes,
                              const std::vector<char> &hidden)
    {
        TextureNeeds needs;
        needs.opaque.assign(bsp.textures.size(), 0);
        needs.alpha.assign(bsp.textures.size(), 0);
        needs.opaqueArea.assign(bsp.textures.size(), 0.0);
        needs.alphaArea.assign(bsp.textures.size(), 0.0);

        const std::vector<const BrushInfo *> owner = brushOfModel(bsp, brushes);
        for (std::size_t m = 0; m < bsp.models.size(); ++m)
        {
            const BrushInfo *brush = owner[m];
            // The world is always drawn. Any other model is drawn as its
            // entity says, and a model no entity names is not drawn at all.
            if (m != 0 && (brush == nullptr || !brush->drawn() ||
                           brush->renderClass == hldata::RenderClass::Colour))
                continue;
            const BspModel &model = bsp.models[m];
            for (int f = model.firstFace; f < model.firstFace + model.faceCount; ++f)
            {
                const BspFace &face = bsp.faces[f];
                const int t = wornTexture(bsp, face);
                const BspTexture &tex = bsp.textures[t];
                if (tex.missing || isUndrawnTexture(tex.name) || (!hidden.empty() && hidden[t]))
                    continue;
                const bool holes = brush != nullptr && brush->cutsHoles() && !tex.name.empty() &&
                                   tex.name[0] == '{';
                (holes ? needs.alpha : needs.opaque)[t] = 1;
                (holes ? needs.alphaArea : needs.opaqueArea)[t] += faceArea(bsp, face);
            }
        }
        // A texture that is one picture of several needs the others too,
        // each as much as itself: the rest of its cycle, and its other face.
        for (std::size_t t = 0; t < bsp.textures.size(); ++t)
        {
            if (bsp.textures[t].missing || bsp.textures[t].name.empty() || bsp.textures[t].name[0] != '+')
                continue;
            if (!needs.opaque[t] && !needs.alpha[t])
                continue;
            const TextureFamily family = textureFamily(bsp, static_cast<int>(t));
            for (const std::vector<int> *part : {&family.frames, &family.other})
            {
                for (int member : *part)
                {
                    needs.opaque[member] = needs.opaque[member] || needs.opaque[t];
                    needs.alpha[member] = needs.alpha[member] || needs.alpha[t];
                    needs.opaqueArea[member] = std::max(needs.opaqueArea[member], needs.opaqueArea[t]);
                    needs.alphaArea[member] = std::max(needs.alphaArea[member], needs.alphaArea[t]);
                }
            }
        }
        return needs;
    }

    namespace
    {
        /// Halve whichever texture is doing least with its memory until the
        /// total fits. Returns false if it cannot be made to.
        bool shrinkToFit(std::vector<TextureSize> &sizes, const std::vector<double> &area,
                         int budget, int &total, std::vector<char> &shrunk)
        {
            auto texelsOf = [&](std::size_t t)
            {
                return static_cast<int>(sizes[t].width) * sizes[t].height;
            };
            total = 0;
            for (std::size_t t = 0; t < sizes.size(); ++t)
                total += texelsOf(t);
            while (total > budget)
            {
                int pick = -1;
                double worst = 0;
                for (std::size_t t = 0; t < sizes.size(); ++t)
                {
                    const TextureSize &size = sizes[t];
                    if (size.width == 0 ||
                        (size.width <= kMinTextureSide && size.height <= kMinTextureSide))
                        continue;
                    const double density = area[t] / texelsOf(t);
                    if (pick < 0 || density < worst)
                    {
                        pick = static_cast<int>(t);
                        worst = density;
                    }
                }
                if (pick < 0)
                    return false;
                TextureSize &size = sizes[pick];
                total -= texelsOf(pick);
                if (size.width >= size.height && size.width > kMinTextureSide)
                    size.width /= 2;
                else if (size.height > kMinTextureSide)
                    size.height /= 2;
                else
                    size.width /= 2;
                total += texelsOf(pick);
                shrunk[pick] = 1;
            }
            return true;
        }
    } // namespace

    TexturePlan planTextures(const BspFile &bsp, const TextureNeeds &needs, int texelBudget,
                             int colourBudget, int alphaBudget)
    {
        TexturePlan plan;
        plan.sizes.assign(bsp.textures.size(), TextureSize{});
        plan.alphaSizes.assign(bsp.textures.size(), TextureSize{});
        plan.paletteColours.assign(bsp.textures.size(), 0);
        for (std::size_t t = 0; t < bsp.textures.size(); ++t)
        {
            const BspTexture &tex = bsp.textures[t];
            if (needs.opaque[t])
            {
                plan.sizes[t] = defaultTextureSize(tex.width, tex.height);
                ++plan.usedTextures;
            }
            if (needs.alpha[t])
            {
                plan.alphaSizes[t] = defaultTextureSize(tex.width, tex.height);
                ++plan.usedTextures;
            }
        }

        std::vector<char> shrunk(bsp.textures.size(), 0);
        if (!shrinkToFit(plan.sizes, needs.opaqueArea, texelBudget, plan.texels, shrunk))
            plan.fits = false;
        if (!shrinkToFit(plan.alphaSizes, needs.alphaArea, alphaBudget, plan.alphaTexels, shrunk))
            plan.fits = false;
        plan.shrunk = static_cast<int>(std::count(shrunk.begin(), shrunk.end(), 1));
        // The pictures of one texture are all one size: the smallest any of
        // them was given. (One that changed size as it changed would jump.)
        for (std::vector<TextureSize> *sizes : {&plan.sizes, &plan.alphaSizes})
        {
            for (std::size_t t = 0; t < bsp.textures.size(); ++t)
            {
                const std::string &name = bsp.textures[t].name;
                if ((*sizes)[t].width == 0 || name.size() < 2 || name[0] != '+' || name[1] != '0')
                    continue;
                const TextureFamily family = textureFamily(bsp, static_cast<int>(t));
                TextureSize least = (*sizes)[t];
                for (const std::vector<int> *part : {&family.frames, &family.other})
                {
                    for (int member : *part)
                    {
                        if ((*sizes)[member].width == 0)
                            continue;
                        least.width = std::min(least.width, (*sizes)[member].width);
                        least.height = std::min(least.height, (*sizes)[member].height);
                    }
                }
                for (const std::vector<int> *part : {&family.frames, &family.other})
                {
                    for (int member : *part)
                    {
                        if ((*sizes)[member].width != 0)
                            (*sizes)[member] = least;
                    }
                }
            }
        }
        plan.texels = plan.alphaTexels = 0;
        for (std::size_t t = 0; t < bsp.textures.size(); ++t)
        {
            plan.texels += static_cast<int>(plan.sizes[t].width) * plan.sizes[t].height;
            plan.alphaTexels += static_cast<int>(plan.alphaSizes[t].width) * plan.alphaSizes[t].height;
        }

        // The palette bank is shared: every A3I5 texture takes 32 colours of
        // it, and the 4x4 textures divide what is left. One colour per 16
        // texels is generous; fall back to fewer for the whole map until the
        // total fits.
        int alphaColours = 0;
        for (std::size_t t = 0; t < plan.alphaSizes.size(); ++t)
        {
            if (plan.alphaSizes[t].width != 0)
                alphaColours += hldata::kA3I5Colours;
        }
        static constexpr int kDivisors[] = {16, 24, 32, 48, 64, 96, 128};
        for (int divisor : kDivisors)
        {
            plan.colours = alphaColours;
            for (std::size_t t = 0; t < plan.sizes.size(); ++t)
            {
                if (plan.sizes[t].width == 0)
                    continue;
                plan.paletteColours[t] = coloursFor(
                    static_cast<int>(plan.sizes[t].width) * plan.sizes[t].height, divisor);
                plan.colours += plan.paletteColours[t];
            }
            if (plan.colours <= colourBudget)
                break;
        }
        if (plan.colours > colourBudget)
            plan.fits = false;
        return plan;
    }

    TexturePlan planTextures(const BspFile &bsp, int texelBudget, int colourBudget,
                             const std::vector<char> &hidden)
    {
        // No entities: every model is a plain opaque one.
        std::vector<BrushInfo> everyModel;
        for (std::size_t m = 1; m < bsp.models.size(); ++m)
        {
            BrushInfo info;
            info.model = static_cast<int>(m);
            everyModel.push_back(info);
        }
        return planTextures(bsp, textureNeeds(bsp, everyModel, hidden), texelBudget, colourBudget,
                            kAlphaTexelBudget);
    }
} // namespace hlds
