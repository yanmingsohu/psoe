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

  u8* point(psmem addr) {
    return &ram[addr];
  }

  size_t size() {
    return RamSize;
  }
};


union CacheControl {
  u32 v;
  struct {
    u32             _u0 : 3;
    u32 scratchpad_enb1 : 1;
    u32             _u1 : 3;
    u32 Scratchpad_enb2 : 1;
    u32             _u2 : 1;
    u32 crash           : 1; // Crash (0=Normal, 1=Crash if code-cache enabled)
    u32             _u3 : 1;
    u32 code_cache_enb  : 1; 
  };
};


class MMU {
public:
  static const u32 RAM_SIZE  = 0x0020'0000;
  static const u32 BIOS_SIZE = 0x0008'0000;
  static const u32 BOOT_ADDR = 0xBFC0'0000;
  static const u32 DCACHE_SZ = 0x0000'0400;

private:
  ExecMapper<RAM_SIZE>  ram;
  ExecMapper<BIOS_SIZE> bios;
  ExecMapper<DCACHE_SZ> scratchpad;
  CacheControl cc;

  u32 expansion1_base = 0x1F00'0000;
  u32 expansion2_base = 0x1F80'2000;

public:
  MMU(MemJit&);
  // 返回内存指针, 地址必须在 ram/bios 范围内
  u8* memPoint(psmem virtual_addr);
  bool loadBios(char const* filename);
};

}