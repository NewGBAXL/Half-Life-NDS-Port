#pragma once

// Streaming a map's textures.
//
// A map cooked with --stream-textures has textures that are bigger than
// video memory can hold all at once, marked kTextureStreamed. TextureCache
// draws those as a flat color; this reads the real ones in from the card as
// they come into view and lets the ones that have not been seen for a while go.
//
// A texture drawn in the last few frames is wanted above all else, one drawn
// a little while ago not as much.
// What a map has room for is found when it is loaded (begin):
// When a map is loaded, it gets what is left over after after NEA handles
// the not-streamed textures, sprites and models.
// The textures used by every view (TextureRef::share) are loaded as resident,
// the rest by how widely they are wanted. Maps whose textures all fit have all
// of them resident and never stream at all.
//
// NEA places each texture itself. If it cannot (memory is not all one piece,
// or part of it cannot take a 4x4 texture), the plan is told how much the
// memory really holds and goes on within that.

#include <cstdint>

namespace hlrt
{
    /// Per texture, the frame it was last drawn in. Written by every surface
    /// that is drawn (noteTextureDrawn); null when nothing is streamed.
    extern std::uint32_t *g_textureSeenAt;
    extern std::uint32_t g_textureFrame;

    inline void noteTextureDrawn(unsigned index)
    {
        // (0xFFFF: no texture. 512 at most.)
        if (g_textureSeenAt != nullptr && index < 512u)
            g_textureSeenAt[index] = g_textureFrame;
    }

    struct StreamStats
    {
        int streamed = 0;   ///< total streamed textures
        int resident = 0;   ///< current streamed textures in memory
        int pinned = 0;
        int loads = 0;
        int evictions = 0;
        int refused = 0;    ///< times NEA refused textures due to out-of-memory
        int flatNow = 0;    ///< textures drawn in last few frames as flat color
        int copies = 0;     ///< textures kept in main memory (cycles' pictures, tiled variants)
        int copiesKb = 0;
        int texelKb = 0;    ///< texels currently in use
        int capacityKb = 0; ///< total estimated capacity
    };

    class TextureStream
    {
    public:
        /// Load after everything else has, as much fits.
        void begin();
        void end();
        bool active() const { return active_; }

        /// Upload right after the vertical blank.
        void upload();
        /// At the end of each frame, calculate what is wanted next,
        /// and how many `budgetBytes` of the card read for it.
        void update(int budgetBytes);

        StreamStats stats() const;

    private:
        bool loadNow(int id);
        void demand();

        bool active_ = false;
    };

    extern TextureStream textureStream;
} // namespace hlrt
