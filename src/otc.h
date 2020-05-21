#pragma once

#include "util.h"
#include "dma.h"

namespace ps1e {

class Bus;


class OrderingTables : public DMADev {
private:
  Bus& bus;

public:
  static const u32 LINK_END = 0x00ff'ffff;

  OrderingTables(Bus&);

  //void dma_order_list(psmem addr) override;
  void dma_dev2ram_block(psmem addr, u32 bytesize, s32 inc) override;
};


}