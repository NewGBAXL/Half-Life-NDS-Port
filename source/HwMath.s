@ The compiler's 64-bit divide, and the square root the fixed-point maths
@ uses, done by the console's own divider and square-root unit.
@
@ The compiler turns every 64-bit "/" and "%" into a call to __aeabi_ldivmod
@ (or the unsigned one): in the library that is a loop of a bit at a time,
@ some hundreds of cycles. The hardware takes 34. Defining the same names
@ here is enough: the linker takes these and never looks for the library's.
@ Nothing in the shared code changes, so the host's tests go on testing the
@ same arithmetic -- and the answers are the same: a divide that rounds
@ towards zero, a root rounded down.
@
@ Interrupts are held off from the first operand to the last word of the
@ answer: an interrupt handler that divided in between would leave its own
@ answer in the unit.
@
@ In ITCM: the code that divides most (the collision trace) is there too.

    .syntax unified
    .arch   armv5te
    .cpu    arm946e-s
    .arm

    .set    REG_IME,     0x04000208
    .set    REG_DIVCNT,  0x04000280     @ +0x10 numerator, +0x18 denominator,
                                        @ +0x20 quotient, +0x28 remainder
    .set    REG_SQRTCNT, 0x040002B0     @ +0x04 root, +0x08 its argument

    .section .itcm.text.hwmath, "ax", %progbits
    .align  2

@ (r1:r0) / (r3:r2), signed. Quotient in r1:r0, remainder in r3:r2.
    .global __aeabi_ldivmod
    .type   __aeabi_ldivmod, %function
__aeabi_ldivmod:
    push    {r4, r5}
#ifdef HLDS_PROFILE
    ldr     r12, =hlHwCount             @ (a build that measures counts them)
    ldr     r4, [r12]
    add     r4, r4, #1
    str     r4, [r12]
#endif
    ldr     r12, =REG_DIVCNT
    ldr     r4, =REG_IME
    ldrh    r5, [r4]
    strh    r4, [r4]                    @ (its own address: bit 0 clear, off)
    mov     r4, #2                      @ 64 by 64
    strh    r4, [r12]
    str     r0, [r12, #0x10]
    str     r1, [r12, #0x14]
    str     r2, [r12, #0x18]
    str     r3, [r12, #0x1C]
1:  ldrh    r4, [r12]
    tst     r4, #0x8000
    bne     1b
    ldr     r0, [r12, #0x20]
    ldr     r1, [r12, #0x24]
    ldr     r2, [r12, #0x28]
    ldr     r3, [r12, #0x2C]
    ldr     r4, =REG_IME
    strh    r5, [r4]
    pop     {r4, r5}
    bx      lr
    .size   __aeabi_ldivmod, . - __aeabi_ldivmod

@ The same, unsigned. The unit divides signed numbers, which is the same
@ thing while neither has its top bit set (nothing in the game has); for
@ the rest, the library's bit-at-a-time routine.
    .global __aeabi_uldivmod
    .type   __aeabi_uldivmod, %function
__aeabi_uldivmod:
    orrs    r12, r1, r3
    bpl     __aeabi_ldivmod
    push    {r4, lr}
    sub     sp, sp, #16                 @ its fifth argument, and the remainder
    add     r12, sp, #8
    str     r12, [sp]
    ldr     r12, =__udivmoddi4
    blx     r12
    ldr     r2, [sp, #8]
    ldr     r3, [sp, #12]
    add     sp, sp, #16
    pop     {r4, lr}
    bx      lr
    .size   __aeabi_uldivmod, . - __aeabi_uldivmod

@ uint32_t hlHwSqrt64(uint64_t value): its square root, rounded down.
    .global hlHwSqrt64
    .type   hlHwSqrt64, %function
hlHwSqrt64:
#ifdef HLDS_PROFILE
    ldr     r12, =hlHwCount
    ldr     r2, [r12, #4]
    add     r2, r2, #1
    str     r2, [r12, #4]
#endif
    ldr     r12, =REG_SQRTCNT
    ldr     r2, =REG_IME
    ldrh    r3, [r2]
    strh    r2, [r2]                    @ (0x04000208: bit 0 clear)
    str     r0, [r12, #0x08]
    str     r1, [r12, #0x0C]
    mov     r0, #1                      @ of 64 bits
    strh    r0, [r12]
1:  ldrh    r0, [r12]
    tst     r0, #0x8000
    bne     1b
    ldr     r0, [r12, #0x04]
    strh    r3, [r2]
    bx      lr
    .size   hlHwSqrt64, . - hlHwSqrt64

    .pool

#ifdef HLDS_PROFILE
@ How many divides and how many roots, since whoever reads it last cleared it.
    .section .bss.hlHwCount, "aw", %nobits
    .align  2
    .global hlHwCount
hlHwCount:
    .space  8
#endif
