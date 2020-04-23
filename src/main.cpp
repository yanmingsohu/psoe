#include <stdio.h>
#include <cstdlib>
#include "util.h"
#include "cpu.h"

using namespace ps1e;

void test();


int main() {
  if (!check_little_endian()) { 
    printf("Cannot support little endian CPU");
    return 1;
  }

  test();

  printf("ok\n\n");
  return 0;
}