#include <stdexcept> 
#include "dma.h"
#include "mem.h"
#include "bus.h"

namespace ps1e {

DMADev::DMADev(Bus& _bus, DeviceIOMapper type0) : 
      bus(_bus), priority(0), running(false), 
      devnum(convertToDmaNumber(type0)), 
      base_io(this), blocks_io(this), ctrl_io(_bus, this)
{
  _mask = 1 << (static_cast<u32>(number()) * 4 + 3);
  bus.bind_io(type0, &base_io);
  bus.bind_io(type0 + 1, &blocks_io);
  bus.bind_io(type0 + 2, &ctrl_io);
  bus.set_dma_dev(this);
}


void DMADev::start() {
  running = true;
  //debug("DMA start\n");
}


void DMADev::stop() {
  running = false;
  //debug("DMA stop\n");
}


void DMADev::transport() {
  if (!running) return;
  debug("DMA transport (%d) mode[%d]\n", devnum, ctrl_io.chcr.mode);
  
  DMAChcr& chcr = ctrl_io.chcr;
  const dma_chcr_dir dir = static_cast<dma_chcr_dir>(chcr.dir);

  switch (chcr.mode) {
    case ChcrMode::Manual:
      if (!chcr.trigger) {
        return;
      }
      chcr.trigger = 0;
      break;

    case ChcrMode::Stream:
    case ChcrMode::LinkedList:
      if (!chcr.start) {
        return;
      }
      break;
  }
  
  const s32 inc = chcr.step ? -1 : 1;
  const s32 bytesize = blocks_io.blocksize << 2;
  const s32 block_inc = chcr.step ? -bytesize : bytesize;
  u32& ramaddr = base_io.base;

  // DOing soming...
  switch (chcr.mode) {
    case ChcrMode::Manual:
      if (dir == dma_chcr_dir::RAM_TO_DEV) {
        dma_ram2dev_block(ramaddr, bytesize, inc);
      } else {
        dma_dev2ram_block(ramaddr, bytesize, inc);
      }
      break;

    case ChcrMode::Stream: {
      u32 block_count = blocks_io.blocks; 
      if (dir == dma_chcr_dir::RAM_TO_DEV) {
        while (running && chcr.start && block_count > 0) {
          --block_count;
          dma_ram2dev_block(ramaddr, bytesize, inc);
          ramaddr += block_inc;
        }
      } else {
        while (running && chcr.start && block_count > 0) {
          --block_count;
          dma_dev2ram_block(ramaddr, bytesize, inc);
          ramaddr += block_inc;
        }
      }
      chcr.start = 0;
    } break;

    case ChcrMode::LinkedList:
      if (dir == dma_chcr_dir::DEV_TO_RAM) {
        dma_order_list(ramaddr);
      } else {
        warn("Cannot support Direct on DMA Order List mode %d\n", chcr.mode);
      }
      chcr.start = 0;
      break;

    default:
      warn("Cannot support DMA mode %d\n", chcr.mode);
      break;
  }

  running = false;
  bus.send_dma_irq(this);
}


void DMADev::dma_order_list(psmem addr) {
  throw std::runtime_error("not implement DMA Linked List");
}


void DMADev::dma_ram2dev_block(psmem addr, u32 bytesize, s32 inc) {
  throw std::runtime_error("not implement DMA RAM to Device");
}


void DMADev::dma_dev2ram_block(psmem addr, u32 bytesize, s32 inc) {
  throw std::runtime_error("not implement DMA Device to RAM");
}


void DMADev::RegBase::write(u32 v) {
  base = v & 0x00FF'FFFC;
  debug("DMA base write (%x) %x\n", parent->devnum, v);
}


u32 DMADev::RegBase::read() {
  return base;
}


void DMADev::RegBlock::write(u32 v) {
  blocks = (v & 0xffff'0000) >> 16;
  blocksize = v & 0x0'FFFF;
  debug("DMA block write (%x) %x\n", parent->devnum, v);
}


u32 DMADev::RegBlock::read() {
  return blocksize | (blocks << 16);
}


void DMADev::RegCtrl::write(u32 v) {
  if (chcr.v != v) {
    bus.change_running_state(parent);
  }
  chcr.v = v;
  debug("DMA ctrl write (%x) %x start:%d trigger:%d\n", 
    parent->devnum, v, chcr.start, chcr.trigger);
}


u32 DMADev::RegCtrl::read() {
  return chcr.v;
}


DmaDeviceNum convertToDmaNumber(DeviceIOMapper s) {
  switch (s) {
    case DeviceIOMapper::dma_mdec_in_base:
      return DmaDeviceNum::MDECin;
    case DeviceIOMapper::dma_mdec_out_base:
      return DmaDeviceNum::MDECout;
    case DeviceIOMapper::dma_gpu_base:
      return DmaDeviceNum::gpu;
    case DeviceIOMapper::dma_cdrom_base:
      return DmaDeviceNum::cdrom;
    case DeviceIOMapper::dma_spu_base:
      return DmaDeviceNum::spu;
    case DeviceIOMapper::dma_pio_base:
      return DmaDeviceNum::pio;
    case DeviceIOMapper::dma_otc_base:
      return DmaDeviceNum::otc;
    default:
      throw std::runtime_error("Not DMA io base");
  }
}

}