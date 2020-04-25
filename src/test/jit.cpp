#include "test.h"
#include <cstdlib>

namespace ps1e_t {
using namespace ps1e;


static void jmp_to_ok(int *a, int b) {
  *a = b;
}


void test_jmp() {
  int t1 = 0;
  int t2 = 0xfaf1;
  MemBlock mb(0, 100);
  // u8 ok_code[100];
  // 分配一段可执行内存
  void* ok_code = mb.get(30);
  // 复制函数代码到生成函数
  memcpy(ok_code, (void*)jmp_to_ok, 40);

  // 生成到生成函数的跳转
  void* jmpcode = mb.get(10);
  make_jump(jmpcode, ok_code);

  u8* b = (u8*) jmpcode;
  // printf("OpCode: %x %02x%02x%02x%02x\n", b[0], b[1], b[2], b[3], b[4]);
  typedef void (*ok_func)(int*, int);
  ok_func f = (ok_func) jmpcode; // jmp_to_ok
  // printf("=== %lx %lx\n", ok_code, jmp_to_ok);
  // 调用生成的跳转
  f(&t1, t2);

  if (t1 != t2) {
    printf("%x != %x\n", t1, t2);
    panic("Inject asm JMP fail");
  }

  mb.free(ok_code);
  mb.free(jmpcode);
}


void test_jit() {
  test_jmp();
}

}