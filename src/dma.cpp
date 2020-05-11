#include <stdexcept> 
#include "dma.h"
#include "mem.h"
#include "bus.h"

namespace ps1e {

void DMADev::start() {
  running = true;
}


void DMADev::stop() {
  running = false;
}


void DMADev::transport() {
  if (running == false || chcr.trigger == 0 || chcr.busy_enb == 1)
    return;

  const dma_chcr_dir dir = static_cast<dma_chcr_dir>(chcr.dir);
  if (!support(dir)) {
    warn("Dev %d not support DMA direct %d\n", number(), chcr.dir);
    return;
  }

  chcr.busy_enb = 1;
  chcr.trigger = 0;

  const u32 bytesize = blocksize << 4;
  const u32 inc = chcr.step ? -1 : 1;
  u32 ramaddr = base;

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
      u32 block_count = blocks; 
      if (dir == dma_chcr_dir::RAM_TO_DEV) {
        while (running && chcr.busy_enb && block_count > 0) {
          --block_count;
          dma_ram2dev_block(ramaddr, bytesize, inc);
          ramaddr += inc * blocksize;
        }
      } else {
        while (running && chcr.busy_enb && block_count > 0) {
          --block_count;
          dma_dev2ram_block(ramaddr, bytesize, inc);
          ramaddr += inc * blocksize;
        }
      }
    } break;

    case ChcrMode::LinkedList:
      throw std::runtime_error("not implement DMA Linked List");
      break;

    default:
      warn("Cannot support DMA mode %d\n", chcr.mode);
      break;
  }

  chcr.busy_enb = 0;
  bus.send_dma_irq(this);
}


void DMADev::dma_ram2dev_block(psmem addr, u32 bytesize, u32 inc) {
  throw std::runtime_error("not implement DMA RAM to Device");
}


void DMADev::dma_dev2ram_block(psmem addr, u32 bytesize, u32 inc) {
  throw std::runtime_error("not implement DMA Device to Mem");
}

}