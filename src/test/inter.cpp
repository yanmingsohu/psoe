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
  const char *biosf[] = { 
    "H:\\EMU-console\\PlayStation1\\bios\\SCPH1000.BIN", //0
    "H:\\EMU-console\\PlayStation1\\bios\\SCPH1001.BIN", //1
    "H:\\EMU-console\\PlayStation1\\bios\\SCPH5000.BIN", //2
    "H:\\EMU-console\\PlayStation1\\bios\\SCPH5500.BIN", //3
    "H:\\EMU-console\\PlayStation1\\bios\\SCPH7001.BIN", //4
    "H:\\EMU-console\\PlayStation1\\bios\\SCPH7502.BIN", //5
    "H:\\EMU-console\\PlayStation1\\bios\\PSXONPSP660.BIN", //6
  };

  MemJit mmjit;
  MMU mmu(mmjit);
  if (!mmu.loadBios(biosf[0])) {
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
  debug_system(cpu, bus, mmu, spu);

  //cdrom.CmdInit();
  //cdrom.CmdMotorOn();
}


static bool inputHexVal(const char* msg, u32& d) {
  putchar('\n');
  printf(msg);
  if (scanf("%x", &d)) {
    return true;
  } else {
    printf("\rFail Value\n");
    while (getchar() != '\n');
    return false;
  }
}


void debug_system(R3000A& cpu, Bus& bus, MMU& mmu, SoundProcessing& spu) {
  DisassemblyMips disa(cpu);
  int ext_count = 0;
  int show_code = 10;
  u64 counter = 0;
  u32 address = 0;
  //t.set_int_exc_point(0xbfc00404);
  //cpu.set_data_rw_point(0x0011'f854, 0x00ff'ffff);
  LocalEvents event;
  auto start = std::chrono::system_clock::now();
  std::string ttybuf;

  // SCPH1000
  //disa.setInterruptPC(0x8004fe54); // 在 bios 菜单中停止
  //disa.setInterruptPC(0x8003e940); // 进入bios菜单后陷入读取cdrom状态
  //disa.setInterruptPC(0x0B0); // 调用系统函数时停止
  //disa.setInterruptPC(0x8003eb68); // 循环了 100000 次, 然后通过 8003eb48 判断<0 跳入死循环

  for (;;) {
    if (show_code > 0) {
      disa.current();
    } else if (show_code == 0) {
      info(" ... \n");
    }

    cpu.next();
    --show_code;
    ++counter;

    // pause on std_out_putchar()
    if ((cpu.getpc() == 0xA0 && cpu.getreg().t1 == 0x3C) || 
        (cpu.getpc() == 0xB0 && cpu.getreg().t1 == 0x3D)) {
      char c = char(cpu.getreg().a0);
      if (c == '\n' || c == '\r' || c == 0) {
        printf("\x1b[30m\x1b[47m TTY(%d): %s \033[m\n", c, ttybuf.c_str());
        ttybuf.clear();
      } else {
        ttybuf += c;
      }
    }

    if (cpu.exception_counter) {
      debug("Exception<%d> %d ON PC %x\n", ext_count, counter, cpu.getepc());
      ext_count += cpu.exception_counter;
      cpu.exception_counter = 0;
      // 打印前后 5 条指令
      /*if (!ext_stop) {
        for (int i=-5; i<0; ++i) disa.decode(i);
        show_code = 5;
      }*/
    }

    if (ext_stop || disa.isDebugInterrupt()) {
      if (show_code < 0) {
        for (int i = 1; i > 0; --i) {
          disa.decode(-i);
        }
      }

wait_input:  
      info("CPU <%d> %d >> ", ext_count, counter);
      fflush(stdin);
      int ch = _getch();
      putchar('\r');

      switch (ch) {
        case ' ':
        case 'r':
          printMipsReg(cpu.getreg());
          printSR(cpu.getcop0().sr);
          goto wait_input;

        case 'x':
          ext_stop = 0;
          start = std::chrono::system_clock::now();
          counter = 0;
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

        case 'b':
          if (inputHexVal("Break Address Hex:", address)) {
            disa.setInterruptPC(address);
          } else {
            disa.setInterruptPC(0);
          }
          break;

        case 'w':
          if (inputHexVal("Address HEX:", address)) {
            u32 v;
            if (inputHexVal("Velue HEX:", v)) {
              bus.write32(address, v);
              printf("change [%08x]: %x\n", address, v);
            }
          }
          goto wait_input;

        case 's':
          if (inputHexVal("Address HEX:", address)) {
            print_hex("SPU MEM", spu.get_spu_mem()+address, 128, address-s32(spu.get_spu_mem()));
          }
          goto wait_input;

        case 'd':
          for (u32 i=1; i<=u32(SpuChVarFlag::__end); ++i) {
            spu.print_var(SpuChVarFlag(i));
          }
          goto wait_input;

        case 'm':
          bus.write32(0x1F80'1D8C, 0xffff'ffff); // Koff
          goto wait_input;

        case 'h': {
          PrintfBuf pb;
          pb.putchar('\n');
          pb.printf("'r' show reg     'x' run, hide debug     's' dump spu memory\n");
          pb.printf("'1' debug 100    'a' show address value  'm' stop all sound\n");
          pb.printf("'2' debug 10000  '3' debug 1000000       'd' show spu register\n");
          pb.printf("'0' Reset        'w' write memory        \n");
          pb.printf("'b' set break    'Enter' next op         \n");
          pb.printf("'h' show help    'ESC' Exit              \n");
          goto wait_input;
          }

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
    else if ((counter & 0x3F'FFFF) == 0) {
      auto end = std::chrono::system_clock::now();
      std::chrono::duration<double> elapsed_sec = end-start;
      u32 freq = counter / elapsed_sec.count() / 1e6;
      printf("\rPC.%08x %09llu %02uMhz\r", cpu.getpc(), counter, freq);
      event.systemEvents();
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