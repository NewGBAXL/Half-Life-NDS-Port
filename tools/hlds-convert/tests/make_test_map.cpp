// hlds-testmap: writes a small synthetic map for looking at things on the
// console.
//
// Not a test and not part of a conversion. It exists because the features
// worth looking at -- a grate, a pane of glass, a door, a lift -- are
// scattered through the real maps, and getting the camera in front of one
// means playing there. This puts them all within a few steps of the player
// start, in a map built from nothing (TestBspBuilder.hpp), so it can be cooked
// and booted in seconds and holds no game data at all.
//
// The player starts at the origin facing +y. Ahead is a wall with a door in
// it that opens when walked into; behind the door, a row of brushes, one of
// each drawing class. To the left (-x) is a button: USE it and the lift to the
// right (+x) goes up.
//
//   hlds-testmap <folder>      writes <folder>/maps/testroom.bsp
//   hlds-convert cook-map <folder> testroom -o <card> --start testroom

#include <cstdio>
#include <filesystem>
#include <fstream>

#include "TestBspBuilder.hpp"

using namespace hldstest;

namespace
{
    BrushBox pillar(float x, const char *cls, const char *keys, const char *texture)
    {
        BrushBox b;
        b.className = cls;
        b.keys = keys;
        b.textureName = texture;
        // A row across the room at y = 72..80, each 32 wide and 64 tall. The
        // player starts at the origin facing +y, looking straight at them.
        b.mins[0] = x;
        b.maxs[0] = x + 32;
        b.mins[1] = 72;
        b.maxs[1] = 80;
        b.mins[2] = 0;
        b.maxs[2] = 64;
        return b;
    }
} // namespace

int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        std::fprintf(stderr, "usage: hlds-testmap <folder>\n");
        return 2;
    }
    BoxRoom room;
    room.brushes = {
        // Left to right as the player sees them (facing +y, +x is to the right).
        pillar(-104, "func_wall", "", ""),                                   // plain wall
        pillar(-64, "func_wall", "\"rendermode\" \"4\"\n\"renderamt\" \"255\"\n", "{grate"), // grate
        pillar(-24, "func_breakable", "\"rendermode\" \"2\"\n\"renderamt\" \"96\"\n", "glass"),
        pillar(16, "func_wall",
               "\"rendermode\" \"1\"\n\"renderamt\" \"128\"\n\"rendercolor\" \"255 0 0\"\n", ""),
        pillar(56, "func_wall", "", "{grate"), // the same texture, render mode 0: no holes
    };

    auto box = [&](const char *cls, const char *keys, float x0, float y0, float z0, float x1,
                   float y1, float z1)
    {
        BrushBox b;
        b.className = cls;
        b.keys = keys;
        b.mins[0] = x0;
        b.mins[1] = y0;
        b.mins[2] = z0;
        b.maxs[0] = x1;
        b.maxs[1] = y1;
        b.maxs[2] = z1;
        room.brushes.push_back(b);
    };
    // The wall across the room at y = 40..48, in two halves with a doorway
    // between them. The halves are INVISIBLE walls: solid, never drawn.
    box("func_wall", "\"rendermode\" \"2\"\n\"renderamt\" \"0\"\n", -128, 40, 0, -32, 48, 128);
    box("func_wall", "\"rendermode\" \"2\"\n\"renderamt\" \"0\"\n", 32, 40, 0, 128, 48, 128);
    // The door: no name, so walking into it opens it; back down after 3 s.
    box("func_door", "\"angle\" \"-1\"\n\"speed\" \"80\"\n\"wait\" \"3\"\n\"lip\" \"8\"\n", -32, 40, 0, 32,
        48, 100);
    // The button, on a post to the player's left, and the lift it starts.
    box("func_button", "\"target\" \"lift\"\n\"spawnflags\" \"1\"\n\"wait\" \"2\"\n", -44, -8, 52, -40, 8,
        68);
    box("func_wall", "", -44, -2, 0, -40, 2, 52);
    box("func_door",
        "\"targetname\" \"lift\"\n\"angle\" \"-1\"\n\"speed\" \"40\"\n\"wait\" \"2\"\n\"lip\" \"-60\"\n", 48,
        -32, 0, 112, 32, 8);

    const std::filesystem::path dir = std::filesystem::path(argv[1]) / "maps";
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    const std::vector<std::uint8_t> bytes = buildBoxRoom(room);
    std::ofstream out(dir / "testroom.bsp", std::ios::binary);
    out.write(reinterpret_cast<const char *>(bytes.data()),
              static_cast<std::streamsize>(bytes.size()));
    if (!out)
    {
        std::fprintf(stderr, "cannot write %s\n", (dir / "testroom.bsp").string().c_str());
        return 1;
    }
    std::printf("%s\n", (dir / "testroom.bsp").string().c_str());
    return 0;
}
