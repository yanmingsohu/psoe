#include "dma.h"
#include "mem.h"

namespace ps1e {

void DMADev::start(MMU* mmu) {
  this->mmu = mmu;
  running = true;
}


void DMADev::stop() {
  running = false;
}


void DMADev::transport() {
  if (running == false || chcr.trigger == 0 || chcr.busy_enb == 1 || mmu == 0)
    return;

  if (!support( static_cast<dma_chcr_dir>(chcr.dir) )) {
    warn("Dev %d not support DMA direct %d\n", number(), chcr.dir);
    return;
  }

  chcr.busy_enb = 1;
  chcr.trigger = 0;
  // DOing soming...
  chcr.busy_enb = 0;
  mmu->send_dma_irq(this);
}

}