#pragma once

// Where a function is put, and as what kind of code.

// HL_ARM on a function builds it as ARM code in a file that is otherwise
// Thumb. Thumb is the default because it is two thirds the size, and most of
// the program (menus, loading, saving) runs seldom; but Thumb here has no
// 32x32->64 multiply and no multiply-by-reciprocal, so every Q12 product and
// every "/ 10" in it is a call to a library routine. A function that runs
// every frame and does sums is worth the room. (A file of nothing but such
// functions is named Foo.arm.cpp instead: build.py.) tools/arm9-report.sh
// lists who calls those routines.
#define HL_ARM __attribute__((target("arm")))

// HL_ITCM on a function of an ARM file puts it in the 32 KB of fast memory,
// from where it runs at the same speed whatever else the frame has pushed
// out of the cache. See HL_HOT (HlFixed.hpp), which is the same for the
// shared code, for what earns a place.
#define HL_ITCM __attribute__((section(".itcm.text.hl")))
