#include "global_header.hpp"

#include "Settings.hpp"

#include "Global.hpp"

Settings settings;

void Settings::load()
{
    if (!global.hasFat)
        return;
    std::FILE *file = std::fopen(global.dataPath("settings.dat").c_str(), "rb");
    if (file == nullptr)
        return;
    std::uint8_t bytes[64];
    const std::size_t size = std::fread(bytes, 1, sizeof(bytes), file);
    std::fclose(file);

    hldata::SettingsData data;
    if (hldata::readSettings(bytes, size, data))
    {
        renderMode = static_cast<hldata::RenderMode>(data.renderMode);
        dspAudio = data.dspAudio != 0;
    }
}

bool Settings::save() const
{
    if (!global.hasFat)
        return false;
    hldata::SettingsData data;
    data.renderMode = static_cast<std::uint8_t>(renderMode);
    data.dspAudio = dspAudio ? 1 : 0;
    const std::vector<std::uint8_t> bytes = hldata::writeSettings(data);

    std::FILE *file = std::fopen(global.dataPath("settings.dat").c_str(), "wb");
    if (file == nullptr)
        return false;
    const bool ok = std::fwrite(bytes.data(), 1, bytes.size(), file) == bytes.size();
    return std::fclose(file) == 0 && ok;
}

hldata::RenderMode Settings::next(hldata::RenderMode mode)
{
    // The order they are offered in, which is not the order of their numbers.
    switch (mode)
    {
    case hldata::RenderMode::SinglePass: return hldata::RenderMode::TwoPass;
    case hldata::RenderMode::TwoPass:    return hldata::RenderMode::TwoPassDma;
    case hldata::RenderMode::TwoPassDma: return hldata::RenderMode::Auto;
    default:                             return hldata::RenderMode::SinglePass;
    }
}

const char *Settings::name(hldata::RenderMode mode)
{
    switch (mode)
    {
    case hldata::RenderMode::TwoPass:    return "Two-pass   (30 fps)";
    case hldata::RenderMode::TwoPassDma: return "Two-pass DMA (30 fps)";
    case hldata::RenderMode::SinglePass: return "Single-pass (60 fps)";
    case hldata::RenderMode::Auto:       return "Auto       (per map)";
    default:                             return "";
    }
}
