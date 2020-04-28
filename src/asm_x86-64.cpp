#include "asm.h"
#include <stdio.h>


namespace ps1e {
#ifdef X86_64


OpCodeLen make_jump(void *mem, void *where) {
  JmpOp *jmp = (JmpOp*) mem;
  jmp->op = 0xE9;
  s64 addr64 = (s64)where - (s64)mem - 5;

  if (addr64 > 0x10000'0000 || addr64 < -0x10000'0000) {
    printf(RED("Cannot generate 64bit jmp %zx %zx\n"), mem, where);
    throw 1;
  }

  jmp->addr = (u32)(addr64);
  // printf("jmp %lx %x from %lx to %lx [%lx]\n", addr64, jmp->addr, mem, where, *((u64*)mem) );
  return sizeof(JmpOp);
}


OpCodeLen make_mov_ecx(void *mem, u32 val) {
  MovOp *mov = (MovOp*) mem;
  mov->op = 0xB9;
  mov->val = val;
  return sizeof(MovOp);
}

#endif
}