#include "test.h"

namespace ps1e_t {
using namespace ps1e;

void test_reg() {
  tsize(sizeof(Cop0SR), 4, "cop0 sr");
  tsize(sizeof(Cop0Reg), 32*4, "cop0 reg");
  tsize(sizeof(MipsReg), 32*4, "mips reg");
  tsize(sizeof(Cop0Cause), 4, "cop0 cause");
  tsize(sizeof(Cop0Dcic), 4, "cop0 DCIC");

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


void test_cpu() {
  test_reg();
}

}