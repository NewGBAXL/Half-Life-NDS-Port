#pragma once

// The pause menu, on the touch screen: two pages of rows -- buttons, which
// act when the stylus is lifted from them; rows that show a setting and
// change it when pressed; and sliders, which follow the stylus -- and the
// keyboard the debug command line is typed on.
//
// What changes a setting is done here; what changes the game (resume, save,
// a new game, a command) is handed back to whoever runs the game.

#include <string>

#include "HlSaveFiles.hpp"

namespace hlrt
{
    class Menu
    {
    public:
        enum class Action
        {
            None,
            Resume,
            Save,     ///< the quick one
            SaveTo,   ///< ... into slot() -- one of the player's six
            Load,     ///< the save in slot()
            Continue, ///< the most recent save (the title's)
            NewGame,
            Command,
        };

        /// Take the touch screen and show the first page.
        void open();
        /// The same, as the game's title: CONTINUE, LOAD GAME, NEW GAME,
        /// OPTIONS, with no game behind it.
        void openTitle();
        /// The list of saves that the load and save pages show (the
        /// caller's: SaveGame::index()).
        void setSaves(const hldata::SaveIndex *saves) { saves_ = saves; }
        /// Which save an action is about (a kSlot*).
        int slot() const { return slot_; }
        /// Once a frame while it is open (after the keys have been scanned).
        Action frame();
        /// A line typed on the keyboard. False if it was given up (B).
        /// Stands the game still meanwhile. The sound is whatever it was
        /// (the menu's caller has it paused); the keys' own clicks are heard.
        bool typeLine(std::string &typed);

    private:
        enum class Page
        {
            Main,
            Options,
            Graphics,
            Title,
            Saves,   ///< the list: to load from, or to save into
            Confirm, ///< "overwrite?", "load?", "new game?"
        };
        const hldata::SaveIndex *saves_ = nullptr;
        bool title_ = false;   ///< it is the title's menu, not the pause's
        bool saving_ = false;  ///< the list is shown to save into
        int slot_ = -1;
        Action asked_ = Action::None; ///< what "yes" on the confirm page does
        Page asker_ = Page::Main;     ///< ... and where "no" goes back to
        void showSaves(bool saving);
        void ask(Action action, const char *question);
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
