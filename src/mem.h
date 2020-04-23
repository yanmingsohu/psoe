#pragma once

#include "util.h"
#include "asm.h"

namespace ps1e {

class Mem {
public:
  static const int RAM_SIZE  = 0x200000;
  static const int BIOS_SIZE = 0x80000;

private:
  u8 ram[RAM_SIZE];
  u8 bios[BIOS_SIZE];
  u8 native[RAM_SIZE];

public:
  void* addrPoint(u32 virtual_addr);
  void error(u32 addr, char const* cause);
};

}