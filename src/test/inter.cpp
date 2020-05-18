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
  InterpreterMips t(bus);
  bus.bind_irq_receiver(&t);
  t.reset();
  //test_gpu(gpu, bus);

  int ext_count = 0;

  t.__show_interpreter = 1;
  //t.set_int_exc_point(0xbfc00404);
  int show_code = 0;
  int counter = 0;

  for (;;) {
    t.next();
    --show_code;
    ++counter;

    /*if (show_code < -100000) {
      t.__show_interpreter = 1;
      show_code = 30;
      debug("\t\t\t\tI-> %d\n", counter);
    } else if (show_code < 0) {
      t.__show_interpreter = 0;
    }*/

    if (t.has_exception()) {
      ++ext_count;
      t.__show_interpreter = 1;
      show_code = 30;
        
      info("Exception %d, Press 'Enter' Key continue\n", ext_count);
      switch (_getch()) {
        case 'r':
          printMipsReg(t.getreg());
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