#include "global_header.hpp"

#include "render/FrameFreeze.hpp"

#include "Global.hpp"

namespace hlrt
{
    FrameFreeze frameFreeze;

    // The next whole frame of the screen into a bank (2: C, 3: D), which
    // must be the hardware's to write just now; every pixel of it marked
    // solid, since a bitmap layer leaves out those that are not.
    static void capture(int bank)
    {
        swiWaitForVBlank();
        REG_DISPCAPCNT = DCAP_BANK(bank) | DCAP_SIZE(DCAP_SIZE_256x192) | DCAP_MODE(DCAP_MODE_A) |
                         DCAP_SRC_A(DCAP_SRC_A_COMPOSITED) | DCAP_ENABLE;
        for (int n = 0; n < 4 && (n < 2 || (REG_DISPCAPCNT & DCAP_ENABLE)); ++n)
            swiWaitForVBlank();
        std::uint16_t *pixels = bank == 2 ? VRAM_C : VRAM_D;
        for (int i = 0; i < 256 * 192; ++i)
            pixels[i] |= 0x8000;
    }

    bool FrameFreeze::freeze()
    {
        if (frozen_)
            return true;
        frozen_ = true;
        layered_ = false;
        if (global.video == Global::Video::TwoPassDma)
            return true; // (what is shown is a finished picture already)

        if (global.video == Global::Video::TwoPass)
        {
            // Half of what is on the screen is a capture and the other half
            // the 3D layer -- which half, turn about; and the layers are
            // put in order for a pass at the moment the next one is begun.
            // With no next one, that is done here: the left half's capture
            // is in bank C and the right half's in D, and it is the right
            // half that has the holes to see the other through.
            const bool leftLive = NEA_TwoPassGetPass() != 0;
            swiWaitForVBlank();
            swiWaitForVBlank();
            REG_DISPCAPCNT = 0;
            if (leftLive)
            {
                vramSetBankC(VRAM_C_LCD);
                vramSetBankD(VRAM_D_MAIN_BG_0x06000000);
            }
            else
            {
                vramSetBankD(VRAM_D_LCD);
                vramSetBankC(VRAM_C_MAIN_BG_0x06000000);
            }
            REG_BG0CNT = (REG_BG0CNT & ~3) | BG_PRIORITY(leftLive ? 1 : 0);
            REG_BG2CNT = (REG_BG2CNT & ~3) | BG_PRIORITY(leftLive ? 0 : 1);
            // Then the whole of it into the bank not being shown, and that
            // one alone in front: the 3D layer is about to lose its
            // textures.
            capture(leftLive ? 2 : 3);
            if (leftLive)
            {
                vramSetBankD(VRAM_D_LCD);
                vramSetBankC(VRAM_C_MAIN_BG_0x06000000);
            }
            else
            {
                vramSetBankC(VRAM_C_LCD);
                vramSetBankD(VRAM_D_MAIN_BG_0x06000000);
            }
            REG_BG0CNT = (REG_BG0CNT & ~3) | BG_PRIORITY(1);
            REG_BG2CNT = (REG_BG2CNT & ~3) | BG_PRIORITY(0);
            return true; // (the next pass drawn sets all of this again)
        }

        // Single-pass: the screen as it is, into bank D, which is kept free
        // for this.
        vramSetBankD(VRAM_D_LCD);
        capture(3);

        // Shown as a layer in front of the 3D one, which is left as it is
        // behind it.
        savedControl_ = REG_DISPCNT;
        savedBg0_ = REG_BG0CNT;
        vramSetBankD(VRAM_D_MAIN_BG_0x06000000);
        REG_BG2CNT = BG_BMP16_256x256 | BG_BMP_BASE(0) | BG_PRIORITY(0);
        REG_BG2PA = 256;
        REG_BG2PB = 0;
        REG_BG2PC = 0;
        REG_BG2PD = 256;
        REG_BG2X = 0;
        REG_BG2Y = 0;
        REG_BG0CNT = (savedBg0_ & ~3) | BG_PRIORITY(1);
        REG_DISPCNT = (savedControl_ & ~7ul) | 5ul | DISPLAY_BG2_ACTIVE;
        layered_ = true;
        return true;
    }

    void FrameFreeze::thaw()
    {
        if (!frozen_)
            return;
        frozen_ = false;
        if (!layered_)
            return;
        layered_ = false;
        REG_DISPCNT = savedControl_;
        REG_BG0CNT = savedBg0_;
        vramSetBankD(VRAM_D_LCD);
    }
} // namespace hlrt
