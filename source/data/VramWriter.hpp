#pragma once

// Writes to video memory that cannot be seen being made.
//
// The 3D engine draws a picture from line 214 of the vertical blank on, 48
// lines ahead of the screen, and goes on until the last line of the picture
// has been drawn: from then until line 214 nothing reads the textures, and that
// is the only time they may be written (a bank given to the CPU for a write is
// a bank the 3D engine draws nothing from). A texture is too big to be sure of
// writing it in that time, so it is not tried: what is to be written is queued
// here, and the vertical blank interrupt writes it a piece of kPiece bytes at a
// time, looking at the line before each piece, and stops at kLastLine. One
// piece takes well under a line; what is left waits for the next blank. The
// interrupt runs at line 192 whatever the game is doing, a slow frame or not.
//
// What is written must be memory nothing draws from (the caller's to make
// sure of: TextureStream's fixed region and its plan, ModelCache's skin
// slots), and is used only once done() says it is all there.

#include <cstdint>

namespace hlrt
{
    class VramWriter
    {
    public:
        /// The last line a piece may start on.
        static constexpr int kLastLine = 206;
        static constexpr std::uint32_t kPiece = 256;

        /// Queue `bytes` (a multiple of 4) from `src` to `dst`, an LCD address
        /// (NEA_VramWrite). `src` must stay as it is until done. Returns the
        /// write's ticket, or 0 if the queue is full.
        std::uint32_t write(void *dst, const void *src, std::uint32_t bytes);
        /// Whether the write with this ticket (and all before it) is done.
        bool done(std::uint32_t ticket) const;
        bool idle() const;
        /// Drop what is not written yet (a map is left).
        void clear();

        /// From the vertical blank interrupt.
        void onVblank();

        /// What it has done, for the overlay and perf.txt.
        struct Stats
        {
            std::uint32_t bytes = 0;  ///< written in all
            std::uint32_t pieces = 0;
            std::uint32_t blanks = 0; ///< blanks that wrote something
            std::uint32_t late = 0;   ///< blanks that ended with writes still waiting
            int mostLine = 0;         ///< the latest line a piece ended on
            int lastLine = 0;         ///< the line the last blank's writing ended on
        };
        Stats stats() const;
    };

    extern VramWriter vramWriter;
} // namespace hlrt
