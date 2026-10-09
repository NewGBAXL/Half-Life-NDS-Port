#pragma once

// The pause menu, on the touch screen: two pages of rows -- buttons, which
// act when the stylus is lifted from them; rows that show a setting and
// change it when pressed; and sliders, which follow the stylus -- and the
// keyboard the debug command line is typed on.
//
// What changes a setting is done here; what changes the game (resume, save,
// a new game, a command) is handed back to whoever runs the game.

#include <string>

namespace hlrt
{
    class Menu
    {
    public:
        enum class Action
        {
            None,
            Resume,
            Save,
            NewGame,
            Command,
        };

        /// Take the touch screen and show the first page.
        void open();
        /// Once a frame while it is open (after the keys have been scanned).
        Action frame();
        /// A line typed on the keyboard. False if it was given up (B).
        /// Stands the game still meanwhile; the sound goes on.
        bool typeLine(std::string &typed);

    private:
        enum class Page
        {
            Main,
            Options,
            Graphics,
        };
        Page page_ = Page::Main;
        int active_ = -1; ///< the row the stylus went down on, or -1
        bool touching_ = false;
        int lastX_ = 0, lastY_ = 0;
        void draw();
        void drawRow(int row, bool down);
        int rowAt(int x, int y) const;
        int rowCount() const;
        Action fire(int row);
        bool slide(int row, int x);
    };

    extern Menu menu;
} // namespace hlrt
