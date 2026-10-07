#include "audio/DspAudio.hpp"

#include <cstdlib>
#include <malloc.h>
#include <cstring>
#include <string>

#include <nds.h>

#include "Global.hpp"

extern "C"
{
// Only the numbers: the arithmetic in this header is the DSP's, not ours.
#include "dsp/hl_dsp.h"
}

extern const std::uint32_t hlDspProgramBytes;
extern const std::uint8_t hlDspProgram[];

namespace hlrt
{
    DspAudio dspAudio;

    namespace
    {
        using namespace hldata;

        /// The channel whose clock the capture runs on (the hardware's rule:
        /// capture unit 0 keeps time by channel 1). Nothing is played on it.
        constexpr int kCaptureClock = 1;
        /// A quarter of the DSP's output rate: what its echo takes in.
        constexpr int kCaptureRate = 11901;
        constexpr int kCaptureSamples = 4096; ///< a third of a second
        /// Samples captured in one vertical blank, in 256ths (11,901.3 Hz
        /// over 59.826 Hz).
        constexpr std::uint32_t kCapturePerBlank = 50927;
        /// Pairs of music played in one blank, in 256ths (23,802.6 over
        /// 59.826).
        constexpr std::uint32_t kPairsPerBlank = 101854;

        /// Room for one block of music as the DSP takes it.
        std::uint16_t scratch[HL_DSP_BLOCK_WHOLE + 3];
    } // namespace

    bool DspAudio::command(std::uint16_t cmd, std::uint16_t a1, std::uint16_t a2, std::uint16_t &answer)
    {
        // Never for ever: a DSP that has stopped answering is done without.
        auto wait = [](bool (*ready)(int), int id)
        {
            for (int spin = 0; spin < 200000; ++spin)
            {
                if (ready(id))
                    return true;
            }
            return false;
        };
        if (!wait(dspSendDataReady, 1))
            return false;
        dspSendData(1, a1);
        if (!wait(dspSendDataReady, 2))
            return false;
        dspSendData(2, a2);
        if (!wait(dspSendDataReady, 0))
            return false;
        dspSendData(0, cmd);
        if (!wait(dspReceiveDataReady, 1))
            return false;
        dspReceiveData(1); // the tick it took effect before: for the tests
        if (!wait(dspReceiveDataReady, 0))
            return false;
        answer = dspReceiveData(0);
        return true;
    }

    bool DspAudio::available()
    {
        return hlDspProgramBytes != 0 && nwramIsAvailable();
    }

    void DspAudio::stop()
    {
        stopMusic();
        if (capture_ != nullptr)
        {
            soundCaptureStop(0);
            // (A blank, so that the ARM7 has stopped writing to it.)
            swiWaitForVBlank();
            std::free(capture_);
            capture_ = nullptr;
        }
        if (up_)
            dspPowerOff();
        up_ = false;
        started_ = false;
        echo_ = RoomEcho{0, 0, 0};
        soundExtSetRatio(8);
    }

    bool DspAudio::start()
    {
        if (up_)
            return true;
        if (!available())
            return false;
        if (dspExecuteDefaultTLF(hlDspProgram) != DSP_EXEC_OK)
            return false;
        std::uint16_t answer = 0;
        if (!command(HL_DSP_CMD_PING, 0, 0, answer) || answer != HL_DSP_MAGIC ||
            !command(HL_DSP_CMD_INFO, HL_DSP_INFO_MUSIC, 0, musicAt_) ||
            !command(HL_DSP_CMD_INFO, HL_DSP_INFO_SEND, 0, sendAt_))
        {
            dspPowerOff();
            return false;
        }
        up_ = true;
        started_ = true;
        failed_ = false;
        faults_ = 0;

        // The copy of the mix that is sent to echo: one channel (the left
        // mixer), sixteen bits, going round a buffer for good.
        // (On cache lines of its own: its cache is thrown away each frame,
        // and that must not take a neighbour's unwritten data with it.)
        capture_ = static_cast<std::int16_t *>(
            memalign(32, kCaptureSamples * sizeof(std::int16_t)));
        if (capture_ != nullptr)
        {
            std::memset(capture_, 0, kCaptureSamples * sizeof(std::int16_t));
            DC_FlushRange(capture_, kCaptureSamples * sizeof(std::int16_t));
            soundSetFreq(kCaptureClock, kCaptureRate);
            soundCaptureStart(capture_, kCaptureSamples / 2, 0, false, true, true,
                              SoundCaptureFormat_16Bit);
            captureBlank_ = Global::vblanks();
            captureRead_ = 0;
        }
        // Half the speakers each to the ordinary channels and to the DSP.
        soundExtSetRatio(4);
        return true;
    }

    void DspAudio::write(const std::uint16_t *words, std::uint16_t ring, std::uint16_t at, int count,
                         int size)
    {
        // Round the end of the ring, in two pieces if it must be.
        const int first = count < size - at ? count : size - at;
        dspFifoWriteData(words, static_cast<std::uint16_t>(ring + at), first);
        if (count > first)
            dspFifoWriteData(words + first, ring, count - first);
    }

    void DspAudio::room(const RoomEcho &echo)
    {
        if (!up_ || (echo.feedback == echo_.feedback && echo.damp == echo_.damp && echo.level == echo_.level))
            return;
        echo_ = echo;
        std::uint16_t answer;
        up_ = command(HL_DSP_CMD_ROOM, echo.feedback,
                      static_cast<std::uint16_t>((echo.damp << 8) | echo.level), answer);
    }

    bool DspAudio::startMusic(int track, std::uint32_t fromPair)
    {
        stopMusic();
        if (!up_)
            return false;
        char name[16];
        musicFileName(track, name, sizeof(name), true);
        const std::string path = global.dataPath("music/") + name;
        std::FILE *file = std::fopen(path.c_str(), "rb");
        if (file == nullptr)
            return false;
        StereoHeader header;
        std::fseek(file, 0, SEEK_END);
        const long size = std::ftell(file);
        std::fseek(file, 0, SEEK_SET);
        const long blockBytes = (kMusicBlockWords + 4) * 2;
        if (std::fread(&header, sizeof(header), 1, file) != 1 ||
            std::memcmp(header.magic, kStereoMagic, 4) != 0 || header.version != kSoundVersion ||
            header.blocks == 0 ||
            size != static_cast<long>(sizeof(header)) + static_cast<long>(header.blocks) * blockBytes)
        {
            std::fclose(file);
            return false;
        }
        const std::uint32_t pairsPerBlock = kMusicBlockWords * 2;
        std::uint32_t block = fromPair / pairsPerBlock;
        if (block >= header.blocks)
        {
            std::fclose(file); // it was over already
            return false;
        }
        std::fseek(file, static_cast<long>(sizeof(header)) + static_cast<long>(block) * blockBytes, SEEK_SET);
        music_ = file;
        track_ = track;
        blocks_ = header.blocks;
        pairs_ = header.pairs;
        block_ = block;
        musicWrite_ = 0;
        startPair_ = block * pairsPerBlock;
        // Three blocks in the ring (a quarter of a second) before it is told
        // to play.
        for (int n = 0; n < 3; ++n)
            sendBlock();
        std::uint16_t answer;
        up_ = command(HL_DSP_CMD_MUSIC_START, musicWrite_, 0, answer);
        startBlank_ = Global::vblanks();
        if (!up_)
            stopMusic();
        return up_;
    }

    bool DspAudio::sendBlock()
    {
        if (music_ == nullptr || block_ >= blocks_)
            return false;
        // A block as it is in the file -- both decoders' states, then its
        // samples -- read whole or not at all, and sent behind the mark the
        // DSP looks for at the start of each: it takes its bearings again
        // from every one.
        scratch[0] = HL_DSP_BLOCK_MARK;
        const std::size_t want = kMusicBlockWords + 4;
        if (std::fread(scratch + 1, 2, want, music_) != want)
        {
            block_ = blocks_; // the file ends short: so does the track
            return false;
        }
        write(scratch, musicAt_, musicWrite_, HL_DSP_BLOCK_WHOLE, HL_DSP_MUSIC_WORDS);
        musicWrite_ = static_cast<std::uint16_t>((musicWrite_ + HL_DSP_BLOCK_WHOLE) % HL_DSP_MUSIC_WORDS);
        ++block_;
        return true;
    }

    void DspAudio::stopMusic()
    {
        if (music_ == nullptr)
            return;
        std::uint16_t answer;
        if (up_)
            command(HL_DSP_CMD_MUSIC_STOP, 0, 0, answer);
        std::fclose(music_);
        music_ = nullptr;
        track_ = -1;
    }

    std::uint32_t DspAudio::musicPair() const
    {
        return startPair_ + (((Global::vblanks() - startBlank_) * kPairsPerBlank) >> 8);
    }

    void DspAudio::feedMusic()
    {
        if (music_ == nullptr)
            return;
        if (musicPair() > pairs_ + 12000)
        {
            stopMusic(); // the track is over, and what was in the ring has played
            return;
        }
        // How much room the ring has: ask where the DSP has got to. A whole
        // block goes in when there is room for one (a block is 86 ms, so
        // that is one frame in five), and only then is the DSP told of it.
        std::uint16_t read = 0;
        if (!command(HL_DSP_CMD_MUSIC_FED, musicWrite_, 0, read))
        {
            up_ = false;
            return;
        }
        int room = static_cast<int>(read) - static_cast<int>(musicWrite_) - 1;
        if (room < 0)
            room += HL_DSP_MUSIC_WORDS;
        if (room < HL_DSP_BLOCK_WHOLE || !sendBlock())
            return;
        std::uint16_t answer;
        up_ = command(HL_DSP_CMD_MUSIC_FED, musicWrite_, 0, answer);
    }

    void DspAudio::feedEcho()
    {
        if (capture_ == nullptr)
            return;
        // Where the capture has got to, by the clock (its position cannot be
        // read), less a little so that what is taken has surely been written.
        const std::uint32_t written = ((Global::vblanks() - captureBlank_) * kCapturePerBlank) >> 8;
        if (written < 96)
            return;
        std::uint32_t upTo = written - 96;
        if (upTo <= captureRead_)
            return;
        std::uint32_t count = upTo - captureRead_;
        // Fallen a long way behind (a map was loading): what is that old is
        // not sent, and the echo starts again from now.
        if (count > 1024)
        {
            captureRead_ = upTo - 256;
            count = 256;
        }
        // With no echo wanted, nothing is sent: the DSP is left alone.
        if (echo_.level == 0)
        {
            captureRead_ = upTo;
            return;
        }
        if (count > 512)
            count = 512;
        const std::uint32_t at = captureRead_ % kCaptureSamples;
        const std::uint32_t first = count < kCaptureSamples - at ? count : kCaptureSamples - at;
        // (The ARM7 wrote it: this processor's cache does not know.)
        DC_InvalidateRange(capture_, kCaptureSamples * sizeof(std::int16_t));
        std::memcpy(scratch, capture_ + at, first * 2);
        std::memcpy(scratch + first, capture_, (count - first) * 2);
        captureRead_ += count;

        std::uint16_t read = 0;
        if (!command(HL_DSP_CMD_SEND_FED, sendWrite_, 0, read))
        {
            up_ = false;
            return;
        }
        int room = static_cast<int>(read) - static_cast<int>(sendWrite_) - 1;
        if (room < 0)
            room += HL_DSP_SEND_WORDS;
        if (static_cast<int>(count) > room)
            count = static_cast<std::uint32_t>(room); // the DSP is behind: the rest is dropped
        if (count == 0)
            return;
        write(scratch, sendAt_, sendWrite_, static_cast<int>(count), HL_DSP_SEND_WORDS);
        sendWrite_ = static_cast<std::uint16_t>((sendWrite_ + count) % HL_DSP_SEND_WORDS);
        std::uint16_t answer;
        up_ = command(HL_DSP_CMD_SEND_FED, sendWrite_, 0, answer);
    }

    void DspAudio::frame()
    {
        if (!up_)
            return;
        feedEcho();
        if (up_)
            feedMusic();
        // Twice a second: how it is getting on. If it says the music has
        // gone wrong (it stops it itself, rather than play noise), the track
        // is started again from where it had got to; if that keeps
        // happening, the DSP is not to be trusted with it.
        if (up_ && ++statusDue_ >= 30)
        {
            statusDue_ = 0;
            std::uint16_t status = 0;
            if (command(HL_DSP_CMD_STATUS, 0, 0, status))
            {
                dry_ += status & 0x7FFF;
                if ((status & HL_DSP_FAULT) && music_ != nullptr)
                {
                    const int track = track_;
                    const std::uint32_t at = musicPair();
                    ++faults_;
                    stopMusic();
                    if (faults_ >= 3 || !startMusic(track, at))
                        failed_ = true;
                }
            }
            std::uint16_t heard = 0;
            if (command(HL_DSP_CMD_HEARD, 0, 0, heard))
                heard_ = heard;
        }
    }
} // namespace hlrt
