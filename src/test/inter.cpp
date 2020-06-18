#include "test.h"
#include "../util.h"
#include "../mips.h"
#include "../inter.h"
#include "../spu.h"
#include "../serial_port.h"
#include "../otc.h"
#include <conio.h>


namespace ps1e_t {
using namespace ps1e;

// 通过这个全局变量在任意地方停止cpu并开始调试
int ext_stop = 0;

// https://alanhogan.com/asu/assembler.php
// https://github.com/oscourse-tsinghua/cpu-testcase
// https://github.com/YurongYou/MIPS-CPU-Test-Cases
static void test_mips_size() {
  tsize(sizeof(instruction_st::R), 4, "mips instruction struct R");
  tsize(sizeof(instruction_st::J), 4, "mips instruction struct J");
  tsize(sizeof(instruction_st::I), 4, "mips instruction struct I");
  tsize(sizeof(instruction_st), 4, "mips instruction struct");
}


static void test_mips_inter() {
  MemJit mmjit;
  MMU mmu(mmjit);
  if (!mmu.loadBios("ps-exe/SCPH1000.BIN")) {
    panic("load bios fail");
  }

  Bus bus(mmu);
  GPU gpu(bus);
  SoundProcessing spu(bus);
  OrderingTables otc(bus);
  SerialPort spi(bus);
  R3000A cpu(bus);
  bus.bind_irq_receiver(&cpu);
  cpu.reset();
  //test_gpu(gpu, bus);
  debug(cpu, bus);
}


static bool inputHexVal(const char* msg, u32& d) {
  printf(msg);
  if (scanf("%x", &d)) {
    return true;
  } else {
    printf("\rFail Value\n");
    while (getchar() != '\n');
    return false;
  }
}


void debug(R3000A& cpu, Bus& bus) {
  DisassemblyMips disa(cpu);
  int ext_count = 0;
  int show_code = 10;
  u32 counter = 0;
  u32 address = 0;
  //t.set_int_exc_point(0xbfc00404);
  //cpu.set_data_rw_point(0x0011'f854, 0x00ff'ffff);
  LocalEvents event;

  for (;;) {
    if (show_code > 0) {
      disa.current();
    } else if (show_code == 0) {
      info(" ... \n");
    } else if ((show_code & 0x1FFFFF) == 0) {
      printf("\rPC.%x %d\r", cpu.getpc(), counter);
      event.systemEvents();
    }

    cpu.next();
    --show_code;
    ++counter;

    if (cpu.exception_counter) {
      info("Exception<%d> %d\n", ext_count, counter);
      ext_count += cpu.exception_counter;
      cpu.exception_counter = 0;
      // 打印前后 5 条指令
      for (int i=-5; i<0; ++i) disa.decode(i);
      show_code = 5;
    }

    if (ext_stop || disa.isDebugInterrupt()) {
      if (show_code < 0) {
        for (int i = 30; i > 0; --i) {
          disa.decode(-i);
        }
      }
        
      info("CPU <%d> %d >>", ext_count, counter);
      int ch = _getch();
      printf("\r                                     \r");

      switch (ch) {
        case ' ':
        case 'r':
          printMipsReg(cpu.getreg());
          break;

        case 'x':
          ext_stop = 0;
          show_code = -1;
          break;

        case '0':
          cpu.reset();
          printf("RESET\n");
          break;

        case '1':
          show_code = 100;
          break;

        case '2':
          show_code = 10000;
          break;

        case '3':
          show_code = 1000000;
          break;

        case 'a':
          if (inputHexVal("Address HEX:", address)) {
            bus.show_mem_console(address, 0x60);
          }
          break;

        case 'd':
          if (inputHexVal("Stop Address Hex:", address)) {
            disa.setInterruptPC(address);
          } else {
            disa.setInterruptPC(0);
          }
          break;

        case 'c':
          if (inputHexVal("Address HEX:", address)) {
            u32 v;
            if (inputHexVal("Velue HEX:", v)) {
              bus.write32(address, v);
              printf("change [%08x]: %x\n", address, v);
            }
          }
          break;

        case 'h':
          printf("\r'r' show reg.\t'x' run, hide debug.\t");
          printf("'1' debug 100.\t'a' show address value.\n");
          printf("'2' debug 10000.\t'3' debug 1000000.\n");
          printf("'0' Reset.\t'c' change mem.");
          printf("'h' show help.\t'ESC' Exit.\n");
          break;

        case 'q':
        case 0x1b: // ESC
          printf("Exit.\n");
          return;

        default:
          show_code = 100000;
          ext_stop = 1;
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