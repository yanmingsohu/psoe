#include "test.h"
#include "../util.h"
#include "../mips.h"
#include "../inter.h"
#include "../spu.h"
#include "../serial_port.h"
#include <conio.h>


namespace ps1e_t {
using namespace ps1e;

// https://alanhogan.com/asu/assembler.php
// https://github.com/oscourse-tsinghua/cpu-testcase
// https://github.com/YurongYou/MIPS-CPU-Test-Cases
static void test_mips_size() {
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
  GPU gpu(bus);
  SoundProcessing spu(bus);
  SerialPort spi(bus);
  InterpreterMips cpu(bus);
  bus.bind_irq_receiver(&cpu);
  cpu.reset();
  DisassemblyMips disa(cpu);
  //test_gpu(gpu, bus);

  int ext_count = 0;

  //t.set_int_exc_point(0xbfc00404);
  int show_code = 10;
  int counter = 0;
  u32 address = 0;

  for (;;) {
    if (show_code > 0) {
      disa.current();
    } else if (show_code == 0) {
      info(" ... \n");
    }

    cpu.next();
    --show_code;
    ++counter;

    if (cpu.exception_counter) {
      for (int i = 30 - (show_code > 0 ? show_code : 0); i > 0; --i) {
        disa.decode(-i);
      }

      ++ext_count;
      show_code = 300;
        
      info("Exception %d >> ?", ext_count);
      int ch = _getch();
      printf("\r                \r");

      switch (ch) {
        case ' ':
        case 'r':
          printMipsReg(cpu.getreg());
          break;

        case 'x':
          cpu.exception_counter = 0;
          show_code = 0;
          break;

        case '1':
          for (int i=0; i<100; ++i) {
            disa.current();
            cpu.next();
          }
          break;

        case 'a':
          printf("Address HEX:");
          fflush(stdin);
          if (scanf("%x", &address)) {
            bus.show_mem_console(address, 0x60);
          } else {
            printf("\rFail Address Value\n");
          }
          break;

        case 'h':
          printf("\r'r' show reg.\t'x' run, hide debug.\t");
          printf("'1' debug 100.\t'a' show address value.\n");
          break;
      }
    }
  }
}


void test_disassembly() {
  test_mips_size();
  test_mips_inter();
}

}