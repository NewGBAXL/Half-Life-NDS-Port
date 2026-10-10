// What the routines share. (Saturation is off -- the start-up code's mod0 --
// so a shift is a shift, and a store is an accumulator's low word.)

// `acc` over two to the `bits`, towards nothing (a negative number is
// shifted as its size is: the echo dies away to silence, not to minus one).
.macro SHIFTZ acc, bits
    cmp     0x0u8, \acc
    brr     1f, ge
    neg     \acc, always
    shfi    \acc, \acc, -\bits
    neg     \acc, always
    brr     2f, always
1:
    shfi    \acc, \acc, -\bits
2:
.endm

// a1 held between -32767 and 32767 (a0 is used up).
.macro CLAMP
    mov     a1, a0
    sub     0x7fff, a0
    brr     1f, le
    mov     0x7fff, a1
    brr     2f, always
1:
    mov     a1, a0
    add     0x7fff, a0
    brr     2f, ge
    mov     0x7fff, a1
    neg     a1, always
2:
.endm

