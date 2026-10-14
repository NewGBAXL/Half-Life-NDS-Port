// The saved game's file format (shared/HlSave.hpp): what is written is read
// back the same, and nothing that is not a whole save of this version is
// taken for one.

#include <cstdio>
#include <cstring>

#include "HlSave.hpp"
#include "TestBspBuilder.hpp"

using namespace hldata;
using namespace hldstest;

int main()
{
    SaveData save;
    save.map = "c1a0";
    save.origin[0] = fxFromInt(44), save.origin[1] = fxFromInt(174), save.origin[2] = fxFromInt(-176);
    save.velocity[1] = -1234;
    save.yaw = 49152;
    save.pitch = -2000;
    save.ducked = 1;
    save.health = 63;
    save.clipMp5 = 31;
    save.clipPython = 4;
    save.ammo357 = 12;
    save.ammoAr = 3;
    save.globalHash = {nameHash("power"), nameHash("gate")};
    save.globalState = {1, 2};
    save.train = nameHash("intro_train");
    save.trainPos[2] = fxFromInt(-254);
    save.trainYaw = 16384;
    save.trainSpeed = fxFromInt(300);
    save.trainSelfDriven = 1;
    SaveMapState visited;
    visited.map = nameHash("c1a0");
    visited.broken = {0x05, 0x00, 0x80};
    save.maps.push_back(visited);

    const std::vector<std::uint8_t> bytes = writeSave(save);
    SaveData back;
    check(readSave(bytes.data(), bytes.size(), back), "a save reads back");
    check(back.map == "c1a0" && back.origin[2] == fxFromInt(-176) && back.velocity[1] == -1234 &&
              back.yaw == 49152 && back.pitch == -2000 && back.ducked == 1 && back.health == 63,
          "the player as they were");
    check(back.clipMp5 == 31 && back.clipPython == 4 && back.ammo357 == 12 && back.ammoAr == 3,
          "what is in the newer guns, and carried for them");
    check(back.globalHash == save.globalHash && back.globalState == save.globalState,
          "the switches");
    check(back.train == save.train && back.trainPos[2] == fxFromInt(-254) &&
              back.trainYaw == 16384 && back.trainSpeed == fxFromInt(300) &&
              back.trainSelfDriven == 1,
          "the train they rode");
    check(back.maps.size() == 1 && back.maps[0].map == nameHash("c1a0") &&
              back.maps[0].broken == visited.broken,
          "and what was broken");

    // Anything else is refused, and leaves what it was read into alone.
    SaveData untouched;
    untouched.map = "sentinel";
    std::vector<std::uint8_t> bad = bytes;
    bad[bad.size() - 3] ^= 0x10;
    check(!readSave(bad.data(), bad.size(), untouched) && untouched.map == "sentinel",
          "a damaged save is not read");
    bad = bytes;
    bad.pop_back();
    check(!readSave(bad.data(), bad.size(), untouched), "nor a truncated one");

    // Version and the maps it follows. SaveHeader: version at 4, levelVersion at 6.
    bad = bytes;
    bad[4] = static_cast<std::uint8_t>(kSaveVersion + 1);
    bad[5] = 0;
    check(!readSave(bad.data(), bad.size(), untouched), "nor one of another version");
    bad = bytes;
    bad[6] = static_cast<std::uint8_t>(kLevelSaveSince - 1); // made against maps older than the saves still follow
    bad[7] = 0;
    check(!readSave(bad.data(), bad.size(), untouched), "nor one of maps whose layout it no longer follows");
    bad = bytes;
    bad[6] = static_cast<std::uint8_t>(kLevelSaveSince); // the oldest maps they still follow
    bad[7] = 0;
    {
        SaveData older;
        check(readSave(bad.data(), bad.size(), older), "but one of the oldest maps it still follows is read");
    }
    bad = bytes;
    bad[6] = static_cast<std::uint8_t>(kLevelVersion + 1);
    bad[7] = 0;
    check(!readSave(bad.data(), bad.size(), untouched), "nor one of maps newer than this build");
    check(!readSave(bytes.data(), 8, untouched), "nor a file too short to be one");

    // A save whose map name is not a plain name is a save that would open
    // some other file.
    SaveData sly = save;
    sly.map = "../../boot";
    const std::vector<std::uint8_t> slyBytes = writeSave(sly);
    check(!readSave(slyBytes.data(), slyBytes.size(), untouched) && untouched.map == "sentinel",
          "nor one that names something other than a map");

    // Dying: back to the latest save -- unless that is itself the trouble.
    {
        DeathLoop loop;
        check(deathChoice(loop, 300000) == kBackLatest && deathChoice(loop, 5000) == kBackLatest &&
                  deathChoice(loop, 5000) == kBackLatest,
              "a death goes back to the latest save, and so does dying soon after, twice");
        check(deathChoice(loop, 5000) == kBackArrival && deathChoice(loop, 3000) == kBackArrival,
              "a third death on end, each soon after going back, goes back to coming into the map instead");
        check(deathChoice(loop, 3000) == kBackArrival && deathChoice(loop, 3000) == kBackStart,
              "... and if that is no better, to the map's own start");
        check(deathChoice(loop, 3000) == kBackLatest, "after which it is counted afresh");
        DeathLoop calm;
        deathChoice(calm, 4000);
        deathChoice(calm, 4000);
        check(deathChoice(calm, 60000) == kBackLatest && deathChoice(calm, 4000) == kBackLatest,
              "a death after a good while alive is not one of a run");
    }

    if (g_failures == 0)
        std::printf("test_save: ok\n");
    return g_failures == 0 ? 0 : 1;
}
