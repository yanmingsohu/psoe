#include "bus.h"

namespace ps1e {


bool Bus::set_dma_dev(DMADev* dd) {
  u32 num = dd->number();
  if (num >= DMA_LEN) {
    return false;
  }
  dmadev[num] = dd;
}


void Bus::set_dma_dev_status() {
  for (int n = 0; n < DMA_LEN; ++n) {
    DMADev* dd = dmadev[n];
    if (!dd) continue;
    dd->set_priority((dma_dpcr.v >> (n * 4)) & 0b0111);
    change_running_state(dd);
  }
}


void Bus::send_dma_irq(DMADev* dd) {
  u32 flag_mask = (1 << (dd->number() + 24));
  dma_irq.v |= flag_mask;
  if (has_dma_irq()) {
    //TODO: send irq to cpu
  }
}


void Bus::change_running_state(DMADev* dd) {
  if ((dd->mask() & dma_dpcr.v) == 0) {
    dd->stop();
    return;
  }
  dd->start();
}


void Bus::write32(psmem addr, u32 val) {
  write<u32>(addr, val);
}


void Bus::write16(psmem addr, u16 val) {
  write<u16>(addr, val);
}


void Bus::write8(psmem addr, u8 val) {
  write<u8>(addr, val);
}


u32 Bus::read32(psmem addr) {
  return read<u32>(addr);
}


u16 Bus::read16(psmem addr) {
  return read<u16>(addr);
}


u8 Bus::read8(psmem addr) {
  return read<u8>(addr);
}

}