#include "mem.h"
#include <stdio.h>

namespace ps1e {

MMU::MMU(MemJit& memjit) : ram(memjit), bios(memjit) {
}


u8* MMU::memPoint(u32 addr) {
  switch (addr >> 28) {
    case 0x0:
    case 0x8:
    case 0xA:
      #ifdef SAFE_MEM
      if (addr & 0x0FE0'0000) {
        error("MMU: bad mem %x", addr);
      }
      #endif
      return ram.point(addr & 0x001F'FFFF);
    
    case 0xB:
      #ifdef SAFE_MEM
      if (0 == (addr & 0xBFC0'0000)) {
        error("MMU: bad bios %x", addr);
        return 0;
      }
      if (addr & 0x0008'0000) { 
        error("MMU: bad bios %x", addr);
        return 0;
      }
      #endif
      return bios.point(addr & 0x0007'FFFF);
  }
  return 0;
}


bool MMU::loadBios(char const* filename) {
  u8* buf = bios.point(0);
  FILE* f = fopen(filename, "rb");
  if (!f) {
    ps1e::error("cannot open bios file %s\n", filename);
    return false;
  }
  auto closeFile = createFuncLocal([f] { fclose(f); debug("file closed\n"); });

  if (bios.size() != fread(buf, 1, bios.size(), f)) {
    printf(RED("cannot read bios %s\n"), filename);
    return false;
  }
  debug("Bios loaded.\n");
  return true;
}


}