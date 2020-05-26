#include <stdexcept> 
#include "dma.h"
#include "mem.h"
#include "bus.h"

namespace ps1e {

DMADev::DMADev(Bus& _bus, DeviceIOMapper type0) : 
      bus(_bus), devnum(convertToDmaNumber(type0)), is_transferring(false),
      base_io(this), blocks_io(this), ctrl_io(_bus, this), idle(0)
{
  _mask = 1 << (static_cast<u32>(number()) * 4 + 3);
  bus.bind_io(type0, &base_io);
  bus.bind_io(type0 + 1, &blocks_io);
  bus.bind_io(type0 + 2, &ctrl_io);
  bus.set_dma_dev(this);
}


void DMADev::start() {
  //debug("DMA(%x) start M:%x T:%x S:%x\n", devnum,
  //      ctrl_io.chcr.mode, ctrl_io.chcr.trigger, ctrl_io.chcr.start);

  switch (ctrl_io.chcr.mode) {
    case ChcrMode::Manual:
      if (ctrl_io.chcr.trigger) {
        ctrl_io.chcr.trigger = 0;
        ctrl_io.chcr.start = 1;
        transport();
        ctrl_io.chcr.start = 0;
      }
      break;

    case ChcrMode::Stream:
    case ChcrMode::LinkedList:
      if (ctrl_io.chcr.start) {
        transport();
        ctrl_io.chcr.start = 0;
      }
      break;
  }
}


bool DMADev::readyTransfer() {
  if (is_transferring) return false;
  switch (ctrl_io.chcr.mode) {
    case ChcrMode::Manual:
      return ctrl_io.chcr.trigger;

    case ChcrMode::Stream:
    case ChcrMode::LinkedList:
      return ctrl_io.chcr.start;
  }
  return false;
}


void DMADev::transport() {
  is_transferring = true;
  
  DMAChcr& chcr = ctrl_io.chcr;
  const dma_chcr_dir dir = static_cast<dma_chcr_dir>(chcr.dir);
  const s32 inc = chcr.step ? -1 : 1;
  const s32 bytesize = blocks_io.blocksize << 2;
  const s32 block_inc = chcr.step ? -bytesize : bytesize;
  u32& ramaddr = base_io.base;
  //info("DMA(%d) transport mode[%d] %dx%d\n", devnum, chcr.mode, bytesize, blocks_io.blocks);

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
        while (idle == 0 && block_count > 0) {
          --block_count;
          dma_ram2dev_block(ramaddr, bytesize, inc);
          ramaddr += block_inc;
        }
      } else {
        while (idle == 0 && block_count > 0) {
          --block_count;
          dma_dev2ram_block(ramaddr, bytesize, inc);
          ramaddr += block_inc;
        }
      }
    } break;

    case ChcrMode::LinkedList:
      if (dir == dma_chcr_dir::RAM_TO_DEV) {
        dma_order_list(ramaddr);
      } else {
        warn("Cannot support DEV to RAM on DMA(%d) Order List mode %d\n", 
             chcr.mode, devnum);
      }
      break;

    default:
      warn("Cannot support DMA mode %d\n", chcr.mode);
      break;
  }

  bus.send_dma_irq(this);
  is_transferring = false;
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
  //debug("DMA(%x) base write %x\n", parent->devnum, v);
}


u32 DMADev::RegBase::read() {
  return base;
}


void DMADev::RegBlock::write(u32 v) {
  blocks = (v & 0xffff'0000) >> 16;
  blocksize = v & 0x0'FFFF;
  //debug("DMA(%x) block write %x\n", parent->devnum, v);
}


u32 DMADev::RegBlock::read() {
  return blocksize | (blocks << 16);
}


void DMADev::RegCtrl::write(u32 v) {
  //debug("DMA(%x) ctrl write %x [old:%x]\n", parent->devnum, v, chcr.v);
  if (chcr.v != v) {
    chcr.v = v;
    if (bus.check_running_state(parent)) {
      parent->start();
    }
  }
}


u32 DMADev::RegCtrl::read() {
  //debug("DMA(%x) ctrl read %x %d\n", parent->devnum, chcr.v, parent->is_transferring);
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