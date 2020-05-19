#include "test.h"
#include "../mips.h"
#include "../inter.h"
#include "../cpu.h"
#include "../serial_port.h"
#include <stdio.h>

namespace ps1e_t {
using namespace ps1e;

static const u32 IO_PRINT = 0x1F80'1050;

static void test_reg() {
  tsize(sizeof(Cop0SR),     4, "cop0 sr");
  tsize(sizeof(Cop0Reg), 32*4, "cop0 reg");
  tsize(sizeof(MipsReg), 32*4, "mips reg");
  tsize(sizeof(Cop0Cause),  4, "cop0 cause");
  tsize(sizeof(Cop0Dcic),   4, "cop0 DCIC");

  Cop0SR sr = {0};

#define tr0(x, s) \
  sr.v = (1 << x); \
  if (!sr.s) { \
    printSR(sr); \
    panic("SR."#s); \
  }

  tr0(1, KUc);
  tr0(7, kx);
  tr0(19, nmi);
  tr0(25, re);
}


class BinLoader {
public:
  MemJit mmjit;
  MMU mmu;
  Bus bus;
  InterpreterMips t;
  SerialPort sio;
  MipsReg& r;
  u32 memp = 0;

  BinLoader() 
  : mmjit(), mmu(mmjit), bus(mmu), t(bus), sio(bus), r(t.getreg()) {
  }

  void w32(u32 d) {
    bus.write32(memp, d);
    memp += 4;
  }

  void i(u32 code) {
    mips_decode(code, &t);
  }

  void printMipsReg() {
    ps1e::printMipsReg(r);
  }

  void init_loader(const u16 baseaddr) {
    memp = 0xBFC0'0000;
    // 加载到内存, 使 pc 重定向
    w32(0x3c1fbfc0); // lui   $31, 0xbfc0
    w32(0x37ff0014); // ori   $31, $31, 0x0010
    w32(0x3c010000); // lui   $1, 0 
    w32(0x34220000 | baseaddr); // ori   $2 $1 0x0100
    w32(0x00400008); // jr    $2
    // 返回后跳转到这里
    w32(0x00000000); // sll   $0,$0,$0
    w32(0x40016000); // mfc0  $1, $12
    w32(0x34210001); // ori   $1, $1, 1  ; 开启中断
    w32(0x40816000); // mtc0  $1, $12
    // 调用中断
    w32(0x20020000); // addi  $v0, $0, 0
    w32(0x0000000C); // syscall
    w32(0x00000000); // nop
    w32(0x000000af); // J 0 ; 死循环, 这种指令可以让宿主机进入睡眠
    w32(0x00000000); // nop
  }

  void load(const char* binfilename, const u16 baseaddr = 0x0100) {
    u8* mem = mmu.memPoint(baseaddr);
    size_t rz = readFile(mem, 0xffff, binfilename);
    if (rz <= 0) {
      panic("Cannot read bin file");
    }

    t.reset();
    r.sp = 0x10000;

    init_loader(baseaddr);

    while (!t.has_exception()) {
      t.next();
    }

    t.next();
    printMipsReg();
  }
};


static void load_bin_exe(BinLoader& bin) {
  bin.load("ps-exe/test-cpu.bin");
}


static void test_instruction() {
  BinLoader bin;
  MipsReg& r = bin.r;

  bin.i(0x00000000); // nop
  bin.i(0x3c011f80); // lui $1 0x1F80
  eq(u32(0x1f80)<<16, r.u[1], "lui");
  bin.i(0x34221050); // ori $2 $1 0x1050
  eq(IO_PRINT, r.u[2], "ori");

  u32 a = 0xe000 << 16;
  u32 b = 0x0100 << 16;

  bin.i(0x3c03e000); // lui $3 0xE000
  bin.i(0x3c040100); // lui $4 0x0100
  bin.i(0x00642820); // add $5 $3 $4
  eq(s32(a) + s32(b), r.s[5], "add");
  bin.i(0x00643021); // addu $6 $3 $4
  eq(a + b, r.u[6], "addu");

  bin.i(0x00643822); // sub $7 $3 $4
  eq(s32(a) - s32(b), r.s[7], "sub");
  bin.i(0x00644023); // subu $8 $3 $4
  eq(u32(a - b), r.u[8], "subu");

  bin.i(0x00644818); // mul $9 $3 $4
  bin.i(0x00645019); // mulu $10 $3 $4

  //bin.printMipsReg();
  load_bin_exe(bin);
}


void test_cpu() {
  test_reg();
  test_instruction();
}

}