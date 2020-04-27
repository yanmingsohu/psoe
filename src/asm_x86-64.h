#pragma once

#include "asm.h"

namespace ps1e {
#ifdef X86_64
#pragma pack(1)

typedef struct {
  u8  op;
  u32 addr;
} JmpOp;

typedef struct {
  u8  op;
  u32 val;
} MovOp;

#pragma pack()
#endif
}