#pragma once

// The part of yas the game's files are written with: binary archives in
// memory, and the few standard types in them -- not <yas/serialize.hpp> and
// <yas/std_types.hpp>, which bring every archive (text, JSON) and every
// standard container with them and cost a compile as much as all the game's
// own headers together.
//
// (The archives and their flags are the same as before: what is written is
// byte for byte what it was.)

#include <yas/binary_iarchive.hpp>
#include <yas/binary_oarchive.hpp>
#include <yas/mem_streams.hpp>
#include <yas/types/std/array.hpp>
#include <yas/types/std/string.hpp>
#include <yas/types/std/vector.hpp>

// The PC writes what the DS reads: both must take the bytes the same way round
// (on the DS, <nds.h> can make yas think otherwise unless its endian check came
// first -- build.py forces it in).
static_assert(__YAS_LITTLE_ENDIAN == 1 && __YAS_BIG_ENDIAN == 0, "yas must see a little-endian machine");
