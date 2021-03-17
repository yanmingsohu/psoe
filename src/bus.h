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
  static const u32 IRQ_BIT_SIZE = 11;
  static const u32 IRQ_REQUEST_BIT = (1 << IRQ_BIT_SIZE)-1;
  u32 mask;
  u32 trigger;

protected:
  IrqReceiver() : mask(0), trigger(0) {}

  // 接收方实现方法, 设置 cpu 外部中断
  virtual void set_ext_int(CpuCauseInt i) = 0;
  // 接收方实现方法, 清除 cpu 外部中断
  virtual void clr_ext_int(CpuCauseInt i) = 0;
  // 准备好接受 irq 信号后, 由接收方调用
  void ready_recv_irq();

public:
  virtual ~IrqReceiver() {}

  // 发送方调用
  void send_irq(u32 m);

  // 接收方实现, 发送总线异常
  virtual void send_bus_exception() = 0;
};


class Bus {
public:
  static const u32 DMA_LEN            = 8;
  static const u32 DMA_MASK           = 0x1F80'108F;
  static const u32 DMA_IRQ_WRITE_MASK = (1 << 24) - 1; // 24:DMAIrq::master_enable

  static const u32 IRQ_ST_WR_MASK     = (1 << 11) - 1; // 11:IrqDevMask count
  static const u32 IRQ_RST_FLG_BIT_MK = 0x7F000000;

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
  bool    use_d_cache;

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
  u32 readOp(psmem addr);

  void show_mem_console(psmem begin, u32 len = 0x20);
  void set_used_dcache(bool use);

  // 类必须有 static DeviceIOMapper 类型的 Port 成员
  template<class DIO>
  void bind_io(DIO* device) {
    bind_io(DIO::Port, device);
  }
  
  // 检查状态, dma 设备可以启动返回 true
  bool check_running_state(DMADev* dd);

  template<class T> void write(psmem addr, T v);
  template<class T, bool opcode = 0> T read(psmem addr);
  void __on_write(psmem addr, u32 v);
  void __on_read(psmem addr);

private:
  // dma_dpcr 同步到 dma 设备上
  void set_dma_dev_status();

  u32 has_dma_irq() {
    return dma_irq.master_flag;
  }

  void update_irq_flag() {
    dma_irq.master_flag = 
      dma_irq.force || (dma_irq.master_enable && (dma_irq.dd_enable & dma_irq.dd_flag));
  }
};


}