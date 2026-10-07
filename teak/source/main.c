// The game's DSP program (DSi): the music in stereo, and the room's echo.
//
// All of what it works out is in shared/dsp/hl_dsp.h, which the host tests
// run as well. What is here is only what belongs to the hardware: the
// interrupt that asks for a pair of samples 47,605 times a second, and the
// loop that takes the ARM9's commands.

#include <teak/teak.h>

#include "hl_dsp.h"

void hlPlainArithmetic(void); // mode.s

// One pair to the speakers' port, if it has room for one.
static void hlSendPair(void)
{
    if (btdmpTransmitFifoFull(0))
        return;
    hlDspPull();
    REG_BTDMP_TRANSMIT_FIFO_DATA(0) = hlPortL;
    REG_BTDMP_TRANSMIT_FIFO_DATA(0) = hlPortR;
}

// (Defined weak in the library's start-up code.) The interrupt only hands
// over what the main loop has worked out -- and as much of it as the port
// will take (eight pairs), not one pair: the port then has a sixth of a
// millisecond in hand, and is not empty the moment anything is late. With
// nothing worked out it still gets one pair (the last, fading): a port that
// is left empty never asks again.
void irqHandlerInt1(void)
{
    hlPlainArithmetic();
    do
    {
        hlSendPair();
    } while (hlOutRead != hlOutWrite && !btdmpTransmitFifoFull(0));

    REG_ICU_IRQ_ACK = ICU_IRQ_MASK_BTDMP0;
}

int main(void)
{
    teakInit();
    hlDspInit();

    // The speakers' port, asking for samples through CPU interrupt 1.
    btdmpSetupOutputSpeakers(0, 1);
    // One pair of silence to start with: the interrupt is "the port has just
    // run out", and a port that has never had anything never runs out.
    REG_BTDMP_TRANSMIT_FIFO_DATA(0) = 0;
    REG_BTDMP_TRANSMIT_FIFO_DATA(0) = 0;
    cpuEnableInt1();
    cpuEnableIrqs();

    while (1)
    {
        uint16_t command, arg1, arg2, answer, tick;

        // Working out what is to be heard comes first, always; a command is
        // taken between two fillings of the ring.
        hlDspFill();
        // The port found empty: it ran out before the interrupt got to it,
        // and it asks only when it runs out. It is started again from here.
        if ((REG_BTDMP_TRANSMIT_FIFO_STAT(0) & BTDMP_TRANSMIT_FIFO_STAT_NOT_EMPTY) == 0)
        {
            cpuDisableIrqs();
            hlSendPair();
            cpuEnableIrqs();
        }
        if ((REG_APBP_STAT & APBP_STAT_CMD0_NEW) == 0)
            continue;

        // The arguments are written before the command, so they are there.
        command = apbpReceiveData(0);
        arg1 = apbpReceiveData(1);
        arg2 = apbpReceiveData(2);
        if (command == HL_DSP_CMD_INFO)
        {
            // Where the ARM9 is to write: addresses in the DSP's data memory.
            if (arg1 == HL_DSP_INFO_MUSIC)
                answer = (uint16_t)(uintptr_t)hlMusicRing;
            else
                answer = (uint16_t)(uintptr_t)hlSendRing;
        }
        else
        {
            answer = hlDspCommand(command, arg1, arg2);
        }
        // (The tick it took effect before: how the tests line the model up.)
        tick = hlTick;
        apbpSendData(1, tick);
        apbpSendData(0, answer);
    }
    return 0;
}
