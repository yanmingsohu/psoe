#include "otc.h"
#include "gpu.h"
#include "bus.h"

namespace ps1e {


OrderingTables::OrderingTables(Bus& b) : 
    DMADev(b, DeviceIOMapper::dma_otc_base), bus(b) {
}

// 这个方法要在 gpu 中实现
//void OrderingTables::dma_order_list(psmem addr) {
//  u32 header = bus.read32(addr);
//  
//  while ((header & LINK_END) == LINK_END) {
//    u32 next = header & 0x001f'fffc;
//    u32 size = header >> 24;
//
//    for (int i=0; i<size; ++i) {
//      addr += 4;
//      u32 cmd = bus.read32(addr);
//      gpu.write_gp0(cmd);
//    }
//
//    header = bus.read32(next);
//  }
//}


void OrderingTables::dma_dev2ram_block(psmem addr, u32 bytesize, s32 inc) {
  const int len = (bytesize >>2) - 1;
  const int step = inc * 4;
  debug("OTC start %x %x %x\n", addr, bytesize, step);

  for (int i = 0; i < len; ++i) {
    psmem next_addr = (addr + step) & 0x1f'fffc;
    bus.write32(addr, next_addr);
    addr = next_addr;
    //debug("OTC block %d %x ", i, addr);
  }
  bus.write32(addr, LINK_END);
  debug("--OTCOVER--\n");
}


}