#include <string.h>
#include <stdio.h>
#include <cstdlib>

#include "test.h"

using namespace ps1e;
using namespace ps1e_t;

namespace ps1e_t {

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

}


int main() {
  if (!check_little_endian()) {
    printf("Cannot support little endian CPU");
    return 1;
  }

  test_cpu();
  test_jit();
  test_util();
  test_dma();
  test_disassembly();
  printf("Test all passd\n");
  return 0;
}