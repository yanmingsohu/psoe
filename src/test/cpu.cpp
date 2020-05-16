#include "test.h"
#include "../mips.h"
#include "../inter.h"
#include "../cpu.h"

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


static void i(InterpreterMips& t, u32 code) {

}


static void test_instruction() {
  MemJit mmjit;
  MMU mmu(mmjit);
  Bus bus(mmu);
  InterpreterMips t(bus);
  MipsReg& r = t.getreg();

#define i(x) mips_decode(x, &t)

  i(0x00000000); // nop
  i(0x3c011f80); // lui $1 0x1F80
  eq(u32(0x1f80)<<16, r.u[1], "lui");
  i(0x34221050); // ori $2 $1 0x1050
  eq(IO_PRINT, r.u[2], "ori");

  u32 a = 0xe000 << 16;
  u32 b = 0x0100 << 16;

  i(0x3c03e000); // lui $3 0xE000
  i(0x3c040100); // lui $4 0x0100
  i(0x00642820); // add $5 $3 $4
  eq(s32(a) + s32(b), r.s[5], "add");
  i(0x00643021); // addu $6 $3 $4
  eq(a + b, r.u[6], "addu");

  i(0x00643822); // sub $7 $3 $4
  eq(s32(a) - s32(b), r.s[7], "sub");
  i(0x00644023); // subu $8 $3 $4
  eq(u32(a - b), r.u[8], "subu");

  i(0x00644818); // mul $9 $3 $4
  i(0x00645019); // mulu $10 $3 $4

  printMipsReg(r);
}


void test_cpu() {
  test_reg();
  test_instruction();
}

}