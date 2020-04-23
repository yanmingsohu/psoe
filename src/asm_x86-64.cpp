#include "asm.h"
#include <stdio.h>

namespace ps1e {


#pragma pack(1)
typedef struct {
  u8  op;
  u32 addr;
} JmpOp;
#pragma pack()


void make_jump(void *mem, void *where) {
  JmpOp *jmp = (JmpOp*) mem;
  jmp->op = 0xE9;
  s64 addr64 = (s64)where - (s64)mem - 5;

  if (addr64 > 0x10000'0000 || addr64 < -0x10000'0000) {
    printf(RED("Cannot generate 64bit jmp %lx %lx\n"), mem, where);
    throw 1;
  }

  jmp->addr = (u32)(addr64);
  // printf("jmp %lx %x from %lx to %lx [%lx]\n", addr64, jmp->addr, mem, where, *((u64*)mem) );
}


}