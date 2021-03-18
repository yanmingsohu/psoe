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
      dma_irq.v = v & 0x00FF'FFE0; // 最高位不变
      // 写 1 清除, 0 不变
      dma_irq.v &= ~(v & IRQ_RST_FLG_BIT_MK);
      return;

    CASE_IO_MIRROR(0x1F80'1070):
      // 写入 0 清除, 1 不变
      irq_status &= v & IRQ_ST_WR_MASK;
      show_irq_msg("stat", irq_status);
      update_irq_status();
      return;

    CASE_IO_MIRROR(0x1F80'1074):
      if (irq_mask != v) {
        show_irq_msg("mask", v);
      }
      irq_mask = v;
      update_irq_status();
      return;

    CASE_IO_MIRROR(0x1f80'2041):
      warn("BIOS Boot status <%X>\n", v);
      return;

    IO_MIRRORS_STATEMENTS(CASE_IO_MIRROR_WRITE, io, v);
  }

  T* tp = (T*)mmu.memPoint(addr, false);
  if (tp) {
    *tp = v;
    return;
  }
  warn("WRIT BUS invaild %x[%d]: %x\n", addr, sizeof(T), v);
  ir->send_bus_exception();
  ps1e_t::ext_stop = 1;
}


template<class T, bool opcode> T Bus::read(psmem addr) {
  __on_read(addr);

  if ((!opcode) && use_d_cache) {
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

  T* tp = (T*)mmu.memPoint(addr, true);
  if (tp) {
    return *tp;
  }
  warn("READ BUS invaild %x[%d]\n", addr, sizeof(T));
  if (ir) ir->send_bus_exception();
  ps1e_t::ext_stop = 1;
  return 0;
}


}