#pragma once

#include "util.h"
#include "dma.h"

namespace ps1e {

class GPU;
class Bus;


class OrderingTables : public DMADev {
private:
  static const u32 LINK_END = 0x00ff'ffff;
  GPU& gpu;
  Bus& bus;

public:
  OrderingTables(Bus&, GPU&);

  void dma_order_list(psmem addr) override;
  void dma_dev2ram_block(psmem addr, u32 bytesize, s32 inc) override;

  void start() {
    DMADev::transport();
  }
};


}