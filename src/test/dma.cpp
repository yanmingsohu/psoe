#include "../gpu.h"
#include "test.h"

namespace ps1e_t {
  using namespace ps1e;

  void dma_basic() {
    tsize(sizeof(DMAChcr), 4, "DMA ctrl register");
    tsize(sizeof(DMAIrq), 4, "DMA irq register");
    tsize(sizeof(DMADpcr), 4, "DMA DPCR register");
    DMAChcr c = {0};
    c.step = 1;
    eq<u32>(c.step, true, "bit is boolean");
  }


  void test_dma() {
    dma_basic();
  }

}