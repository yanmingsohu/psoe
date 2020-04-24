#pragma once

#include "util.h"
#include "asm.h"

namespace ps1e {

union IndexTable {
  JmpOp j;
  struct {
    MovOp m;
    JmpOp ji;
  } mj;
};


template<int RamSize> class ExecMapper {
private:
  typedef void (asm_func *Run)(u32 index);

  u8 ram;
  IndexTable *native_asm;

public:
  ExecMapper() {
  }

  void exec(u32 addr) {
    Run run = native_asm[addr];
    run(addr);
  }

  u8* point(u32 addr) {
    return &ram[addr];
  }
};


class Mem {
public:
  static const int RAM_SIZE  = 0x200000;
  static const int BIOS_SIZE = 0x80000;

private:
  ExecMapper<RAM_SIZE> ram;
  ExecMapper<BIOS_SIZE> bios;

public:
  void* addrPoint(u32 virtual_addr);
  void error(u32 addr, char const* cause);
};

}