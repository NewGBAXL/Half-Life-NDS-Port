#pragma once

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <utility>
#include <array>
#include <vector>
#include <string>
#include <algorithm>
// yas's endian detection has to run before <nds.h>: nds.h pulls in picolibc's
// <machine/endian.h> which defines _BIG_ENDIAN as a comparison constant, and
// yas treats `defined(_BIG_ENDIAN)` as "is big-endian". build.py forces
// yas/detail/config/endian.hpp (with -D_LITTLE_ENDIAN) in ahead of everything
// in every file, so the files that do serialize can include yas wherever they
// like (HlYas.hpp) -- and the rest, which is most, need not pay for it.
//
// The converter serializes the manifest on the host and the DS deserializes it,
// so if the two disagree about byte order it decodes to garbage without any
// error.

#include <nds.h>

#include <NEAMain.h>

#include "Hot.hpp"
#include <filesystem.h>
#include <fat.h>
