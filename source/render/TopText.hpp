#pragma once

// What the game writes over the picture: a chapter's name, the names of the
// introduction, the game's own title (the level's Messages: HlLevel.hpp).
//
// It is not drawn in 3D. The letters are put, by the processor, on sprites of
// the top screen, in front of the 3D picture: video bank G -- which nothing
// else uses -- is a sheet of 256 by 128 pixels in sixteen colours, the same
// form as the touch screen's canvas (shared/HlFont.hpp), shown as four bands
// of 256 by 32 that are each put where a line of writing goes. So it costs
// no texture memory and is the same in every way of drawing the world; and
// since the sprites are added to what is behind them, as the game adds its
// writing, a fade is nothing but their colours.
//
// Two messages at once at most (a title and a name, say), two lines each.

#include <cstdint>

#include "HlFont.hpp"
#include "HlLevel.hpp"

namespace hlrt
{
    class TopText
    {
    public:
        /// Show `message`, whose text is `text` (which must stay where it
        /// is: the level's Strings).
        void show(const hldata::MessageRecord &message, const char *text);
        /// `ms` more milliseconds.
        void tick(int ms);
        /// Before each pass is drawn. (In the two-pass mode that joins its
        /// halves in video memory, every other picture on the screen is a
        /// capture of the whole screen -- the writing with it: over that
        /// one the sprites add nothing, or it would be written twice as
        /// bright every other frame.)
        void pass();
        /// Nothing written: the map is going.
        void clear();

    private:
        static constexpr int kSlots = 2;
        static constexpr int kLines = 2;
        struct Line
        {
            const char *text = nullptr;
            int count = 0;
            int x = 0, y = 0;
            int font = 0;
        };
        struct Slot
        {
            bool busy = false;
            hldata::MessageRecord message{};
            Line lines[kLines];
            int lineCount = 0;
            int letters = 0;  ///< in all its lines
            int elapsed = 0;  ///< ms
            int written = 0;  ///< letters on the sheet
            int settled = 0;  ///< ... and of those, in their final colour
            std::uint32_t order = 0;
        };
        Slot slots_[kSlots];
        std::uint32_t orders_ = 0;
        bool ready_ = false;
        hldata::Canvas canvas_;

        void ensure();
        void place(const Slot &slot, int index);
        void colours(int index, const Slot &slot, int strength);
        void letter(const Slot &slot, int index, int n, bool fresh);
        void drawLogo(Slot &slot, int index);
    };

    extern TopText topText;
} // namespace hlrt
