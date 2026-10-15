#pragma once

// The models of the loaded map: each one's cooked file (mdl/<map>/<name>.hmd)
// held in memory and opened, and its skin (mdl/<name>.htx) in video memory,
// except weapons' (v_*), of which only the one in hand is in video memory.
//
// One entry per ModelRef of the level, in the level's order. A model that is
// not on the card, or does not fit, is simply not there: its view's header is
// null and the actors made of it are not drawn.

#include <NEAMain.h>

#include "HlModel.hpp"

namespace hlrt
{
    class ModelCache
    {
    public:
        /// Load the models of the level that is loaded, out of its archive.
        bool create();
        void destroy();
        /// As destroy(), for when the 3D engine has been shut down under it:
        /// the skins went with the engine.
        void forget();

        /// For when the 3D engine is brought up again under a map that
        /// stays loaded: the skins are gone with the old engine and are
        /// loaded into the new one. The models themselves are not touched.
        void forgetSkins();
        void loadSkins();

        const hldata::ModelView *views() const { return views_; }
        int count() const { return count_; }
        NEA_Material *skin(unsigned index) const { return materials_[index]; }
        int missing() const { return missing_; }

        /// `index`: weapon in hand; -1: none.
        void wield(int index) { wanted_ = index; }
        /// Do what wield() asked: straight after the vertical blank.
        void apply();
        /// Whether the model can be drawn: not a weapon whose skin is not
        /// in video memory yet.
        bool drawable(unsigned index) const
        {
            return kept_ == nullptr || index >= static_cast<unsigned>(count_) || kept_[index] == 0 ||
                   resident_ == static_cast<int>(index);
        }
        /// How much video memory the weapons will want that is not taken yet,
        /// for whoever else shares the memory to leave.
        int reservedBytes() const;
        /// Models whose skin did not load (not on the card, or no room).
        int bare() const { return bare_; }

        ModelCache() = default;
        ModelCache(const ModelCache &) = delete;
        ModelCache &operator=(const ModelCache &) = delete;

    private:
        void release(bool engineGone);

        hldata::ModelView *views_ = nullptr;
        std::uint8_t **blocks_ = nullptr;
        NEA_Material **materials_ = nullptr;
        NEA_Palette **palettes_ = nullptr;
        std::uint8_t *kept_ = nullptr;
        int wanted_ = -1;
        int resident_ = -1;
        int count_ = 0;
        int missing_ = 0;
        int bare_ = 0;
    };

    extern ModelCache modelCache;
} // namespace hlrt
