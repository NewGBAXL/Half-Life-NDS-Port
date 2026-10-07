// The manifest: the one yas archive the DS reads.
//
// yas reports a damaged archive by throwing and the ARM9 is built without
// exceptions, so the wrapper around the payload is the only thing between a
// bad card and a crash. Every way the file can be wrong has to be caught by
// readManifest() BEFORE yas runs, and that is what most of this checks.

#include <cstdio>
#include <filesystem>

#include "HlManifest.hpp"
#include "TestBspBuilder.hpp"
#include "hlds/AtomicWrite.hpp"
#include "hlds/Converter.hpp"

namespace fs = std::filesystem;
using namespace hldata;
using namespace hldstest;

namespace
{
    void write(const fs::path &path, const std::vector<std::uint8_t> &bytes)
    {
        std::string error;
        hlds::writeFileAtomic(path, bytes, error);
    }
} // namespace

int main()
{
    Manifest manifest;
    manifest.converterVersion = "0.1.0";
    manifest.startMap = "c1a0";
    ManifestMap a;
    a.name = "c0a0";
    a.levelBytes = 674980;
    a.worstPolygons = 2234;
    a.worstVertices = 7846;
    a.textureCount = 64;
    ManifestMap b = a;
    b.name = "c1a0";
    b.levelBytes = 1;
    manifest.maps = {a, b};

    const std::vector<std::uint8_t> bytes = writeManifest(manifest);
    Manifest back;
    check(readManifest(bytes.data(), bytes.size(), back) == ManifestError::Ok, "round trip");
    check(back.converterVersion == "0.1.0" && back.startMap == "c1a0" && back.maps.size() == 2,
          "strings and list");
    check(back.maps[0].name == "c0a0" && back.maps[0].levelBytes == 674980 &&
              back.maps[0].worstVertices == 7846 && back.maps[0].textureCount == 64,
          "map fields");
    check(back.find("c1a0") == &back.maps[1] && back.find("nope") == nullptr, "find");

    check(readManifest(bytes.data(), 5, back) == ManifestError::NotAManifest, "a stub");
    auto bad = bytes;
    bad[0] = 'X';
    check(readManifest(bad.data(), bad.size(), back) == ManifestError::NotAManifest, "wrong magic");
    bad = bytes;
    bad[4] ^= 0x40;
    check(readManifest(bad.data(), bad.size(), back) == ManifestError::OtherVersion,
          "another manifest version");
    bad = bytes;
    bad[6] ^= 0x40;
    check(readManifest(bad.data(), bad.size(), back) == ManifestError::OtherVersion,
          "another LEVEL version: the maps it lists are not for this build");
    check(readManifest(bytes.data(), bytes.size() - 1, back) == ManifestError::NotAManifest,
          "a truncated payload is caught by its length");
    bad = bytes;
    bad.back() ^= 1;
    check(readManifest(bad.data(), bad.size(), back) == ManifestError::NotAManifest,
          "a damaged payload is caught by its checksum, before yas reads it");

    // The whole run, on a game folder made of the box room: one map cooked,
    // then the manifest rebuilt from what is on the card.
    const fs::path root = fs::temp_directory_path() / "hlds_test_manifest";
    fs::remove_all(root);
    fs::create_directories(root / "valve" / "maps");
    fs::create_directories(root / "card");
    write(root / "valve" / "maps" / "BoxRoom.BSP", buildBoxRoom());
    write(root / "valve" / "maps" / "second.bsp", buildBoxRoom());
    write(root / "valve" / "maps" / "notes.txt", {'x'});

    hlds::ConvertOptions options;
    options.gameDir = root / "valve";
    options.cardRoot = root / "card";
    options.textures = false; // no encoder needed for this test
    hlds::Converter converter(options);
    check(converter.preflight().empty(), "preflight passes on a good setup");
    const auto maps = converter.listMaps();
    check(maps.size() == 2 && maps[0] == "boxroom" && maps[1] == "second",
          "maps are listed lower-case, sorted, .bsp only");

    std::string error;
    int progressCalls = 0;
    int failed = converter.cookMaps(
        {"boxroom"}, [&](std::size_t, const std::string &) { ++progressCalls; },
        [&](const hlds::MapResult &r)
        {
            check(r.ok && r.name == "boxroom" && r.surfaces == 6 && r.worstVertices == 24,
                  "the map result reports the cook");
        },
        nullptr, error);
    check(failed == 0 && progressCalls == 1, "one map cooked");
    const fs::path data = hlds::Converter::dataDir(root / "card");
    check(fs::exists(data / "maps" / "boxroom.hlm"), "the level is on the card, lower-case");

    std::vector<std::uint8_t> onCard;
    check(hlds::readFile(data / "manifest.bin", onCard, error) &&
              readManifest(onCard.data(), onCard.size(), back) == ManifestError::Ok,
          "and so is a manifest the DS reader accepts");
    check(back.maps.size() == 1 && back.maps[0].name == "boxroom" &&
              back.maps[0].worstPolygons == 6,
          "listing the map with its header's figures");
    check(back.startMap == "boxroom", "with no c1a0, the start map is the first there is");

    // A second run of a different map must not forget the first; a file that
    // is not a level must not be listed; a failed map must be counted.
    write(data / "maps" / "junk.hlm", {1, 2, 3});
    failed = converter.cookMaps({"second", "missing"}, nullptr, nullptr, nullptr, error);
    check(failed == 1, "a map that does not exist is one failure");
    hlds::readFile(data / "manifest.bin", onCard, error);
    readManifest(onCard.data(), onCard.size(), back);
    check(back.maps.size() == 2 && back.find("boxroom") && back.find("second"),
          "the manifest lists both runs' maps, and not the junk file");
    check(back.startMap == "boxroom", "and keeps the start map it had");

    options.startMap = "SECOND";
    hlds::Converter restart(options);
    restart.cookMaps({}, nullptr, nullptr, nullptr, error);
    hlds::readFile(data / "manifest.bin", onCard, error);
    readManifest(onCard.data(), onCard.size(), back);
    check(back.startMap == "second", "--start changes it, without regard to case");

    hlds::ConvertOptions wrong = options;
    wrong.gameDir = root / "card";
    check(!hlds::Converter(wrong).preflight().empty(), "preflight names a folder with no maps");
    wrong = options;
    wrong.cardRoot = root / "no-such-card";
    check(!hlds::Converter(wrong).preflight().empty(), "and a card folder that is not there");

    fs::remove_all(root);
    if (g_failures == 0)
        std::printf("test_manifest: ok\n");
    return g_failures == 0 ? 0 : 1;
}
