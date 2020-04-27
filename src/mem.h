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
  typedef u32 psmem;
  static const int RAM_SIZE  = 0x0020'0000;
  static const int BIOS_SIZE = 0x0008'0000;
  static const int BOOT_ADDR = 0xBFC0'0000;

private:
  ExecMapper<RAM_SIZE> ram;
  ExecMapper<BIOS_SIZE> bios;

public:
  MMU(MemJit&);
  u8* memPoint(psmem virtual_addr);
  void error(psmem addr, char const* cause);

  template<class T> void write(psmem addr, T v) {
    T* tp = (T*) memPoint(addr);
    if (tp) {
      *tp = v;
      return;
    }
    //TODO
    printf("WRIT BUS %x %x\n", addr, v);
  }

  template<class T> T read(psmem addr) {
    T* tp = (T*) memPoint(addr);
    if (tp) {
      return *tp;
    }
    //TODO
    printf("READ BUS %x\n", addr);
    return 0;
  }

  void write32(psmem addr, u32 val);
  void write16(psmem addr, u16 val);
  void write8(psmem addr, u8 val);
  u32 read32(psmem addr);
  u16 read16(psmem addr);
  u8 read8(psmem addr);

  void loadBios(char const* filename);
};

}