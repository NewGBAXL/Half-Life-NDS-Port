#pragma once

// The cooked level in memory.
//
// A level is one file and one heap block: load() reads the whole .hlm with a
// single fread and hands the bytes to LevelView::open (shared/HlLevelReader.hpp),
// which checks them and points into them. Nothing is parsed or copied after
// that, and unload() is one free.
//
// Only one level is ever loaded; `level` is it.

#include <string>

#include "HlLevelReader.hpp"

namespace hlrt
{
    class Level
    {
    public:
        hldata::LevelView view;

        /// Replace the loaded level. On failure nothing is loaded and `error`
        /// holds a line fit to show the player.
        bool load(const std::string &path, std::string &error);
        void unload();

        /// Read just a level's header, without loading it. False if the file
        /// is not there or is not a level of this version; the full load is
        /// what reports why.
        static bool peekHeader(const std::string &path, hldata::LevelHeader &header);
        bool loaded() const { return block_ != nullptr; }
        std::size_t sizeBytes() const { return size_; }

        Level() = default;
        Level(const Level &) = delete;
        Level &operator=(const Level &) = delete;

    private:
        std::uint8_t *block_ = nullptr;
        std::size_t size_ = 0;
    };

    extern Level level;
} // namespace hlrt
