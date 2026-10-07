#pragma once

// The touch screen: the aim pad with its buttons, and the pause menu.
//
// Deliberately plain for now -- a text layer on the sub engine with the
// buttons drawn in characters -- because what matters at this stage is that
// the regions the player touches are the regions the code tests. The pad's
// come from Input's constants and the menu's from the constants below, each
// used both to draw and to hit-test, so they cannot disagree.
//
// VRAM: bank H (32 KB, sub background) and nothing else. The four big banks
// stay free for 3D textures and capture, which is the reason for choosing the
// smallest bank that can hold a text layer. What is in it:
//
//   tile base 0   0 ..  4 KB   the console font (shared by both layers below)
//   map base 4    8 .. 10 KB   NEA's debug console, background 3 -- two-pass only
//   map base 5   10 .. 14 KB   the debug command line's keyboard, background 1
//   map base 7   14 .. 16 KB   this screen, background 0
//   tile base 1  16 .. 32 KB   the keyboard's own tiles

#include "HlSettings.hpp"

namespace hlrt
{
    class BottomScreen
    {
    public:
        enum class MenuItem
        {
            None,
            RenderMode,
            Save,
            NewGame,
            Resume,
            Console,
            Dsp,
        };

        /// Set up the sub engine and draw the pad. Idempotent, until reset().
        void create();
        /// Forget that the sub engine was set up: the video mode was changed
        /// under it. The next create() starts over.
        void reset() { created_ = false; }

        void showPad();
        /// What is shown above the pad: health and armour, and what is in
        /// hand (`weapon` is a kWeapon*; `clip` and `spare` its rounds).
        /// Redrawn only when something changes.
        void setStatus(int health, int armor, int weapon, int clip, int spare);
        /// Two lines in the middle of the screen, in place of the pad: for
        /// the moments the game is busy.
        void showMessage(const char *line1, const char *line2);
        /// `dspAvailable`: there is a DSP to switch (a DSi); `dspOn`: it is on.
        void showMenu(hldata::RenderMode mode, bool dspAvailable, bool dspOn);
        /// What is being typed at the debug command line, under a line of
        /// `note`. (The keyboard itself is libnds's, on a layer of its own.)
        void showCommand(const char *typed, const char *note);
        /// Which menu item a touch at (x, y) lands on.
        MenuItem menuItemAt(int x, int y) const;

    private:
        // Menu rows, in text cells (8 pixels each).
        // (Two rows each: there are six of them now.)
        static constexpr int kModeRow = 9;
        static constexpr int kDspRow = 11;
        static constexpr int kSaveRow = 13;
        static constexpr int kNewRow = 15;
        static constexpr int kResumeRow = 17;
        static constexpr int kConsoleRow = 19;
        static constexpr int kItemRows = 2;

        bool created_ = false;
        void drawStatus();
        int health_ = 100;
        int armor_ = 0;
        int weapon_ = 0;
        int clip_ = 0;
        int spare_ = 0;
        bool padShown_ = false;
        bool dspShown_ = false;
    };

    extern BottomScreen bottomScreen;
} // namespace hlrt
