#include "audio/CardShare.hpp"

#include <cstdint>
#include <malloc.h>
#include <cstdio>
#include <cstring>

#include <nds.h>
#include <nds/arm9/dldi.h>
#include <nds/disc_io.h>

#include "audio/A7SdProto.h"

namespace hlrt::cardshare
{
    namespace
    {
        int lastStatus = A7SD_OK;
        constexpr int kWaitFrames = 900; //15s
    } // namespace

    int detail()
    {
        return lastStatus;
    }

    namespace
    {
        /// ARM7's request, from IPC FIFO, handled from the main loop
        /// The driver must not be entered from an interrupt while the main loop may be in it.
        std::uint8_t *bounce = nullptr;
        volatile bool pending = false;
        A7SdRead request;

        void onRead(int bytes, void *)
        {
            A7SdRead read;
            if (bytes > static_cast<int>(sizeof(read)))
                bytes = sizeof(read);
            fifoGetDatamsg(A7SD_FIFO, bytes, reinterpret_cast<u8 *>(&read));
            request = read;
            pending = true;
        }
    } // namespace

    void service()
    {
        if (!pending || bounce == nullptr)
            return;
        const A7SdRead read = request;
        pending = false;
        const DISC_INTERFACE *io = dldiGetInternal();
        bool ok = io != nullptr && read.count >= 1 && read.count <= A7SD_BOUNCE_SECTORS &&
                  io->readSectors(read.sector, read.count, bounce);
        if (ok)
        {
            DC_FlushRange(bounce, static_cast<std::size_t>(read.count) * 512); // ARM7 reads main RAM
        }
        fifoSendValue32(A7SD_FIFO, (static_cast<std::uint32_t>(ok ? 0 : 1) << 24) | read.tag);
    }

    Result attach(const char *device, const char *image)
    {
        lastStatus = A7SD_OK;
        A7SdSetup message;
        std::memset(&message, 0, sizeof(message));
        message.magic = A7SD_MAGIC;
        message.version = A7SD_VERSION;

        // The DSi's own SD slot is the ARM7's TMIO controller whatever the
        // driver; anything else goes through DLDI, which only the ARM7 can
        // use if it is told to (BlocksDS runs a driver there only when it is
        // FEATURE_ARM7_CAPABLE).
        if (isDSiMode() && device != nullptr && std::strcmp(device, "sd:") == 0)
        {
            message.kind = A7SD_KIND_DSI_SD;
        }
        else
        {
            const DISC_INTERFACE *io = dldiGetInternal();
            if (io == nullptr)
                return Result::DriverOnArm9;
            if (dldiGetMode() == DLDI_MODE_ARM7) // FEATURE_ARM7_CAPABLE or HL_DLDI_FORCE_ARM7
            {
                message.kind = A7SD_KIND_DLDI;
                message.dldi = reinterpret_cast<std::uint32_t>(io);
            }
            else
            {
                // ARM7 asks for what it reads
                bounce = static_cast<std::uint8_t *>(memalign(32, A7SD_BOUNCE_SECTORS * 512));
                if (bounce == nullptr)
                    return Result::DriverOnArm9;
                DC_FlushRange(bounce, A7SD_BOUNCE_SECTORS * 512);
                fifoSetDatamsgHandler(A7SD_FIFO, onRead, nullptr);
                message.kind = A7SD_KIND_ARM9;
                message.dldi = reinterpret_cast<std::uint32_t>(bounce);
            }
        }

        std::snprintf(message.path, sizeof(message.path), "%s", image);
        if (!fifoSendDatamsg(A7SD_FIFO, sizeof(message), reinterpret_cast<u8 *>(&message)))
        {
            return Result::NoReply;
        }

        for (int frame = 0; frame < kWaitFrames; ++frame)
        {
            // Mounting the card and finding the image is 12 reads, served here
            for (int i = 0; i < 64 && pending; ++i)
                service();
            if (fifoCheckValue32(A7SD_FIFO))
            {
                const std::uint32_t answer = fifoGetValue32(A7SD_FIFO);
                lastStatus = static_cast<int>(answer >> 24);
                return lastStatus == A7SD_OK ? Result::Ok : Result::Refused;
            }
            swiWaitForVBlank();
        }
        return Result::NoReply;
    }
} // namespace hlrt::cardshare
