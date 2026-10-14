#include <ctime>

#include "global_header.hpp"

#include "DebugLog.hpp"
#include "Global.hpp"
#include "Settings.hpp"
#include "data/LevelFile.hpp"
#include "data/ModelCache.hpp"
#include "data/SpriteCache.hpp"
#include "data/TextureCache.hpp"
#include "data/TextureStream.hpp"
#include "game/Game.hpp"
#include "game/Input.hpp"
#include "game/Player.hpp"
#include "game/SaveGame.hpp"
#include "render/BottomScreen.hpp"
#include "render/Camera.hpp"
#include "render/DebugOverlay.hpp"
#include "render/WorldDraw.hpp"
#include "audio/DspAudio.hpp"
#include "audio/Sound.hpp"
#include "render/GpuStats.hpp"
#include "render/ModelDraw.hpp"
#include "render/HudDraw.hpp"
#include "render/FrameFreeze.hpp"
#include "render/ScreenFade.hpp"
#include "render/TopText.hpp"
#include "ui/Menu.hpp"

namespace hlrt
{
    int hwMathCheck(int count); // HwMathCheck.cpp
}

namespace
{
    /// Show why the game cannot start, and stop. Everything past this point
    /// reads data that is not there, so this is a dead end on purpose.
    [[noreturn]] void haltWith(const char *l1, const char *l2, const char *l3)
    {
        // (The touch screen may not have been set up yet: this does it.)
        hlrt::bottomScreen.showFatal(l1, l2, l3);
        while (true)
            swiWaitForVBlank();
    }

    std::string currentMap;

    /// How many frames are handed to the hardware before the curtain goes
    /// up on a map: the first has then been on the screen for one, and the
    /// map has had a few frames to put itself in order. (Seen: the first
    /// map's first frames with a pale band across their top, which a
    /// curtain of two frames let through.)
    constexpr int curtainFrames()
    {
        return 12;
    }

    /// A line in perf.txt about the map being played: what the hardware made
    /// of it (GpuStats).
    void logPerformance()
    {
        hlrt::gpuStats.writeLog(currentMap.c_str(), "1P");
        // With the DSi's DSP in use, how it got on over that map.
        if (hlrt::sound.extended())
        {
            if (std::FILE *log = std::fopen(global.dataPath("perf.txt").c_str(), "a"))
            {
                std::fprintf(log, "dsp: ran short %lu, faults %d, block %lu\n",
                             static_cast<unsigned long>(hlrt::dspAudio.dry()), hlrt::dspAudio.faults(),
                             static_cast<unsigned long>(hlrt::dspAudio.block()));
                std::fclose(log);
            }
        }
    }

#ifdef HLDS_PROFILE
    /// Where a load went: a line in perf.txt, a stage at a time, in
    /// milliseconds. (The card's share of each can only be judged on a card.)
    struct LoadClock
    {
        static constexpr int kStages = 8;
        const char *names[kStages] = {};
        std::uint32_t ms[kStages] = {};
        int count = 0;
        std::uint32_t last = hlrt::DebugLog::Perf::now();

        void mark(const char *name)
        {
            const std::uint32_t now = hlrt::DebugLog::Perf::now();
            if (count < kStages)
            {
                names[count] = name;
                // (The timer counts the 33.5 MHz bus clock.)
                ms[count++] = static_cast<std::uint32_t>((std::uint64_t(now - last) * 2u) / 67000u);
            }
            last = hlrt::DebugLog::Perf::now();
        }
        void write(const char *map)
        {
            std::FILE *log = std::fopen(global.dataPath("perf.txt").c_str(), "a");
            if (log == nullptr)
                return;
            std::uint32_t total = 0;
            std::fprintf(log, "load: %s", map);
            for (int i = 0; i < count; ++i)
            {
                std::fprintf(log, " %s %lu", names[i], static_cast<unsigned long>(ms[i]));
                total += ms[i];
            }
            std::fprintf(log, " = %lu ms (check %lu), heap %lu\n", static_cast<unsigned long>(total),
                         static_cast<unsigned long>(hlrt::level.checkMs),
                         static_cast<unsigned long>(global.heapHeadroom()));
            std::fclose(log);
        }
    };
#else
    struct LoadClock
    {
        void mark(const char *) {}
        void write(const char *) {}
    };
#endif

    void drawScene()
    {
        // NEA leaves all four lights on and the modelview at identity. The
        // world is lit by its vertex colours: no lights.
        hlrt::worldDraw.applyFog();
        NEA_PolyFormat(31, 0, static_cast<NEA_LightEnum>(0), hlrt::WorldDraw::kCull,
                       hlrt::worldDraw.polygonMode());
        hlrt::camera.apply();
        hlrt::worldDraw.draw();
        hlrt::hudDraw.draw();
    }

    enum class LoadResult
    {
        Ok,
        NotOnCard, ///< nothing was touched: the game goes on where it is
        Failed,    ///< the old map is gone and the new one is not there
    };

    /// Load `name`, in place of whatever is loaded. With an `arrival` the
    /// player is put where it says; without, at the map's own start.
    ///
    /// The old map is let go BEFORE the new one is read: a DS has 4 MB and a
    /// map is a quarter of that, so two do not fit. That is why the one thing
    /// that can be checked first -- that the file is there and is a level of
    /// this version -- is.
    LoadResult loadLevel(const std::string &name, const hldata::Arrival *arrival,
                         std::string &error)
    {
        hldata::LevelHeader header{};
        LoadClock clock;
        if (!hlrt::Level::peekHeader(name, header))
        {
            error = "Map not on the card:";
            return LoadResult::NotOnCard;
        }
        clock.mark("peek");
        hlrt::bottomScreen.showMessage("LOADING", name.c_str());
        // The last picture of the map being left stays on the screen while
        // this one loads, if there is one (FrameFreeze). Otherwise nothing of
        // the old picture is seen from here on: the hardware would go on
        // drawing it out of video memory, which is about to be written over.
        const bool kept = hlrt::level.loaded() && !hlrt::screenFade.curtained() && hlrt::frameFreeze.freeze();
        if (!kept)
            hlrt::screenFade.curtainDown();

        // What is broken in the map being left is noted first: it is still
        // broken if the player comes back.
        if (hlrt::level.loaded())
        {
            hlrt::game.rememberMap(hldata::nameHash(currentMap.c_str()));
            logPerformance();
        }
        hlrt::gpuStats.beginMap();
        // Silence first: the sounds are the map's, and are played out of its
        // memory.
        hlrt::sound.unload();
        hlrt::game.destroy();
        hlrt::worldDraw.destroy();
        hlrt::textureStream.end();
        hlrt::textureCache.destroy();
        hlrt::spriteCache.destroy();
        hlrt::modelCache.destroy();

        // With no picture kept: the game's loading picture, on the screen
        // and left there while the map is read (video memory is empty just
        // now: there is room). That is the first load, and little else.
        if (!kept && hlrt::hudDraw.showLoading())
            hlrt::screenFade.curtainUp();

        clock.mark("down");
        // The map's archive in the place of the last one's: everything
        // from here to the end of the load is read out of it.
        if (!global.mountMap(name))
        {
            error = "Map not found on the card.";
            return LoadResult::Failed;
        }
        if (!hlrt::level.load(error))
            return LoadResult::Failed;
        clock.mark("level");
        // Textures before models: if video memory runs short it is a model
        // that goes without its skin, not a wall. Models before the game:
        // actors are set up from their models.
        hlrt::modelDraw.forgetPoses();
        bool made = hlrt::worldDraw.create() && hlrt::textureCache.create();
        clock.mark("tex");
        made = made && hlrt::spriteCache.create();
        clock.mark("spr");
        made = made && hlrt::modelCache.create();
        clock.mark("mdl");
        // What video memory has left is for the textures that stream.
        hlrt::textureStream.begin();
        clock.mark("stream");
        made = made && hlrt::game.create();
        if (!made)
        {
            error = "Not enough memory for this map.";
            return LoadResult::Failed;
        }
        hlrt::worldDraw.setBrushStates(hlrt::game.brushStates());
        hlrt::game.restoreMap(hldata::nameHash(name.c_str()));
        currentMap = name;
        hlrt::debugOverlay.setMapName(currentMap.c_str());

        hlrt::player.spawn();
        if (arrival != nullptr)
            hlrt::game.applyArrival(*arrival);
        clock.mark("game");
        hlrt::sound.load();
        clock.mark("snd");
        clock.write(name.c_str());
        if (hlrt::player.debugTrack >= 0)
        {
            hlrt::game.logic.musicRequest = hlrt::player.debugTrack;
            hlrt::player.debugTrack = -1;
        }
        hlrt::bottomScreen.showPad();
        // Down again while the picture gives way to the map's first.
        hlrt::screenFade.curtainDown();
        hlrt::hudDraw.hideLoading();
        hlrt::frameFreeze.thaw();
        hlrt::screenFade.curtainUp(curtainFrames());
        return LoadResult::Ok;
    }

    /// Until what was last said on the touch screen gives way to the pad
    /// again (a blank count; 0: nothing is waiting to).
    std::uint32_t saidUntil = 0;
    void say(const char *line, const char *under = "")
    {
        hlrt::bottomScreen.showMessage(line, under);
        saidUntil = Global::vblanks() + 90;
    }

    /// The game as it is, into `slot` (a kSlot*). Said on the screen either
    /// way: that it was saved, or that the card would not take it.
    bool saveNow(int slot, bool quiet = false)
    {
        if (!hlrt::level.loaded() || hlrt::player.noclip || hlrt::player.state.health <= 0)
            return false;
        hldata::SaveHeading heading = hlrt::game.heading(currentMap);
        // (When, by the console's clock, if it has the time.)
        const std::time_t now = std::time(nullptr);
        const std::tm *when = std::localtime(&now);
        if (when != nullptr && when->tm_year + 1900 >= 2005)
        {
            heading.year = static_cast<std::uint16_t>(when->tm_year + 1900);
            heading.month = static_cast<std::uint8_t>(when->tm_mon + 1);
            heading.day = static_cast<std::uint8_t>(when->tm_mday);
            heading.hour = static_cast<std::uint8_t>(when->tm_hour);
            heading.minute = static_cast<std::uint8_t>(when->tm_min);
        }
        const bool saved = hlrt::SaveGame::write(hlrt::game.snapshot(currentMap), slot, heading);
        if (!saved)
            say("COULD NOT SAVE", "the card would not take it");
        else if (!quiet)
            say(slot == hldata::kSlotAuto ? "AUTOSAVED" : "SAVED");
        return saved;
    }
    /// What a death goes back to, and when the game last went back.
    hldata::DeathLoop deathLoop;
    std::uint32_t wentBack = 0;

    /// Go to where the save in `slot` says: its map begun, and the whole of
    /// its state laid over it. False -- with nothing changed -- if there is
    /// none, or its map is not on the card.
    bool loadSave(std::string &error, int slot)
    {
        static hldata::SaveData save; // not on the stack: it holds vectors of vectors
        save = hldata::SaveData();
        hldata::LevelHeader header{};
        if (slot < 0 || !hlrt::SaveGame::read(save, slot) ||
            !hlrt::Level::peekHeader(save.map, header))
            return false;
        hlrt::game.adoptProgress(save);
        const std::string map = save.map; // loadLevel changes currentMap, which this may be
        // (Whoever came into that map with the player comes into it again.)
        static hldata::Arrival came;
        const bool arrived = hlrt::Game::arrivalOf(save, came);
        if (loadLevel(map, arrived ? &came : nullptr, error) != LoadResult::Ok)
            haltWith("Cannot go on:", error.c_str(), map.c_str());
        hlrt::game.placeFromSave(save);
        // Every door, trigger and actor as it was -- or, from a save that
        // has not that (or not for this map as it is cooked now), the map
        // as it begins with the player put back.
        if (!hlrt::game.layState(save))
            say("LOADED", "the map from its beginning");
        save = hldata::SaveData(); // (tens of kilobytes: not kept)
        return true;
    }
    /// The most recent save of those the player is shown; -1: none.
    int latestSave()
    {
        return hldata::saveMostRecent(hlrt::SaveGame::index());
    }

    /// The debug command line: a line typed on libnds's on-screen keyboard,
    /// and done. Returns what there is to say about it (shown where the pad
    /// is, until the next touch), or an empty string.
    ///
    ///   map <name>     go to a map (as a new arrival: at its own start)
    ///   give           every weapon, full of ammunition, and the suit
    ///   heal           health back to 100
    ///   noclip         fly through walls, or stop
    std::string runCommand(std::string line)
    {
        while (!line.empty() && line.front() == ' ')
            line.erase(0, 1);
        const std::size_t space = line.find(' ');
        const std::string verb = line.substr(0, space);
        std::string rest = space == std::string::npos ? "" : line.substr(space + 1);
        while (!rest.empty() && rest.front() == ' ')
            rest.erase(0, 1);
        while (!rest.empty() && rest.back() == ' ')
            rest.pop_back();
        if (verb == "map" && !rest.empty())
        {
            // A map's name becomes a file's: letters, digits and underscores.
            for (const char c : rest)
            {
                const bool plain = (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_';
                if (!plain)
                    return "not a map's name";
            }
            std::string error;
            switch (loadLevel(rest, nullptr, error))
            {
            case LoadResult::Ok:
                return "";
            case LoadResult::NotOnCard:
                return "no such map on the card";
            case LoadResult::Failed:
                haltWith("Cannot go on:", error.c_str(), rest.c_str());
            }
            return "";
        }
        if (verb == "give")
        {
            hldata::Inventory &inv = hlrt::player.state.inv;
            inv.have = static_cast<std::uint8_t>((1u << hldata::kWeaponCount) - 1u);
            if (inv.current == hldata::kWeaponNone)
                inv.current = hldata::kWeaponCrowbar;
            inv.clipGlock = 17;
            inv.clipShotgun = 8;
            inv.ammo9mm = 250;
            inv.ammoBuck = 125;
            inv.grenades = 10;
            inv.clipMp5 = 50;
            inv.clipPython = 6;
            inv.ammo357 = 36;
            inv.ammoAr = 10;
            inv.armor = 100;
            return "given";
        }
        if (verb == "heal")
        {
            hlrt::player.state.health = 100;
            return "healed";
        }
        if (verb == "noclip")
        {
            hlrt::player.noclip = !hlrt::player.noclip;
            return hlrt::player.noclip ? "flying" : "walking";
        }
        return "map <name> / give / heal / noclip";
    }

    /// Show the keyboard, take a line, run it. The game stands still
    /// meanwhile, and its sound with it (the caller's pause is still on).
    std::string commandLine()
    {
        std::string typed;
        return hlrt::menu.typeLine(typed) ? runCommand(typed) : std::string();
    }

    /// A development aid (tools/emu): a card with debug_start.txt on it starts
    /// where that says, save or no save.
    bool debugStart()
    {
        std::FILE *file = std::fopen(global.dataPath("debug_start.txt").c_str(), "r");
        if (file == nullptr)
            return false;
        // (The readout is on: whoever starts this way wants to see it.)
        settings.debugReadout = true;
        // (The sixth number, the seconds until the player dies, is read by
        // Player. The seventh is how many models may be drawn at once.)
        int n[7] = {0, 0, 0, 0, -1, 0, 0};
        const int given = std::fscanf(file, "%d %d %d %d %d %d %d", &n[0], &n[1], &n[2], &n[3], &n[4], &n[5], &n[6]);
        if (given == 7 && n[6] > 0)
            hlrt::modelDraw.setLimit(n[6]);
        std::fclose(file);
        return true;
    }
}

int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    global.Init();
    // (Before the first load, which is timed too.)
    hlrt::DebugLog::Perf::begin();
    hlrt::DebugLog::Perf::markStack();
    hlrt::screenFade.curtainDown();
    // How many models at once: what there is time for. Measured: with five
    // soldiers in sight a DS's frame is already 15 ms of its 16.7, each
    // model costing it about a millisecond; a DSi's is 10. In the game's
    // maps more than eight are almost never in sight together anyway.
    hlrt::modelDraw.setLimit(isDSiMode() ? 12 : 8);

    // The manifest says what the converter put on the card and which map to start in.
    std::string startMap;
    const Global::DataStatus status = global.ReadManifest(startMap);

    settings.load();
    const bool debugging = debugStart();

    global.InitVideo();

    if (status != Global::DataStatus::Ok)
        haltWith("Cannot start:", Global::DataStatusTitle(status), Global::DataStatusHint(status));

    hlrt::bottomScreen.create();
    hlrt::hudDraw.load();
    // (Before any map: a map's sounds are handed to it as they are read.)
    hlrt::sound.init();
    std::string error;
    hlrt::menu.setSaves(&hlrt::SaveGame::index());
    // The title: go on with the last save, load another, or begin. (A card
    // set up to start somewhere for a test goes straight there.)
    bool begun = false;
    if (!debugging)
    {
        if (hlrt::hudDraw.showLoading())
            hlrt::screenFade.curtainUp();
        hlrt::menu.openTitle();
        while (!begun)
        {
            swiWaitForVBlank();
            hlrt::input.poll();
            hlrt::sound.frame();
            using hlrt::Menu;
            const Menu::Action item = hlrt::menu.frame();
            if (item == Menu::Action::Continue || item == Menu::Action::Load)
            {
                begun = loadSave(error, hlrt::menu.slot());
                if (!begun)
                    hlrt::menu.openTitle(); // (it could not be: the title again)
            }
            else if (item == Menu::Action::NewGame)
            {
                break;
            }
        }
        settings.save();
    }
    if (!begun)
    {
        hlrt::game.resetProgress();
        if (loadLevel(startMap, nullptr, error) != LoadResult::Ok)
            haltWith("Cannot start:", error.c_str(), startMap.c_str());
        // (Somewhere to come back to from the first: the game's own save,
        // not one of the player's.)
        if (!debugging)
        {
            saveNow(hldata::kSlotAuto, true);
            saveNow(hldata::kSlotArrival, true);
        }
    }

#ifdef HLDS_PROFILE
    // (A build that measures also checks the hardware's arithmetic.)
    // (... and says how long the console took to get to the first map.)
    if (std::FILE *log = std::fopen(global.dataPath("perf.txt").c_str(), "a"))
    {
        std::fprintf(log, "boot: the first map was up %lu blanks after the start\n",
                     static_cast<unsigned long>(Global::vblanks()));
        std::fclose(log);
    }
    if (hlrt::hwMathCheck(4096) != 0)
        hlrt::bottomScreen.showMessage("HARDWARE MATHS", "does not agree: see perf.txt");
#endif
    std::uint32_t lastVblank = Global::vblanks();
    std::uint32_t lastPass = lastVblank;
    bool paused = false;

    while (true)
    {
        NEA_WaitForVBL(static_cast<NEA_UpdateFlags>(0));
        // Upload/update texture stream while 3D engine is not reading it.
        hlrt::textureStream.upload();
        {
            const hlrt::DebugLog::Perf::Scope frame(hlrt::DebugLog::Section::Frame);

            // One step of the game: input, the world, the menu, and whatever
            // changes the map.
            const auto step = [&]()
            {
                {
                    const hlrt::DebugLog::Perf::Scope scope(hlrt::DebugLog::Section::Input);
                    hlrt::input.poll();
                }
                const std::uint32_t now = Global::vblanks();
                const int elapsed = static_cast<int>(now - lastVblank);
                lastVblank = now;

                if (!paused)
                {
                    if (hlrt::input.startPressed || hlrt::input.menuPressed)
                    {
                        paused = true;
                        hlrt::sound.setPaused(true);
                        hlrt::menu.open();
                    }
                    else
                    {
                        hlrt::bottomScreen.setPressed(hlrt::input.touchOn);
                        hlrt::game.frame(elapsed);
                    }
                }
                else
                {
                    using hlrt::Menu;
                    const Menu::Action item = hlrt::menu.frame();
                    if (item == Menu::Action::Save || item == Menu::Action::SaveTo)
                    {
                        // The quick one, or the one of the player's six
                        // that was picked.
                        hlrt::bottomScreen.showPad();
                        saveNow(item == Menu::Action::Save ? hldata::kSlotQuick : hlrt::menu.slot());
                        logPerformance();
                        hlrt::gpuStats.beginMap();
                        paused = false;
                        // (After the card has been written to: the sound
                        // goes on once nothing stands in its way.)
                        hlrt::sound.setPaused(false);
                    }
                    else if (item == Menu::Action::Load)
                    {
                        paused = false;
                        hlrt::sound.setPaused(false);
                        if (!loadSave(error, hlrt::menu.slot()))
                        {
                            hlrt::bottomScreen.showPad();
                            say("COULD NOT LOAD", "that save cannot be read");
                        }
                        deathLoop = hldata::DeathLoop();
                        lastVblank = Global::vblanks();
                    }
                    else if (item == Menu::Action::NewGame)
                    {
                        // (No save of the player's is touched: only the
                        // game's own, which are this game's from here.)
                        paused = false;
                        hlrt::sound.setPaused(false);
                        hlrt::game.resetProgress();
                        if (loadLevel(startMap, nullptr, error) != LoadResult::Ok)
                            haltWith("Cannot start:", error.c_str(), startMap.c_str());
                        saveNow(hldata::kSlotAuto, true);
                        saveNow(hldata::kSlotArrival, true);
                        deathLoop = hldata::DeathLoop();
                        lastVblank = Global::vblanks();
                    }
                    else if (item == Menu::Action::Command)
                    {
                        // The debug command line. Whatever it has to say is
                        // left on the screen until the pad is next drawn.
                        // (The sound stands still with the game for as long
                        // as the keyboard is up, as on the menu's other
                        // pages; a map the line asks for is loaded with it
                        // still, and heard once the line is done with.)
                        const std::string said = commandLine();
                        paused = false;
                        hlrt::sound.setPaused(false);
                        hlrt::bottomScreen.showPad();
                        if (!said.empty())
                            hlrt::bottomScreen.showMessage(said.c_str(), "START, then RESUME");
                        lastVblank = Global::vblanks();
                    }
                    else if (item == Menu::Action::Resume || hlrt::input.startPressed)
                    {
                        paused = false;
                        settings.save();
                        // (Once the card has been written to and the
                        // pictures are back: nothing then stands in the
                        // sound's way.)
                        hlrt::sound.setPaused(false);
                        hlrt::bottomScreen.showPad();
                    }
                }

                // Dead: back to the last save, or to the start of the map if
                // there is none.
                if (!paused && hlrt::player.state.health <= 0 &&
                    (hlrt::player.noclip || hlrt::player.view.deadMs >= hldata::kDeathWaitMs))
                {
#ifdef HLDS_PROFILE
                    if (std::FILE *log = std::fopen(global.dataPath("perf.txt").c_str(), "a"))
                    {
                        std::fprintf(log, "died: health %d at %d %d %d, %lu blanks in\n",
                                     static_cast<int>(hlrt::player.state.health),
                                     static_cast<int>(hldata::fxToInt(hlrt::player.state.origin[0])),
                                     static_cast<int>(hldata::fxToInt(hlrt::player.state.origin[1])),
                                     static_cast<int>(hldata::fxToInt(hlrt::player.state.origin[2])),
                                     static_cast<unsigned long>(Global::vblanks()));
                        std::fclose(log);
                    }
#endif
                    // (Not to a save that is itself the trouble: HlSave.hpp.)
                    const int back = hldata::deathChoice(
                        deathLoop, static_cast<int>((Global::vblanks() - wentBack) * 1000 / 60));
                    const bool went =
                        back == hldata::kBackStart
                            ? false
                            : (back == hldata::kBackArrival && loadSave(error, hldata::kSlotArrival)) ||
                                  loadSave(error, latestSave()) || loadSave(error, hldata::kSlotArrival);
                    if (!went)
                    {
                        const std::string again = currentMap;
                        if (loadLevel(again, nullptr, error) != LoadResult::Ok)
                            haltWith("Cannot go on:", error.c_str(), again.c_str());
                    }
                    lastVblank = wentBack = Global::vblanks();
                }
                // The map asked for a save (a trigger_autosave).
                if (hlrt::game.logic.saveRequested)
                {
                    hlrt::game.logic.saveRequested = false;
                    saveNow(hldata::kSlotAuto);
                }
                // What was said on the touch screen has been read.
                if (saidUntil != 0 && !paused && Global::vblanks() > saidUntil)
                {
                    saidUntil = 0;
                    hlrt::bottomScreen.showPad();
                }

                // The way into the next map was taken this frame. What
                // crosses is packed first, while the old map can still say
                // where its landmark is.
                if (hlrt::game.pendingChange() >= 0)
                {
                    const int record = hlrt::game.pendingChange();
                    const hldata::Arrival arrival = hlrt::game.makeArrival();
                    const std::string next =
                        hlrt::level.view.strings + hlrt::level.view.logic[record].extra[0];
                    switch (loadLevel(next, &arrival, error))
                    {
                    case LoadResult::Ok:
                        // Every new map is somewhere to come back to: the
                        // game's own save, and the one a run of deaths
                        // goes back to.
                        saveNow(hldata::kSlotAuto);
                        saveNow(hldata::kSlotArrival, true);
                        break;
                    case LoadResult::NotOnCard:
                        // Not converted: say so, and do not ask again every
                        // frame the player stands in the doorway.
                        hlrt::game.logic.pendingChange = -1;
                        hlrt::game.logic.states[record].dead = true;
                        hlrt::bottomScreen.showMessage(error.c_str(), next.c_str());
                        break;
                    case LoadResult::Failed:
                        haltWith("Cannot go on:", error.c_str(), next.c_str());
                    }
                    lastVblank = Global::vblanks();
                }
            };

            step();
            NEA_Process(drawScene);
            hlrt::sound.frame();
            // What is in view now says what to read for the next frames;
            // a few kilobytes a frame, more where the card is quicker.
            hlrt::textureStream.update(isDSiMode() ? 8192 : 4096);
            // What the hardware made of the pass before this one, and the
            // budget corrected by it.
            {
                const std::uint32_t now = Global::vblanks();
                const hlrt::WorldStats &stats = hlrt::worldDraw.stats();
                const int took = static_cast<int>(now - lastPass);
                hlrt::gpuStats.frame(stats.hwPolygons, stats.hwVertices, took);
                hlrt::gpuStats.view(stats.budgetHit, hlrt::worldDraw.pressure() != 0);
                lastPass = now;
                // A fade the map has asked for: the top screen's brightness.
                hlrt::screenFade.setFade(hlrt::game.logic.fadeLevel);
                hlrt::screenFade.tick();
                const hlrt::GpuStats::Frame &seen = hlrt::gpuStats.last();
                // (More than a second is a load, not a slow pass.)
                hlrt::worldDraw.govern(seen.overflow, seen.underflow, seen.lines,
                                       took > 1 && took < 60);
            }
        }
        hlrt::DebugLog::Perf::endFrame();
        hlrt::DebugLog::Perf::logEvery(currentMap.c_str(), 512);
        hlrt::debugOverlay.frame();
    }
}
