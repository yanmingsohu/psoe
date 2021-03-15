#include "test.h"
#include "../util.h"
#include "../mips.h"
#include "../inter.h"
#include "../spu.h"
#include "../serial_port.h"
#include "../otc.h"
#include "../cdrom.h"
#include "../time.h"
#include <conio.h>
#include <mutex>
#include <condition_variable>


namespace ps1e_t {
using namespace ps1e;

// 通过这个全局变量在任意地方停止cpu并开始调试
int ext_stop = 0;
int exit_debug = 0;
std::mutex debug_lck;
std::condition_variable wait_debug;

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
  char image[] = "ps-exe/Raiden Project, The (Europe).cue";
  char biosf[] = "H:\\EMU-console\\PlayStation1\\bios\\SCPH1000.BIN";

  MemJit mmjit;
  MMU mmu(mmjit);
  if (!mmu.loadBios(biosf)) {
    panic("load bios fail");
  }

  Bus bus(mmu);
  TimerSystem ti(bus);
  GPU gpu(bus, ti);
  SoundProcessing spu(bus);
  OrderingTables otc(bus);
  SerialPort spi(bus);
  CdDrive dri;
  dri.loadImage(image);
  CDrom cdrom(bus, dri);

  R3000A cpu(bus, ti);
  bus.bind_irq_receiver(&cpu);
  cpu.reset();
  //test_gpu(gpu, bus); //!!
  debug(cpu, bus);

  //cdrom.CmdInit();
  //cdrom.CmdMotorOn();
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

  //disa.setInterruptPC(0x8004fe54); // 在 bios 菜单中停止
  //disa.setInterruptPC(0x8003ea54); // 进入bios菜单后陷入读取cdrom状态
  //disa.setInterruptPC(0x800560e4); // 调用 cdrom setloc 后陷入

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
      if (!ext_stop) {
        for (int i=-5; i<0; ++i) disa.decode(i);
        show_code = 5;
      }
    }

    if (ext_stop || disa.isDebugInterrupt()) {
      if (show_code < 0) {
        for (int i = 30; i > 0; --i) {
          disa.decode(-i);
        }
      }

wait_input:  
      info("CPU <%d> %d >>", ext_count, counter);
      int ch = _getch();
      printf("\r                                     \r");

      switch (ch) {
        case ' ':
        case 'r':
          printMipsReg(cpu.getreg());
          goto wait_input;

        case 'x':
          ext_stop = 0;
          show_code = -1;
          wait_debug.notify_all();
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
          goto wait_input;

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
          goto wait_input;

        case 'h':
          printf("\n'r' show reg.       'x' run, hide debug.  \n");
          printf("'1' debug 100.      'a' show address value.\n");
          printf("'2' debug 10000.    '3' debug 1000000.\n");
          printf("'0' Reset.          'c' write memory.\n");
          printf("'d' set break.      'Enter' next op\n");
          printf("'h' show help.      'ESC' Exit.\n");
          goto wait_input;

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


void wait_anykey_debug() {
  printf("Press any key debug\n");
  std::unique_lock<std::mutex> lk(debug_lck);
  while (!exit_debug) {
    _getch();
    ext_stop = 1;
    wait_debug.wait(lk);
  }
}


void test_disassembly() {
  auto anykey = std::thread(wait_anykey_debug);
  test_mips_size();
  test_mips_inter();

  exit_debug = 1;
  wait_debug.notify_all();
  anykey.join();
}

}