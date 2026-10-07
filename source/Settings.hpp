#pragma once

// The player's settings on the console: loaded once at boot, saved when one
// changes. The format and its checks are in shared/HlSettings.hpp.

#include "HlSettings.hpp"

class Settings
{
public:
    hldata::RenderMode renderMode = hldata::RenderMode::SinglePass;
    /// DSi: the DSP's music and echo (off unless asked for).
    bool dspAudio = false;

    /// Read settings.dat. An absent or unreadable file is a first boot, not an
    /// error: the defaults stand.
    void load();
    /// Write settings.dat. False if the card cannot be written.
    bool save() const;

    /// The next mode in the menu's cycle.
    static hldata::RenderMode next(hldata::RenderMode mode);
    /// At most 24 characters, for the menu.
    static const char *name(hldata::RenderMode mode);
};

extern Settings settings;
