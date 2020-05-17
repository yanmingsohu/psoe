#include "mem.h" 
#include <stdio.h>

namespace ps1e {

MMU::MMU(MemJit& memjit) : ram(memjit), bios(memjit), scratchpad(memjit), cc{0} {
}


u8* MMU::memPoint(u32 addr) {
  switch (addr & 0xff00'0000) {
    case 0x0000'0000:
    case 0x8000'0000:
    case 0xA000'0000:
      if (addr & 0x0FE0'0000) {
        //warn("MMU: mem out of bounds %x fix: %x\n", addr, addr & (RAM_SIZE-1));
        return 0;
      }
      return ram.point(addr & (RAM_SIZE-1));
  }

  switch (addr) {
    case 0xFFFE'0130:
      return (u8*)&cc.v;

    CASE_MEM_MIRROR(0x1F80'1000):
      return (u8*)&expansion1_base;

    CASE_MEM_MIRROR(0x1F80'1004):
      return (u8*)&expansion2_base;

    CASE_MEM_MIRROR(0x1F80'1008):
      warn("Cannot change Exp 1 delay/size\n");
      return 0;

    CASE_MEM_MIRROR(0x1F80'100C):
      warn("Cannot change Exp 3 delay/size\n");
      return 0;

    CASE_MEM_MIRROR(0x1F80'1010):
      warn("Cannot change BIOS delay/size\n");
      return 0;

    CASE_MEM_MIRROR(0x1F80'1014):
      warn("Cannot change SPU delay/size\n");
      return 0;

    CASE_MEM_MIRROR(0x1F80'1018):
      warn("Cannot change CDROM delay/size\n");
      return 0;

    CASE_MEM_MIRROR(0x1F80'101C):
      warn("Cannot change Exp 2 delay/size\n");
      return 0;
  }

  switch (addr & 0xFFF0'0000) {
    CASE_MEM_MIRROR(0xBFC0'0000):
      if (addr & 0x0038'0000) { 
        //warn("MMU: bios out of bounds %x\n", addr);
        return 0;
      }
      return bios.point(addr & (BIOS_SIZE-1));

    case 0x1F80'0000:
    case 0x9F80'0000:
      if (addr & 0x000F'800) {
        //warn("MMU: scratchpad out of bounds %x\n", addr);
        return 0;
      }
      return scratchpad.point(addr & (DCACHE_SZ-1));

    CASE_MEM_MIRROR(0x1FA0'0000):
      //warn("Expansion MEM 3 not implements");
      return 0;
  }

  #ifdef SAFE_MEM
  u32 e1 = (expansion1_base & 0x1F00'0000);
  u32 e2 = (expansion2_base & 0x1F00'0000);
  if (addr >=e1 && addr < e1+0x7'ffff) {
    warn("Expansion MEM 1 not implements\n");
  }
  else if (addr >= e2 && addr < e2+128) {
    warn("Expansion MEM 2 not implements\n");
  }
  #endif

  return 0;
}


bool MMU::loadBios(char const* filename) {
  u8* buf = bios.point(0);
  size_t rz = readFile(buf, bios.size(), filename);
  if (rz <= 0) {
    printf(RED("cannot read bios %s\n"), filename);
    return false;
  }
  return true;
}


}