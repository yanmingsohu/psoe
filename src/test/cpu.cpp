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
  tr0(19, cm);
  tr0(25, re);
}


class BinLoader {
public:
  MemJit mmjit;
  MMU mmu;
  Bus bus;
  TimerSystem ti;
  R3000A t;
  SerialPort sio;
  MipsReg& r;
  u32 memp = 0;

  BinLoader() 
  : mmjit(), mmu(mmjit), bus(mmu), ti(bus), t(bus, ti), sio(bus), r(t.getreg()) {
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
    u8* mem = mmu.memPoint(baseaddr, true);
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
  // syscall 的实现改变导致失败
  //bin.load("ps-exe/test-cpu.bin");
}


static void test_lwl(BinLoader& bin) {
  bin.bus.write32(0x0, 0x11223344);
  bin.bus.write32(0x4, 0x55667788);
  bin.bus.write32(0x8, 0x99ffff99);
  bin.bus.write32(0xc, 0xff9999ff);
  bin.r.u[1] = 0xaabb'ccdd;

  //u32 a = 0x1122'3344;
  //u32 b = 0;
  //memcpy(&b, &a, 4);
  //eq(b, u32(0x44332211), "mem copy");

  // memory test 大端模式
  eq(bin.bus.read8(0), u8(0x11), "mem 1byte 0");
  eq(bin.bus.read8(1), u8(0x22), "mem 1byte 1");
  eq(bin.bus.read8(2), u8(0x33), "mem 1byte 2");
  eq(bin.bus.read8(3), u8(0x44), "mem 1byte 3");
  eq(bin.r.u[1] & 0xff, u32(0xdd), "mem1");
  eq(bin.bus.read32(0) & 0xff, u32(0x44), "mem2");

  // 大端模式
  bin.t.lwl(1, 0, 0x01);
  eq(bin.r.u[1], u32(0x223344dd), "lwl");
  bin.t.lwr(1, 0, 0x04);
  eq(bin.r.u[1], u32(0x22334455), "lwr");

  bin.t.lwl(1, 0, 0x03);
  eq(bin.r.u[1], u32(0x44334455), "lwl");
  bin.t.lwr(1, 0, 0x06);
  eq(bin.r.u[1], u32(0x44556677), "lwr");

  bin.r.u[1] = 0xaabb'ccdd;
  bin.t.swl(1, 0, 0x5);
  eq(bin.bus.read32(0x4), u32(0x55aabbcc), "swl");
  bin.t.swr(1, 0, 0x9);
  eq(bin.bus.read32(0x8), u32(0xccddff99), "swr");
}


static void test_bit_order() {
  // 小端模式有效
  union {
    u32 v;
    struct {
      u32 l : 16;
      u32 h : 16;
    };
    struct {
      u32 a1 : 1;
      u32 a2 : 1;
      u32 a3 : 1;
      u32 a4 : 1;
      u32 a5 : 1;
      u32 a6 : 1;
      u32 a7 : 1;
      u32 a8 : 1;

      u32 b1 : 1;
      u32 b2 : 1;
      u32 b3 : 1;
      u32 b4 : 1;
      u32 b5 : 1;
      u32 b6 : 1;
      u32 b7 : 1;
      u32 b8 : 1;
    };
  } t;
  t.v = 0xffee5511;
  eq(t.l, u32(0x5511), "low");
  eq(t.h, u32(0xffee), "low");
  eq(t.a1, u32(1), "a1");
  eq(t.a2, u32(0), "a2");
  eq(t.a8, u32(0), "a8");
  eq(t.b1, u32(1), "b1");
  eq(t.b8, u32(0), "b8");
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

  //test_lwl(bin);
  //bin.printMipsReg();
  load_bin_exe(bin);
}


static void test_rfe() {
  const u32 from = 0xEEEE'EEEE;
  const u32 succ = 0xEEEE'EECB;
  u32 sr = (COP0_SR_RFE_SHIFT_MASK & from) >> 2;
  eq(sr, u32(0xB), "bad right shift");
  u32 to = (COP0_SR_RFE_RESERVED_MASK & from) | sr;
  eq(to, succ, "rfe mask");

  MemJit j;
  MMU m(j);
  Bus b(m);
  TimerSystem t(b);
  R3000A c(b, t);
  b.write32(0, from);
  c.lw(1, 0, 0);
  c.mtc0(1, 12); 
  c.rfe();
  c.mfc0(2, 12);
  c.sw(2, 0, 4);
  u32 v = b.read32(4);
  eq(v, succ, "rfe fail");
}


void test_cpu() {
  test_rfe();
  test_reg();
  test_instruction();
  test_bit_order();
}

}