#pragma once

#include "util.h"
#include "asm.h"
#include "dma.h"

namespace ps1e {

union IndexTable {
  JmpOp j;
  struct {
    MovOp m;
    JmpOp ji;
  } mj;
};


template<int RamSize> class ExecMapper {
private:
  typedef void (asm_func *Run)(u32 index);

  u8 *ram;
  IndexTable *instruction_entry_table;
  MemJit& memjit;

public:
  ExecMapper(MemJit& mj) : memjit(mj) {
    ram = (u8*) memjit.get(RamSize);
    instruction_entry_table = (IndexTable*) memjit.get((RamSize >> 2)+1);
  }

  ~ExecMapper() {
    memjit.free(ram);
    memjit.free(instruction_entry_table);
  }

  void exec(u32 addr) {
    Run run = instruction_entry_table[addr];
    run(addr);
  }

  u8* point(u32 addr) {
    return &ram[addr];
  }

  size_t size() {
    return RamSize;
  }
};


class MMU {
public:
  typedef u32 psmem;
  static const u32 RAM_SIZE  = 0x0020'0000;
  static const u32 BIOS_SIZE = 0x0008'0000;
  static const u32 BOOT_ADDR = 0xBFC0'0000;
  static const u32 DMA_CTRL  = 0x1F80'10F0;
  static const u32 DMA_IRQ   = 0x1F80'10F4;
  static const u32 DMA_LEN   = 8;
  static const u32 DMA_MASK  = 0x1F80'108F;

private:
  ExecMapper<RAM_SIZE> ram;
  ExecMapper<BIOS_SIZE> bios;
  DMADev* dmadev[DMA_LEN];
  DMAIrq irq;
  DMADpcr dma_dpcr;

public:
  MMU(MemJit&);
  u8* memPoint(psmem virtual_addr);
  void error(psmem addr, char const* cause);

  bool set_dma_dev(DMADev* dd);

  void write32(psmem addr, u32 val);
  void write16(psmem addr, u16 val);
  void write8(psmem addr, u8 val);
  u32 read32(psmem addr);
  u16 read16(psmem addr);
  u8 read8(psmem addr);

  bool loadBios(char const* filename);

  // dma 传输结束后被调用, 发送中断
  void send_irq(DMADev*);


  template<class T> void write(psmem addr, T v) {
    switch (addr) {
      case DMA_CTRL:
        dma_dpcr.v = v;
        set_dma_dev_status();
        return;
      case DMA_IRQ:
        irq.v = v;
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

    T* tp = (T*) memPoint(addr);
    if (tp) {
      *tp = v;
      return;
    }
    //TODO
    warn("WRIT BUS %x %x\n", addr, v);
  }


  template<class T> T read(psmem addr) {
    switch (addr) {
      case DMA_CTRL:
        return dma_dpcr.v;
      case DMA_IRQ:
        return irq.v;
    }

    if (isDMA(addr)) {
      DMADev* dd = dmadev[(addr & 0x70) >> 4];
      if (!dd) {
        warn("DMA Device Not exist %x", addr);
        return 0;
      }
      switch (addr & 0xF) {
        case 0x0:
          return (T) dd->read_base();
        case 0x4:
          return (T) dd->read_block();
        case 0x8:
          return (T) dd->read_status();
        default:
          warn("Unknow DMA address %x", addr);
      }
      return 0;
    }

    T* tp = (T*) memPoint(addr);
    if (tp) {
      return *tp;
    }
    //TODO
    warn("READ BUS %x\n", addr);
    return 0;
  }

private:
  // dma_dpcr 同步到 dma 设备上
  void set_dma_dev_status();
  // 检查状态, 满足条件则启动/停止DMA过程
  void change_running_state(DMADev* dd);

  u32 has_irq() {
    return irq.master_flag = irq.master_enable && (irq.dd_enable & irq.dd_flag);
  }

  inline bool isDMA(psmem addr) {
    return ((addr & 0xFFFF'FF80) | DMA_MASK) == 0;
  }
};

}