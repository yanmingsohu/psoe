#include "util.h"

namespace ps1e {

bool check_little_endian() {
  union {
    u16 a;
    struct {
      u8  l;
      u8  h;
    };
  } test;
  test.a = 0x0100;
  return test.h;
}

}