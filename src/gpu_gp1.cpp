#include "gpu.h"

namespace ps1e {


class GpuReadU32 : public IGpuReadData {
private:
  u32 data;
  bool empty;
public:
  GpuReadU32(u32 d) : data(d), empty(false) {}

  u32 read() {
    empty = true;
    return data;
  }

  bool has() {
    return !empty;
  }
};


void GPU::GP1::parseCommand(const GpuCommand c) {
  switch (c.cmd & 0b0011'1111) {
    // 重置GPU
    case 0x00:
      p.dirtyAttr().reset();
      break;

    // 复位命令缓冲器
    case 0x01:
      p.dirtyAttr().gp0.reset_fifo();
      break;

    // 确认GPU中断（IRQ1）
    case 0x02:
      p.s_irq = 0;
      break;

    // 显示使能
    case 0x03:
      p.status.display = 0x01 & c.parm;
      break;

    case 0x04:
      p.status.dma_md = 0b11 & c.parm;
      break;

    // 显示区域的开始（在VRAM中）
    case 0x05:
      p.display.x = 0x0011'1111'1111 & c.parm;
      p.display.y = 0x0001'1111'1111 & (c.parm >> 10);
      p.dirtyAttr();
      break;

    // 水平显示范围（在屏幕上）
    case 0x06:
      p.disp_hori.x = 0x1111'1111'1111 & c.parm;
      p.disp_hori.y = 0x1111'1111'1111 & (c.parm >> 12);
      p.dirtyAttr();
      break;

    // 垂直显示范围（在屏幕上)
    case 0x07:
      p.disp_veri.x = 0x0011'1111'1111 & c.parm;
      p.disp_veri.y = 0x0011'1111'1111 & (c.parm >> 10);
      p.dirtyAttr();
      break;

    // 显示模式
    case 0x08:
      p.status.width0     = 0b11 & c.parm;
      p.status.height     = 0b1  & (c.parm >> 2);
      p.status.video      = 0b1  & (c.parm >> 3);
      p.status.isrgb24    = 0b1  & (c.parm >> 4); //TODO
      p.status.isinter    = 0b1  & (c.parm >> 5);
      p.status.width1     = 0b1  & (c.parm >> 6);
      p.status.distorted  = 0b1  & (c.parm >> 7);
      p.dirtyAttr();
      break;

    case 0x09:
      p.status.text_off = 1 & c.parm;
      p.dirtyAttr();
      break;

    // 获取GPU信息
    case 0x10: 
    case 0x11: case 0x12: case 0x13: case 0x14: case 0x15: 
    case 0x16: case 0x17: case 0x18: case 0x19: case 0x1A: 
    case 0x1B: case 0x1C: case 0x1D: case 0x1E: case 0x1F: 
      switch (c.parm & 0b0111) {
        case 0x02:
          p.add(new GpuReadU32(p.text_win.v));
          break;

        case 0x03:
          p.add(new GpuReadU32(p.draw_tp_lf.v));
          break;

        case 0x04:
          p.add(new GpuReadU32(p.draw_bm_rt.v));
          break;

        case 0x05:
          p.add(new GpuReadU32(p.draw_offset.v));
          break;

        default:
          break;
      }
      break;

    case 0x20:
      debug("0x20 ? texture command");
      break;

    case 0x0B:
      debug("0x0B ? crashes command");
      break;
  }
}


void GPU::GP1::write(u32 v) {
  parseCommand(v);
}


u32 GPU::GP1::read() {
  GpuStatus s = p.status;
  s.irq_on = p.s_irq;
  s.r_dma = p.s_r_dma;
  s.r_cpu = p.s_r_cpu;
  
  switch (s.dma_md) {
    case 0x02:
      s.dma_req = p.s_r_dma;
      break;
    case 0x03:
      s.dma_req = p.s_r_cpu;
      break;
    default:
      s.dma_req = 0;
  }
  /*printf("GP1 read status %x dma_md.%x dma_req.%x r_dma.%x r_cpu.%x irq.%x\n", 
    s.v, s.dma_md, s.dma_req, s.r_dma, s.r_cpu, s.irq_on);
  ps1e_t::ext_stop = 1;*/
  return s.v;
}


GPU::GP1::GP1(GPU &_p) : p(_p) {
}


}