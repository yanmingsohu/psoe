#include "dma.h"
#include "mem.h"

namespace ps1e {

bool DMADev::start(MMU* mmu) {
  if (chcr.trigger != 1 || chcr.busy_enb != 0)
    return false;
  chcr.busy_enb = 1;
  chcr.trigger = 0;
  // DOing soming
  chcr.busy_enb = 0;
  mmu->send_irq(this);
  return true;
}

}