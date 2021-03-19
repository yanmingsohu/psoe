#pragma once

#include "util.h"
#include "io.h"
#include "bus.h"

namespace ps1e {

// 512K
#define SPU_MEM_SIZE        0x8'0000 
#define SPU_FIFO_SIZE       0x20
#define SPU_FIFO_MASK       (SPU_FIFO_SIZE-1)
#define SPU_FIFO_INDEX(x)   ((x) & SPU_FIFO_MASK)
#define not_aligned_nosupport(x) error("Cannot read SPU IO %u, Memory misalignment\n", u32(x))


class SoundProcessing;


//
// Bit0-1的可能组合为:
//  Code 0 = Normal     (continue at next 16-byte block)
//  Code 1 = End+Mute   (jump to Loop-address, set ENDX flag, Release, Env=0000h)
//  Code 2 = Ignored    (same as Code 0)
//  Code 3 = End+Repeat (jump to Loop-address, set ENDX flag)
union AdpcmFlag {
  u8 v;
  struct {
    u8 loop_end : 1;    //00 (0=No change, 1=Set ENDX flag and Jump to [1F801C0Eh+N*10h])
    u8 loop_repeat: 1;  //01 (0=Force Release and set ADSR Level to Zero; only if Bit0=1)
    u8 loop_start: 1;   //02 (0=No change, 1=Copy current address to [1F801C0Eh+N*10h])
    u8 unknow : 5;      //03-07
  };
};


union AdpcmData {
  u8 v;
  struct {
    u8 s0 : 4; // LSBs = n+1 Sample
    u8 s1 : 4; // MSBs = n+2 Sample, 每个数据包总共 28 个采样
  };
};


// ADPCM 样本数据, 16字节一个块
// 0x00         | 0x01      | 0x02-0x0F
// Shift/Filter | Flag Bits | Compressed Data (LSBs=1st Sample, MSBs=2nd Sample)
struct AdpcmBlock {
  u8 filter;
  AdpcmFlag flag;
  AdpcmData data[14];
};


union SpuReg {
  u32 v;
  struct {
    u32 low : 16;
    u32 hi  : 16;
  };
};


union ADSRReg {
  u32 v;
  struct {
    u32 low : 16; // 1F801C08h+N*10h
    u32 hi  : 16; // 1F801C0Ah+N*10h
  };
  struct {
    u32 su_lv : 4; //00-03 延音 (0..0Fh)  ;Level=(N+1)*800h
    u32 de_sh : 4; //04-07 衰减 (0..0Fh = Fast..Slow) 固定 "-8", 总是指数
    u32 at_st : 2; //08-09 音头 step (0..3 = "-7,-6,-5,-4") 总是减
    u32 at_sh : 5; //11-14 音头 (0..1Fh = Fast..Slow)
    u32 at_md : 1; //15    音头 (0=Linear, 1=Exponential) 
    u32 re_sh : 5; //16-20 释放 (0..1Fh = Fast..Slow) 固定 -8 直到 0
    u32 re_md : 1; //21    释放 (0=Linear, 1=Exponential) 
    u32 su_st : 2; //22-23 延音 (0..3 = "+7,+6,+5,+4" or "-8,-7,-6,-5") (inc/dec)
    u32 su_sh : 5; //24-28 延音 (0..1Fh = Fast..Slow)
    u32 __    : 1; //29         (should be zero)
    u32 su_di : 1; //30    延音 (0=Increase, 1=Decrease) (until Key OFF flag)
    u32 su_md : 1; //31    延音 (0=Linear, 1=Exponential)
  };
};


union VolData {
  u16 v;
  struct {
    u16 vol  : 15; // Voice volume/2
    u16 type : 1;  // 0 模式, vol 为音量
  };
  struct {
    u16 step  : 2; //00-01 (0..3 = "+7,+6,+5,+4" or "-8,-7,-6,-5") (inc/dec)
    u16 shift : 5; //02-06 (0..1Fh = Fast..Slow)
    u16 _z    : 5; //07-11 0
    u16 phase : 1; //12    (0=Positive, 1=Negative)
    u16 dir   : 1; //13    (0=Increase, 1=Decrease)
    u16 mode  : 1; //14    (0=Linear, 1=Exponential)
    u16 _type : 1; //15
  };
};


union VolumnReg {
  u32 v;
  struct {
    u32 low : 16;
    u32 hi  : 16;
  };
  struct {
    VolData left;
    VolData right;
  };
};


enum class SpuDmaDir : u8 {
  Stop = 0,
  ManualWrite = 1,
  DMAwrite = 2,
  DMAread = 3,
};


union SpuCntReg {
  u32 v;
  struct {
    u32 low : 16;
    u32 hi  : 16;
  };
  struct {
    u32 cda_enb : 1; //00    (0=Off, 1=On) (for CD-DA and XA-ADPCM)
    u32 exa_enb : 1; //01 
    u32 cda_rev : 1; //02
    u32 exa_rev : 1; //03
    u32 dma_trs : 2; //04-05 (0=Stop, 1=ManualWrite, 2=DMAwrite, 3=DMAread)
    u32 irq_enb : 1; //06    IRQ9 Enable (0=关闭/应答, 1=启用; only when Bit15=1)
    u32 mst_rev : 1; //07    (0=Disabled, 1=Enabled)
    u32 nos_stp : 2; //08-09 (0..03h = Step "4,5,6,7")
    u32 nos_shf : 4; //10-13 (0..0Fh = Low .. High Frequency)
    u32 mute    : 1; //14    (0=Mute, 1=Unmute)  (Don't care for CD Audio)
    u32 spu_enb : 1; //15    (0=Off, 1=On)       (Don't care for CD Audio)
  };
};


union SpuStatusReg {
  u32 v;
  struct {
    u32 low : 16;
    u32 hi  : 16;
  };
  struct {
    u32 mode   : 6; //00-05 (same as SPUCNT.Bit5-0, but, applied a bit delayed)
    u32 irq    : 1; //06    IRQ9 Flag (0=No, 1=发生)
    u32 dma_rw : 1; //07    Data Transfer DMA Read/Write Request ;seems to be same as SPUCNT.Bit5
    u32 dma_w  : 1; //08    Data Transfer DMA Write Request  (0=No, 1=Yes)
    u32 dma_r  : 1; //09    Data Transfer DMA Read Request   (0=No, 1=Yes)
    u32 busy   : 1; //10    Data Transfer Busy Flag          (0=Ready, 1=Busy)
    u32 half   : 1; //11    Writing to First/Second half of Capture Buffers (0=First, 1=Second)
    u32 _z     : 4; //12-15 Unknown/Unused
  };
};


template<class Parent, DeviceIOMapper N>
class SpuIOBase : public DeviceIO {
private:
  SpuIOBase();

protected:
  Parent& parent;
  virtual ~SpuIOBase() {}

  SpuIOBase(Parent& _p, Bus& b) : parent(_p) {
    b.bind_io(N, this);
  }

public:
  void write1(u8 v) { not_aligned_nosupport(N); }
  void write3(u8 v) { not_aligned_nosupport(N); }
  u32 read1() { not_aligned_nosupport(N); return 0; } 
  u32 read3() { not_aligned_nosupport(N); return 0; }

friend Parent;
};


template<class P, DeviceIOMapper N>
class SpuIOFunction : public SpuIOBase<P, N> {
public:
  typedef void (P::*OnWrite)(u32);
  typedef u32 (P::*OnRead)();

protected:
  SpuReg reg;
  const OnWrite w;
  const OnRead r;

  SpuIOFunction(P& p, Bus& b, OnWrite _w, OnRead _r = NULL) 
  : SpuIOBase<P, N>(p, b), w(_w), r(_r)
  {
  }

public:
  u32 read() { 
    if (r) return (this->parent.*r)();
    return reg.v; 
  }

  void write(u32 v) {
    reg.v = v;
    if (w) {
      (this->parent.*w)(v);
    }
  }

friend P;
};


// SPU连接到16位数据总线。实现了8位/ 16位/ 32位
// 读取和16位/ 32位写入。但是，未实现8位写操作：
// 对ODD地址的8位写操作将被忽略（不会引起任何异常），
// 对偶数地址的8位写操作将作为16位写操作执行??
template<class P, class R, DeviceIOMapper N, bool ReadOnly = false>
class SpuIO : public SpuIOBase<P, N> {
protected:
  R r;

  SpuIO(P& parent, Bus& b) : SpuIOBase<P, N>(parent, b) {
  }

public:
  u32 read() { return r.v; }
  u32 read2() { return r.hi; }

  void write(u32 v)  { if (ReadOnly) return; r.v   = v; }
  void write(u16 v)  { if (ReadOnly) return; r.low = v; }
  void write2(u16 v) { if (ReadOnly) return; r.hi  = v; }
  void write(u8 v)   { if (ReadOnly) return; r.low = v; }
  void write2(u8 v)  { if (ReadOnly) return; r.hi  = v; }

friend P;
};


#define FROM_BIT(r, f, n, o)   r |= (f[n+o] << (n))
#define TO_BIT(v, f, n, o)     f[n+o] = (1 & ((v) >> (n)))
#define DO_ARR8(arr, ret, x, o, F) \
  F(ret, arr, 0+x, o); F(ret, arr, 1+x, o); F(ret, arr, 2+x, o); F(ret, arr, 3+x, o); \
  F(ret, arr, 4+x, o); F(ret, arr, 5+x, o); F(ret, arr, 6+x, o); F(ret, arr, 7+x, o);

// 0-23 bit 分离的寄存器
template<class P, DeviceIOMapper N, bool ReadOnly = 0>
class SpuBit24IO : public SpuIOBase<P, N> {
protected:
  u8 f[24];

  SpuBit24IO(P& parent, Bus& b) : SpuIOBase<P, N>(parent, b) {
  }

public:
  u32 read() {
    u32 r = 0;
    DO_ARR8(f, r,  0, 0, FROM_BIT);
    DO_ARR8(f, r,  8, 0, FROM_BIT);
    DO_ARR8(f, r, 16, 0, FROM_BIT);
    return r; 
  }

  u32 read2() {
    u32 r = 0;
    DO_ARR8(f, r, 0, 16, FROM_BIT);
    return r;
  }

  void write(u32 v)  {
    if (ReadOnly) return;
    DO_ARR8(f, v,  0, 0, TO_BIT);
    DO_ARR8(f, v,  8, 0, TO_BIT);
    DO_ARR8(f, v, 16, 0, TO_BIT);
  }

  void write(u16 v)  { 
    if (ReadOnly) return;
    DO_ARR8(f, v,  0, 0, TO_BIT);
    DO_ARR8(f, v,  8, 0, TO_BIT);
  }

  void write2(u16 v) { 
    if (ReadOnly) return;
    DO_ARR8(f, v,  0, 16, TO_BIT);
  }

  void write(u8 v) {
    if (ReadOnly) return;
    DO_ARR8(f, v,  0, 0, TO_BIT);
  }

  void write2(u8 v) {
    if (ReadOnly) return;
    DO_ARR8(f, v, 0, 16, TO_BIT);
  }

friend P;
};

#undef DO_ARR8
#undef TO_BIT
#undef FROM_BIT


template<DeviceIOMapper t_vol, DeviceIOMapper t_sr,
         DeviceIOMapper t_sa,  DeviceIOMapper t_adsr,
         DeviceIOMapper t_acv, DeviceIOMapper t_ra,
         DeviceIOMapper t_cv,  int Number>
class SPUChannel {
private:
  // 0x1F801Cn0
  SpuIO<SPUChannel, VolumnReg, t_vol> volume;  
  // 0x1F801Cn4 (0=stop, 4000h=fastest, 4001h..FFFFh == 4000h, 1000h=44100Hz)
  SpuIO<SPUChannel, SpuReg, t_sr> pcmSampleRate;
  // 0x1F801Cn6 声音开始地址, 样本由一个或多个16字节块组成
  SpuIO<SPUChannel, SpuReg, t_sa> pcmStartAddr;
  // 0x1F801Cn8
  SpuIO<SPUChannel, ADSRReg, t_adsr> adsr;
  // 0x1F801CnC
  SpuIO<SPUChannel, SpuReg, t_acv> adsrVol;
  // 0x1F801CnE 声音重复地址, 播放时被adpcm数据更新
  SpuIO<SPUChannel, SpuReg, t_ra> pcmRepeatAddr;
  // 0x1F801E0n
  SpuIO<SPUChannel, SpuReg, t_cv> currVolume;

public:
  SPUChannel(SoundProcessing& parent, Bus& b);
};


#define SPU_CHANNEL_TYPES(_addr, name, _arr, _v, _wide)  DeviceIOMapper::name,
#define SPU_CHANNEL_CLASS(n)  SPUChannel<IO_SPU_CHANNEL(SPU_CHANNEL_TYPES, 0, 0, n) n>
#define SPU_DEF_VAL(name, n)  SPU_CHANNEL_CLASS(n) name ## n;
#define SPU_DEF_ALL_CHANNELS(name, func) \
        func(name,  0) func(name,  1) func(name,  2) func(name,  3) \
        func(name,  4) func(name,  5) func(name,  6) func(name,  7) \
        func(name,  8) func(name,  9) func(name, 10) func(name, 11) \
        func(name, 12) func(name, 13) func(name, 14) func(name, 15) \
        func(name, 16) func(name, 17) func(name, 18) func(name, 19) \
        func(name, 20) func(name, 21) func(name, 22) func(name, 23) \

class SoundProcessing {
private:

  template<DeviceIOMapper P>
  class Reg32 : public InnerDeviceIO<SoundProcessing, U32Reg> {
  public:
    Reg32(SoundProcessing& parent, Bus& b) : InnerDeviceIO(parent) {
      b.bind_io(P, this);
    }
  };

  template<DeviceIOMapper P>
  class Reg16 : public InnerDeviceIO<SoundProcessing, U16Reg> {
  public:
    Reg16(SoundProcessing& parent, Bus& b) : InnerDeviceIO(parent) {
      b.bind_io(P, this);
    }
  };

  // 0x1F80'1D80 主音量
  SpuIO<SoundProcessing, VolumnReg, DeviceIOMapper::spu_main_vol> mainVol;

  // 0x1F80'1DB0 CD 音量, 包络操作取决于
  // Shift / Step / Mode / Direction
  SpuIO<SoundProcessing, VolumnReg, DeviceIOMapper::spu_cd_vol> cdVol;

  // 0x1F80'1DB4 外部音频输入音量, 
  // 包络操作取决于Shift / Step / Mode / Direction
  SpuIO<SoundProcessing, VolumnReg, DeviceIOMapper::spu_ext_vol> externVol;

  // 0x1F80'1DB8 这些是内部寄存器，通常不被软件使用
  SpuIO<SoundProcessing, VolumnReg, DeviceIOMapper::spu_curr_main_vol> mainCurrVol;

  // 0x1F80'1D88（W）（bit0-23 1:开始起音/衰减/延音）
  // 启动ADSR信封，并自动将ADSR音量初始化为零，并将
  // “语音开始地址”复制到“语音重复地址”。
  SpuBit24IO<SoundProcessing, DeviceIOMapper::spu_n_key_on> nKeyOn;

  // 0x1F80'1D8C（W）（bit0-23 1:开始释放)
  // 对于完整的ADSR模式，通常会在持续时间段内发出OFF，
  // 但是它可以在任何时候发出
  SpuBit24IO<SoundProcessing, DeviceIOMapper::spu_n_key_off> nKeyOff;

  // 0x1F80'1D90 使用前一个(-1)通道数据作为当前
  // 通道数据的的调制器 1-23位有效
  SpuBit24IO<SoundProcessing, DeviceIOMapper::spu_n_fm> nFM;

  // 0x1F80'1D94 (0=ADPCM, 1=Noise) 
  // bit0-23 1:启用噪音模式
  SpuBit24IO<SoundProcessing, DeviceIOMapper::spu_n_noise> nNoise;

  // 0x1F80'1D9C （R）(0=Newly Keyed On, 1=Reached LOOP-END)
  // 当设置相应的KEY ON位时，这些位将被清除 = 0
  // 当到达ADPCM标头.bit0中的LOOP-END标志时，这些位将置位 = 1
  SpuBit24IO<SoundProcessing, DeviceIOMapper::spu_n_status, true> endx;

  // 0x1F80'1DA4 声音RAM IRQ地址, 写入/读取都会发生中断 TODO
  SpuIO<SoundProcessing, SpuCntReg, DeviceIOMapper::spu_ram_irq> ramIrqAdress;

  // 0x1F80'1DA6 声音RAM数据传输地址
  SpuIOFunction<SoundProcessing, DeviceIOMapper::spu_ram_trans_addr> ramTransferAddress;
  // 0x1F80'1DA8 声音RAM数据传输Fifo
  SpuIOFunction<SoundProcessing, DeviceIOMapper::spu_ram_trans_fifo> ramTransferFifo;
  // 0x1F80'1DAC 声音RAM数据传输控制（应为0004h）
  SpuIO<SoundProcessing, SpuReg, DeviceIOMapper::spu_ram_trans_ctrl> ramTransferCtrl;

  // 0x1F80'1DAA SPU控制寄存器
  SpuIO<SoundProcessing, SpuCntReg, DeviceIOMapper::spu_ctrl> ctrl;
  // 0x1F80'1DAE（R）SPU状态寄存器
  SpuIO<SoundProcessing, SpuStatusReg, DeviceIOMapper::spu_status, true> status;

  Reg32<DeviceIOMapper::spu_reverb_vol> reverbVol;
  Reg32<DeviceIOMapper::spu_n_reverb> nReverb;
  Reg16<DeviceIOMapper::spu_ram_rev_addr> ramReverbStartAddress;

  Reg16<DeviceIOMapper::spu_unknow1> _un1;
  Reg32<DeviceIOMapper::spu_unknow2> _un2;

  Reg16<DeviceIOMapper::spu_rb_apf_off1>    dAPF1;
  Reg16<DeviceIOMapper::spu_rb_apf_off2>    dAPF2;
  Reg16<DeviceIOMapper::spu_rb_ref_vol1>    vIIR;
  Reg16<DeviceIOMapper::spu_rb_comb_vol1>   vCOMB1;
  Reg16<DeviceIOMapper::spu_rb_comb_vol2>   vCOMB2;
  Reg16<DeviceIOMapper::spu_rb_comb_vol3>   vCOMB3;
  Reg16<DeviceIOMapper::spu_rb_comb_vol4>   vCOMB4;
  Reg16<DeviceIOMapper::spu_rb_ref_vol2>    vWALL;
  Reg16<DeviceIOMapper::spu_rb_apf_vol1>    vAPF1;
  Reg16<DeviceIOMapper::spu_rb_apf_vol2>    vAPF2;
  Reg32<DeviceIOMapper::spu_rb_same_ref1>   mSAME;
  Reg32<DeviceIOMapper::spu_rb_comb1>       mCOMB1;
  Reg32<DeviceIOMapper::spu_rb_comb2>       mCOMB2;
  Reg32<DeviceIOMapper::spu_rb_same_ref2>   dSAME;
  Reg32<DeviceIOMapper::spu_rb_diff_ref1>   mDIFF;
  Reg32<DeviceIOMapper::spu_rb_comb3>       mCOMB3;
  Reg32<DeviceIOMapper::spu_rb_comb4>       mCOMB4;
  Reg32<DeviceIOMapper::spu_rb_diff_ref2>   dDIFF;
  Reg32<DeviceIOMapper::spu_rb_apf_addr1>   mAPF1;
  Reg32<DeviceIOMapper::spu_rb_apf_addr2>   mAPF2;
  Reg32<DeviceIOMapper::spu_rb_in_vol>      vIN;

  // ch0 ... ch23
  SPU_DEF_ALL_CHANNELS(ch, SPU_DEF_VAL)

private:
  u8 mem[SPU_MEM_SIZE];
  u32 trans_point = 0;
  u16 fifo[SPU_FIFO_SIZE];
  u8 fifo_point = 0;

  void set_transfer_address(u32 a);
  void push_fifo(u32 a);

  // dma/fifo 数据处理
  void process();
  void manual_write();

public:
  SoundProcessing(Bus&);
};

}