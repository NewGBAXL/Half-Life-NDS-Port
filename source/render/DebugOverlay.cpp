#include "render/DebugOverlay.hpp"

#include <cstdio>

#include "DebugLog.hpp"
#include "Global.hpp"
#include "Settings.hpp"
#include "data/ModelCache.hpp"
#include "data/TextureCache.hpp"
#include "game/Player.hpp"
#include "render/Camera.hpp"
#include "render/GpuStats.hpp"
#include "audio/DspAudio.hpp"
#include "audio/Sound.hpp"
#include "render/BottomScreen.hpp"
#include "render/ModelDraw.hpp"
#include "render/WorldDraw.hpp"

namespace hlrt
{
    DebugOverlay debugOverlay;

    namespace
    {
#ifdef HLDS_PROFILE
        constexpr int kRows = 9;
#else
        constexpr int kRows = 6;
#endif
    } // namespace

    void DebugOverlay::frame()
    {
        if (!settings.debugReadout || --countdown_ > 0)
            return;
        countdown_ = kInterval;

        const WorldStats &stats = worldDraw.stats();
        // Every row is padded to the screen's 32 columns so that a shorter
        // value leaves nothing of a longer one behind.
        char line[40];
        switch (row_)
        {
        case 0:
            std::snprintf(line, sizeof(line), "%-8s %-3s %s fps %2d cpu %3d%%", mapName_,
                          global.isDSi ? "DSi" : "DS", global.video == Global::Video::TwoPassDma ? "2D" : global.twoPass ? "2P" : "1P",
                          static_cast<int>(NEA_GetFPS()), static_cast<int>(NEA_GetCPUPercent()));
            break;
        case 1:
            // CUT marks a frame where the walk ran out of budget and dropped
            // the far end of the view.
            // pN: the pressure the view is under, when it is (WorldDraw).
        {
            // (... and the level of detail each model drawn is at, nearest
            // first: 0 the finest its file has.)
            char levels[12];
            modelDraw.levelsText(levels, sizeof(levels));
            std::snprintf(line, sizeof(line), "leaf %4d surf %3d br %2d %s%.0d %s L%s", stats.leaf,
                          stats.surfaces, stats.brushes, worldDraw.pressure() ? "p" : "",
                          worldDraw.pressure(), stats.budgetHit ? "CUT" : "", levels);
        }
            break;
        case 2:
            // "chosen/used": what the level's own counts said, then what the
            // geometry engine reports after clipping.
            // ... and "m": the textures and skins of this map that did not
            // load (none, unless video memory ran out).
            std::snprintf(line, sizeof(line), "poly %4d/%4d vert %4d/%4d m%d", stats.polygons,
                          stats.hwPolygons, stats.vertices, stats.hwVertices,
                          textureCache.missing() + modelCache.missing() + modelCache.bare());
            break;
        case 3:
            std::snprintf(line, sizeof(line), "pos %5d %5d %5d %s",
                          hldata::fxToInt(camera.pos[0]), hldata::fxToInt(camera.pos[1]),
                          hldata::fxToInt(camera.pos[2]),
                          player.noclip ? "fly"
                                        : (player.state.stuck
                                               ? "STUCK"
                                               : (player.state.onGround ? "ground" : "air")));
            break;
        case 4:
            // ... and the sound: how many of the map's sounds are in memory,
            // and on a DSi whether its output and DSP are in use ("dsp", with
            // how often the DSP ran short of music or echo to play, how often
            // it said its music had gone wrong, and the block of the track it
            // has been given).
            if (sound.extended())
                std::snprintf(line, sizeof(line), "dsp dry %lu flt %d blk %lu in %d",
                              static_cast<unsigned long>(dspAudio.dry()), dspAudio.faults(),
                              static_cast<unsigned long>(dspAudio.block()), dspAudio.heard());
            else
                // (... and how many voices each layer has: the player's,
                // the world's, the voice, the loops, the menu.)
                std::snprintf(line, sizeof(line), "heap %5uK snd %d %d %d %d %d",
                              static_cast<unsigned>(global.heapHeadroom() / 1024),
                              sound.voicesOf(hldata::SoundLayer::Player), sound.voicesOf(hldata::SoundLayer::World),
                              sound.voicesOf(hldata::SoundLayer::Voice), sound.voicesOf(hldata::SoundLayer::Ambience),
                              sound.voicesOf(hldata::SoundLayer::Ui));
            break;
        case 5:
            // The hardware's own account (GpuStats): the least lead the
            // rasteriser had (46 is idle), passes with lines unfinished and
            // with polygons dropped in this map, and what the budget is
            // holding back because of them.
            // ... and how far off the fog is whole, while a cut is fogged.
            std::snprintf(line, sizeof(line), "ln %2d u%3d o%3d %+d fog %d",
                          gpuStats.takeLowLines(), gpuStats.underflows(), gpuStats.overflows(),
                          -worldDraw.heldBack(), worldDraw.fogDistance());
            break;
#ifdef HLDS_PROFILE
        case 6:
            std::snprintf(line, sizeof(line), "us frm %5u mov %4u lgc %4u",
                          static_cast<unsigned>(DebugLog::Perf::micros(DebugLog::Section::Frame)),
                          static_cast<unsigned>(DebugLog::Perf::micros(DebugLog::Section::Move)),
                          static_cast<unsigned>(DebugLog::Perf::micros(DebugLog::Section::Logic)));
            break;
        case 7:
            std::snprintf(line, sizeof(line), "us walk  %5u emit %4u",
                          static_cast<unsigned>(DebugLog::Perf::micros(DebugLog::Section::Walk)),
                          static_cast<unsigned>(DebugLog::Perf::micros(DebugLog::Section::Emit)));
            break;
        case 8:
            std::snprintf(line, sizeof(line), "us pose  %5u mdl  %4u",
                          static_cast<unsigned>(DebugLog::Perf::micros(DebugLog::Section::Pose)),
                          static_cast<unsigned>(DebugLog::Perf::micros(DebugLog::Section::Models)));
            break;
#endif
        default:
            line[0] = '\0';
            break;
        }
        bottomScreen.readout(row_, line);
        row_ = (row_ + 1) % kRows;
    }
} // namespace hlrt
