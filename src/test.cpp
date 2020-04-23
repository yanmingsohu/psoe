#include <string.h>
#include <stdio.h>
#include <cstdlib>
#include <sys/mman.h>

#include "cpu.h"
#include "asm.h"

using namespace ps1e;


static void panic(char const* msg) {
  printf(RED("Fail:")" %s\n", msg);
  exit(1);
}


static void tsize(int s, int t, char const* msg) {
  if (s != t) {
    char tmsg[255];
    sprintf(tmsg, "Bad %s size %d!=%d", msg, s, t);
    panic(tmsg);
  }
}


static void test_reg() {
  tsize(sizeof(Cop0SR), 4, "cop0 sr");
  tsize(sizeof(Cop0Reg), 16*4, "cop0 reg");
  tsize(sizeof(MipsReg), 32*4, "mips reg");

  Cop0SR sr = {0};

#define tr0(x, s) \
  sr.v = (1 << x); \
  if (!sr.s) { \
    printSR(sr); \
    panic("SR."#s); \
  }

  tr0(1, exl);
  tr0(7, kx);
  tr0(19, nmi);
  tr0(25, re);
}


static void jmp_to_ok(int *a, int b) {
  *a = b;
}


static void test_jmp() {
  int t1 = 0;
  int t2 = 0xfaf1;
  // u8 ok_code[100];
  // 分配一段可执行内存
  void* buffer = mmap(NULL, 100, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_PRIVATE | MAP_ANON, -1, 0);
  void* ok_code = (u8*)buffer + 50;
  // 复制函数代码到生成函数
  memcpy(ok_code, (void*)jmp_to_ok, 40);

  // 生成到生成函数的跳转
  void* jmpcode = (u8*)buffer;
  make_jump(jmpcode, ok_code);

  u8* b = (u8*) jmpcode;
  printf("OpCode: %x %02x%02x%02x%02x\n", b[0], b[1], b[2], b[3], b[4]);
  typedef void (*ok_func)(int*, int);
  ok_func f = (ok_func) jmpcode; // jmp_to_ok
  printf("=== %lx %lx\n", ok_code, jmp_to_ok);
  // 调用生成的跳转
  f(&t1, t2);

  if (t1 != t2) {
    printf("%x != %x\n", t1, t2);
    panic("Inject asm JMP fail");
  }
}


void test() {
  test_reg();
  test_jmp();
  printf("Test all passd\n");
}