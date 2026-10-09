#include "Global.hpp"

#include "HlManifest.hpp"
#include "render/Camera.hpp"
#include "render/GpuStats.hpp"

Global global;

namespace
{
    volatile std::uint32_t vblankCount = 0;

    /// NEA's own vblank work, plus the tick the game's clock runs on.
    void onVblank()
    {
        vblankCount = vblankCount + 1;
        hlrt::GpuStats::onVblank();
        NEA_VBLFunc();
    }
} // namespace

std::uint32_t Global::vblanks()
{
    return vblankCount;
}

void Global::Init()
{
    // First, before anything is timed against the CPU: every later measurement
    // and every budget chosen from isDSi assumes the clock is already what it
    // is going to be. setCpuClock is a no-op outside DSi mode.
    isDSi = isDSiMode();
    if (isDSi)
    {
        setCpuClock(true);
        fastClock = true;
    }

    irqEnable(IRQ_HBLANK);
    irqSet(IRQ_VBLANK, onVblank);
    irqSet(IRQ_HBLANK, NEA_HBLFunc);

    // SD / flashcard FAT access -- every byte of game content comes from here.
    const bool fatOk = fatInitDefault();
    fatDevice = fatGetDefaultDrive();
    hasFat = fatOk && (fatDevice != nullptr);
    if (hasFat)
    {
        fatDeviceCPP = fatDevice;
    }

}

void Global::InitVideo(Video want)
{
    videoReady_ = true;

    // The DMA mode keeps two pictures in main memory. If there is no room
    // for them it says so, and the other two-pass mode needs none.
    if (want == Video::TwoPassDma && NEA_Init3D_TwoPass_DMA() != 0)
        want = Video::TwoPass;
    if (want == Video::TwoPass)
        NEA_Init3D_TwoPass_FB();
    else if (want == Video::SinglePass)
        NEA_Init3D();
    video = want;
    twoPass = want != Video::SinglePass;
    // NEA puts the 3D screen on top by default; said explicitly because the
    // whole control scheme depends on the touch screen NOT being the 3D one.
    NEA_MainScreenSetOnTop();
    NEA_SetDepthBufferMode(NEA_WBUFFER);
    // A DSi has a second set of DMA channels, which NEA uses for display
    // lists when it is told it may: they can feed the geometry engine in the
    // modes where the old ones are busy with the screen. (The world's own
    // list is sent the same way: WorldDraw::emitList.)
    NEA_DisplayListEnableNDMA(isDSiMode());
    swiWaitForVBlank();
    swiWaitForVBlank();

    // Room for a map's worth of textures: NEA's defaults (128 materials, 64
    // palettes) are below what one retail map uses, and every 4x4 compressed
    // texture has a palette of its own. The banks must be said explicitly: in
    // two-pass mode C and D (or D alone, in the DMA mode) are the capture
    // buffers, and "all of them" would hand those to the texture allocator.
    // Single-pass keeps D back as well: it is where the last picture is
    // captured to and shown from while a map loads (FrameFreeze). Every map
    // fits without it -- the DMA mode has always had the same three.
    NEA_TextureSystemReset(256, 256, video == Video::TwoPass ? NEA_VRAM_AB : NEA_VRAM_ABC);
    if (video == Video::SinglePass)
        vramSetBankD(VRAM_D_LCD);

    // The projection the world is drawn through. NEA rebuilds it every frame
    // from these, so they are set once.
    NEA_SetFov(hlrt::Camera::kFovDegrees);
    NEA_ClippingPlanesSetI(hlrt::Camera::kNearClip, hlrt::Camera::kFarClip);
    NEA_ClearColorSet(RGB15(0, 0, 0), 31, 63);

    // (No text console: what there is to read is drawn on the touch screen,
    // in the game's own letters -- ui/TouchCanvas.)
}

std::size_t Global::heapHeadroom() const
{
    return static_cast<std::size_t>(getHeapLimit() - getHeapEnd());
}

Global::DataStatus Global::ReadManifest(std::string &startMap) const
{
    if (!hasFat)
        return DataStatus::NoCard;

    std::FILE *file = std::fopen(dataPath("manifest.bin").c_str(), "rb");
    if (file == nullptr)
        return DataStatus::NoData;
    std::fseek(file, 0, SEEK_END);
    const long size = std::ftell(file);
    std::fseek(file, 0, SEEK_SET);
    // A manifest is a few kilobytes. The size is the card's word, so it is
    // bounded before it is used.
    if (size <= 0 || size > 256 * 1024)
    {
        std::fclose(file);
        return DataStatus::Damaged;
    }
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(size));
    const bool got = std::fread(bytes.data(), 1, bytes.size(), file) == bytes.size();
    std::fclose(file);
    if (!got)
        return DataStatus::Damaged;

    // readManifest checks magic, versions, length and checksum BEFORE yas
    // touches the payload: yas reports a bad archive by throwing, and this
    // build has no exceptions.
    hldata::Manifest manifest;
    switch (hldata::readManifest(bytes.data(), bytes.size(), manifest))
    {
    case hldata::ManifestError::Ok:
        break;
    case hldata::ManifestError::OtherVersion:
        return DataStatus::OtherVersion;
    default:
        return DataStatus::Damaged;
    }
    if (manifest.startMap.empty() || manifest.find(manifest.startMap) == nullptr)
        return DataStatus::Damaged;
    startMap = manifest.startMap;
    return DataStatus::Ok;
}

const char *Global::DataStatusTitle(DataStatus s)
{
    switch (s)
    {
    case DataStatus::NoCard:       return "No SD card found.";
    case DataStatus::NoData:       return "No Half-Life data on the card.";
    case DataStatus::OtherVersion: return "The data is from another version.";
    case DataStatus::Damaged:      return "The data on the card is damaged.";
    default:                       return "";
    }
}

const char *Global::DataStatusHint(DataStatus s)
{
    switch (s)
    {
    case DataStatus::NoCard:       return "Insert a card the console can read.";
    case DataStatus::NoData:       return "Run the converter first.";
    case DataStatus::OtherVersion: return "Run this version's converter again.";
    case DataStatus::Damaged:      return "Run the converter again.";
    default:                       return "";
    }
}
