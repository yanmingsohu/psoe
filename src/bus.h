#pragma once

#include "util.h"
#include "dma.h"
#include "mem.h"

namespace ps1e {


enum class IrqMask : u32 {
  vblank  = 1,
  gpu     = 1 << 1,
  cdrom   = 1 << 2,
  dma     = 1 << 3,
  dotclk  = 1 << 4,
  hblank  = 1 << 5,
  sysclk  = 1 << 6,
  pad_mm  = 1 << 7,
  sio     = 1 << 8,
  spu     = 1 << 9,
  pio     = 1 << 10,
};


class Bus {
public:
  static const u32 DMA_CTRL = 0x1F80'10F0;
  static const u32 DMA_IRQ  = 0x1F80'10F4;
  static const u32 DMA_LEN  = 8;
  static const u32 DMA_MASK = 0x1F80'108F;

private:
  MMU& mmu;

  DMADev* dmadev[DMA_LEN];
  DMAIrq  dma_irq;  // DMA_IRQ
  DMADpcr dma_dpcr; // DMA_CTRL

public:
  Bus(MMU& _mmu) : mmu(_mmu), dmadev{0}, dma_dpcr{0} {}
  ~Bus() {}

  // 安装 DMA 设备
  bool set_dma_dev(DMADev* dd);

  // dma 传输结束后被调用, 发送中断
  void send_dma_irq(DMADev*);

  void write32(psmem addr, u32 val);
  void write16(psmem addr, u16 val);
  void write8(psmem addr, u8 val);
  u32 read32(psmem addr);
  u16 read16(psmem addr);
  u8 read8(psmem addr);


  template<class T> void write(psmem addr, T v) {
    switch (addr) {
      case DMA_CTRL:
        dma_dpcr.v = v;
        set_dma_dev_status();
        return;
      case DMA_IRQ:
        dma_irq.v = v;
        return;
    }

    if (isDMA(addr)) {
      u32 devnum = (addr & 0x70) >> 4;
      DMADev* dd = dmadev[devnum];
      if (!dd) {
        warn("DMA Device Not exist %x: %x", devnum, v);
        return;
      }
      switch (addr & 0xF) {
        case 0x0:
          dd->send_base(v);
          break;
        case 0x4:
          dd->send_block(v);
          break;
        case 0x8:
          if (dd->send_ctrl(v) != v) {
            change_running_state(dd);
          }
          break;
        default:
          warn("Unknow DMA address %x: %x", devnum, v);
      }
      return;
    }

    T* tp = (T*)mmu.memPoint(addr);
    if (tp) {
      *tp = v;
      return;
    }
    warn("WRIT BUS %x %x\n", addr, v);
  }


  template<class T> T read(psmem addr) {
    switch (addr) {
      case DMA_CTRL:
        return dma_dpcr.v;
      case DMA_IRQ:
        return dma_irq.v;
    }

    if (isDMA(addr)) {
      DMADev* dd = dmadev[(addr & 0x70) >> 4];
      if (!dd) {
        warn("DMA Device Not exist %x", addr);
        return 0;
      }
      switch (addr & 0xF) {
        case 0x0:
          return (T)dd->read_base();
        case 0x4:
          return (T)dd->read_block();
        case 0x8:
          return (T)dd->read_status();
        default:
          warn("Unknow DMA address %x", addr);
      }
      return 0;
    }

    T* tp = (T*)mmu.memPoint(addr);
    if (tp) {
      return *tp;
    }
    warn("READ BUS %x\n", addr);
    return 0;
  }

private:
  // dma_dpcr 同步到 dma 设备上
  void set_dma_dev_status();
  // 检查状态, 满足条件则启动/停止DMA过程
  void change_running_state(DMADev* dd);

  u32 has_dma_irq() {
    return dma_irq.master_flag = dma_irq.master_enable 
                              && (dma_irq.dd_enable & dma_irq.dd_flag);
  }

  inline bool isDMA(psmem addr) {
    return ((addr & 0xFFFF'FF80) | DMA_MASK) == 0;
  }
};

}