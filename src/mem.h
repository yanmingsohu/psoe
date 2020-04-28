#pragma once

#include "util.h"
#include "asm.h"
#include "dma.h"

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

  u8 *ram;
  IndexTable *instruction_entry_table;
  MemJit& memjit;

public:
  ExecMapper(MemJit& mj) : memjit(mj) {
    ram = (u8*) memjit.get(RamSize);
    instruction_entry_table = (IndexTable*) memjit.get((RamSize >> 2)+1);
  }

  ~ExecMapper() {
    memjit.free(ram);
    memjit.free(instruction_entry_table);
  }

  void exec(u32 addr) {
    Run run = instruction_entry_table[addr];
    run(addr);
  }

  u8* point(u32 addr) {
    return &ram[addr];
  }

  size_t size() {
    return RamSize;
  }
};


class MMU {
public:
  static const u32 RAM_SIZE  = 0x0020'0000;
  static const u32 BIOS_SIZE = 0x0008'0000;
  static const u32 BOOT_ADDR = 0xBFC0'0000;

private:
  ExecMapper<RAM_SIZE> ram;
  ExecMapper<BIOS_SIZE> bios;

public:
  MMU(MemJit&);
  // 返回内存指针, 地址必须在 ram/bios 范围内
  u8* memPoint(psmem virtual_addr);
  bool loadBios(char const* filename);
};

}