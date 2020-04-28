#include "test.h"
#include "../util.h"
#include "../mips.h"
#include "../inter.h"


namespace ps1e_t {
using namespace ps1e;

#ifdef WIN_NT
  #include <conio.h>
  int _getc() {
    int c = _getch();
    printf("\n");
    return c;
  }
#elif defined(LINUX) || defined(MACOS)
  #include <curses.h>
  int _getc() {
    return getchar();
  }
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
  tsize(sizeof(instruction_st::R), 4, "mips instruction struct R");
  tsize(sizeof(instruction_st::J), 4, "mips instruction struct J");
  tsize(sizeof(instruction_st::I), 4, "mips instruction struct I");
  tsize(sizeof(instruction_st), 4, "mips instruction struct");
}


void test_mips_inter() {
  MemJit mmjit;
  MMU mmu(mmjit);
  if (!mmu.loadBios("demo/SCPH1000.BIN")) {
    panic("load bios fail");
  }

  Bus bus(mmu);
  InterpreterMips t(bus);
  bus.bind_irq_receiver(&t);
  t.reset();
  test_cpu_help();

  for (;;) {
    int c = _getc();
    switch (c) {
      case 'r':
        t.reset();
        info("reset\n");
        break;

      case 'h':
        test_cpu_help();
        break;

      case 'n':
      case 0xD:
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
        warn("Unknow %c(%x)\n", c, c);
        test_cpu_help();
        break;
    }
  }
}


void test_disassembly() {
  test_mips();
  test_mips_inter();
}

}