#include <stdexcept>
#include "bus.h" 

namespace ps1e {


Bus::Bus(MMU& _mmu, IrqReceiver* _ir) : 
    mmu(_mmu), ir(_ir), dmadev{0}, dma_dpcr{0}, 
    irq_status(0), irq_mask(0), use_d_cache(false)
{
  io = new DeviceIO*[io_map_size];
  io[0] = NULL;
  for (int i=1; i<io_map_size; ++i) {
    io[i] = &nullio;
  }
}


Bus::~Bus() {
  delete [] io;
}


void Bus::bind_irq_receiver(IrqReceiver* _ir) {
  ir = _ir;
}


void Bus::set_used_dcache(bool use) {
  use_d_cache = use;
}


void Bus::bind_io(DeviceIOMapper m, DeviceIO* i) {
  if (!i) std::runtime_error("DeviceIO parm null pointer");
  if (io[static_cast<size_t>(m)] != &nullio) {
    std::runtime_error("Device IO conflict");
  }
  io[ static_cast<size_t>(m) ] = i;
}


bool Bus::set_dma_dev(DMADev* dd) {
  u32 num = static_cast<u32>(dd->number());
  if (num >= DMA_LEN) {
    return false;
  }
  dmadev[num] = dd;
}


void Bus::set_dma_dev_status() {
  for (int n = 0; n < DMA_LEN; ++n) {
    DMADev* dd = dmadev[n];
    if (!dd) continue;
    dd->set_priority((dma_dpcr.v >> (n * 4)) & 0b0111);
    change_running_state(dd);
  }
}


void Bus::send_dma_irq(DMADev* dd) {
  u32 flag_mask = (1 << static_cast<u32>(dd->number()));
  dma_irq.dd_flag |= flag_mask;
  update_irq_flag();

  if (has_dma_irq()) {
    send_irq(IrqDevMask::dma);
  }
}


void Bus::send_irq(IrqDevMask dev_irq_mask) {
  irq_status |= static_cast<u32>(dev_irq_mask);
  update_irq_to_reciver();
}


void Bus::update_irq_to_reciver() {
  if (ir) {
    ir->send_irq(irq_mask & irq_status);
  }
}


void Bus::change_running_state(DMADev* dd) {
  //debug("DMA mask (%d) %x %x %x\n", 
  //  dd->number(), dd->mask(), dma_dpcr.v, dd->mask() & dma_dpcr.v);
  if ((dd->mask() & dma_dpcr.v) == 0) {
    dd->stop();
  } else {
    dd->start();
  }
}


void Bus::write32(psmem addr, u32 val) {
  write<u32>(addr, val);
}


void Bus::write16(psmem addr, u16 val) {
  write<u16>(addr, val);
}


void Bus::write8(psmem addr, u8 val) {
  write<u8>(addr, val);
}


u32 Bus::read32(psmem addr) {
  return read<u32>(addr);
}


u16 Bus::read16(psmem addr) {
  return read<u16>(addr);
}


u8 Bus::read8(psmem addr) {
  return read<u8>(addr);
}


void Bus::show_mem_console(psmem begin, u32 len) {
  printf("|----------|-");
  for (int i=0; i<0x10; ++i) {
    printf(" -%X", (i+begin)%0x10);
  }
  printf(" | ----------------");

  for (u32 i=begin; i<len+begin; i+=16) {
    printf("\n 0x%08X| ", i);

    for (u32 j=i; j<i+16; ++j) {
      printf(" %02X", read8(j));
    }
    
    printf("   ");
    for (u32 j=i; j<i+16; ++j) {
      u8 c = read8(j);
      if (c >= 32) {
        putchar(c);
      } else {
        putchar('.');
      }
    }
  }
  printf("\n|-over-----|- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- | ----------------\n");
}

}