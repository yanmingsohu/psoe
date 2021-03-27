#pragma once

#include "util.h"
#include "io.h"
#include "bus.h"
#include <mutex>

class RtAudio;
extern "C" struct SRC_STATE_tag;

namespace ps1e {

#define SPU_DEBUG_INFO
// 自动压限会影响性能
#define SPU_AUTO_LIMIT_VOLUME
// 512K
#define SPU_MEM_SIZE        0x8'0000
#define SPU_MEM_MASK        (SPU_MEM_SIZE-1)
#define SPU_MEM_ECHO_MASK   (SPU_MEM_MASK-1)
// 32 个半字, 64个字节
#define SPU_FIFO_SIZE       0x20
#define SPU_FIFO_MASK       (SPU_FIFO_SIZE-1)
#define SPU_FIFO_INDEX(x)   ((x) & SPU_FIFO_MASK)
#define SPU_ADPCM_BLK_SZ    0x10
#define SPU_PCM_BLK_SZ      28
#define SPU_CHANNEL_COUNT   24
#define SPU_WORK_FREQ       44100
#define SPU_ADPCM_RETE      22050
// 转换 spu 整数音量到浮点值 x[-8000h..+7FFEh] 输出 -n ~ +n 倍, x==0 则没有变化
//#define SPU_F_VOLUME(x)     (1 + s16(x)/float(0x8000) * 2)
#define SPU_F_VOLUME(x)     (s16(x) / float(0x8000))

#define not_aligned_nosupport(x) error("Cannot read SPU IO %u, Memory misalignment\n", u32(x))

// 如果 begin 到 begin+size(不包含) 之间穿过了 point 则返回 true, size > 0
#define ADDR_IN_SCOPE(begin, size, point) \
    ( ((begin) <= (point)) && (((begin) + (size)) > (point)) )

#define SPU_COMM_BIT(n) \
  u32 n; \
  struct { \
    u32 _hw0 : 16; \
    u32 _hw2 : 16; \
  }

#ifdef SPU_DEBUG_INFO
  #define spudbg _spudbg
  void _spudbg(const char* format, ...);
#else
  #define spudbg
#endif


typedef float PcmSample;
class SoundProcessing;
class PcmLowpassInner;


enum class SpuDmaDir : u8 {
  Stop = 0,
  ManualWrite = 1,
  DMAwrite = 2,
  DMAread = 3,
};


enum class SpuChVarFlag : u32 {
  volume = 1,
  work_volume,
  sample_rate,
  start_address,
  repeat_address,
  adsr,
  adsr_volume,
  adsr_state,
  __end_channel__,
  key_on,
  key_off,
  endx,
  fm,
  noise,
  echo,
  ctrl,
  __end_core__,
};


//
// Bit0-1的可能组合为:
//  Code 0 = Normal     (continue at next 16-byte block)
//  Code 1 = End+Mute   (jump to Loop-address, set ENDX flag, Release, Env=0000h)
//  Code 2 = Ignored    (same as Code 0)
//  Code 3 = End+Repeat (jump to Loop-address, set ENDX flag)
//
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


//
// ADPCM 样本数据, 16字节一个块
// 0x00         | 0x01      | 0x02-0x0F
// Shift/Filter | Flag Bits | Compressed Data (LSBs=1st Sample, MSBs=2nd Sample)
//
struct AdpcmBlock {
  u8 filter;
  AdpcmFlag flag;
  AdpcmData data[14];
};


union SpuReg {
  SPU_COMM_BIT(v);
  struct {
    s16 sl;
    s16 sh;
  };
};


union AddrReg {
  SPU_COMM_BIT(v);

  u32 address() {
    return (0xFFFF & v) << 3; // *8
  }

  void saveAddr(u32 d) {
    v = d >> 3;
  }
};


union ADSRReg {
  SPU_COMM_BIT(v);
  struct {
    u32 su_lv : 4; //00-03 延音 (0..0Fh)  ;Level=(N+1)*800h
    u32 de_sh : 4; //04-07 衰减 (0..0Fh = Fast..Slow) 固定 "-8", 总是指数
    u32 at_st : 2; //08-09 音头 step (0..3 = "+7,+6,+5,+4") 总是加
    u32 at_sh : 5; //10-14 音头 (0..1Fh = Fast..Slow)
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
    u16 _type : 1; //15    1 模式, 扫频
  };
};


union VolumnReg {
  SPU_COMM_BIT(v);
  struct {
    VolData left;
    VolData right;
  };
};


union SpuCntReg {
  SPU_COMM_BIT(v);
  struct {
    u32 mode : 6; //00-05
  };
  struct {
    u32 cda_enb : 1; //00    CD 使能(0=Off, 1=On) (for CD-DA and XA-ADPCM)
    u32 exa_enb : 1; //01    外部音源使能
    u32 cda_rev : 1; //02    CD 混响
    u32 exa_rev : 1; //03    外部混响
    u32 dma_trs : 2; //04-05 dma 传输模式(0=Stop, 1=ManualWrite, 2=DMAwrite, 3=DMAread)
    u32 irq_enb : 1; //06    IRQ9 Enable (0=关闭/应答, 1=启用; only when Bit15=1)
    u32 mst_rev : 1; //07    混响主使能 (0=Disabled, 1=Enabled)
    u32 nos_stp : 2; //08-09 噪音 step (0..03h = Step "4,5,6,7")
    u32 nos_shf : 4; //10-13 噪音 shift (0..0Fh = Low .. High Frequency)
    u32 mute    : 1; //14    静音(0=Mute, 1=Unmute)  (Don't care for CD Audio)
    u32 spu_enb : 1; //15    spu 使能 (0=Off, 1=On)       (Don't care for CD Audio)
  };
};


union SpuStatusReg {
  SPU_COMM_BIT(v);
  struct {
    u32 cda_enb : 1; //00    CD 使能(0=Off, 1=On) (for CD-DA and XA-ADPCM)
    u32 exa_enb : 1; //01    外部音源使能
    u32 cda_rev : 1; //02    CD 混响
    u32 exa_rev : 1; //03    外部混响
    u32 dma_trs : 2; //04-05 dma 传输模式(0=Stop, 1=ManualWrite, 2=DMAwrite, 3=DMAread)
    u32 _x      :10; //06-15
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


template<class P, DeviceIOMapper N, class Reg = SpuReg, bool ReadOnly = false>
class SpuIOFunction : public SpuIOBase<P, N> {
public:
  typedef void (P::*OnWrite)(u32 _new, u32 _old);
  typedef u32 (P::*OnRead)();

protected:
  Reg r;
  const OnWrite writeFn;
  const OnRead readFn;

  SpuIOFunction(P& p, Bus& b, OnWrite _w, OnRead _r = NULL) 
  : SpuIOBase<P, N>(p, b), writeFn(_w), readFn(_r)
  {
  }

public:
  u32 read() { 
    if (readFn) return (this->parent.*readFn)();
    return r.v; 
  }

  void write(u32 v) {
    if (ReadOnly) return;
    u32 old = r.v;
    r.v = v;
    if (writeFn) {
      (this->parent.*writeFn)(v, old);
    }
  }

  Reg* operator->() {
    return &r;
  }
friend P;
};


//
// SPU连接到16位数据总线。实现了8位/ 16位/ 32位
// 读取和16位/ 32位写入。但是，未实现8位写操作：
// 对ODD地址的8位写操作将被忽略（不会引起任何异常），
// 对偶数地址的8位写操作将作为16位写操作执行??
//
template<class P, class R, DeviceIOMapper N, bool ReadOnly = false>
class SpuIO : public SpuIOBase<P, N> {
protected:
  R r;

  SpuIO(P& parent, Bus& b) : SpuIOBase<P, N>(parent, b) {
  }

public:
  u32 read() { return r.v; }
  u32 read2() { return r._hw2; }

  void write(u32 v)  { if (ReadOnly) return; r.v   = v; }
  void write(u16 v)  { if (ReadOnly) return; r._hw0 = v; }
  void write2(u16 v) { if (ReadOnly) return; r._hw2  = v; }
  void write(u8 v)   { if (ReadOnly) return; r._hw0 = v; }
  void write2(u8 v)  { if (ReadOnly) return; r._hw2  = v; }

  R* operator->() { return &r; }
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
  typedef void (P::*OnChange)();
  volatile u8 f[SPU_CHANNEL_COUNT];
  OnChange pchange;

  // 只有总线上的 write 操作会触发 OnChange 调用
  SpuBit24IO(P& parent, Bus& b, OnChange c=0) : SpuIOBase<P, N>(parent, b), pchange(c) {
  }

  void set(u8 bitIndex) {
    f[bitIndex & 0x1F] = 1;
  }

  void reset(u8 bitIndex) {
    f[bitIndex & 0x1F] = 0;
  }

  u8 get(u8 bitIndex) {
    return f[bitIndex & 0x1F];
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
    if (pchange) (this->parent.*pchange)();
  }

  void write(u16 v)  { 
    if (ReadOnly) return;
    DO_ARR8(f, v,  0, 0, TO_BIT);
    DO_ARR8(f, v,  8, 0, TO_BIT);
    if (pchange) (this->parent.*pchange)();
  }

  void write2(u16 v) { 
    if (ReadOnly) return;
    DO_ARR8(f, v,  0, 16, TO_BIT);
    if (pchange) (this->parent.*pchange)();
  }

  void write(u8 v) {
    if (ReadOnly) return;
    DO_ARR8(f, v,  0, 0, TO_BIT);
    if (pchange) (this->parent.*pchange)();
  }

  void write2(u8 v) {
    if (ReadOnly) return;
    DO_ARR8(f, v, 0, 16, TO_BIT);
    if (pchange) (this->parent.*pchange)();
  }

friend P;
};

#undef DO_ARR8
#undef TO_BIT
#undef FROM_BIT


class SpuAdsr {
private:
  bool isExponential;
  bool isInc;
  s32 step;
  u32 initCyc;

public:
  // 重置内部状态
  void reset(u8 mode, u8 dir, u8 shift, u8 _step);
  // 计算并返回 level 和 cycles, 一个 cycles 是 1/44100
  void next(s32& level, u32& cycles);
};


struct PcmHeader {
  PcmSample hist1 = 0;
  PcmSample hist2 = 0;
  u32 addr = 0;
  bool changed = 0;

  void set(u32 a, PcmSample h1, PcmSample h2, bool c = 0);
  bool sameAddr(PcmHeader& o);
};


// 可以安全的复制, 抽象类
class VolumeEnvelope {
public:
  VolumeEnvelope() {}
  virtual ~VolumeEnvelope() {};
  // 应用音量包络
  virtual void apply(PcmSample& v) = 0;
  // 同步音量, 在 apply 的时候音量可能更改, 将更改的音量同步回寄存器
  virtual s16 getVol() = 0;
};


class VolumeEnvelopeSet : public NonCopy {
private:

  class DoZero : public VolumeEnvelope {
  public:
    void apply(PcmSample& v);
    s16 getVol();
  };


  class FixVol : public VolumeEnvelope {
   public:
    PcmSample vol;

    void apply(PcmSample& v);
    void reset(VolData& vd);
    s16 getVol();
  };


  class Sweep : public VolumeEnvelope {
   public:
    SpuAdsr filter;
    float phase;
    s32 level;
    u32 cycles;
    VolData arg;
    u8 dir;

    void apply(PcmSample& v);
    void reset(VolData& vd, s32 nlevel);
    s16 getVol();
  };

private:
  DoZero lz, rz;
  FixVol lf, rf;
  Sweep  ls, rs;

public:
  // 返回的 VolumeEnvelope 由 VolumeEnvelopeSet 管理
  VolumeEnvelope* get(bool left, VolData& vd, s32 clevel);
};


class PcmStreamer {
public:
  virtual ~PcmStreamer() {}
  // 读取一个原始采样
  virtual PcmSample readPcmSample() = 0;
  // 读取一个采样块, 经过 adsr/过滤器/重采样, 如果因为某种原因块被忽略(0采样等)返回 false.
  virtual bool readSampleBlocks(PcmSample *_in, PcmSample *_out, u32 sample_count) = 0;
  // 返回的 VolumeEnvelope 由当前 PcmStreamer 对象管理
  virtual VolumeEnvelope* getVolumeEnvelope(bool left) = 0;
  virtual void copyStartToRepeat() = 0;
  // 同步音量, 在 apply 的时候音量可能更改, 将更改的音量同步回寄存器
  virtual void syncVol(VolumeEnvelope* left, VolumeEnvelope *right) = 0;
  // 用于测试目的, 返回内部变量的值
  virtual u32 getVar(SpuChVarFlag) = 0;
};


class PcmResample {
private:
  static const u32 buf_size = 64;
  PcmStreamer* stream;
  SRC_STATE_tag *stage;
  float *buf;
  void check_error(int e, bool throwErr = false);
public:
  PcmResample(PcmStreamer*);
  ~PcmResample();
  // 读取采样后的音频帧, 输出到 out 中
  bool read(float* out, long frames, double ratio);
  // 不要调用, 读取原始音频帧
  long readSrc(float **data);
};


//if (pitch < 32640) then
//低通滤波 3000.0 6bB
//if (pitch < 20000) then
//低通滤波 500.0 6bB
class PcmLowpass {
private:
  PcmLowpassInner* inner;
public:
  PcmLowpass(float samplingrate = SPU_WORK_FREQ);
  ~PcmLowpass();
  void filter(PcmSample *data, u32 frame, double freq);
};


template<DeviceIOMapper t_vol, DeviceIOMapper t_sr,
         DeviceIOMapper t_sa,  DeviceIOMapper t_adsr,
         DeviceIOMapper t_acv, DeviceIOMapper t_ra,
         DeviceIOMapper t_cv,  int Number>
class SPUChannel : public NonCopy, public PcmStreamer {
private:
  // 0x1F801Cn0
  SpuIOFunction<SPUChannel, t_vol, VolumnReg> volume;  
  // 0x1F801Cn4 (0=stop, 4000h=fastest, 4001h..FFFFh == 4000h, 1000h=44100Hz)
  SpuIOFunction<SPUChannel, t_sr, SpuReg> pcmSampleRate;
  // 0x1F801Cn6 声音开始地址, 样本由一个或多个16字节块组成
  SpuIOFunction<SPUChannel, t_sa, AddrReg> pcmStartAddr;
  // 0x1F801Cn8
  SpuIO<SPUChannel, ADSRReg, t_adsr> adsr;
  // 0x1F801CnC
  SpuIO<SPUChannel, SpuReg, t_acv> adsrVol;
  // 0x1F801CnE 声音重复地址, 播放时被adpcm数据更新
  SpuIOFunction<SPUChannel, t_ra, AddrReg> pcmRepeatAddr;
  // 0x1F801E0n These are internal registers, normally not used by software
  SpuIO<SPUChannel, SpuReg, t_cv> currVolume;

  SoundProcessing& spu;
  PcmHeader currentReadAddr;
  PcmHeader repeatAddr;
  SpuAdsr adsr_filter;
  u32 adsr_cycles_remaining = 0;
  // 在缓冲区中存储一整块采样, 然后读取单个采样点
  PcmSample pcm_read_buf[SPU_PCM_BLK_SZ];
  s32 pcm_buf_remaining = 0;
  double play_rate = 0;
  PcmResample resample;
  PcmLowpass lowpass;
  VolumeEnvelopeSet sweet_ve;

  enum class AdsrState : u8 {
    Wait    = 0,
    Attack  = 1,
    Decay   = 2,
    Sustain = 3,
    Release = 4,
  } adsr_state = AdsrState::Wait;

  void set_start_address(u32, u32);
  void set_repeat_addr(u32, u32);
  void set_sample_rate(u32, u32);
  void set_volume(u32, u32);
  u32 read_curr_volume();

public:
  SPUChannel(SoundProcessing& parent, Bus& b);

  // 从指定的通道中读取并解码采样数据到 buf, 读取结束会修改通道的声音地址,
  // 必要时读取会触发 irq, 读取会检测出数据循环标记并修改循环地址,
  // 如果进入了循环地址, 则执行循环; 应用全部音量包络.
  // _in 通常是前一个通道的输出(FM模式使用) 该缓冲区不会用作其他, 可以随意写入.
  bool readSampleBlocks(PcmSample *_in, PcmSample *_out, u32 sample_count);
  void applyADSR(PcmSample *_in, PcmSample *out, u32 sample_count);
  void copyStartToRepeat();
  // 从 pcm 缓冲区读取一个采样, 保证效率
  PcmSample readPcmSample();
  VolumeEnvelope* getVolumeEnvelope(bool left);
  void syncVol(VolumeEnvelope* left, VolumeEnvelope *right);
  u32 getVar(SpuChVarFlag);
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

class SoundProcessing : public NonCopy, public DMADev {
private:
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
  // 对于完整的ADSR模式，通常会在 '延音' 时间段内发出OFF，
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
  SpuIO<SoundProcessing, AddrReg, DeviceIOMapper::spu_ram_irq> ramIrqAdress;
  // 0x1F80'1DA6 声音RAM数据传输地址
  SpuIOFunction<SoundProcessing, DeviceIOMapper::spu_ram_trans_addr> ramTransferAddress;
  // 0x1F80'1DA8 声音RAM数据传输Fifo
  SpuIOFunction<SoundProcessing, DeviceIOMapper::spu_ram_trans_fifo> ramTransferFifo;
  // 0x1F80'1DAC 声音RAM数据传输控制（应为0004h）
  SpuIO<SoundProcessing, SpuReg, DeviceIOMapper::spu_ram_trans_ctrl> ramTransferCtrl;

  // 0x1F80'1DAA SPU控制寄存器
  SpuIOFunction<SoundProcessing, DeviceIOMapper::spu_ctrl, SpuCntReg> ctrl;
  // 0x1F80'1DAE（R）SPU状态寄存器
  SpuIO<SoundProcessing, SpuStatusReg, DeviceIOMapper::spu_status, true> status;

  // 1F801D98h 设置通道的混响。采样结束后，
  // 该通道的混响就关闭了……很好，但是什么时候结束？
  // 在混响模式下，声音似乎同时（正常）和通过混响（延迟）输出
  SpuBit24IO<SoundProcessing, DeviceIOMapper::spu_n_reverb> nReverb;
  // 0x1F80'1D84 混响输出音量
  SpuIO<SoundProcessing, SpuReg, DeviceIOMapper::spu_reverb_vol> reverbVol;
  // 0x1F80'1DA2 混响工作内存起始地址
  SpuIO<SoundProcessing, AddrReg, DeviceIOMapper::spu_ram_rev_addr> reverbBegin;

  SpuIO<SoundProcessing, SpuReg, DeviceIOMapper::spu_unknow1> _un1;
  SpuIO<SoundProcessing, SpuReg, DeviceIOMapper::spu_unknow2> _un2;

  SpuIO<SoundProcessing, AddrReg, DeviceIOMapper::spu_rb_apf_off1>    dAPF1;
  SpuIO<SoundProcessing, AddrReg, DeviceIOMapper::spu_rb_apf_off2>    dAPF2;
  SpuIO<SoundProcessing, SpuReg, DeviceIOMapper::spu_rb_ref_vol1>     vIIR;
  SpuIO<SoundProcessing, SpuReg, DeviceIOMapper::spu_rb_comb_vol1>    vCOMB1;
  SpuIO<SoundProcessing, SpuReg, DeviceIOMapper::spu_rb_comb_vol2>    vCOMB2;
  SpuIO<SoundProcessing, SpuReg, DeviceIOMapper::spu_rb_comb_vol3>    vCOMB3;
  SpuIO<SoundProcessing, SpuReg, DeviceIOMapper::spu_rb_comb_vol4>    vCOMB4;
  SpuIO<SoundProcessing, SpuReg, DeviceIOMapper::spu_rb_ref_vol2>     vWALL;
  SpuIO<SoundProcessing, SpuReg, DeviceIOMapper::spu_rb_apf_vol1>     vAPF1;
  SpuIO<SoundProcessing, SpuReg, DeviceIOMapper::spu_rb_apf_vol2>     vAPF2;
  SpuIO<SoundProcessing, AddrReg, DeviceIOMapper::spu_rb_same_ref1l>  mLSAME;
  SpuIO<SoundProcessing, AddrReg, DeviceIOMapper::spu_rb_same_ref1r>  mRSAME;
  SpuIO<SoundProcessing, AddrReg, DeviceIOMapper::spu_rb_comb1l>      mLCOMB1;
  SpuIO<SoundProcessing, AddrReg, DeviceIOMapper::spu_rb_comb1r>      mRCOMB1;
  SpuIO<SoundProcessing, AddrReg, DeviceIOMapper::spu_rb_comb2l>      mLCOMB2;
  SpuIO<SoundProcessing, AddrReg, DeviceIOMapper::spu_rb_comb2r>      mRCOMB2;
  SpuIO<SoundProcessing, AddrReg, DeviceIOMapper::spu_rb_same_ref2l>  dLSAME;
  SpuIO<SoundProcessing, AddrReg, DeviceIOMapper::spu_rb_same_ref2r>  dRSAME;
  SpuIO<SoundProcessing, AddrReg, DeviceIOMapper::spu_rb_diff_ref1l>  mLDIFF;
  SpuIO<SoundProcessing, AddrReg, DeviceIOMapper::spu_rb_diff_ref1r>  mRDIFF;
  SpuIO<SoundProcessing, AddrReg, DeviceIOMapper::spu_rb_comb3l>      mLCOMB3;
  SpuIO<SoundProcessing, AddrReg, DeviceIOMapper::spu_rb_comb3r>      mRCOMB3;
  SpuIO<SoundProcessing, AddrReg, DeviceIOMapper::spu_rb_comb4l>      mLCOMB4;
  SpuIO<SoundProcessing, AddrReg, DeviceIOMapper::spu_rb_comb4r>      mRCOMB4;
  SpuIO<SoundProcessing, AddrReg, DeviceIOMapper::spu_rb_diff_ref2l>  dLDIFF;
  SpuIO<SoundProcessing, AddrReg, DeviceIOMapper::spu_rb_diff_ref2r>  dRDIFF;
  SpuIO<SoundProcessing, AddrReg, DeviceIOMapper::spu_rb_apf_addr1l>  mLAPF1;
  SpuIO<SoundProcessing, AddrReg, DeviceIOMapper::spu_rb_apf_addr1r>  mRAPF1;
  SpuIO<SoundProcessing, AddrReg, DeviceIOMapper::spu_rb_apf_addr2l>  mLAPF2;
  SpuIO<SoundProcessing, AddrReg, DeviceIOMapper::spu_rb_apf_addr2r>  mRAPF2;
  SpuIO<SoundProcessing, SpuReg, DeviceIOMapper::spu_rb_in_voll>      vLIN;
  SpuIO<SoundProcessing, SpuReg, DeviceIOMapper::spu_rb_in_volr>      vRIN;

  // ch0 ... ch23
  SPU_DEF_ALL_CHANNELS(ch, SPU_DEF_VAL)
  PcmStreamer* channel_stream[24];

private:
  u32 devSampleRate = SPU_WORK_FREQ;
  u32 bufferFrames = 128; 

  Bus &bus;
  u8 *mem;
  std::mutex for_copy_data;
  u32 mem_write_addr = 0;
  u16 fifo[SPU_FIFO_SIZE];
  u8 fifo_point = 0;
  RtAudio* dac;
  s32 noiseTimer;
  s16 nsLevel;
  SmallBuf<PcmSample> swap1;
  SmallBuf<PcmSample> swap2;
  SmallBuf<PcmSample> echoOutSwap;
  s32 echo_addr_offset;

  // 寄存器函数
  void set_transfer_address(u32 a, u32);
  void push_fifo(u32 a, u32);
  void set_ctrl_req(u32 a, u32);
  void key_on_changed();

  // dma/fifo 数据处理
  void copy_fifo_to_mem();
  void trigger_manual_write();
  // 立即发送中断
  void send_irq();
  // 如果启用了中断, 并且中断地址位于 'beginAddr' 和 
  // 'beginAddr+偏移'(不包含) 之间, 则发送中断并返回 true
  bool check_irq(u32 beginAddr, u32 offset);
  void init_dac();
  // echo 收集所有通道必要的混响数据, 处理后将混响数据输出到 echo
  void apply_reverb(PcmSample* echo, u32 nframe);

  //
  // 进行最后的混合, 对buf 进行主音量包络, 并将混响/CD/外部音源混合到输出
  // 缓冲区都是左右格式
  // buf    -- 输入输出缓冲区, 所有通道的混音, 并最终输出
  // echo   -- 所有通道的混响数据
  // nframe -- 一个声道的输出帧数量
  //
  void mix_end(PcmSample* buf, PcmSample* echo, u32 nframe);

  //
  // 将通道中的所有帧混合到输出缓冲区
  // cn         -- 通道号
  // dst        -- 音频输出缓冲区, 将通道数据混合到其中, 左右格式
  // median     -- 前一通道的输出数据, 未经过通道音量包络, 单通道
  // channelout -- 当前通道的输出数据, 未经过通道音量包络, 单通道
  // nframe     -- 一个声道的输出帧数量
  // echoOut    -- 混响输出缓冲区, 必要时将通道数据混合到其中, 左右格式
  //
  void process_channel(int cn, PcmSample *dst, PcmSample *median, PcmSample* channelout, 
                       u32 nframe, PcmSample* echoOut);

protected:
  void dma_ram2dev_block(psmem addr, u32 bytesize, s32 inc) override;
  void dma_dev2ram_block(psmem addr, u32 bytesize, s32 inc) override;

public:
  SoundProcessing(Bus&);
  ~SoundProcessing();

  // 通常为 true 用于对比测试
  bool use_low_pass = true;
  void print_fifo();
  u8 *get_spu_mem();
  u32 get_var(SpuChVarFlag, int c);

  void setEndxFlag(u8 channelIndex);
  bool isAttackOn(u8 channelIndex); // 查询后复位对应位
  bool isReleaseOn(u8 channelIndex); // 查询后复位对应位
  bool isNoise(u8 channelIndex);
  bool usePrevChannelFM(u8 channelIndex);
  bool isEnableEcho(u8 channelIndex);
  bool isEnableCDEcho();
  bool isEnableExternalEcho();
  bool isEnableCDVolume();
  // 从 spu 内存中的 readAddr 开始, 读取 1 个 SPU-ADPCM 块并解码(块总是 16 字节对齐的).
  // 必要时读取会触发 irq, 返回当前块的 flag, 解码后一个块长度为 28 个采样.
  // 应用混音算法, 将采样与缓冲区中的声音快进行混音.
  AdpcmFlag readAdpcmBlock(PcmSample *buf, PcmHeader& ph);
  void requestAudioData(PcmSample *buf, u32 nframe, double time);
  u32 getOutputRate();
  void readNoiseSampleBlocks(PcmSample *buf, u32 nframe);
};


#undef SPU_COMM_BIT
}