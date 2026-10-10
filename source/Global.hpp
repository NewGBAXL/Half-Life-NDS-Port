#pragma once

// Global process/init singleton. Brings up the console hardware (IRQs, DSi
// clock, FAT, Nitro Engine Advanced) and exposes the on-SD data paths the rest
// of the runtime builds on. Same shape as the Riven port's Global.
//
// SCREENS: the 3D world is on the TOP screen and the touch screen is the aim
// pad, the way DS first-person shooters do it.
//
// TWO STEPS. Init() brings up everything that does not depend on the map;
// InitVideo() brings up the 3D engine, and has to know one thing about the map
// first: whether it needs TWO-PASS rendering.
//
// The hardware draws 2048 polygons and 6144 vertices a frame and no more. In
// two-pass mode the screen is drawn as a left half and a right half on
// alternate frames, each with that whole allowance to itself, and the two are
// put together with the display capture unit. It doubles what can be on
// screen and halves the frame rate to 30, so it is used only for the maps
// that need it -- which the converter measured and wrote in the level's
// header.
//
// Of NEA's three two-pass variants this uses the FB one: the two capture
// buffers are VRAM banks C and D rather than 192 KB of main RAM (a twentieth
// of a DS's memory), and textures get banks A and B, which is exactly the one
// compressed-texture pair the converter budgets a map's world textures for.

#include "global_header.hpp"

class Global
{
public:
    const char *fatDevice = nullptr;   ///< libfat default drive, e.g. "fat:" / "sd:"
    std::string fatDeviceCPP;          ///< same as a std::string, for path building
    bool hasFat = false;               ///< FAT init succeeded and a drive is mounted
    /// Booted in DSi mode: 16 MB of main RAM and the 134 MHz clock are available.
    /// False on a DS, a DS Lite, and on a DSi/3DS running the ROM in DS mode
    /// (most flashcards).
    bool isDSi = false;
    /// The ARM9 is running at 134 MHz. Only ever true when isDSi is.
    bool fastClock = false;

    /// How the 3D engine is brought up.
    enum class Video : std::uint8_t
    {
        SinglePass,
        TwoPass,    ///< halves put together in video memory (NEA's "FB")
        TwoPassDma, ///< halves put together in main memory (NEA's "HBL DMA")
    };
    /// Set by InitVideo().
    Video video = Video::SinglePass;
    /// The world is drawn in two passes, at 30 frames a second, either way.
    bool twoPass = false;

    /// IRQs, clock and FAT: everything but the picture. Call once at startup.
    void Init();

    /// The 3D engine, in single-pass or two-pass mode, and the debug console.
    /// Call after Init().
    ///
    /// May be called again to change mode. NEA tears itself down and starts
    /// over when it is, which frees every material and palette it held: the
    /// caller must drop its own pointers to them first (TextureCache::forget)
    /// and load the textures again afterwards.
    void InitVideo(Video want);
    bool videoReady() const { return videoReady_; }

    /// Vertical blanks since boot: the game's clock. Sixty to the second.
    static std::uint32_t vblanks();

    /// Bytes the heap can still grow by. Not the same as "free memory" -- blocks
    /// already released back to the allocator are not counted -- but it is the
    /// number that says whether a level of a given size can still be loaded.
    std::size_t heapHeadroom() const;

    // SD data paths (only meaningful when hasFat). Mirrors the layout the asset
    // converter writes: <drive>/_nds/hl_nds/data/.
    //
    // What the game reads is in archives (Nitro Engine Advanced's NPAC, each
    // mounted as a drive of its own): one a map, holding all the map loads
    // in the order it loads it, and one for what belongs to no map. Opening a
    // file on a card is slow; a map used to be two hundred of them. Loose
    // beside the archives: what the game writes (saves, settings) and the
    // music, which is played off the card as it goes.
    std::string dataDir() const { return fatDeviceCPP + "_nds/hl_nds/data/"; }
    /// A map's archive: maps/<name>.npac.
    std::string mapArchive(const std::string &name) const { return dataDir() + "maps/" + name + ".npac"; }
    /// Inside the archive of the map being played: "level.0", "tex/...",
    /// "spr/...", "mdl/...", "snd/...".
    std::string mapPath(const std::string &rel) const { return "map:/" + rel; }
    std::string texDir() const { return mapPath("tex/"); }
    /// Inside the card's common archive: "ui/...", "snd/...".
    std::string commonPath(const std::string &rel) const { return "common:/" + rel; }
    /// Written by the game, never by the converter.
    std::string savesDir() const { return dataDir() + "saves/"; }
    std::string dataPath(const std::string &rel) const { return dataDir() + rel; }

    /// Put a map's archive in the place of whichever is mounted. False if
    /// it is not on the card (and then none is).
    bool mountMap(const std::string &name);
    void unmountMap();
    /// The common archive, if it is not mounted already. (The 3D engine
    /// lets go of every archive when it is brought up again: InitVideo.)
    void mountCommon();

    /// Why the game cannot start. Everything below Ok means the card does not
    /// carry a usable conversion, which would otherwise show up as a black
    /// screen rather than as anything the player could act on.
    enum class DataStatus
    {
        Ok,
        NoCard,       ///< no FAT device at all (no SD, or the flashcard's DLDI failed)
        NoData,       ///< the card is readable but holds no manifest: never converted
        OtherVersion, ///< converted by a converter this build does not match
        Damaged,      ///< a manifest that does not check out, or lists no maps
    };

    /// Read the converter's manifest and with it the map to start in. Cheap --
    /// one small file -- and done before the 3D engine is up.
    DataStatus ReadManifest(std::string &startMap) const;

    /// Two lines of player-facing explanation for a status: what is wrong and
    /// what to do about it. Never null.
    static const char *DataStatusTitle(DataStatus s);
    static const char *DataStatusHint(DataStatus s);

private:
    bool videoReady_ = false;
    std::string mapMounted_; ///< the map whose archive is the "map" drive, if any
};

extern Global global;
