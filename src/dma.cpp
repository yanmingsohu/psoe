#include <stdexcept> 
#include "dma.h"
#include "mem.h"
#include "bus.h"

namespace ps1e {

DMADev::DMADev(Bus& _bus, DeviceIOMapper type0) : 
      bus(_bus), priority(0), running(false), 
      devnum(convertToDmaNumber(type0)), ctrl_io(_bus, this)
{
  _mask = 1 << (static_cast<u32>(number()) * 4);
  bus.bind_io(type0, &base_io);
  bus.bind_io(type0 + 4, &blocks_io);
  bus.bind_io(type0 + 8, &ctrl_io);
  bus.set_dma_dev(this);
}


void DMADev::start() {
  running = true;
}


void DMADev::stop() {
  running = false;
}


void DMADev::transport() {
  DMAChcr& chcr = ctrl_io.chcr;
  u32& ramaddr = base_io.base;

  if (running == false || chcr.trigger == 0 || chcr.busy_enb == 1)
    return;

  const dma_chcr_dir dir = static_cast<dma_chcr_dir>(chcr.dir);
  if (!support(dir)) {
    warn("Dev %d not support DMA direct %d\n", number(), chcr.dir);
    return;
  }

  chcr.busy_enb = 1;
  chcr.trigger = 0;

  const u32 bytesize = blocks_io.blocksize << 4;
  const u32 inc = chcr.step ? -1 : 1;

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
        while (running && chcr.busy_enb && block_count > 0) {
          --block_count;
          dma_ram2dev_block(ramaddr, bytesize, inc);
          ramaddr += inc * blocks_io.blocksize;
        }
      } else {
        while (running && chcr.busy_enb && block_count > 0) {
          --block_count;
          dma_dev2ram_block(ramaddr, bytesize, inc);
          ramaddr += inc * blocks_io.blocksize;
        }
      }
    } break;

    case ChcrMode::LinkedList:
      dma_order_list(ramaddr, bytesize, inc);
      break;

    default:
      warn("Cannot support DMA mode %d\n", chcr.mode);
      break;
  }

  chcr.busy_enb = 0;
  bus.send_dma_irq(this);
}


void DMADev::dma_order_list(psmem addr, u32 bytesize, u32 inc) {
  throw std::runtime_error("not implement DMA Linked List");
}


void DMADev::dma_ram2dev_block(psmem addr, u32 bytesize, u32 inc) {
  throw std::runtime_error("not implement DMA RAM to Device");
}


void DMADev::dma_dev2ram_block(psmem addr, u32 bytesize, u32 inc) {
  throw std::runtime_error("not implement DMA Device to Mem");
}


void DMADev::RegBase::write(u32 v) {
  base = v & 0x00FF'FFFC;
}


u32 DMADev::RegBase::read() {
  return base;
}


void DMADev::RegBlock::write(u32 v) {
  blocks = (v & 0xffff'0000) >> 16;
  blocksize = v & 0x0'FFFF;
}


u32 DMADev::RegBlock::read() {
  return blocksize | (blocks << 16);
}


void DMADev::RegCtrl::write(u32 v) {
  if (chcr.v != v) {
    bus.change_running_state(parent);
  }
  chcr.v = v;
}


u32 DMADev::RegCtrl::read() {
  return chcr.v;
}


DmaDeviceNum convertToDmaNumber(DeviceIOMapper s) {
  u32 r = (static_cast<u32>(s) - 0x1080'1080) >> 4;
  return static_cast<DmaDeviceNum>(r);
}

}