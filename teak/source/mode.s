// The arithmetic mode the compiler's code needs, for an interrupt handler.
//
// The Teak has two copies of its mode register mod0 and swaps them on an
// interrupt. libteak's start-up code switches accumulator saturation off in
// the one the main program runs with; a handler runs with the other, where
// it is still on -- and the compiler widens a number by shifting it to the
// top of the accumulator and back, which with saturation on pins anything
// over 23 bits (and any sample over 8). Call this first in a handler.

#include <teak/asminc.h>
#include <teak/cpuregs.h>

BEGIN_ASM_FUNC hlPlainArithmetic

    set     (MOD0_SATURATION_ON | MOD0_SATURATION_STORE_ON), mod0
    ret     always
