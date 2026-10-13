#include "global_header.hpp"

#include "ui/Menu.hpp"

#include "Settings.hpp"
#include "audio/DspAudio.hpp"
#include "audio/Sound.hpp"
#include "net/NetGame.hpp"
#include "render/BottomScreen.hpp"
#include "ui/TouchCanvas.hpp"

namespace hlrt
{
    Menu menu;

    namespace
    {
        using namespace hldata;

        enum class Kind
        {
            Button,
            Value,  ///< shows a setting; a press changes it
            Slider,
        };
        enum Id
        {
            kResume,
            kSave,
            kSaveGame,
            kLoadGame,
            kContinue,
            kYes,
            kNo,
            kSlot, ///< a line of the list (which, by its row)
            kNewGame,
            kOptions,
            kCommand,
            kRender,
            kDsp,
            kMusic,
            kEffects,
            kVoices,
            kAmbience,
            kReadout,
            kHand,
            kControls,
            kScheme,
            kPadSide,
            kPadUp,
            kMultiplayer,
            kNetHost,
            kNetJoin,
            kNetMap,
            kNetFrags,
            kNetMinutes,
            kNetStart,
            kNetFound, ///< a game found (which, by its row)
            kLeave,
            kBob,
            kBack,
            kGraphics,
            kFog,
            kFogStrength,
            kFogDistance,
            kFogColour,
            kModelDetail,
            kWorldDetail,
            kModelLimit,
            kDrawDistance,
        };
        struct Row
        {
            Kind kind;
            Id id;
            const char *label;
        };
        const Row kMain[] = {{Kind::Button, kResume, "RESUME"},       {Kind::Button, kSave, "QUICK SAVE"},
                             {Kind::Button, kSaveGame, "SAVE GAME"}, {Kind::Button, kLoadGame, "LOAD GAME"},
                             {Kind::Button, kNewGame, "NEW GAME"},   {Kind::Button, kOptions, "OPTIONS"},
                             {Kind::Button, kCommand, "COMMAND"}};
        const Row kTitleRows[] = {{Kind::Button, kContinue, "CONTINUE"},     {Kind::Button, kLoadGame, "LOAD GAME"},
                                  {Kind::Button, kNewGame, "NEW GAME"},       {Kind::Button, kMultiplayer, "MULTIPLAYER"},
                                  {Kind::Button, kOptions, "OPTIONS"}};
        // A game between consoles (source/net/NetGame.hpp).
        const Row kMultiRows[] = {{Kind::Button, kNetHost, "HOST GAME"}, {Kind::Button, kNetJoin, "JOIN GAME"},
                                  {Kind::Button, kBack, "BACK"}};
        const Row kHostRows[] = {{Kind::Value, kNetMap, "Map"},
                                 {Kind::Value, kNetFrags, "Frag limit"},
                                 {Kind::Value, kNetMinutes, "Time limit"},
                                 {Kind::Button, kNetStart, "START"},
                                 {Kind::Button, kBack, "BACK"}};
        const Row kLobbyRows[] = {{Kind::Button, kBack, "LEAVE"}};
        const Row kNetPauseRows[] = {{Kind::Button, kResume, "RESUME"}, {Kind::Button, kOptions, "OPTIONS"},
                                     {Kind::Button, kLeave, "LEAVE GAME"}};
        // The games found: made when they are shown, then BACK.
        constexpr int kFoundListed = 4;
        Row foundRows[kFoundListed + 1];
        NetGame::Found foundList[kFoundListed];
        int foundCount = 0;
        const Row kConfirmRows[] = {{Kind::Button, kYes, "YES"}, {Kind::Button, kNo, "NO"}};
        // The list of saves: its lines are made when it is shown (they say
        // what is in each), then BACK.
        Row listRows[kSlotsListed + 1];
        char listText[kSlotsListed][72];
        bool listDead[kSlotsListed];
        char question[40];
        const Row kOptionRows[] = {{Kind::Button, kGraphics, "GRAPHICS"}, {Kind::Value, kDsp, "DSi sound"},
                                   {Kind::Slider, kMusic, "Music"},     {Kind::Slider, kEffects, "Effects"},
                                   {Kind::Slider, kVoices, "Voices"},   {Kind::Slider, kAmbience, "Ambience"},
                                   {Kind::Button, kControls, "CONTROLS"}, {Kind::Value, kBob, "View bob"},
                                   {Kind::Value, kReadout, "Debug readout"},
                                   {Kind::Button, kBack, "BACK"}};

        // How the picture is drawn: the rendering mode, the fog, and how
        // much detail is given up for speed (shared/HlFog.hpp).
        const Row kGraphicsRows[] = {{Kind::Value, kRender, ""},
                                     {Kind::Value, kFog, "Fog"},
                                     {Kind::Slider, kFogStrength, "Fog strength"},
                                     {Kind::Slider, kFogDistance, "Fog distance"},
                                     {Kind::Value, kFogColour, "Fog colour"},
                                     {Kind::Value, kModelDetail, "Model detail"},
                                     {Kind::Value, kWorldDetail, "World detail"},
                                     {Kind::Value, kModelLimit, "Models at once"},
                                     {Kind::Value, kDrawDistance, "Draw distance"},
                                     {Kind::Button, kBack, "BACK"}};

        // The buttons: which layout, which hand, and which of the pad and
        // the stylus turns and which moves (shared/HlControls.hpp). Under
        // the rows, what each button then does.
        const Row kControlRows[] = {{Kind::Value, kScheme, "Layout"},
                                    {Kind::Value, kHand, "Stylus hand"},
                                    {Kind::Value, kPadSide, "Pad left/right"},
                                    {Kind::Value, kPadUp, "Pad up/down"},
                                    {Kind::Button, kBack, "BACK"}};

        // Where the rows are: the first page's are big and few, the
        // others' as many as the screen holds.
        struct Layout
        {
            int x, top, width, height, pitch;
        };
        constexpr Layout kMainLayout = {28, 34, 200, 19, 22};
        constexpr Layout kOptionLayout = {8, 2, 240, 17, 19};
        constexpr int kTrackLeft = 100, kTrackWidth = 132;

        /// What a slider slides (0..10, all of them).
        std::uint8_t &slid(Id id)
        {
            switch (id)
            {
            case kFogStrength: return settings.graphics.fogStrength;
            case kFogDistance: return settings.graphics.fogDistance;
            case kMusic:       return settings.volume[kVolumeMusic];
            case kEffects:     return settings.volume[kVolumeEffects];
            case kVoices:      return settings.volume[kVolumeVoices];
            default:           return settings.volume[kVolumeAmbience];
            }
        }
        const Row *rowsOf(int page, int &count)
        {
            if (page == 0)
            {
                count = static_cast<int>(sizeof(kMain) / sizeof(kMain[0]));
                return kMain;
            }
            if (page == 3)
            {
                count = static_cast<int>(sizeof(kTitleRows) / sizeof(kTitleRows[0]));
                return kTitleRows;
            }
            if (page == 4)
            {
                count = kSlotsListed + 1;
                return listRows;
            }
            if (page == 5)
            {
                count = 2;
                return kConfirmRows;
            }
            if (page == 6)
            {
                count = static_cast<int>(sizeof(kControlRows) / sizeof(kControlRows[0]));
                return kControlRows;
            }
            if (page == 7)
            {
                count = static_cast<int>(sizeof(kMultiRows) / sizeof(kMultiRows[0]));
                return kMultiRows;
            }
            if (page == 8)
            {
                count = static_cast<int>(sizeof(kHostRows) / sizeof(kHostRows[0]));
                return kHostRows;
            }
            if (page == 9)
            {
                count = foundCount + 1;
                return foundRows;
            }
            if (page == 10)
            {
                count = static_cast<int>(sizeof(kLobbyRows) / sizeof(kLobbyRows[0]));
                return kLobbyRows;
            }
            if (page == 11)
            {
                count = static_cast<int>(sizeof(kNetPauseRows) / sizeof(kNetPauseRows[0]));
                return kNetPauseRows;
            }
            if (page == 1)
            {
                count = static_cast<int>(sizeof(kOptionRows) / sizeof(kOptionRows[0]));
                return kOptionRows;
            }
            count = static_cast<int>(sizeof(kGraphicsRows) / sizeof(kGraphicsRows[0]));
            return kGraphicsRows;
        }
    } // namespace

    int Menu::rowCount() const
    {
        int count = 0;
        rowsOf(static_cast<int>(page_), count);
        return count;
    }

    void Menu::openTitle()
    {
        open();
        title_ = true;
        page_ = Page::Title;
        draw();
    }

    /// The list, to load from or to save into: a line for each save, saying
    /// what it is and what is in it.
    void Menu::showSaves(bool saving)
    {
        static const char *const kNames[kSlotsListed] = {"AUTO", "QUICK", "1", "2", "3", "4", "5", "6"};
        saving_ = saving;
        const FontView &font = touchCanvas.font(kFontSmall);
        for (int i = 0; i < kSlotsListed; ++i)
        {
            const SaveSlot none;
            const SaveSlot &slot = saves_ != nullptr ? saves_->slots[i] : none;
            // What it is, the chapter (as much of its title as there is
            // room for), and when: the day and the time if the console's
            // clock said, else how long had been played.
            std::string line = std::string(kNames[i]) + "  " + saveLine(slot);
            if (slot.status == kSaveGood)
            {
                const SaveHeading &h = slot.heading;
                char when[24];
                if (h.year != 0)
                    std::snprintf(when, sizeof(when), "  %02u/%02u %02u:%02u", h.day, h.month, h.hour, h.minute);
                else
                    std::snprintf(when, sizeof(when), "  %u:%02u", static_cast<unsigned>(h.playSeconds / 3600),
                                  static_cast<unsigned>(h.playSeconds / 60 % 60));
                std::string what = h.chapter[0] != 0 ? h.chapter : h.map;
                const std::string head = std::string(kNames[i]) + "  ";
                while (what.size() > 3 && textWidth(font, (head + what + when).c_str()) > kOptionLayout.width - 8)
                    what.pop_back();
                line = head + what + when;
            }
            while (line.size() > 4 && textWidth(font, line.c_str()) > kOptionLayout.width - 8)
                line.pop_back();
            std::snprintf(listText[i], sizeof(listText[i]), "%s", line.c_str());
            // To save into: one of the player's six. To load: one that is
            // there and whole.
            listDead[i] = saving ? i < kSlotManual : slot.status != kSaveGood;
            listRows[i] = Row{Kind::Button, kSlot, listText[i]};
        }
        listRows[kSlotsListed] = Row{Kind::Button, kBack, "BACK"};
        asker_ = page_ == Page::Saves ? asker_ : page_;
        page_ = Page::Saves;
        draw();
    }

    void Menu::ask(Action action, const char *text)
    {
        asked_ = action;
        std::snprintf(question, sizeof(question), "%s", text);
        page_ = Page::Confirm;
        draw();
    }

    int netlinkFailStage();

    bool Menu::big() const
    {
        return page_ == Page::Main || page_ == Page::Title || page_ == Page::Confirm || page_ == Page::Multiplayer ||
               page_ == Page::NetPause;
    }

    /// What of the lobby is on the screen: who is there, what they are
    /// called, how they stand -- to draw it again when it is not so.
    unsigned Menu::netSignature() const
    {
        const NetMatch &match = netGame.match();
        unsigned sum = static_cast<unsigned>(netGame.phase()) * 7919u;
        for (int p = 0; p < kNetPlayers; ++p)
        {
            sum = sum * 31u + (netHere(match, p) ? 1u : 0u);
            for (const char *c = match.names[p]; *c != 0; ++c)
                sum = sum * 31u + static_cast<unsigned char>(*c);
            sum = sum * 31u + static_cast<unsigned>(match.frags[p] + 1000) + static_cast<unsigned>(match.deaths[p]) * 131u;
        }
        return sum;
    }

    /// Who is in the game, from `top` down: their names, and with `scores`
    /// their frags and deaths.
    void Menu::drawPlayers(int top, bool scores)
    {
        Canvas &canvas = touchCanvas.canvas();
        const FontView &small = touchCanvas.font(kFontSmall);
        const NetMatch &match = netGame.match();
        canvas.fill(0, top, 256, 4 * (small.height() + 3) + 2, kUiBack);
        int line = 0;
        for (int p = 0; p < kNetPlayers; ++p)
        {
            if (!netHere(match, p))
                continue;
            char text[48];
            const char *name = match.names[p][0] != 0 ? match.names[p] : "...";
            if (scores)
                std::snprintf(text, sizeof(text), "%s%s   %d frags   %d deaths", name, p == match.me ? " (you)" : "",
                              match.frags[p], match.deaths[p]);
            else
                std::snprintf(text, sizeof(text), "%s%s%s", name, p == 0 ? " (host)" : "", p == match.me ? " (you)" : "");
            drawTextCentred(canvas, small, 128, top + line++ * (small.height() + 3), text,
                            inkFor(small, p == match.me ? kUiWhite : kUiGrey));
        }
        netShown_ = netSignature();
    }

    /// The games found, as rows; then BACK.
    void Menu::showFound()
    {
        foundCount = netGame.found(foundList, kFoundListed);
        for (int i = 0; i < foundCount; ++i)
            foundRows[i] = Row{Kind::Button, kNetFound, foundList[i].line};
        foundRows[foundCount] = Row{Kind::Button, kBack, "BACK"};
        page_ = Page::NetJoin;
        draw();
    }

    void Menu::open()
    {
        bottomScreen.release();
        title_ = false;
        page_ = netGame.playing() ? Page::NetPause : Page::Main;
        active_ = -1;
        // (The touch that opened it, if there was one, is not a press.)
        touching_ = (keysHeld() & KEY_TOUCH) != 0;
        draw();
    }

    void Menu::draw()
    {
        Canvas &canvas = touchCanvas.canvas();
        canvas.clear();
        if (big())
        {
            const FontView &title = touchCanvas.font(page_ == Page::Confirm ? kFontMedium : kFontTitle);
            drawTextCentred(canvas, title, 128, page_ == Page::Confirm ? 14 : 8,
                            page_ == Page::Main || page_ == Page::NetPause
                                ? "PAUSED"
                                : (page_ == Page::Title ? "HALF-LIFE"
                                                        : (page_ == Page::Multiplayer ? "MULTIPLAYER" : question)),
                            inkFor(title, kUiOrange));
        }
        for (int row = 0; row < rowCount(); ++row)
            drawRow(row, false);
        if (page_ == Page::NetHost || page_ == Page::NetLobby || page_ == Page::NetJoin)
        {
            const FontView &small = touchCanvas.font(kFontSmall);
            const int top = kOptionLayout.top + rowCount() * kOptionLayout.pitch + 4;
            const char *said = page_ == Page::NetHost
                                   ? "In the game:"
                                   : (page_ == Page::NetJoin
                                          ? (netGame.phase() == NetGame::Phase::Joining ? "Joining..."
                                                                                        : "Looking for games...")
                                          : "Waiting for the host to start");
            drawTextCentred(canvas, small, 128, top, said, inkFor(small, kUiOrange));
            if (page_ != Page::NetJoin)
                drawPlayers(top + small.height() + 5, false);
        }
        if (page_ == Page::NetPause)
            drawPlayers(kMainLayout.top + rowCount() * kMainLayout.pitch + 6, true);
        if (page_ == Page::Controls)
        {
            // What the buttons do, as they are now set.
            const FontView &small = touchCanvas.font(kFontSmall);
            const int top = kOptionLayout.top + rowCount() * kOptionLayout.pitch + 6;
            for (int line = 0; controlsLegend(settings.controls(), line) != nullptr; ++line)
                drawTextCentred(canvas, small, 128, top + line * (small.height() + 3),
                                controlsLegend(settings.controls(), line), inkFor(small, kUiGrey));
        }
    }

    void Menu::drawRow(int row, bool down)
    {
        Canvas &canvas = touchCanvas.canvas();
        const bool main = big();
        int count = 0;
        const Row &item = rowsOf(static_cast<int>(page_), count)[row];
        const Layout &at = main ? kMainLayout : kOptionLayout;
        const int y = at.top + row * at.pitch;
        const FontView &font = touchCanvas.font(main ? kFontMedium : kFontSmall);
        const int textY = y + (at.height - font.height()) / 2;
        // (A DS has no DSP to switch: the row is there, and says so. A save
        // that cannot be loaded, or is not the player's to write, likewise;
        // and CONTINUE with nothing to continue.)
        const bool dead = (item.id == kDsp && !DspAudio::available()) || (item.id == kSlot && listDead[row]) ||
                          (item.id == kContinue && (saves_ == nullptr || saveMostRecent(*saves_) < 0)) ||
                          (item.id == kCommand && title_) || (item.id == kMultiplayer && netGame.maps().empty());
        if (item.kind == Kind::Slider)
        {
            canvas.fill(at.x, y, at.width, at.height, kUiBack);
            drawText(canvas, font, at.x + 4, textY, item.label, inkFor(font, kUiWhite));
            const int level = slid(item.id);
            const int knob = kTrackLeft + (kTrackWidth * level) / kVolumeMost;
            canvas.fill(kTrackLeft, y + at.height / 2 - 3, kTrackWidth, 6, kUiTrack);
            canvas.frame(kTrackLeft - 1, y + at.height / 2 - 4, kTrackWidth + 2, 8, kUiEdge);
            canvas.fill(kTrackLeft, y + at.height / 2 - 3, knob - kTrackLeft, 6, kUiOrange + (down ? 2 : 1));
            canvas.fill(knob - 3, y + 2, 7, at.height - 4, down ? kUiWhite + 2 : kUiWhite + 1);
            canvas.frame(knob - 3, y + 2, 7, at.height - 4, kUiShadow);
            return;
        }
        char text[48];
        switch (item.id)
        {
        case kRender:
            std::snprintf(text, sizeof(text), "%s", Settings::name(settings.renderMode));
            break;
        case kDsp:
            std::snprintf(text, sizeof(text), "%s: %s", item.label,
                          dead ? "not on a DS" : (sound.extended() ? "on" : "off"));
            break;
        case kReadout:
            std::snprintf(text, sizeof(text), "%s: %s", item.label, settings.debugReadout ? "on" : "off");
            break;
        case kBob:
            std::snprintf(text, sizeof(text), "%s: %s", item.label, settings.viewBob ? "on" : "off");
            break;
        case kFog:
            std::snprintf(text, sizeof(text), "%s: %s", item.label, kFogTypeName[settings.graphics.fog]);
            break;
        case kFogColour:
            std::snprintf(text, sizeof(text), "%s: %s", item.label, kFogColourName[settings.graphics.fogColour]);
            break;
        case kModelDetail:
            std::snprintf(text, sizeof(text), "%s: %s", item.label, kDetailName[settings.graphics.modelDetail]);
            break;
        case kWorldDetail:
            std::snprintf(text, sizeof(text), "%s: %s", item.label, kDetailName[settings.graphics.worldDetail]);
            break;
        case kModelLimit:
            if (settings.graphics.modelLimit == 0)
                std::snprintf(text, sizeof(text), "%s: auto", item.label);
            else
                std::snprintf(text, sizeof(text), "%s: %d", item.label, settings.graphics.modelLimit);
            break;
        case kDrawDistance:
            std::snprintf(text, sizeof(text), "%s: %s", item.label, kDrawDistanceName[settings.graphics.drawDistance]);
            break;
        case kHand:
            // (Which side of the touch screen its column of buttons is,
            // and which hand has the pad and the trigger.)
            std::snprintf(text, sizeof(text), "%s: %s", item.label, settings.leftHanded ? "left" : "right");
            break;
        case kNetMap:
            std::snprintf(text, sizeof(text), "%s: %s", item.label,
                          netGame.maps().empty() ? "none" : netGame.maps()[netMap_ % netGame.maps().size()].c_str());
            break;
        case kNetFrags:
            if (netFrags_ == 0)
                std::snprintf(text, sizeof(text), "%s: none", item.label);
            else
                std::snprintf(text, sizeof(text), "%s: %d", item.label, netFrags_);
            break;
        case kNetMinutes:
            if (netMinutes_ == 0)
                std::snprintf(text, sizeof(text), "%s: none", item.label);
            else
                std::snprintf(text, sizeof(text), "%s: %d minutes", item.label, netMinutes_);
            break;
        case kScheme:
            std::snprintf(text, sizeof(text), "%s: %s", item.label,
                          settings.controlScheme == kControlsModern ? "modern" : "classic");
            break;
        case kPadSide:
            std::snprintf(text, sizeof(text), "%s: %s", item.label, settings.padTurns ? "turn" : "sidestep");
            break;
        case kPadUp:
            std::snprintf(text, sizeof(text), "%s: %s", item.label, settings.padLooks ? "look" : "move");
            break;
        default:
            std::snprintf(text, sizeof(text), "%s", item.label);
            break;
        }
        canvas.button(at.x, y, at.width, at.height, down && !dead);
        drawTextCentred(canvas, font, at.x + at.width / 2, textY, text,
                        inkFor(font, dead ? kUiGrey : (down ? kUiWhite : kUiOrange)));
    }

    int Menu::rowAt(int x, int y) const
    {
        const Layout &at = big() ? kMainLayout : kOptionLayout;
        if (x < at.x || x >= at.x + at.width || y < at.top)
            return -1;
        const int row = (y - at.top) / at.pitch;
        return row < rowCount() && (y - at.top) % at.pitch < at.height ? row : -1;
    }

    /// The slider of `row` put where the stylus is. True if that moved it.
    bool Menu::slide(int row, int x)
    {
        int count = 0;
        const Row &item = rowsOf(static_cast<int>(page_), count)[row];
        int level = ((x - kTrackLeft) * kVolumeMost + kTrackWidth / 2) / kTrackWidth;
        level = x < kTrackLeft ? 0 : (level > kVolumeMost ? kVolumeMost : level);
        std::uint8_t &value = slid(item.id);
        if (value == level)
            return false;
        value = static_cast<std::uint8_t>(level);
        sound.applyVolumes();
        sound.ui(kUiSoundTick);
        return true;
    }

    Menu::Action Menu::fire(int row)
    {
        int count = 0;
        const Row &item = rowsOf(static_cast<int>(page_), count)[row];
        sound.ui(kUiSoundPress);
        switch (item.id)
        {
        case kResume:  return Action::Resume;
        case kSave:    return Action::Save;
        case kCommand: return Action::Command;
        case kNewGame:
            // (From the title there is nothing to lose. No save is touched
            // either way: a new game only begins.)
            if (title_)
                return Action::NewGame;
            asker_ = page_;
            ask(Action::NewGame, "NEW GAME?");
            break;
        case kContinue:
            if (saves_ != nullptr && saveMostRecent(*saves_) >= 0)
            {
                slot_ = saveMostRecent(*saves_);
                return Action::Continue;
            }
            break;
        case kSaveGame:
            showSaves(true);
            break;
        case kLoadGame:
            showSaves(false);
            break;
        case kSlot:
            if (listDead[row])
            {
                drawRow(row, false);
                break;
            }
            slot_ = row;
            if (saving_)
            {
                // One with a save in it is asked about first.
                if (saves_ != nullptr && saves_->slots[row].status != kSaveNone)
                    ask(Action::SaveTo, "OVERWRITE?");
                else
                    return Action::SaveTo;
            }
            else if (title_)
            {
                return Action::Load;
            }
            else
            {
                ask(Action::Load, "LOAD? UNSAVED IS LOST");
            }
            break;
        case kYes:
        {
            const Action action = asked_;
            asked_ = Action::None;
            return action;
        }
        case kNo:
            page_ = asker_ == Page::Saves || asked_ == Action::SaveTo || asked_ == Action::Load ? Page::Saves : asker_;
            asked_ = Action::None;
            draw();
            break;
        case kOptions:
            page_ = Page::Options;
            draw();
            break;
        case kBack:
            // Out of a lobby: out of the game, and the radio silent.
            if (page_ == Page::NetHost || page_ == Page::NetJoin || page_ == Page::NetLobby)
            {
                netGame.leave();
                page_ = Page::Multiplayer;
                draw();
                break;
            }
            // (From the graphics, back to the options they are among; from
            // the list, to where it was asked for.)
            page_ = page_ == Page::Graphics || page_ == Page::Controls
                        ? Page::Options
                        : (page_ == Page::Saves
                               ? asker_
                               : (title_ ? Page::Title : (netGame.playing() ? Page::NetPause : Page::Main)));
            settings.save();
            draw();
            break;
        case kMultiplayer:
            if (!netGame.maps().empty())
            {
                page_ = Page::Multiplayer;
                draw();
            }
            break;
        case kNetHost:
            if (netGame.host())
            {
                page_ = Page::NetHost;
                draw();
            }
            else
            {
                std::snprintf(question, sizeof(question), "The wireless would not start (%d)", netlinkFailStage());
                page_ = Page::Multiplayer;
                draw();
                const FontView &small = touchCanvas.font(kFontSmall);
                drawTextCentred(touchCanvas.canvas(), small, 128, 170, question, inkFor(small, kUiGrey));
            }
            break;
        case kNetJoin:
            if (netGame.scan())
                showFound();
            break;
        case kNetFound:
            if (row < foundCount && netGame.join(foundList[row].index))
                draw();
            break;
        case kNetMap:
            netMap_ = netGame.maps().empty() ? 0 : (netMap_ + 1) % static_cast<int>(netGame.maps().size());
            drawRow(row, false);
            break;
        case kNetFrags:
        {
            static const int kLimits[] = {0, 5, 10, 20, 30, 50};
            int at = 0;
            for (int i = 0; i < 6; ++i)
                at = kLimits[i] == netFrags_ ? i : at;
            netFrags_ = kLimits[(at + 1) % 6];
            drawRow(row, false);
            break;
        }
        case kNetMinutes:
        {
            static const int kLimits[] = {0, 5, 10, 15, 20, 30};
            int at = 0;
            for (int i = 0; i < 6; ++i)
                at = kLimits[i] == netMinutes_ ? i : at;
            netMinutes_ = kLimits[(at + 1) % 6];
            drawRow(row, false);
            break;
        }
        case kNetStart:
            // (Whoever runs the screens loads the map when the match says
            // it has been started: main.cpp.)
            // (Alone, too: a host may look round a map before anyone comes --
            // though nobody can then, a match under way lets no one in.)
            if (!netGame.maps().empty())
                netGame.start(netGame.maps()[netMap_ % netGame.maps().size()], netFrags_, netMinutes_);
            break;
        case kLeave:
            return Action::Leave;
        case kGraphics:
            page_ = Page::Graphics;
            draw();
            break;
        case kFog:
            settings.graphics.fog = static_cast<std::uint8_t>((settings.graphics.fog + 1) % kFogTypes);
            drawRow(row, false);
            break;
        case kFogColour:
            settings.graphics.fogColour = static_cast<std::uint8_t>((settings.graphics.fogColour + 1) % kFogColours);
            drawRow(row, false);
            break;
        case kModelDetail:
            settings.graphics.modelDetail = static_cast<std::uint8_t>((settings.graphics.modelDetail + 1) % kDetailLevels);
            drawRow(row, false);
            break;
        case kWorldDetail:
            settings.graphics.worldDetail = static_cast<std::uint8_t>((settings.graphics.worldDetail + 1) % kDetailLevels);
            drawRow(row, false);
            break;
        case kModelLimit:
            // auto, 4, 6, 8, 10, 12, and round again.
            settings.graphics.modelLimit = settings.graphics.modelLimit == 0
                                               ? 4
                                               : (settings.graphics.modelLimit >= 12 ? 0 : settings.graphics.modelLimit + 2);
            drawRow(row, false);
            break;
        case kDrawDistance:
            settings.graphics.drawDistance = static_cast<std::uint8_t>((settings.graphics.drawDistance + 1) % kDrawDistances);
            drawRow(row, false);
            break;
        case kRender:
            // (It takes effect on the way out of the menu.)
            settings.renderMode = Settings::next(settings.renderMode);
            drawRow(row, false);
            break;
        case kDsp:
            if (DspAudio::available())
            {
                // At once: the music goes on from where it was, by the
                // other way of playing it.
                settings.dspAudio = sound.setDsp(!sound.extended());
                settings.save();
            }
            drawRow(row, false);
            break;
        case kReadout:
            settings.debugReadout = !settings.debugReadout;
            drawRow(row, false);
            break;
        case kBob:
            settings.viewBob = !settings.viewBob;
            drawRow(row, false);
            break;
        case kControls:
            page_ = Page::Controls;
            draw();
            break;
        // (The whole page again: the lines under the rows say what the
        // buttons now do.)
        case kHand:
            settings.leftHanded = !settings.leftHanded;
            draw();
            break;
        case kScheme:
            settings.controlScheme = static_cast<std::uint8_t>((settings.controlScheme + 1) % kControlsSchemes);
            draw();
            break;
        case kPadSide:
            settings.padTurns = !settings.padTurns;
            draw();
            break;
        case kPadUp:
            settings.padLooks = !settings.padLooks;
            draw();
            break;
        default:
            break;
        }
        return Action::None;
    }

    Menu::Action Menu::frame()
    {
        // A game between consoles: the lobby changes without being touched.
        if (page_ == Page::NetJoin)
        {
            // Let in: the host's lobby. Not let in, or the host gone: out.
            if (netGame.phase() == NetGame::Phase::Lobby)
            {
                page_ = Page::NetLobby;
                draw();
            }
            else if (netGame.phase() == NetGame::Phase::Off)
            {
                page_ = Page::Multiplayer;
                draw();
            }
            else if (netGame.phase() == NetGame::Phase::Scanning && ++netTick_ >= 45 && active_ < 0)
            {
                netTick_ = 0;
                showFound();
            }
        }
        else if (page_ == Page::NetHost || page_ == Page::NetLobby || page_ == Page::NetPause)
        {
            if (page_ != Page::NetPause && netGame.phase() == NetGame::Phase::Off)
            {
                page_ = Page::Multiplayer;
                draw();
            }
            else if (netSignature() != netShown_ && active_ < 0)
                draw();
        }
        const bool down = (keysHeld() & KEY_TOUCH) != 0;
        if (down)
        {
            touchPosition touch;
            touchRead(&touch);
            lastX_ = touch.px;
            lastY_ = touch.py;
        }
        int rowTotal = 0;
        const Row *rows = rowsOf(static_cast<int>(page_), rowTotal);
        const bool sliding = active_ >= 0 && active_ < rowTotal && rows[active_].kind == Kind::Slider;
        Action action = Action::None;
        if (down && !touching_)
        {
            active_ = rowAt(lastX_, lastY_);
            if (active_ >= 0)
            {
                if (rows[active_].kind == Kind::Slider)
                    slide(active_, lastX_);
                drawRow(active_, true);
            }
        }
        else if (down && sliding)
        {
            if (slide(active_, lastX_))
                drawRow(active_, true);
        }
        else if (!down && touching_ && active_ >= 0)
        {
            // Lifted: a button acts if it was lifted from where it was
            // pressed.
            const int row = active_;
            active_ = -1;
            if (sliding)
                drawRow(row, false);
            else if (rowAt(lastX_, lastY_) == row)
                action = fire(row);
            else
                drawRow(row, false);
        }
        touching_ = down;
        return action;
    }

    bool Menu::typeLine(std::string &typed)
    {
        static const char *const kKeys[4] = {"1234567890", "qwertyuiop", "asdfghjkl_", "zxcvbnm  <"};
        constexpr int kKey = 24, kLeft = 8, kTop = 58, kBottom = kTop + 4 * kKey + 4;
        bottomScreen.release();
        Canvas &canvas = touchCanvas.canvas();
        const FontView &font = touchCanvas.font(kFontMedium), &small = touchCanvas.font(kFontSmall);
        typed.clear();
        auto prompt = [&]()
        {
            canvas.fill(0, 30, 256, 22, kUiBack);
            canvas.frame(6, 30, 244, 22, kUiEdge);
            char line[40];
            std::snprintf(line, sizeof(line), "> %s_", typed.c_str());
            drawText(canvas, font, 12, 30 + (22 - font.height()) / 2, line, inkFor(font, kUiWhite));
        };
        auto key = [&](int row, int col, bool down)
        {
            const char text[2] = {kKeys[row][col], '\0'};
            if (text[0] == ' ')
                return;
            canvas.button(kLeft + col * kKey, kTop + row * kKey, kKey - 1, kKey - 1, down);
            drawTextCentred(canvas, font, kLeft + col * kKey + kKey / 2, kTop + row * kKey + (kKey - font.height()) / 2,
                            text, inkFor(font, down ? kUiWhite : kUiOrange));
        };
        auto wide = [&](int n, bool down)
        {
            static const char *const kNames[3] = {"SPACE", "ENTER", "CANCEL"};
            canvas.button(kLeft + n * 80, kBottom, 79, 26, down);
            drawTextCentred(canvas, font, kLeft + n * 80 + 40, kBottom + (26 - font.height()) / 2, kNames[n],
                            inkFor(font, down ? kUiWhite : kUiOrange));
        };
        canvas.clear();
        drawText(canvas, small, 8, 2, "COMMAND", inkFor(small, kUiOrange));
        drawText(canvas, small, 8, 15, "map <name>    give    heal    noclip", inkFor(small, kUiGrey));
        prompt();
        for (int row = 0; row < 4; ++row)
        {
            for (int col = 0; col < 10; ++col)
                key(row, col, false);
        }
        for (int n = 0; n < 3; ++n)
            wide(n, false);

        // The touch that asked for this is still down, and may be on a key
        // now: not until it has lifted is anything typed.
        bool touching = true;
        int heldRow = -1, heldCol = -1;
        while (true)
        {
            swiWaitForVBlank();
            sound.frame();
            scanKeys();
            if (keysDown() & KEY_B)
                return false;
            if (keysDown() & (KEY_START | KEY_A))
                return true;
            const bool down = (keysHeld() & KEY_TOUCH) != 0;
            if (down && !touching)
            {
                touchPosition touch;
                touchRead(&touch);
                const int col = (touch.px - kLeft) / kKey, row = (touch.py - kTop) / kKey;
                if (touch.py >= kBottom && touch.py < kBottom + 26 && touch.px >= kLeft)
                {
                    const int n = (touch.px - kLeft) / 80;
                    sound.ui(kUiSoundPress);
                    if (n == 1)
                        return true;
                    if (n == 2)
                        return false;
                    if (n == 0 && typed.size() < 24)
                        typed.push_back(' ');
                    prompt();
                }
                else if (touch.px >= kLeft && touch.py >= kTop && col < 10 && row < 4 && kKeys[row][col] != ' ')
                {
                    const char c = kKeys[row][col];
                    sound.ui(kUiSoundTick);
                    if (c == '<')
                    {
                        if (!typed.empty())
                            typed.pop_back();
                    }
                    else if (typed.size() < 24)
                    {
                        typed.push_back(c);
                    }
                    heldRow = row;
                    heldCol = col;
                    key(row, col, true);
                    prompt();
                }
            }
            else if (!down && heldRow >= 0)
            {
                key(heldRow, heldCol, false);
                heldRow = heldCol = -1;
            }
            touching = down;
        }
    }
} // namespace hlrt
