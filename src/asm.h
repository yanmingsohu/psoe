#pragma once

#include "util.h"

// https://sourceforge.net/p/predef/wiki/Architectures/
#if defined(__amd64__)
  #define JMP_CODE_LEN 5
#elif defined(__i386__)
  #error "Cannot support x86-32bit"
#elif defined(__arm__)
  #error "Cannot support ARM"
#else
  #error "Cannot support this CPU"
#endif


namespace ps1e {

// Write 'jmp' code to mem[byte 5 length], jump to 'where'
void make_jump(void* mem, void* where);

}