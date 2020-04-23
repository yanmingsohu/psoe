#include "mem.h"
#include <stdio.h>

namespace ps1e {

void* Mem::addrPoint(u32 addr) {
  switch (addr >> 24) {
    case 0x0:
    case 0x8:
    case 0xA:
      #ifdef SAFE_MEM
      if (addr & 0x0FE0'0000) {
        error(addr, "bad mem");
      }
      #endif
      return &ram[addr & 0x003F'FFFF];
    
    case 0xB:
      #ifdef SAFE_MEM
      if (0 == (addr & 0xBFC0'0000)) {
        error(addr, "bad bios");
        return 0;
      }
      if (addr & 0x0008'0000) { 
        error(addr, "bad bios");
        return 0;
      }
      #endif
      return &bios[addr & 0x0007'FFFF];
  }
  return 0;
}


void Mem::error(u32 addr, char const* cause) {
  //TODO: cpu int
  printf("Mem Err %s: %x", cause, addr);
}

}