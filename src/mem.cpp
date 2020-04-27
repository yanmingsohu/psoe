#include "mem.h"
#include <stdio.h>

namespace ps1e {

MMU::MMU(MemJit& memjit) : ram(memjit), bios(memjit), dmadev{0}, dma_dpcr{0} {
}


u8* MMU::memPoint(u32 addr) {
  switch (addr >> 28) {
    case 0x0:
    case 0x8:
    case 0xA:
      #ifdef SAFE_MEM
      if (addr & 0x0FE0'0000) {
        error(addr, "bad mem");
      }
      #endif
      return ram.point(addr & 0x001F'FFFF);
    
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
      return bios.point(addr & 0x0007'FFFF);
  }
  return 0;
}


void MMU::write32(psmem addr, u32 val) {
  write<u32>(addr, val);
}


void MMU::write16(psmem addr, u16 val) {
  write<u16>(addr, val);
}


void MMU::write8(psmem addr, u8 val) {
  write<u8>(addr, val);
}


u32 MMU::read32(psmem addr) {
  return read<u32>(addr);
}


u16 MMU::read16(psmem addr) {
  return read<u16>(addr);
}


u8 MMU::read8(psmem addr) {
  return read<u8>(addr);
}


void MMU::error(u32 addr, char const* cause) {
  //TODO: cpu int
  char msg[100];
  sprintf(msg, "Mem Err %s: %x\n", cause, addr);
  // throw std::runtime_error(msg);
  printf(msg);
}


bool MMU::loadBios(char const* filename) {
  u8* buf = bios.point(0);
  FILE* f = fopen(filename, "rb");
  if (!f) {
    printf(RED("cannot open bios file %s\n"), filename);
    return false;
  }
  auto closeFile = createFuncLocal([f] { fclose(f); printf("file closed\n"); });

  if (bios.size() != fread(buf, 1, bios.size(), f)) {
    printf(RED("cannot read bios %s\n"), filename);
    return false;
  }
  printf("Bios loaded.\n");
  return true;
}


bool MMU::set_dma_dev(DMADev* dd) {
  u32 num = dd->number();
  if (num >= DMA_LEN) {
    return false;
  }
  dmadev[num] = dd;
}


void MMU::set_dma_dev_status() {
  for (int n = 0; n < DMA_LEN; ++n) {
    DMADev* dd = dmadev[n];
    if (!dd) continue;
    dd->set_priority((dma_dpcr.v >> (n * 4)) & 0b0111);
    change_running_state(dd);
  }
}


void MMU::send_irq(DMADev* dd) {
  u32 flag_mask = (1 << (dd->number() + 24));
  irq.v |= flag_mask;
  if (has_irq()) {
    //TODO: send irq to cpu
  }
}


void MMU::change_running_state(DMADev* dd) {
  if ((dd->mask() & dma_dpcr.v) == 0) {
    dd->stop();
    return;
  }
  dd->start(this);
}

}