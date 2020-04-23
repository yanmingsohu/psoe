#pragma once

#include <cstdint>

namespace ps1e {

// Check bound when read/write memory 
#define SAFE_MEM 
#define RED(s) "\x1b[31m" s "\033[0m"

using s8 = int8_t;
using u8 = uint8_t;
using s16 = int16_t;
using u16 = uint16_t;
using s32 = int32_t;
using u32 = uint32_t;
using s64 = int64_t;
using u64 = uint64_t;

bool check_little_endian();

}