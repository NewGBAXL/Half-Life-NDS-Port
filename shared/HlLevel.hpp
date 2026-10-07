#pragma once

// The cooked level: the file the converter writes for each map and the DS
// reads, <card>/_nds/hl_nds/data/maps/<map>.hlm.
//
// THE SHAPE. A fixed header, a directory of sections, then the sections: each
// one a flat array of fixed-size records, 4-byte aligned. The DS reads the
// whole file into one heap block with one fread and points into it -- there is
// no parsing, no per-record allocation and nothing to free but the block.
// (yas is used for the manifest and the saves, where the data is a graph. A
// level is tables, and tables are what this is.)
//
// THE CONTRACT. LevelView::open() (HlLevelReader.hpp) checks every index a
// record holds into another section before anything uses it, the same promise
// the converter's BspFile makes about a .bsp. The renderer and the hull trace
// run in ITCM with no bounds checks; they can because of that one pass.
//
// VERSIONING. Bump kLevelVersion on ANY change to a record or to what a field
// means. The DS refuses a file whose version is not exactly its own and tells
// the player to run the converter again; there is no migration, because the
// source of truth is the player's copy of the game and re-cooking is cheap.
//
// Coordinates keep Half-Life's axes (X forward, Y left, Z up). The swap to the
// geometry engine's axes lives in the view matrix, not in the data.
//
// Formats named in comments (Q12, Q30, ...) are defined in HlFixed.hpp.

#include <cstdint>

#include "HlFixed.hpp"
#include "HlTexture.hpp"

namespace hldata
{
    inline constexpr char kLevelMagic[4] = {'H', 'L', 'D', 'S'};
    inline constexpr std::uint16_t kLevelVersion = 14;

    enum class Section : std::uint16_t
    {
        Planes,     ///< Plane
        PlaneTypes, ///< u8, parallel to Planes: 0/1/2 = exactly +X/+Y/+Z, 3 = other
        Nodes,      ///< Node: the render tree (hull 0)
        Leaves,     ///< Leaf; leaf 0 is the shared solid leaf
        Marks,      ///< u16: surface indices, grouped per leaf
        Vis,        ///< u8: the BSP's run-length visibility rows, unchanged
        Surfaces,   ///< Surface: world model first, sorted by texture
        GxStream,   ///< u32: packed geometry-engine command words
        Models,     ///< Model; model 0 is the world
        ClipNodes,  ///< ClipNode: hulls 1 (standing) and 3 (crouched)
        Textures,   ///< TextureRef
        Entities,   ///< EntityRecord
        EntityText, ///< u8: the original entity lump, for the converter's own use
        LightPatch, ///< reserved for switchable light styles; still empty
        Brushes,    ///< Brush: the map's brush entities (doors, grates, glass...)
        BrushLeaves, ///< u16: the world leaves each brush touches, grouped per brush
        Logic,      ///< LogicRecord: the entities that do something
        LogicAux,   ///< LogicAux: lists hanging off logic records
        PathCorners, ///< PathCorner: the stops of every train's route
        NameHashes, ///< u32, indexed by name id: nameHash() of each name; [0] is 0
        Landmarks,  ///< Landmark: the points two neighbouring maps have in common
        Strings,    ///< u8: NUL-terminated strings records point into (map names)
        ModelRefs,  ///< ModelRef: the models the map's actors are made of
        Actors,     ///< ActorRecord: the people and props that are models
        PathNodes,  ///< PathNode: where something that walks can stand (info_node)
        PathNext,   ///< u8, nodes * nodes: [from * n + to] = the node to go to next, 255 = no way
        Sounds,     ///< SoundRef: the sounds the map plays (HlSound.hpp); the common ones first
        LogicSounds, ///< LogicSound, parallel to Logic: what each record sounds like
        Sentences,  ///< SentenceRecord: the spoken lines the map has
        SentenceWords, ///< u16: indices into Sounds, grouped per sentence
        Rooms,      ///< RoomRecord: what the map's rooms sound like (env_sound)
        Count
    };
    inline constexpr int kSectionCount = static_cast<int>(Section::Count);

    struct SectionDir
    {
        std::uint32_t offset; ///< from the start of the file, a multiple of 4
        std::uint32_t size;   ///< in bytes, before padding
        std::uint32_t count;  ///< records
    };

    struct LevelHeader
    {
        char magic[4];
        std::uint16_t version;
        std::uint16_t sectionCount;
        std::uint32_t fileSize;
        std::uint32_t crc32; ///< of every byte after this header
        std::int16_t mins[3]; ///< world bounds, whole units
        std::int16_t maxs[3];
        std::uint16_t visLeafCount; ///< leaves 1..n have a visibility row
        /// The most the PVS of any one leaf lists, before the runtime's own
        /// frustum and distance culling. What the two-pass choice is made from.
        std::uint16_t worstPolygons;
        std::uint16_t worstVertices;
        /// The map's sky: one more than the index in Textures of the first
        /// of its six sides (in the order right, back, left, front, up,
        /// down); 0 for a map with no sky.
        std::uint16_t skyTexture;
        /// What the player holds: one more than the index in ModelRefs of the
        /// first-person model drawn in front of the view; 0 for none.
        std::uint16_t viewModel;
        std::uint16_t reserved1;
        std::uint32_t reserved[6];
    };

    struct Plane
    {
        std::int32_t normal[3]; ///< Q30
        fx dist;                ///< Q12
    };

    inline constexpr std::uint8_t kPlaneAxisX = 0;
    inline constexpr std::uint8_t kPlaneAxisY = 1;
    inline constexpr std::uint8_t kPlaneAxisZ = 2;
    inline constexpr std::uint8_t kPlaneOther = 3;

    struct Node
    {
        std::uint16_t plane;
        /// [0] in front of the plane, [1] behind. Negative: -(leaf + 1).
        std::int16_t children[2];
        std::int16_t mins[3]; ///< whole units
        std::int16_t maxs[3];
        std::uint16_t parent; ///< kNoParent at the root
    };
    inline constexpr std::uint16_t kNoParent = 0xFFFF;

    struct Leaf
    {
        std::int32_t visOfs; ///< into Vis; -1 = sees everything
        std::int16_t mins[3];
        std::int16_t maxs[3];
        std::uint16_t firstMark;
        std::uint16_t markCount;
        std::uint16_t parent; ///< the node this leaf hangs from
        std::int8_t contents; ///< kContents*
        std::uint8_t flags;   ///< kLeaf*
    };
    /// Sky can be seen from this leaf: the sky box is drawn only then.
    inline constexpr std::uint8_t kLeafSeesSky = 1;

    inline constexpr std::int8_t kContentsEmpty = -1;
    inline constexpr std::int8_t kContentsSolid = -2;
    inline constexpr std::int8_t kContentsWater = -3;
    inline constexpr std::int8_t kContentsSlime = -4;
    inline constexpr std::int8_t kContentsLava = -5;
    inline constexpr std::int8_t kContentsSky = -6;

    /// One drawable piece of the map: a ready-made run of geometry-engine
    /// commands and what the renderer needs to decide whether to send it.
    ///
    /// A surface is a GROUP of the map's faces -- neighbours on one plane with
    /// one texture -- so that vertices they share are sent once (the
    /// converter's MeshOptimize). One plane means one back-face test decides
    /// for all of it.
    struct Surface
    {
        std::uint32_t gxOfs;    ///< first word in GxStream
        std::uint16_t gxWords;
        /// Plane index in the low 15 bits; bit 15 set when the face looks down
        /// the plane's back, i.e. is visible from BEHIND it.
        std::uint16_t planeRef;
        std::int16_t center[3]; ///< whole units
        std::uint16_t radius;   ///< whole units, rounded up
        std::uint16_t texture;  ///< into Textures, or kNoTexture
        std::uint16_t polygons; ///< polygon RAM this surface uses
        std::uint16_t vertices; ///< vertex RAM this surface uses
        std::uint8_t flags;
        std::uint8_t pad;
    };
    /// A surface drawn in a flat colour (a brush with render mode "colour").
    inline constexpr std::uint16_t kNoTexture = 0xFFFF;
    inline constexpr std::uint16_t kSurfacePlaneBack = 0x8000;
    inline constexpr std::uint16_t kSurfacePlaneMask = 0x7FFF;

    struct Model
    {
        std::int16_t mins[3]; ///< whole units
        std::int16_t maxs[3];
        fx origin[3];
        /// Root of the render tree: a node, or -(leaf + 1) for a model with no
        /// tree of its own.
        std::int32_t headNode;
        /// Roots in ClipNodes; negative when the whole hull is one contents value.
        std::int32_t clipStanding;
        std::int32_t clipCrouched;
        std::uint16_t firstSurface;
        std::uint16_t surfaceCount;
    };

    struct ClipNode
    {
        std::uint16_t plane;
        /// [0] in front, [1] behind. Negative: a kContents* value.
        std::int16_t children[2];
        std::uint16_t pad;
    };

    struct TextureRef
    {
        char name[16];        ///< lower-case, NUL padded; the file is tex/<name>.htx
        std::uint16_t width;  ///< as cooked: a power of two
        std::uint16_t height;
        std::uint16_t sourceWidth; ///< as the game had it
        std::uint16_t sourceHeight;
        std::uint32_t flags; ///< kTexture* (HlTexture.hpp)
    };

    /// How a brush entity is drawn. From the entity's "rendermode" and
    /// "renderamt", by the rules the PC engine applies.
    enum class RenderClass : std::uint8_t
    {
        Opaque, ///< like the world
        /// "Solid" mode: '{' textures have real holes. Those textures are
        /// A3I5; the polygons are sent at full alpha.
        Cutout,
        /// "Texture", "glow" and "additive" modes: the whole brush is
        /// see-through by `alpha`. Glass.
        Blend,
        Colour, ///< "colour" mode: no texture, a flat colour at `alpha`
        /// Not drawn at all -- a blended mode with amount 0, or a class that
        /// is never visible (triggers, ladders). It can still be solid.
        Hidden,
    };

    inline constexpr std::uint8_t kBrushSolid = 1;   ///< the player collides with it
    inline constexpr std::uint8_t kBrushHasPivot = 2; ///< built round its origin: it can turn
    inline constexpr std::uint8_t kBrushStartsOff = 4; ///< absent until something switches it on
    /// It turns about something other than the vertical, so the box that
    /// holds it at any angle is a cube round its pivot, not a square column.
    inline constexpr std::uint8_t kBrushTumbles = 8;
    /// It travels far from where it was built (a tram): which leaves it can
    /// be seen from is not known ahead of time, so it is tested against the
    /// view alone.
    inline constexpr std::uint8_t kBrushRoams = 16;

    /// One brush entity: a piece of the map that is its own object -- a door,
    /// a grate, a pane of glass, an invisible wall.
    ///
    /// Its geometry is a Model (surfaces and clip hulls) in the model's own
    /// space; `origin` is where that space sits in the world. For most brushes
    /// that is zero and the model is already in world coordinates; a brush
    /// made to turn is built round its pivot and placed by its origin.
    struct Brush
    {
        std::uint16_t model;       ///< into Models; never 0
        std::uint8_t renderClass;  ///< RenderClass
        std::uint8_t alpha;        ///< 1..31: the polygon alpha it is drawn with
        std::uint8_t flags;        ///< kBrush*
        /// For a body of liquid (func_water): what it is full of, as a
        /// kContents* value stored unsigned; 0 for every other brush.
        std::uint8_t kind;
        std::uint16_t colour;      ///< RGB555, for RenderClass::Colour
        fx origin[3];
        std::int16_t mins[3];      ///< world bounds at rest, whole units
        std::int16_t maxs[3];
        /// The world leaves its bounds touch, in BrushLeaves: it is drawn when
        /// any of them is visible.
        std::uint16_t firstLeaf;
        std::uint16_t leafCount;
        std::uint16_t logic;       ///< its LogicRecord, or kNoLogic if it only stands there
        std::uint16_t pad;
    };
    inline constexpr std::uint16_t kNoLogic = 0xFFFF;

    // The three angles of a brush, in the order the game's maps give them.
    inline constexpr int kPitch = 0; ///< about the brush's Y: positive tips its front down
    inline constexpr int kYaw = 1;   ///< about the vertical: positive turns left
    inline constexpr int kRoll = 2;  ///< about the brush's X

    /// What a logic record is. The set is deliberately small: these are the
    /// entities that make the first chapters' doors open and lifts run.
    enum class LogicKind : std::uint8_t
    {
        /// Does nothing when used. It exists so that it can be NAMED: removed
        /// by a killtarget, or switched by something that only needs it to be
        /// there.
        Static,
        Door,
        Button,
        /// Something that breaks: it vanishes and fires its target. extra[0]
        /// is how much damage it takes, extra[1] what it is made of (7 is the
        /// glass that never breaks). It breaks when used by name; when stood
        /// on, if kLogicPressure; when hit, unless kLogicTriggerOnly.
        Breakable,
        WallToggle,     ///< used: appears / disappears
        TriggerOnce,    ///< a volume: fires when the player enters, once
        TriggerMultiple, ///< ... and again after `waitMs`
        TriggerAuto,    ///< fires by itself when the map starts
        TriggerRelay,   ///< used: fires its target with its own use type
        MultiManager,   ///< used: fires a list of targets, each after its own delay
        MultiSource,    ///< an AND gate: fires when every entity targeting it is on
        /// A brush that travels from path corner to path corner. Lifts are
        /// these as often as they are doors.
        Train,
        /// Not something that acts: a volume (mins/maxs) inside which the
        /// player climbs. It is a logic record so that it can be removed by
        /// name like anything else.
        Ladder,
        /// A door that turns about an axis through its origin: extra[0] says
        /// which angle it is (0 pitch, 1 yaw -- the ordinary hinge -- 2 roll).
        DoorRotating,
        /// The way into the next map: a volume (mins/maxs) that works by being
        /// walked into (kLogicTouch) or by being fired by name. extra[0] is
        /// the map's name in Strings, extra[1] the landmark's hash, extra[2]
        /// the hash of a name to fire in the new map and extra[3] how many
        /// milliseconds after arriving.
        ChangeLevel,
        /// Sets a state that outlives the map (extra[0]: its hash). extra[1]
        /// is what using it does: 0 off, 1 on, 2 dead, 3 toggle. With
        /// kLogicStartOpen it sets extra[2] (0 off, 1 on, 2 dead) when the
        /// map starts, if the state is not known yet.
        EnvGlobal,
        /// A brush that spins: a fan. extra[0] is the angle it turns (as for
        /// DoorRotating), `speed` its full speed in turns * 65536 a second
        /// (negative: the other way), extra[1] how much speed it gains or
        /// loses every tenth of a second while spinning up or down.
        /// kLogicStartOpen: spinning from the start. kLogicToggle: it spins UP
        /// gradually too (it always spins down gradually).
        Rotating,
        /// A train that follows a track of path_tracks and turns to face along
        /// it: the tram. Its brush is placed by its origin, `height`
        /// (extra[1], Q12) above the track. `speed` is its speed when
        /// started, extra[3] the speed it starts the map at (Q12), extra[0]
        /// the distance between its "wheels" (Q12): how far ahead it looks to
        /// know which way to face. extra[2] is the hash of its global name,
        /// by which the same train is found in the next map; firstAux is the
        /// track node it starts at. kLogicSelfDriven: track nodes set its
        /// speed.
        TrackTrain,
        /// A platform that carries a train from the end of one track down to
        /// the start of another. `move` is its travel, extra[0] how far it
        /// turns on the way (turns * 65536, signed), extra[1] the node the
        /// upper track ends at, extra[2] the node the lower one starts at,
        /// extra[3] the name id of its train.
        TrackChange,
        /// A box the player shoves about. It is a Breakable as well if
        /// kLogicBreaks. extra[2] is the fastest it can be pushed (Q12 units
        /// a second), extra[3] which of the player's two hulls it moves
        /// through the world as (0 standing, 1 crouched).
        Pushable,
        /// A volume (mins/maxs) that hurts whoever is in it: extra[0] is the
        /// damage a second (signed: a negative one heals). Used, it switches
        /// on and off; kLogicStartOpen is "starts off".
        TriggerHurt,
        /// A volume that pushes whoever is in it along `move` (Q12 units a
        /// second).
        TriggerPush,
        /// A volume that puts whoever enters it at `move`, facing extra[0]
        /// (turns * 65536).
        TriggerTeleport,
        /// A volume that saves the game the first time the player enters it.
        AutoSave,
        /// A scripted sequence: takes an actor, has it walk to a mark and act
        /// something out, and fires its target when that is done. `move` is
        /// the mark and `speed` the way to face there (turns * 65536).
        /// extra[0] is the actor's name id, or 0 to take the nearest actor of
        /// kind `damage - 1` within `waitMs` units; extra[1] the clip to hold
        /// until triggered and extra[2] the clip to act out (nameHashes, 0 =
        /// none); extra[3] how it gets to the mark (kScriptMove*).
        /// kLogicStartOpen: it starts by itself.
        Script,
        /// A monstermaker: puts one of its actors (the dormant ones from
        /// extra[0], extra[1] of them) where it is. extra[2] is how many it
        /// may make in all (0: no end to them), `waitMs` the time between
        /// two while it is on (kLogicStartOpen: on from the start;
        /// kLogicToggle: one each time it is used instead). Its target is
        /// fired for each one made.
        Maker,
        /// A wall unit that gives health (or, the other, armour) while USE
        /// is held on it: extra[0] is how much it holds.
        HealthCharger,
        SuitCharger,
        /// Something heard at a place (`move`): extra[0] is the sound (into
        /// Sounds), or a sentence if extra[3] is 1; extra[1] how far it
        /// carries in units (0: everywhere); extra[2] how loud, 0..127.
        /// kLogicToggle: it loops, and using it starts and stops it
        /// (kLogicStartOpen: it starts the map playing). Otherwise each use
        /// plays it once. Its target, if it has one, is fired when it is
        /// used (a scripted_sentence).
        Ambient,
        /// Starts a track of the game's music: extra[0] is its number. A
        /// volume (mins/maxs) walked into once (kLogicTouch), or used.
        Music,
        Count
    };

    // What a "use" asks for.
    inline constexpr std::uint8_t kUseOff = 0;
    inline constexpr std::uint8_t kUseOn = 1;
    inline constexpr std::uint8_t kUseSet = 2;
    inline constexpr std::uint8_t kUseToggle = 3;

    // LogicRecord::flags. The map's own spawnflags mean different things per
    // class; the converter turns them into these.
    inline constexpr std::uint16_t kLogicStartOpen = 1; ///< a door that begins open
    /// A door that stays where it is sent until used again; a button that
    /// stays in until pressed again.
    inline constexpr std::uint16_t kLogicToggle = 2;
    inline constexpr std::uint16_t kLogicUseOnly = 4;   ///< a door the player opens with USE
    inline constexpr std::uint16_t kLogicTouch = 8;     ///< works by being walked into
    inline constexpr std::uint16_t kLogicFireOnce = 16; ///< a relay that removes itself
    inline constexpr std::uint16_t kLogicThread = 32;   ///< a multi_manager that can overlap itself
    /// A rotating door that always opens the same way. Without it, it opens
    /// away from whoever opens it.
    inline constexpr std::uint16_t kLogicOneWay = 64;
    /// A track train nobody drives: the track's nodes set its speed.
    inline constexpr std::uint16_t kLogicSelfDriven = 128;
    // A Breakable's:
    inline constexpr std::uint16_t kLogicTriggerOnly = 256; ///< nothing but its name breaks it
    inline constexpr std::uint16_t kLogicPressure = 512;    ///< standing on it breaks it
    inline constexpr std::uint16_t kLogicInstant = 1024;    ///< one blow of the crowbar does
    /// A Pushable that is breakable too.
    inline constexpr std::uint16_t kLogicBreaks = 2048;
    /// A trigger volume that a pushed box sets off as the player does...
    inline constexpr std::uint16_t kLogicPushables = 4096;
    /// ... and one that the player does not.
    inline constexpr std::uint16_t kLogicNoPlayer = 8192;

    /// One entity that does something, with everything it needs already worked
    /// out by the converter: names are numbers, times are milliseconds, a
    /// door's travel is a vector. Nothing here is parsed at run time.
    struct LogicRecord
    {
        std::uint8_t kind;    ///< LogicKind
        std::uint8_t useType; ///< kUse*: what a relay or trigger_auto sends
        std::uint16_t flags;  ///< kLogic*
        // Names, as ids; 0 = none. Entities with the same name share an id.
        std::uint16_t name;       ///< what it is called: how others target it
        std::uint16_t target;     ///< whom it fires
        std::uint16_t killTarget; ///< whom it removes when it fires
        std::uint16_t master;     ///< the multisource that must be on for it to work
        std::uint16_t netName;    ///< a door's second target, fired when it shuts
        std::uint16_t brush;      ///< into Brushes, or kNoBrush
        /// Into LogicAux -- except for a train, where it is the path corner
        /// the train starts at (into PathCorners) or kNoCorner.
        std::uint16_t firstAux;
        std::uint16_t auxCount;
        std::int32_t waitMs;      ///< before returning / re-arming; -1 = never
        std::uint32_t delayMs;    ///< between being triggered and firing its targets
        fx speed;                 ///< units a second
        /// A door or button's whole travel. For a train: the middle of its
        /// brush, which is the point that visits each corner. For a rotating
        /// door: move[0] is how far it turns, in turns * 65536 (signed), and
        /// `speed` is in the same unit per second.
        fx move[3];
        std::int16_t mins[3];     ///< a trigger's volume, world, whole units
        std::int16_t maxs[3];
        std::uint16_t damage;     ///< what a blocked door does to what blocks it
        std::uint16_t pad;
        /// For a trigger_auto or a multisource: the hash of a global state
        /// that has to be ON for it to work; 0 = none.
        std::uint32_t globalState;
        std::uint32_t extra[4];   ///< what its kind says (LogicKind)
    };
    inline constexpr std::uint16_t kNoBrush = 0xFFFF;

    /// An entry of a list a logic record owns. For a multi_manager: one target
    /// and how long after the manager is used it fires. For a multisource: one
    /// of its inputs, as a record index in `target`.
    struct LogicAux
    {
        std::uint16_t target;
        std::uint16_t pad;
        std::uint32_t delayMs;
    };

    /// One stop on a train's route.
    struct PathCorner
    {
        fx pos[3];
        fx speed;             ///< the speed to LEAVE this corner at; 0 = as before
        std::int32_t waitMs;  ///< how long the train stops here
        std::uint16_t next;   ///< into PathCorners, or kNoCorner where the route ends
        std::uint16_t fireTarget; ///< a name fired each time a train arrives; 0 = none
        std::uint16_t flags;  ///< kCorner*
        /// On a track: a name fired when a train comes to rest here because
        /// the track goes no further; 0 = none.
        std::uint16_t netName;
    };
    inline constexpr std::uint16_t kNoCorner = 0xFFFF;
    /// The train stops here until it is used again.
    inline constexpr std::uint16_t kCornerWaitForUse = 1;
    /// The train does not travel to the next corner: it is simply there.
    inline constexpr std::uint16_t kCornerTeleport = 2;
    // On a track (path_track):
    inline constexpr std::uint16_t kCornerDisabled = 4;     ///< the track ends before it
    inline constexpr std::uint16_t kCornerFireOnce = 8;     ///< its fireTarget fires once only
    inline constexpr std::uint16_t kCornerDisableTrain = 16; ///< the train stops being drivable

    /// A point that is the same place in two maps: info_landmark. The player
    /// leaves one map so far from it and arrives in the next as far from the
    /// landmark of the same name there.
    struct Landmark
    {
        std::uint32_t hash; ///< nameHash() of its name
        fx origin[3];
    };

    /// How a name is known ACROSS maps, where name ids (which are per map)
    /// mean nothing: 32-bit FNV-1a of its lower-cased text; never 0.
    constexpr std::uint32_t nameHash(const char *text)
    {
        std::uint32_t hash = 2166136261u;
        for (; *text != '\0'; ++text)
        {
            char c = *text;
            if (c >= 'A' && c <= 'Z')
                c = static_cast<char>(c - 'A' + 'a');
            hash = (hash ^ static_cast<std::uint8_t>(c)) * 16777619u;
        }
        return hash == 0 ? 1 : hash;
    }

    // How a scripted actor gets to its mark (the map's "m_fMoveTo").
    inline constexpr std::uint32_t kScriptMoveNone = 0;    ///< acts where it stands
    inline constexpr std::uint32_t kScriptMoveWalk = 1;
    inline constexpr std::uint32_t kScriptMoveRun = 2;
    inline constexpr std::uint32_t kScriptMoveInstant = 4; ///< put on the mark at once
    inline constexpr std::uint32_t kScriptMoveTurn = 5;    ///< turns to face, no more

    /// A model a map uses: mdl/<map>/<name>.hmd, with the clips THIS map
    /// plays, and its skin mdl/<name>.htx.
    struct ModelRef
    {
        char name[24]; ///< lower-case, NUL padded
    };

    /// What an actor is: how it behaves when nothing is directing it.
    enum class ActorKind : std::uint8_t
    {
        Scientist,
        Guard,
        Gman,
        Generic,   ///< monster_generic: any model, stands and idles
        Furniture, ///< monster_furniture: stands still until a script moves it
        Cycler,    ///< plays its first sequence over and over
        Pose,      ///< dead: one frame, held
        // What fights (HlMonsters.hpp).
        Headcrab,
        Zombie,
        Barnacle,
        /// Something to pick up, lying where it is: ActorRecord::item says
        /// what (ItemKind).
        Item,
        Slave,     ///< the alien slave: zaps from a distance, claws up close
        Bullsquid, ///< spits from a distance, bites and whips up close
        Houndeye,  ///< runs up and lets off a blast
        /// The scientist at a desk: sits where the map has him, going from
        /// one of his five ways of sitting to another.
        Sitter,
        Count
    };

    inline constexpr std::uint8_t kActorSolid = 1; ///< the player cannot walk through it
    /// Not there when the map starts: a place kept for what a monstermaker
    /// makes.
    inline constexpr std::uint8_t kActorDormant = 2;

    enum class ItemKind : std::uint8_t
    {
        None,
        Crowbar,
        Glock,
        Shotgun,
        GlockClip,
        Buckshot,
        HealthKit,
        Battery,
        Suit, ///< the HEV suit: armour is for someone wearing it
        Grenades, ///< hand grenades: five of them
        Count
    };

    /// A sound the map plays: the file it is in under snd/ on the card,
    /// without the extension ("ambience_labdrone2.11k"). Empty: the map has
    /// no use for the sound of this index.
    struct SoundRef
    {
        char name[48];
    };
    /// What a logic record sounds like: indices into Sounds, 0xFFFF for
    /// none. A door's or a button's moving and stopping; what something that
    /// breaks sounds like breaking (in `move`).
    struct LogicSound
    {
        std::uint16_t move;
        std::uint16_t stop;
    };
    /// A spoken line: `wordCount` sounds from SentenceWords, one after
    /// another.
    struct SentenceRecord
    {
        std::uint16_t firstWord;
        std::uint8_t wordCount;
        std::uint8_t volume; ///< 0..127
    };

    /// A place with a sound of its own: within `radius` units of `origin`,
    /// what is heard echoes as a room of the game's type `type` does
    /// (HlSound.hpp, roomEcho).
    struct RoomRecord
    {
        fx origin[3];
        std::uint16_t radius;
        std::uint8_t type;
        std::uint8_t pad;
    };

    /// Where something that walks can stand, as the map's author marked it:
    /// its feet, in the world.
    struct PathNode
    {
        fx pos[3];
    };
    inline constexpr std::uint8_t kNoPathNode = 255;
    inline constexpr int kMaxPathNodes = 254;

    /// A person or a prop that is a model: where it starts and as what.
    struct ActorRecord
    {
        std::uint16_t model;  ///< into ModelRefs
        std::uint16_t name;   ///< name id; 0 = none
        fx origin[3];         ///< its feet
        std::uint16_t yaw;    ///< turns
        std::uint8_t body;
        std::uint8_t kind;    ///< ActorKind
        std::uint32_t clip;   ///< nameHash of the clip it starts in; 0 = the model's first
        std::uint8_t flags;   ///< kActor*
        std::uint8_t light;   ///< how bright it stands, 0..31
        std::uint16_t target; ///< name id of what an item fires when it is taken; 0 = nothing
        std::uint32_t item;   ///< ItemKind, for an ActorKind::Item
    };

    enum class EntityClass : std::uint16_t
    {
        Worldspawn,
        InfoPlayerStart,
    };

    struct EntityRecord
    {
        std::uint16_t classId; ///< EntityClass
        std::uint16_t flags;
        fx origin[3];
        std::uint16_t angles[3]; ///< pitch, yaw, roll in turns
        std::uint16_t model;     ///< into Models; 0 = none
        std::uint32_t reserved[2];
    };

    static_assert(sizeof(SectionDir) == 12);
    static_assert(sizeof(LevelHeader) == 64);
    static_assert(sizeof(Plane) == 16);
    static_assert(sizeof(Node) == 20);
    static_assert(sizeof(Leaf) == 24);
    static_assert(sizeof(Surface) == 24);
    static_assert(sizeof(Model) == 40);
    static_assert(sizeof(ClipNode) == 8);
    static_assert(sizeof(TextureRef) == 28);
    static_assert(sizeof(EntityRecord) == 32);
    static_assert(sizeof(Brush) == 40);
    static_assert(sizeof(LogicRecord) == 80);
    static_assert(sizeof(Landmark) == 16);
    static_assert(sizeof(ModelRef) == 24);
    static_assert(sizeof(ActorRecord) == 32);
    static_assert(sizeof(LogicAux) == 8);
    static_assert(sizeof(PathCorner) == 28);

    /// The size of one record of a section, which is how both the writer and
    /// the reader know that `size == count * recordSize`.
    constexpr std::uint32_t sectionRecordSize(Section section)
    {
        switch (section)
        {
        case Section::Planes:     return sizeof(Plane);
        case Section::PlaneTypes: return 1;
        case Section::Nodes:      return sizeof(Node);
        case Section::Leaves:     return sizeof(Leaf);
        case Section::Marks:      return 2;
        case Section::Vis:        return 1;
        case Section::Surfaces:   return sizeof(Surface);
        case Section::GxStream:   return 4;
        case Section::Models:     return sizeof(Model);
        case Section::ClipNodes:  return sizeof(ClipNode);
        case Section::Textures:   return sizeof(TextureRef);
        case Section::Entities:   return sizeof(EntityRecord);
        case Section::EntityText: return 1;
        case Section::LightPatch: return 1;
        case Section::Brushes:    return sizeof(Brush);
        case Section::BrushLeaves: return 2;
        case Section::Logic:      return sizeof(LogicRecord);
        case Section::LogicAux:   return sizeof(LogicAux);
        case Section::PathCorners: return sizeof(PathCorner);
        case Section::NameHashes: return 4;
        case Section::Landmarks:  return sizeof(Landmark);
        case Section::Strings:    return 1;
        case Section::ModelRefs:  return sizeof(ModelRef);
        case Section::Actors:     return sizeof(ActorRecord);
        case Section::PathNodes:  return sizeof(PathNode);
        case Section::PathNext:   return 1;
        case Section::Sounds:     return sizeof(SoundRef);
        case Section::LogicSounds: return sizeof(LogicSound);
        case Section::Sentences:  return sizeof(SentenceRecord);
        case Section::SentenceWords: return 2;
        case Section::Rooms:      return sizeof(RoomRecord);
        default:                  return 0;
        }
    }

    inline constexpr std::uint32_t kLevelDataStart =
        sizeof(LevelHeader) + kSectionCount * sizeof(SectionDir);

    /// CRC-32 (the zlib polynomial), bit by bit: a level is checked once per
    /// load, and a table would cost a kilobyte of the DS's memory for that.
    constexpr std::uint32_t crc32(const std::uint8_t *data, std::uint32_t size)
    {
        std::uint32_t crc = 0xFFFFFFFFu;
        for (std::uint32_t i = 0; i < size; ++i)
        {
            crc ^= data[i];
            for (int bit = 0; bit < 8; ++bit)
            {
                crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
            }
        }
        return ~crc;
    }
} // namespace hldata
