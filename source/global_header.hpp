#pragma once

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <utility>
#include <array>
#include <filesystem>
#include <vector>
#include <string>
#include <algorithm>
#include <optional>
// yas must be included before <nds.h>: nds.h pulls in picolibc's
// <machine/endian.h> which defines _BIG_ENDIAN as a comparison constant, and
// yas's endian detection treats `defined(_BIG_ENDIAN)` as "is big-endian".
// Including yas first (together with -D_LITTLE_ENDIAN) makes it detect correctly.
//
// The converter serializes the manifest on the host and the DS deserializes it,
// so if the two disagree about byte order it decodes to garbage without any
// error.
#include <yas/serialize.hpp>
#include <yas/std_types.hpp>

#include <nds.h>

#include <NEAMain.h>

#include "Hot.hpp"
#include <filesystem.h>
#include <fat.h>
