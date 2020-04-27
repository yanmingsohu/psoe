#include "test.h"
#include "../util.h"
#include "../mips.h"
#include "../disassembly.h"


namespace ps1e_t {
using namespace ps1e;

#ifdef WIN_NT
#include <conio.h>
#elif defined(LINUX) || defined(MACOS)
#include <curses.h>
#endif


void test_cpu_help() {
  printf("  h : show help\n\
  r : reset\n\
  n : run next instruction\n\
  q : quit\n\
  x : run without stop, except exception\n");
}


// https://alanhogan.com/asu/assembler.php
// https://github.com/oscourse-tsinghua/cpu-testcase
// https://github.com/YurongYou/MIPS-CPU-Test-Cases
void test_mips() {
  tsize(sizeof(instruction_st), 4, "mips instruction struct");
}


void test_bios() {
  MemJit mmjit;
  MMU mmu(mmjit);
  mmu.loadBios("demo/SCPH1000.BIN");
  DisassemblyMips t(mmu); 
  t.reset();
  test_cpu_help();

  for (;;) {
    int c = getchar();
    switch (c) {
      case 'r':
        t.reset();
        printf("reset\n");
        break;

      case 'h':
        test_cpu_help();
        break;

      case 'n':
      case 0xA:
        for (int i=10; i>0; --i) {
          t.next();
        }
        break;

      case 'f':
      case 0x20:
        break;

      case 'q':
        return;

      case 'x':
        for (;;) {
          t.next();
          if (t.has_exception()) break;
        }
        break;

      default:
        printf("Unknow %c(%x)\n", c, c);
        test_cpu_help();
        break;
    }
  }
}


void test_disassembly() {
  test_bios();
  test_mips();
}

}