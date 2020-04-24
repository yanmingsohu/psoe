#pragma once

#include "util.h"

// https://sourceforge.net/p/predef/wiki/Architectures/
#if defined(__amd64__)
  #define X86_64
  #include "asm_x86-64.h"
#elif defined(__i386__)
  #error "Cannot support x86-32bit"
#elif defined(__arm__)
  #error "Cannot support ARM"
#else
  #error "Cannot support this CPU"
#endif


namespace ps1e {

// x32:func(ecx, edx, stack...)
// x64:func(ecx, edx, r8d, r9d, stack...)
#define asm_func __cdecl

typedef int OpCodeLen;
typedef void (asm_func *AsmFuncPf)();

// Write 'jmp' code to mem[JMP_CODE_LEN], jump to 'where'
OpCodeLen make_jump(void* mem, void* where);
OpCodeLen make_mov_ecx(void *mem, u32 val);

}