#include <stdexcept>
#include "bus.h" 
#include "bus.inl"

namespace ps1e {


static void show_irq_curr(u32 mask) {
  //ps1e_t::ext_stop = 1;
  switch ((IrqDevMask) mask) {
    case IrqDevMask::cdrom:
      printf("CDROM irq\n");
      break;
    case IrqDevMask::vblank:
      printf("VBLANK irq\n");
      break;
    case IrqDevMask::gpu:
      printf("GPU irq\n");
      break;
    case IrqDevMask::dma:
      printf("DMA irq\n");
      break;
    case IrqDevMask::dotclk:
      printf("CLOCK irq\n");
      break;
    case IrqDevMask::hblank:
      printf("HBLANK irq\n");
      break;
    case IrqDevMask::sysclk:
      printf("SYS clock irq\n");
      break;
    case IrqDevMask::sio:
      printf("SIO irq\n");
      break;
    case IrqDevMask::pad_mm:
      printf("PAD irq\n");
      break;
    case IrqDevMask::spu:
      printf("SPU irq\n");
      break;
    case IrqDevMask::pio:
      printf("PIO irq\n");
      break;
  }
}


u32 operator&(u32 a, IrqDevMask b) {
  return a & static_cast<u32>(b);
}


void show_irq_msg(const char *msg, u32 v) {
  char i[] = {
    v & IrqDevMask::cdrom   ?'1':'.',
    v & IrqDevMask::vblank  ?'1':'.',
    v & IrqDevMask::gpu     ?'1':'.',
    v & IrqDevMask::dma     ?'1':'.',
    v & IrqDevMask::dotclk  ?'1':'.',
    v & IrqDevMask::hblank  ?'1':'.',
    v & IrqDevMask::sysclk  ?'1':'.',
    v & IrqDevMask::sio     ?'1':'.',
    v & IrqDevMask::pad_mm  ?'1':'.',
    v & IrqDevMask::spu     ?'1':'.',
    v & IrqDevMask::pio     ?'1':'.',
  };
  printf("IRQ %s: CD.%c VBk.%c GPu.%c Dma.%c Dot.%c HBk.%c Sys.%c Sio.%c Pad.%c Spu.%c Pio.%c\n",
         msg, i[0], i[1], i[2], i[3], i[4], i[5], i[6], i[7], i[8], i[9], i[10]);
}


void IrqReceiver::ready_recv_irq() {
  if (trigger == 0) {
    clr_ext_int(CpuCauseInt::hardware);
    return;
  }

  if (trigger) {
    show_irq_curr(trigger);
    set_ext_int(CpuCauseInt::hardware);
    trigger = 0;
  }
}


void IrqReceiver::send_irq(u32 i) {
  if (i == trigger) return;
  trigger = i;
}


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
  if (!i) throw std::runtime_error("DeviceIO parm null pointer");
  if (io[static_cast<size_t>(m)] != &nullio) {
    throw std::runtime_error("Device IO conflict");
  }
  io[ static_cast<size_t>(m) ] = i;
}


bool Bus::set_dma_dev(DMADev* dd) {
  u32 num = static_cast<u32>(dd->number());
  if (num >= DMA_LEN) {
    return false;
  }
  dmadev[num] = dd;
  return true;
}


void Bus::set_dma_dev_status() {
  DMADev * ready_start;
  u32 ready_pri;
  for (;;) {
    ready_start = 0;
    ready_pri = 0xff;

    // 当优先级相同, 高通道号优先运行
    for (int n = DMA_LEN-1; n >= 0; --n) {
      DMADev* dd = dmadev[n];
      if (!dd) continue;

      if (check_running_state(dd)) {
        u32 pri = (dma_dpcr.v >> (n * 4)) & 0b0111;
        if ((!ready_start) || (pri < ready_pri)) {
          ready_start = dd;
          ready_pri = pri;
          // 0 是最高优先级, 无需继续比较
          if (pri == 0) break;
        }
      }
    }

    if (ready_start) {
      ready_start->start();
    } else {
      break;
    }
  }
}


void Bus::send_dma_irq(DMADev* dd) {
  u32 flag_mask = (1 << static_cast<u32>(dd->number()));
  dma_irq.dd_flag |= flag_mask;
  update_dma_irq_flag();

  if (has_dma_irq()) {
    send_irq(IrqDevMask::dma);
  }
}


void Bus::send_irq(IrqDevMask dev_irq_mask) {
  std::lock_guard<std::mutex> lk(for_irq);
  // 上升沿触发中断
  if ((irq_status & dev_irq_mask) == 0) {
    if (irq_mask & dev_irq_mask) {
      send_irq_to_reciver(dev_irq_mask);
    }
  }
  irq_status |= static_cast<u32>(dev_irq_mask);
}


void Bus::send_irq_to_reciver(IrqDevMask i) {
  if (ir) {
    ir->send_irq(static_cast<u32>(i));
  }
}


bool Bus::check_running_state(DMADev* dd) {
  //debug("DMA mask (%d) %x %x %x\n", 
  //  dd->number(), dd->mask(), dma_dpcr.v, dd->mask() & dma_dpcr.v);
  return ((dd->mask() & dma_dpcr.v) != 0) && dd->readyTransfer();
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


u32 Bus::readOp(psmem addr) {
  return read<u32, true>(addr);
}


// 该函数为测试用, 最终可以删除其中的代码
void Bus::__on_write(psmem addr, u32 v) {
  /*if (addr == 0x1F80'1070) {
    printf("BUS write irq status %x\n", v);
  }*/
  /*else if (addr >= 0x1F801040 && addr <= 0x1F80104f) {
    printf("BUS write JOY %x = %x\n", addr, v);
  }*/
  //else if (addr >= 0x1F80'1C00 && addr <= 0x1F80'1DFC) {
  //  printf("BUS write SPU %x = %x\n", addr, v);
  //}
  if (ps1e_t::ext_stop && (0xFFFF'F000 & addr) == 0x1F80'1000) {
    printf("BUS Write IO %x %x\n", addr, v);
  }
}


// 该函数为测试用, 最终可以删除其中的代码
void Bus::__on_read(psmem addr) {
  /*if (addr == 0x1F80'1070) {
    printf("BUS read irq status %x\n", irq_status);
  }*/
  /*else if (addr >= 0x1F801040 && addr <= 0x1F80104f) {
    printf("BUS read JOY %x\n", addr);
  }*/
  //else if (addr >= 0x1F80'1C00 && addr <= 0x1F80'1DFC) {
  //  printf("BUS read SPU %x\n", addr);
  //}
  if (ps1e_t::ext_stop && (0xFFFF'F000 & addr) == 0x1F80'1000) {
    printf("BUS read IO %x\n", addr);
  }
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