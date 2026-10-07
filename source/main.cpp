#include "global_header.hpp"

#include "DebugLog.hpp"
#include "Global.hpp"
#include "Settings.hpp"
#include "data/LevelFile.hpp"
#include "data/ModelCache.hpp"
#include "data/TextureCache.hpp"
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

namespace
{
    /// Show why the game cannot start, and stop. Everything past this point
    /// reads data that is not there, so this is a dead end on purpose.
    [[noreturn]] void haltWith(const char *l1, const char *l2, const char *l3)
    {
        // The touch screen may not have been set up yet, and a display left
        // switched off shows white: switch it on, black. Bank H so that this
        // is right in either video mode.
        if ((REG_DISPCNT_SUB & 0x1F00) == 0)
        {
            videoSetModeSub(MODE_0_2D);
            BG_PALETTE_SUB[0] = 0;
        }
        // Each line is at most the console's 32 columns: a wrapped line reads
        // as two messages.
        std::printf("\x1b[2J-- Half-Life DS --\n\n%.32s\n%.32s\n%.32s\n", l1, l2, l3);
        while (true)
            swiWaitForVBlank();
    }

    /// How a map is drawn under a render setting. For Auto it is the map that
    /// says: the converter wrote the worst case the PVS can ask for in the
    /// level's header. That bound counts every direction at once and the view
    /// only ever looks in one, so a map needs two passes when it is over
    /// about twice a single pass's budget.
    Global::Video videoFor(hldata::RenderMode mode, const hldata::LevelHeader &header)
    {
        switch (mode)
        {
        case hldata::RenderMode::TwoPass:
            return Global::Video::TwoPass;
        case hldata::RenderMode::TwoPassDma:
            return Global::Video::TwoPassDma;
        case hldata::RenderMode::SinglePass:
            return Global::Video::SinglePass;
        default:
            return header.worstVertices > 2 * hlrt::WorldDraw::kVertexBudget ||
                           header.worstPolygons > 2 * hlrt::WorldDraw::kPolygonBudget
                       ? Global::Video::TwoPass
                       : Global::Video::SinglePass;
        }
    }

    std::string currentMap;

    /// A line in perf.txt about the map being played: what the hardware made
    /// of it (GpuStats).
    void logPerformance()
    {
        const char *mode = global.video == Global::Video::TwoPassDma ? "2D"
                           : global.twoPass                           ? "2P"
                                                                      : "1P";
        hlrt::gpuStats.writeLog(currentMap.c_str(), mode);
    }

    void drawScene()
    {
        // NEA's two-pass shows the left half a frame before the right half
        // that goes with it: every other frame is the new left beside the old
        // right, and the picture tears down the middle whenever the view
        // turns. During the left pass, though, what is ON SCREEN is whole --
        // the captured left half with the live right half of the same moment
        // -- so capture that, the composited screen, instead of the 3D layer
        // alone. NEA then shows the capture in front for the right pass, and
        // being a whole opaque picture it covers the early left half behind
        // it. Written here because this runs straight after NEA has set the
        // register, well before the capture starts at the first line.
        // (The DMA mode has no need of it: it shows a picture only when both
        // of its halves are in.)
        if (global.video == Global::Video::TwoPass && NEA_TwoPassGetPass() == 0)
        {
            REG_DISPCAPCNT = DCAP_BANK(DCAP_BANK_VRAM_C) | DCAP_SIZE(DCAP_SIZE_256x192) |
                             DCAP_MODE(DCAP_MODE_A) | DCAP_SRC_A(DCAP_SRC_A_COMPOSITED) |
                             DCAP_ENABLE;
        }

        // NEA leaves all four lights on and the modelview at identity. The
        // world is lit by its vertex colours: no lights.
        hlrt::worldDraw.applyFog();
        NEA_PolyFormat(31, 0, static_cast<NEA_LightEnum>(0), hlrt::WorldDraw::kCull,
                       hlrt::worldDraw.polygonMode());
        hlrt::camera.apply();
        hlrt::worldDraw.draw();
    }
    /// The header of the map that is loaded (which says how it is drawn).
    hldata::LevelHeader currentHeader{};

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
        const std::string path = global.mapsDir() + name + ".hlm";
        hldata::LevelHeader header{};
        if (!hlrt::Level::peekHeader(path, header))
        {
            error = "Map not on the card:";
            return LoadResult::NotOnCard;
        }
        hlrt::bottomScreen.showMessage("LOADING", name.c_str());

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
        // The 3D engine is brought up again only if this map is drawn the
        // other way (the automatic setting): the textures go with it.
        const Global::Video want = videoFor(settings.renderMode, header);
        if (want != global.video)
        {
            hlrt::textureCache.forget();
            hlrt::modelCache.forget();
            hlrt::level.unload();
            global.InitVideo(want);
            hlrt::bottomScreen.reset();
            hlrt::bottomScreen.create();
            hlrt::bottomScreen.showMessage("LOADING", name.c_str());
        }
        else
        {
            hlrt::textureCache.destroy();
            hlrt::modelCache.destroy();
        }

        if (!hlrt::level.load(path, error))
            return LoadResult::Failed;
        // Textures before models: if video memory runs short it is a model
        // that goes without its skin, not a wall. Models before the game:
        // actors are set up from their models.
        hlrt::modelDraw.forgetPoses();
        if (!hlrt::worldDraw.create() || !hlrt::textureCache.create() ||
            !hlrt::modelCache.create(name.c_str()) || !hlrt::game.create())
        {
            error = "Not enough memory for this map.";
            return LoadResult::Failed;
        }
        hlrt::worldDraw.setBrushStates(hlrt::game.brushStates());
        hlrt::game.restoreMap(hldata::nameHash(name.c_str()));
        currentMap = name;
        currentHeader = header;
        hlrt::debugOverlay.setMapName(currentMap.c_str());

        hlrt::player.spawn();
        if (arrival != nullptr)
            hlrt::game.applyArrival(*arrival);
        hlrt::sound.load();
        if (hlrt::player.debugTrack >= 0)
        {
            hlrt::game.logic.musicRequest = hlrt::player.debugTrack;
            hlrt::player.debugTrack = -1;
        }
        hlrt::bottomScreen.showPad();
        return LoadResult::Ok;
    }

    void saveNow()
    {
        if (hlrt::level.loaded() && !hlrt::player.noclip && hlrt::player.state.health > 0)
            hlrt::SaveGame::write(hlrt::game.snapshot(currentMap));
    }

    /// Go to where the save on the card says. False -- with nothing changed
    /// -- if there is none, or its map is not on the card.
    bool loadSave(std::string &error)
    {
        static hldata::SaveData save; // not on the stack: it holds vectors of vectors
        save = hldata::SaveData();
        hldata::LevelHeader header{};
        if (!hlrt::SaveGame::read(save) ||
            !hlrt::Level::peekHeader(global.mapsDir() + save.map + ".hlm", header))
            return false;
        hlrt::game.adoptProgress(save);
        const std::string map = save.map; // loadLevel changes currentMap, which this may be
        if (loadLevel(map, nullptr, error) != LoadResult::Ok)
            haltWith("Cannot go on:", error.c_str(), map.c_str());
        hlrt::game.placeFromSave(save);
        return true;
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
    /// meanwhile (the sound goes on).
    std::string commandLine()
    {
        std::string typed;
        hlrt::bottomScreen.showCommand("", "COMMAND   (B: cancel)");
        // On a background of its own (1: NEA's console may be on 3), in the
        // part of the bank this screen leaves free (BottomScreen.hpp).
        const Keyboard *layout = keyboardGetDefault();
        if (layout == nullptr || layout->tileLen > 16 * 1024)
            return "no room for the keyboard";
        keyboardInit(layout, 1, BgType_Text4bpp, BgSize_T_256x512, 5, 1, false, true);
        keyboardShow();
        // The touch that asked for it is still down, and is on a key now:
        // not until it has lifted is anything typed.
        do
        {
            swiWaitForVBlank();
            scanKeys();
        } while (keysHeld() & KEY_TOUCH);
        bool run = false;
        while (true)
        {
            swiWaitForVBlank();
            hlrt::sound.frame();
            scanKeys();
            if (keysDown() & KEY_B)
                break;
            const int key = keyboardUpdate();
            if (key == DVK_ENTER)
            {
                run = true;
                break;
            }
            if (key == DVK_BACKSPACE)
            {
                if (!typed.empty())
                    typed.pop_back();
            }
            else if (key >= 32 && key < 127 && typed.size() < 24)
            {
                // Maps are named in small letters; so is everything else here.
                typed.push_back(static_cast<char>(key >= 'A' && key <= 'Z' ? key - 'A' + 'a' : key));
            }
            else
            {
                continue;
            }
            hlrt::bottomScreen.showCommand(typed.c_str(), "COMMAND   (B: cancel)");
        }
        keyboardHide();
        REG_DISPCNT_SUB &= ~DISPLAY_BG1_ACTIVE;
        return run ? runCommand(typed) : std::string();
    }

    /// A development aid (tools/emu): a card with debug_start.txt on it starts
    /// where that says, save or no save.
    bool debugStart()
    {
        std::FILE *file = std::fopen(global.dataPath("debug_start.txt").c_str(), "r");
        if (file == nullptr)
            return false;
        std::fclose(file);
        return true;
    }
}

int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    global.Init();

    // The manifest says what the converter put on the card and which map to
    // start in. Read before the 3D engine is up, because the map decides how
    // the engine is brought up.
    std::string startMap;
    const Global::DataStatus status = global.ReadManifest(startMap);

    settings.load();

    hldata::LevelHeader header{};
    const bool haveHeader =
        status == Global::DataStatus::Ok &&
        hlrt::Level::peekHeader(global.mapsDir() + startMap + ".hlm", header);
    global.InitVideo(haveHeader ? videoFor(settings.renderMode, header) : Global::Video::SinglePass);

    if (status != Global::DataStatus::Ok)
        haltWith("Cannot start:", Global::DataStatusTitle(status), Global::DataStatusHint(status));

    hlrt::bottomScreen.create();
    std::string error;
    // A game in progress is gone back to; otherwise a new one starts.
    if ((debugStart() || !loadSave(error)) &&
        loadLevel(startMap, nullptr, error) != LoadResult::Ok)
        haltWith("Cannot start:", error.c_str(), startMap.c_str());

    hlrt::sound.init();
    hlrt::DebugLog::Perf::begin();
    std::uint32_t lastVblank = Global::vblanks();
    std::uint32_t lastPass = lastVblank;
    bool paused = false;

    while (true)
    {
        NEA_WaitForVBL(static_cast<NEA_UpdateFlags>(0));
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
                    if (hlrt::input.startPressed)
                    {
                        paused = true;
                        hlrt::bottomScreen.showMenu(settings.renderMode, hlrt::DspAudio::available(), hlrt::sound.extended());
                    }
                    else
                    {
                        hlrt::game.frame(elapsed);
                    }
                }
                else
                {
                    using hlrt::BottomScreen;
                    const BottomScreen::MenuItem item =
                        hlrt::input.tapped
                            ? hlrt::bottomScreen.menuItemAt(hlrt::input.tapX, hlrt::input.tapY)
                            : BottomScreen::MenuItem::None;
                    if (item == BottomScreen::MenuItem::RenderMode)
                    {
                        settings.renderMode = Settings::next(settings.renderMode);
                        hlrt::bottomScreen.showMenu(settings.renderMode, hlrt::DspAudio::available(), hlrt::sound.extended());
                    }
                    else if (item == BottomScreen::MenuItem::Dsp)
                    {
                        // At once: the music goes on from where it was, by
                        // the other way of playing it.
                        settings.dspAudio = hlrt::sound.setDsp(!hlrt::sound.extended());
                        settings.save();
                        hlrt::bottomScreen.showMenu(settings.renderMode, hlrt::DspAudio::available(),
                                                    hlrt::sound.extended());
                    }
                    else if (item == BottomScreen::MenuItem::Save)
                    {
                        saveNow();
                        logPerformance();
                        hlrt::gpuStats.beginMap();
                        paused = false;
                        hlrt::bottomScreen.showPad();
                    }
                    else if (item == BottomScreen::MenuItem::NewGame)
                    {
                        paused = false;
                        hlrt::game.resetProgress();
                        if (loadLevel(startMap, nullptr, error) != LoadResult::Ok)
                            haltWith("Cannot start:", error.c_str(), startMap.c_str());
                        saveNow();
                        lastVblank = Global::vblanks();
                    }
                    else if (item == BottomScreen::MenuItem::Console)
                    {
                        // The debug command line. Whatever it has to say is
                        // left on the screen until the pad is next drawn.
                        paused = false;
                        const std::string said = commandLine();
                        hlrt::bottomScreen.showPad();
                        if (!said.empty())
                            hlrt::bottomScreen.showMessage(said.c_str(), "START, then RESUME");
                        lastVblank = Global::vblanks();
                    }
                    else if (item == BottomScreen::MenuItem::Resume || hlrt::input.startPressed)
                    {
                        paused = false;
                        settings.save();
                        // A changed mode takes effect here, on the way out of
                        // the menu, and only on a frame boundary (pass 0): the
                        // 3D engine is brought up again in the other mode and
                        // the textures, which went with it, are loaded again.
                        const Global::Video want =
                            videoFor(settings.renderMode, currentHeader);
                        if (want != global.video)
                        {
                            // (The models themselves stay as they are:
                            // only their skins went with the engine.)
                            hlrt::textureCache.forget();
                            hlrt::modelCache.forgetSkins();
                            global.InitVideo(want);
                            hlrt::bottomScreen.reset();
                            hlrt::bottomScreen.create();
                            hlrt::textureCache.create();
                            hlrt::modelCache.loadSkins();
                            lastVblank = Global::vblanks();
                        }
                        hlrt::bottomScreen.showPad();
                    }
                }

                // Dead: back to the last save, or to the start of the map if
                // there is none.
                if (!paused && hlrt::player.state.health <= 0)
                {
                    if (!loadSave(error))
                    {
                        const std::string again = currentMap;
                        if (loadLevel(again, nullptr, error) != LoadResult::Ok)
                            haltWith("Cannot go on:", error.c_str(), again.c_str());
                    }
                    lastVblank = Global::vblanks();
                }
                // The map asked for a save (a trigger_autosave).
                if (hlrt::game.logic.saveRequested)
                {
                    hlrt::game.logic.saveRequested = false;
                    saveNow();
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
                        saveNow(); // every new map is somewhere to come back to
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

            // In two-pass mode the game advances once per DISPLAYED frame:
            // the right half has to be drawn from the same place the left
            // one was, or the two do not meet in the middle. Normally that
            // is just before the left pass. In the DMA mode it is just after
            // the right one instead, so that nothing stands between the
            // vertical blank and NEA's part of a pass: that is where it
            // copies the half captured a frame ago out of video memory, and
            // if a long step made it late, the capture it copied would be
            // of the other half.
            const bool stepsLate = global.video == Global::Video::TwoPassDma;
            if (!stepsLate && (!global.twoPass || NEA_TwoPassGetPass() == 0))
                step();

            if (global.twoPass)
            {
                hlrt::worldDraw.setPass(NEA_TwoPassGetPass());
                hlrt::camera.buildPlanes(NEA_TwoPassGetPass() == 0 ? hlrt::ViewHalf::Left
                                                                   : hlrt::ViewHalf::Right);
                NEA_ProcessTwoPass(drawScene);
            }
            else
            {
                hlrt::worldDraw.setPass(0);
                NEA_Process(drawScene);
            }
            hlrt::sound.frame();
            // What the hardware made of the pass before this one, and the
            // budget corrected by it.
            {
                const std::uint32_t now = Global::vblanks();
                const hlrt::WorldStats &stats = hlrt::worldDraw.stats();
                const int took = static_cast<int>(now - lastPass);
                hlrt::gpuStats.frame(stats.hwPolygons, stats.hwVertices, took);
                hlrt::gpuStats.view(stats.budgetHit, hlrt::worldDraw.pressure() != 0);
                lastPass = now;
                const hlrt::GpuStats::Frame &seen = hlrt::gpuStats.last();
                // (More than a second is a load, not a slow pass.)
                hlrt::worldDraw.govern(seen.overflow, seen.underflow, seen.lines,
                                       took > 1 && took < 60);
            }
            if (stepsLate && global.video == Global::Video::TwoPassDma &&
                NEA_TwoPassGetPass() == 0)
                step();
        }
        hlrt::DebugLog::Perf::endFrame();
        hlrt::debugOverlay.frame();
    }
}
