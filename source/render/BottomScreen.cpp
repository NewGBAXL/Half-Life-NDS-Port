#include "render/BottomScreen.hpp"

#include <cstdio>

#include <nds.h>

#include "Settings.hpp"
#include "game/Input.hpp"

namespace hlrt
{
    BottomScreen bottomScreen;

    namespace
    {
        PrintConsole pad;

        /// A box of text cells with a label in the middle.
        void drawButton(int column, int row, int columns, int rows, const char *label, int labelLen)
        {
            for (int y = 0; y < rows; ++y)
            {
                std::printf("\x1b[%d;%dH", row + y, column);
                for (int x = 0; x < columns; ++x)
                {
                    const bool edge = y == 0 || y == rows - 1 || x == 0 || x == columns - 1;
                    std::putchar(edge ? '#' : ' ');
                }
            }
            std::printf("\x1b[%d;%dH%s", row + rows / 2, column + (columns - labelLen) / 2, label);
        }

        /// Clear the rows this screen owns. Not the whole console: rows 0..7
        /// are where the debug readout lands when NEA has to put it here.
        void clearOwnRows()
        {
            for (int row = 8; row < 24; ++row)
                std::printf("\x1b[%d;0H%32s", row, "");
        }
    } // namespace

    void BottomScreen::create()
    {
        if (created_)
            return;
        created_ = true;

        // In two-pass mode NEA's debug console is already on this screen
        // (background 3, the same bank, map base 4), so the layers that are
        // switched on are kept, not wiped: videoSetModeSub() assigns the whole
        // register.
        const std::uint32_t layers = REG_DISPCNT_SUB & 0x1F00;
        videoSetModeSub(MODE_0_2D);
        REG_DISPCNT_SUB |= layers;
        vramSetBankH(VRAM_H_SUB_BG);

        // Not the default console: that one is NEA's, and every debug printf
        // in the port goes to it. This one is selected only while this screen
        // is drawn.
        //
        // The previous console is taken BEFORE consoleInit, not after:
        // consoleInit selects the console it sets up, so asking afterwards
        // returns the pad itself and "restoring" it leaves every later printf
        // on the touch screen.
        PrintConsole *previous = consoleSelect(&pad);
        consoleInit(&pad, 0, BgType_Text4bpp, BgSize_T_256x256, 7, 0, false, true);
        BG_PALETTE_SUB[0] = RGB15(2, 2, 3);
        consoleSelect(previous);

        showPad();
    }

    void BottomScreen::showPad()
    {
        PrintConsole *previous = consoleSelect(&pad);
        clearOwnRows();
        std::printf("\x1b[10;3HDRAG  look around");
        std::printf("\x1b[11;3HTAP TWICE  jump");
        std::printf("\x1b[12;3HD-pad / ABXY  move");
        std::printf("\x1b[13;3HL  fire / swing");
        std::printf("\x1b[14;3HSTART  menu");

        // Text cells are 8 pixels, so the buttons' pixel rectangles become
        // cell rectangles by dividing by 8.
        const int columns = Input::kButtonWidth / 8;
        const int top = Input::kButtonTop / 8;
        const int rows = 24 - top;
        drawButton(Input::kUseLeft / 8, top, columns, rows, "USE", 3);
        drawButton(32 - columns, top, columns, rows, "DUCK", 4);
        drawButton(Input::kWeaponLeft / 8, Input::kWeaponTop / 8, columns,
                   (Input::kWeaponBottom - Input::kWeaponTop) / 8, "WEAPON", 6);
        drawStatus();
        consoleSelect(previous);
        padShown_ = true;
    }

    void BottomScreen::drawStatus()
    {
        static const char *const kNames[] = {"", "CROWBAR", "PISTOL", "SHOTGUN", "GRENADES"};
        std::printf("\x1b[8;2HHEALTH %3d    ARMOUR %3d", health_ < 0 ? 0 : health_, armor_);
        if (weapon_ <= 0 || weapon_ > 4)
            std::printf("\x1b[9;2H%-28s", "");
        else if (weapon_ == 1)
            std::printf("\x1b[9;2H%-28s", kNames[1]);
        else if (weapon_ == 4)
            std::printf("\x1b[9;2H%-8s %3d                ", kNames[4], clip_);
        else
            std::printf("\x1b[9;2H%-8s %3d / %-3d          ", kNames[weapon_], clip_, spare_);
    }

    void BottomScreen::setStatus(int health, int armor, int weapon, int clip, int spare)
    {
        if (health == health_ && armor == armor_ && weapon == weapon_ && clip == clip_ &&
            spare == spare_)
            return;
        health_ = health;
        armor_ = armor;
        weapon_ = weapon;
        clip_ = clip;
        spare_ = spare;
        if (!padShown_)
            return;
        PrintConsole *previous = consoleSelect(&pad);
        drawStatus();
        consoleSelect(previous);
    }

    void BottomScreen::showMessage(const char *line1, const char *line2)
    {
        padShown_ = false;
        PrintConsole *previous = consoleSelect(&pad);
        clearOwnRows();
        std::printf("\x1b[12;2H%.28s", line1);
        std::printf("\x1b[14;2H%.28s", line2);
        consoleSelect(previous);
    }

    void BottomScreen::showCommand(const char *typed, const char *note)
    {
        // The top of the screen's own rows: the keyboard has the rest.
        padShown_ = false;
        PrintConsole *previous = consoleSelect(&pad);
        clearOwnRows();
        std::printf("\x1b[8;1H%.30s", note);
        std::printf("\x1b[10;1H> %.26s_", typed);
        consoleSelect(previous);
    }

    void BottomScreen::showMenu(hldata::RenderMode mode, bool dspAvailable, bool dspOn)
    {
        padShown_ = false;
        dspShown_ = dspAvailable;
        PrintConsole *previous = consoleSelect(&pad);
        clearOwnRows();
        const char *name = Settings::name(mode);
        int length = 0;
        while (name[length] != '\0')
            ++length;
        std::printf("\x1b[%d;2HRENDERING  (touch to change)", kModeRow - 1);
        drawButton(2, kModeRow, 28, kItemRows, name, length);
        // (Only where there is a DSP to switch.)
        if (dspAvailable)
            drawButton(2, kDspRow, 28, kItemRows, dspOn ? "DSi SOUND  on" : "DSi SOUND  off",
                       dspOn ? 13 : 14);
        drawButton(2, kSaveRow, 28, kItemRows, "SAVE", 4);
        drawButton(2, kNewRow, 28, kItemRows, "NEW GAME", 8);
        drawButton(2, kResumeRow, 28, kItemRows, "RESUME", 6);
        drawButton(2, kConsoleRow, 28, kItemRows, "COMMAND", 7);
        consoleSelect(previous);
    }

    BottomScreen::MenuItem BottomScreen::menuItemAt(int x, int y) const
    {
        if (x < 16 || x >= 240)
            return MenuItem::None;
        const int row = y / 8;
        if (row >= kModeRow && row < kModeRow + kItemRows)
            return MenuItem::RenderMode;
        if (dspShown_ && row >= kDspRow && row < kDspRow + kItemRows)
            return MenuItem::Dsp;
        if (row >= kSaveRow && row < kSaveRow + kItemRows)
            return MenuItem::Save;
        if (row >= kNewRow && row < kNewRow + kItemRows)
            return MenuItem::NewGame;
        if (row >= kResumeRow && row < kResumeRow + kItemRows)
            return MenuItem::Resume;
        if (row >= kConsoleRow && row < kConsoleRow + kItemRows)
            return MenuItem::Console;
        return MenuItem::None;
    }
} // namespace hlrt
