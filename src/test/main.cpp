#include <string.h>
#include <stdio.h>
#include <cstdlib>

#include "test.h"

namespace ps1e_t {

using namespace ps1e;


void panic(char const* msg) {
  printf(RED("Fail:")" %s\n", msg);
  exit(1);
}


void tsize(int s, int t, char const* msg) {
  if (s != t) {
    char tmsg[255];
    sprintf(tmsg, "Bad %s size %d!=%d", msg, s, t);
    panic(tmsg);
  }
}


void test() {
  test_cpu();
  test_jit();
  test_util();
  test_disassembly();
  printf("Test all passd\n");
}

}