#pragma once

#include "bus.h"

namespace ps1e {

template<class T> void Bus::write(psmem addr, T v) {
  __on_write(addr, v);

  if (use_d_cache) {
    T* p = (T*) mmu.d_cache(addr);
    if (p) {
      *p = v;
      return;
    }
  }

  switch (addr) {
    CASE_IO_MIRROR(0x1F80'10F0):
      dma_dpcr.v = v;
      set_dma_dev_status();
      //debug("BUS::dma write status %x\n", v);
      return;

    CASE_IO_MIRROR(0x1F80'10F4):
      dma_irq.v = v;
      if (v & IRQ_RST_FLG_BIT_MK) { 
        dma_irq.dd_flag = 0;
      }
      return;

    CASE_IO_MIRROR(0x1F80'1070):
      irq_status &= v & IRQ_ST_WR_MASK;
      update_irq_to_reciver();
      return;

    CASE_IO_MIRROR(0x1F80'1074):
      irq_mask = v;
      update_irq_to_reciver();
      return;

    CASE_IO_MIRROR(0x1f80'2041):
      warn("BIOS Boot status <%X>\n", v);
      return;

    IO_MIRRORS_STATEMENTS(CASE_IO_MIRROR_WRITE, io, v);
  }

  T* tp = (T*)mmu.memPoint(addr);
  if (tp) {
    *tp = v;
    return;
  }
  warn("WRIT BUS invaild %x[%d]: %x\n", addr, sizeof(T), v);
  ir->send_bus_exception();
}


template<class T> T Bus::read(psmem addr) {
  __on_read(addr);

  if (use_d_cache) {
    T* p = (T*) mmu.d_cache(addr);
    if (p) return *p;
  }

  switch (addr) {
    CASE_IO_MIRROR(0x1F80'10F0):
      return dma_dpcr.v;

    CASE_IO_MIRROR(0x1F80'10F4):
      return dma_irq.v;

    CASE_IO_MIRROR(0x1F80'1070):
      return irq_status;

    CASE_IO_MIRROR(0x1F80'1074):
      return irq_mask;

    IO_MIRRORS_STATEMENTS(CASE_IO_MIRROR_READ, io, NULL);
  }

  T* tp = (T*)mmu.memPoint(addr);
  if (tp) {
    return *tp;
  }
  warn("READ BUS invaild %x[%d]\n", addr, sizeof(T));
  ir->send_bus_exception();
  return 0;
}


}