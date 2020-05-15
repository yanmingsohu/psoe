#pragma once 

#include "util.h"
#include "dma.h"
#include "mem.h"
#include "cpu.h"
#include "io.h"

namespace ps1e {



enum class IrqDevMask : u32 {
  vblank  = 1,
  gpu     = 1 << 1,
  cdrom   = 1 << 2,
  dma     = 1 << 3, // 在 bus 中, 有单独的方法发送 DMA 中断.
  dotclk  = 1 << 4,
  hblank  = 1 << 5,
  sysclk  = 1 << 6,
  pad_mm  = 1 << 7,
  sio     = 1 << 8,
  spu     = 1 << 9,
  pio     = 1 << 10,
};


class IrqReceiver {
private:
  static const u32 IRQ_REQUEST_BIT = (1 << 10);
  u32 mask;
  u32 mask_prev;

protected:
  IrqReceiver() : mask(0) {}

  // 接收方实现方法, 设置 cpu 外部中断
  virtual void set_ext_int(CpuCauseInt i) = 0;
  // 接收方实现方法, 清除 cpu 外部中断
  virtual void clr_ext_int(CpuCauseInt i) = 0;

  // 准备好接受 irq 信号后, 由接收方调用
  void ready_recv_irq() {
    if (mask == 0) {
      clr_ext_int(CpuCauseInt::hardware);
      return;
    }

    u32 m = mask & IRQ_REQUEST_BIT;
    u32 p = mask_prev & IRQ_REQUEST_BIT;
    if ((mask & IRQ_REQUEST_BIT) && NOT(mask_prev & IRQ_REQUEST_BIT)) {
      set_ext_int(CpuCauseInt::hardware);
    }
    mask_prev = mask;
  }

public:
  virtual ~IrqReceiver() {}

  // 发送方调用
  void send_irq(u32 m) {
    mask = m;
  }

  // 接收方实现, 发送总线异常
  virtual void send_bus_exception() = 0;
};


class Bus {
public:
  static const u32 DMA_LEN            = 8;
  static const u32 DMA_MASK           = 0x1F80'108F;
  static const u32 DMA_IRQ_WRITE_MASK = (1 << 24) - 1; // 24:DMAIrq::master_enable

  static const u32 IRQ_ST_WR_MASK     = (1 << 11) - 1; // 11:IrqDevMask count
  static const u32 IRQ_RST_FLG_BIT_MK = (1 << 31);

private:
  MMU& mmu;
  IrqReceiver* ir;
  DeviceIO nullio;
  DeviceIO **io;

  DMADev* dmadev[DMA_LEN];
  DMAIrq  dma_irq;
  DMADpcr dma_dpcr;
  u32     irq_status;     // I_STAT
  u32     irq_mask;       // I_MASK

  // irq_status/irq_mask 寄存器状态改变必须调用方法, 
  // 模拟硬件拉回引脚, 并把状态发送给 IrqReceiver.
  void update_irq_to_reciver();

public:
  Bus(MMU& _mmu, IrqReceiver* _ir = 0);
  ~Bus();

  // 安装 DMA 设备
  bool set_dma_dev(DMADev* dd);

  // dma 传输结束后被调用, 发送中断
  void send_dma_irq(DMADev*);

  // 发送除了 DMA 之外的设备中断
  void send_irq(IrqDevMask m);

  // 绑定 IRQ 接收器, 通常是 CPU
  void bind_irq_receiver(IrqReceiver* _ir);

  // 绑定设备的 IO, 不释放解除绑定的对象
  void bind_io(DeviceIOMapper, DeviceIO*);

  void write32(psmem addr, u32 val);
  void write16(psmem addr, u16 val);
  void write8(psmem addr, u8 val);
  u32 read32(psmem addr);
  u16 read16(psmem addr);
  u8 read8(psmem addr);


  template<class T> void write(psmem addr, T v) {
    switch (addr) {
      CASE_IO_MIRROR(0x1F80'10F0):
        dma_dpcr.v = v;
        set_dma_dev_status();
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

      IO_MIRRORS_STATEMENTS(CASE_IO_MIRROR_WRITE, io, v);
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
    warn("WRIT BUS invaild %x: %x\n", addr, v);
    ir->send_bus_exception();
  }


  template<class T> T read(psmem addr) {
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
    warn("READ BUS invaild %x\n", addr);
    ir->send_bus_exception();
    return 0;
  }

private:
  // dma_dpcr 同步到 dma 设备上
  void set_dma_dev_status();
  // 检查状态, 满足条件则启动/停止DMA过程
  void change_running_state(DMADev* dd);

  u32 has_dma_irq() {
    return dma_irq.master_flag = dma_irq.force || (dma_irq.master_enable 
                              && (dma_irq.dd_enable & dma_irq.dd_flag));
  }

  inline bool isDMA(psmem addr) {
    return ((addr & 0xFFFF'FF80) | DMA_MASK) == 0;
  }
};


}