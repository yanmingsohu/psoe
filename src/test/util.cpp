#include "test.h"


namespace ps1e_t {
using namespace ps1e;


void test_mem_jit() {
  MemJit mj;
  for (int i=0; i<3; ++i) {
    void* p1  = mj.get(0x100);
    void* p11 = mj.get(0x100);
    void* p2  = mj.get(0x10000);
    void* p3  = mj.get(0x200);
    // printf("%02d %lx %lx %lx %lx\n", i, p1, p11, p2, p3);
    mj.free(p1);
    mj.free(p2);
    mj.free(p3);
    mj.free(p11);
  }
}


template<class T> void tover(T a, T b, bool over) {
  Overflow<T> o(a, b);
  bool c = o.check(a+b);
  // over = (a+b) != (T)(a+b);
  if (c != over) {
    printf("%d == %d ? %d = %d + %d\n", c, over, (T)(a+b), a, b);
    panic("overflow");
  }
}


void test_overflow() {
  tover(1, 1, false);
  tover(1, -2, false);
  tover<s8>(127, +1, true); 
  tover<s8>(-128, -1, true);
  tover<u8>(-0xFF, 2, false);
  tover<u8>(0xff, -2, false);
  tover<s8>(0xff, 1, false);
  tover<s8>(-0xff, -1, false);
  tover<s32>(0xf000'0000, 1, false);
  tover<s32>(0xffff'ffff, 1, false); 
  tover<s16>(32767, 1, true);
  tover<s8>(120, 10, true);
  tover<s8>(127, -10, false);
}


void test_util() {
  test_mem_jit();
  test_overflow();
}

}