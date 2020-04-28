#pragma once

#include "util.h"
#include "dma.h"

namespace ps1e {

struct GpuCtrl {
  u32 tx      : 4; // 纹理页 X = ty * 64 (0~3)
  u32 ty      : 1; // 纹理页 Y = ty * 256
  u32 abr     : 2; // 半透明 0{0.5xB+0.5xF}, 1{1.0xB+1.0xF}, 2{1.0xB-1.0xF}, 3{1.0xB+0.25xF}
  u32 tp      : 2; // 纹理页颜色模式 0{4bit CLUT}, 1{8bit CLUT}, 2:{15bit}
  u32 dtd     : 1; // 1:开启抖动Dither
  u32 dfe     : 1; // 1:Draw to display area allowed, 0:prohibited
  u32 md      : 1; // 1:Apply mask bit to drawn pixels
  u32 me      : 1; // 1:No drawing to pixels with set mask bit.
  u32 _1      : 1; 
  u32 _2      : 1;
  u32 _3      : 1;

  u32 width1  : 1; // width1=1{width0{0:384}}
  u32 width0  : 2; // 屏幕宽度, width1=0{0:256, 1:320, 2:512, 3:640}
  u32 height  : 1; // 屏幕高度, 0:240, 1:480
  u32 video   : 1; // 1:PAL, 0:NTSC
  u32 isrgb24 : 1; // 1:24bit, 0:15bit
  u32 isinter : 1; // 1:交错开启(隔行扫描)
  u32 den     : 1; // 0:开启显示, 1:黑屏
  u32 _4      : 1; 
  u32 _5      : 1;
  u32 busy    : 1; // 0:gpu忙, 1:gpu空闲
  u32 img     : 1; // 1:就绪, 0:不能发送图像(packet $c0)
  u32 com     : 1; // 1:就绪, 0:不能发送指令
  u32 dma     : 2; // DMA 0:关, 1:未知, 2:CPU to GPU, 3:GPU to CPU
  u32 lcf     : 1; // 交错时 0:绘制偶数行, 1:绘制奇数行
};


struct Command {
  u16 parm;
  u16 cmd;
};


enum class CommandEnum {
  rst_gpu    = 0x00, // reset gpu, sets status to $14802000
  rst_buffer = 0x01, // reset command buffer
  rst_irq    = 0x02, // reset IRQ
  display    = 0x03, // Turn on(1)/off display
  dma        = 0x04, // DMA 0:off, 1:unknow, 2:C to G, 3 G toC
  startxy    = 0x05, // 屏幕左上角的内存区域
  setwidth   = 0x06,
  setheight  = 0x07,
  setmode    = 0x08,
  info       = 0x10,
};


class GPU : public DMADev {
public:
  static const u32 IO_DATA      = 0x1F80'1810;
  static const u32 IO_CTRL      = 0x1F80'1814;

private:
  u8 frame[0x10'0000];
  GpuCtrl ct;

public:
  GPU(Bus& bus) : DMADev(bus), ct{0} {
  }

  DmaDeviceNum number() {
    return DmaDeviceNum::gpu;
  }

  bool support(dma_chcr_dir dir) {
    //TODO: 做完gpu寄存器
    return false;
  }
};

}