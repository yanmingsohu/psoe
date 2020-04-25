#include <stdio.h>
#include <cstdlib>
#include "util.h"
#include "cpu.h"
#include "test/test.h"

using namespace ps1e;


int main() {
  if (!check_little_endian()) { 
    printf("Cannot support little endian CPU");
    return 1;
  }

  ps1e_t::test();

  printf("ok\n\n");
  return 0;
}