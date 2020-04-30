#include <string.h>
#include <cstdlib>

#include "test.h"
#include "../gpu.h"

using namespace ps1e;
using namespace ps1e_t;

namespace ps1e_t {

void panic(char const* msg) {
  error("Fail: %s\n", msg);
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
  log_level = LogLevel::all;
  if (!check_little_endian()) {
    error("Cannot support little endian CPU\n");
    return 1;
  }
  
  OpenGLScope opengl;
  test_util();
  test_cpu();
  test_jit();
  test_dma();
  test_disassembly();
  info("Test all passd\n");
  return 0;
}