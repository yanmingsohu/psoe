#include <rtaudio/RtAudio.h>
#include <libsamplerate/include/samplerate.h>
#include <iir1/Iir.h>
#include "spu.h"
#include "spu.inl"

namespace ps1e {

#define SPU_INIT_CHANNEL(name, n)  name ## n(*this, b),
#define SPU_II(name) name(*this, b)
#define SET_TO_STREAM_ARR(name, n)  channel_stream[n] = &name ## n;

#ifdef SPU_DEBUG_INFO
  void _spudbg(const char* format, ...) {
    warp_printf(format, "\x1b[32m\x1b[44m");
  }
#endif


static s8 nibble_dict[16] = {0,1,2,3,4,5,6,7,-8,-7,-6,-5,-4,-3,-2,-1};
static const PcmSample adpcm_coefs_dict[16][2] = {
    { 0.0       ,  0.0       }, //{   0.0        ,   0.0        },
    { 0.9375    ,  0.0       }, //{  60.0 / 64.0 ,   0.0        },
    { 1.796875  , -0.8125    }, //{ 115.0 / 64.0 , -52.0 / 64.0 },
    { 1.53125   , -0.859375  }, //{  98.0 / 64.0 , -55.0 / 64.0 },
    { 1.90625   , -0.9375    }, //{ 122.0 / 64.0 , -60.0 / 64.0 },
    /* extended table used in few PS3 games, found in ELFs, NOT PSX ? */
    { 0.46875   , -0.0       }, //{  30.0 / 64.0 ,  -0.0 / 64.0 },
    { 0.8984375 , -0.40625   }, //{  57.5 / 64.0 , -26.0 / 64.0 },
    { 0.765625  , -0.4296875 }, //{  49.0 / 64.0 , -27.5 / 64.0 },
    { 0.953125  , -0.46875   }, //{  61.0 / 64.0 , -30.0 / 64.0 },
    { 0.234375  , -0.0       }, //{  15.0 / 64.0 ,  -0.0 / 64.0 },
    { 0.44921875, -0.203125  }, //{  28.75/ 64.0 , -13.0 / 64.0 },
    { 0.3828125 , -0.21484375}, //{  24.5 / 64.0 , -13.75/ 64.0 },
    { 0.4765625 , -0.234375  }, //{  30.5 / 64.0 , -15.0 / 64.0 },
    { 0.5       , -0.9375    }, //{  32.0 / 64.0 , -60.0 / 64.0 },
    { 0.234375  , -0.9375    }, //{  15.0 / 64.0 , -60.0 / 64.0 },
    { 0.109375  , -0.9375    }, //{   7.0 / 64.0 , -60.0 / 64.0 },
};

static PcmSample fix_volume_overload = 0.22;

// 混音算法 
// int32: C = A + B - (A * B >> 0x10)
// float: C = A + B - (A * B)
static inline PcmSample mixer(PcmSample a, PcmSample b) {
  //return (a + b) - (a * b);
  PcmSample r = a + b * fix_volume_overload;
#ifdef SPU_AUTO_LIMIT_VOLUME
  if (r >= 1.0 || r <= -1.0) {
    fix_volume_overload -= (1.0/44100.0);
    warn("Volume overload %f\n", fix_volume_overload);
  }
#endif
  return r;
}


int SpuRtAudioCallback( void *outputBuffer, void *inputBuffer,
                        unsigned int nFrames,
                        double streamTime,
                        RtAudioStreamStatus status,
                        void *userData ) 
{
  try {
    auto p = static_cast<SoundProcessing*>(userData);
    p->requestAudioData(static_cast<PcmSample*>(outputBuffer), nFrames, streamTime);
  } catch(...) {
    error("SpuRtAudioCallback has error\n");
  }
  return 0;
}


void SpuRtErrorCallback(RtAudioError::Type type, const std::string &txt) {
  error("SpuRtAudioCallback has error %s\n", txt.c_str());
}
  

SoundProcessing::SoundProcessing(Bus& b) : 
  DMADev(b, DeviceIOMapper::dma_spu_base), bus(b), dac(0), mem(0),
  SPU_II(mainVol),  SPU_II(cdVol),    SPU_II(reverbVol),
  SPU_II(externVol),                  SPU_II(mainCurrVol),
  SPU_II(nKeyOff),  SPU_II(nFM),      SPU_II(nNoise),
  SPU_II(nReverb),  SPU_II(endx),     SPU_II(status),
  SPU_II(dAPF1),    SPU_II(dAPF2),    SPU_II(vIIR),
  SPU_II(vCOMB1),   SPU_II(vCOMB2),   SPU_II(vCOMB3),
  SPU_II(vCOMB4),   SPU_II(vWALL),    SPU_II(vAPF1),
  SPU_II(vAPF2),    SPU_II(mRSAME),   SPU_II(mRCOMB1),
                    SPU_II(mLSAME),   SPU_II(mLCOMB1),
  SPU_II(mRCOMB2),  SPU_II(dRSAME),   SPU_II(mRDIFF),
  SPU_II(mLCOMB2),  SPU_II(dLSAME),   SPU_II(mLDIFF),
  SPU_II(mRCOMB3),  SPU_II(mRCOMB4),  SPU_II(dRDIFF),
  SPU_II(mLCOMB3),  SPU_II(mLCOMB4),  SPU_II(dLDIFF),
  SPU_II(mRAPF1),   SPU_II(mRAPF2),   SPU_II(vRIN),
  SPU_II(mLAPF1),   SPU_II(mLAPF2),   SPU_II(vLIN),
  SPU_DEF_ALL_CHANNELS(ch, SPU_INIT_CHANNEL) _un1(*this, b), _un2(*this, b),
  SPU_II(reverbBegin), SPU_II(ramIrqAdress), SPU_II(ramTransferCtrl),
  ramTransferFifo(*this, b, &SoundProcessing::push_fifo),
  ramTransferAddress(*this, b, &SoundProcessing::set_transfer_address),
  ctrl(*this, b, &SoundProcessing::set_ctrl_req),
  nKeyOn(*this, b, &SoundProcessing::key_on_changed)
{
  reverbBegin.r.v = SPU_MEM_ECHO_MASK;
  mem = new u8[SPU_MEM_SIZE];
  memset(mem, 0, SPU_MEM_SIZE);
  memset(fifo, 0, SPU_FIFO_SIZE << 1);
  SPU_DEF_ALL_CHANNELS(ch, SET_TO_STREAM_ARR);
  init_dac();
}


void SoundProcessing::init_dac() {
  try {
    dac = new RtAudio();
    RtAudio::StreamParameters parameters;
    parameters.deviceId = dac->getDefaultOutputDevice();
    parameters.nChannels = 2;
    parameters.firstChannel = 0;

    RtAudio::DeviceInfo di = dac->getDeviceInfo(parameters.deviceId);
    info("Audio Device: %s - %dHz\n", di.name.c_str(), di.preferredSampleRate);
    //devSampleRate = di.preferredSampleRate;

    const RtAudioFormat raf = sizeof(PcmSample)==4 ? RTAUDIO_FLOAT32 : RTAUDIO_FLOAT64;
    dac->openStream(&parameters, NULL, raf, devSampleRate, &bufferFrames, 
                    &SpuRtAudioCallback, this, 0, &SpuRtErrorCallback);

    info("Audio Use %d buffer frames, delay %f ms\n", 
        bufferFrames, float(bufferFrames)/devSampleRate*1000.0);
    dac->startStream();
  } catch (RtAudioError& e) {
    error("No Sound! %s\n", e.getMessage().c_str());
  }
}


SoundProcessing::~SoundProcessing() {
  if (dac) {
    dac->closeStream();
    delete dac;
  }
  delete [] mem;
}


static void setzero(PcmSample *buf, u32 nframe) {
  for (u32 i=0; i<nframe; ++i) {
    buf[i] = 0;
  }
}


void SoundProcessing::process_channel(int cn, 
                                      PcmSample *dst, 
                                      PcmSample *median, 
                                      PcmSample* channelout, 
                                      u32 nframe, 
                                      PcmSample* echo_out) 
{
  PcmStreamer* ps = channel_stream[cn];
  if (ps->readSampleBlocks(median, channelout, nframe)) {
    VolumeEnvelope* lve = ps->getVolumeEnvelope(true);
    VolumeEnvelope* rve = ps->getVolumeEnvelope(false);
    PcmSample left, right;

    for (u32 i=0; i < (nframe << 1); i += 2) {
      left  = channelout[i>>1];
      right = channelout[i>>1];
      lve->apply(left);
      rve->apply(right);
      dst[i+0] = mixer(dst[i+0], left);
      dst[i+1] = mixer(dst[i+1], right);

      if (isEnableEcho(cn)) {
        echo_out[i+0] = mixer(echo_out[i+0], left);
        echo_out[i+1] = mixer(echo_out[i+1], left);
      }
    }
    ps->syncVol(lve, rve);
  } else {
    setzero(channelout, nframe);
  }
}


// buf 实际长度 = 通道 * nframe, 通道数据交错存放
void SoundProcessing::requestAudioData(PcmSample *buf, u32 nframe, double time) {
  //spudbg("\r\t\t\t\tReQ audio data %d %f", nframe, time);
  noiseTimer -= nframe;
  setzero(buf, nframe << 1);
  if (ctrl.r.mute == 0) {
    return;
  }

  PcmSample* ec = echoOutSwap.get(nframe << 1);
  PcmSample* p1 = swap1.get(nframe);
  PcmSample* p2 = swap2.get(nframe);
  setzero(p1, nframe);
  setzero(ec, nframe << 1);

  process_channel( 0, buf, p1, p2, nframe, ec);
  process_channel( 1, buf, p2, p1, nframe, ec);
  process_channel( 2, buf, p1, p2, nframe, ec);
  process_channel( 3, buf, p2, p1, nframe, ec);
  process_channel( 4, buf, p1, p2, nframe, ec);
  process_channel( 5, buf, p2, p1, nframe, ec);
  process_channel( 6, buf, p1, p2, nframe, ec);
  process_channel( 7, buf, p2, p1, nframe, ec);

  process_channel( 8, buf, p1, p2, nframe, ec);
  process_channel( 9, buf, p2, p1, nframe, ec);
  process_channel(10, buf, p1, p2, nframe, ec);
  process_channel(11, buf, p2, p1, nframe, ec);
  process_channel(12, buf, p1, p2, nframe, ec);
  process_channel(13, buf, p2, p1, nframe, ec);
  process_channel(14, buf, p1, p2, nframe, ec);
  process_channel(15, buf, p2, p1, nframe, ec);

  process_channel(16, buf, p1, p2, nframe, ec);
  process_channel(17, buf, p2, p1, nframe, ec);
  process_channel(18, buf, p1, p2, nframe, ec);
  process_channel(19, buf, p2, p1, nframe, ec);
  process_channel(20, buf, p1, p2, nframe, ec);
  process_channel(21, buf, p2, p1, nframe, ec);
  process_channel(22, buf, p1, p2, nframe, ec);
  process_channel(23, buf, p2, p1, nframe, ec);

  apply_reverb(ec, nframe);
  mix_end(buf, ec, nframe);
}


  //TODO 混合CD/混响/外部音源
void SoundProcessing::mix_end(PcmSample* buf, PcmSample* echo, u32 nframe) {
  PcmSample main_left  = SPU_F_VOLUME(mainVol.r.v & 0xFFFF);
  PcmSample main_right = SPU_F_VOLUME(mainVol.r.v >> 16);
  PcmSample echo_left  = SPU_F_VOLUME(reverbVol.r.v & 0xFFFF);
  PcmSample echo_right = SPU_F_VOLUME(reverbVol.r.v >> 16);

  for (u32 i = 0; i < (nframe<<1); i+=2) {
    buf[i+0] = buf[i+0] * main_left  + echo[i+0] * echo_left;
    buf[i+1] = buf[i+1] * main_right + echo[i+1] * echo_right;
  }
}


void SoundProcessing::apply_reverb(PcmSample* echo, u32 nframe) {
#define A(reg)    s16(reg->address()>>1)
#define V(reg)    s16(reg->sl)
#define T         double
  const s16 mLSAME = A(this->mLSAME);
  const s16 mRSAME = A(this->mRSAME);
  const s16 dLSAME = A(this->dLSAME);
  const s16 dRSAME = A(this->dRSAME);
  const T   vWALL  = V(this->vWALL);
  const T   vIIR   = V(this->vIIR);
  const s16 mLDIFF = A(this->mLDIFF);
  const s16 mRDIFF = A(this->mRDIFF);
  const s16 dRDIFF = A(this->dRDIFF);
  const s16 dLDIFF = A(this->dLDIFF);
  const T   vCOMB1 = V(this->vCOMB1);
  const T   vCOMB2 = V(this->vCOMB2);
  const T   vCOMB3 = V(this->vCOMB3);
  const T   vCOMB4 = V(this->vCOMB4);
  const s16 mLCOMB1= A(this->mLCOMB1);
  const s16 mLCOMB2= A(this->mLCOMB2);
  const s16 mLCOMB3= A(this->mLCOMB3);
  const s16 mLCOMB4= A(this->mLCOMB4);
  const s16 mRCOMB1= A(this->mRCOMB1);
  const s16 mRCOMB2= A(this->mRCOMB2);
  const s16 mRCOMB3= A(this->mRCOMB3);
  const s16 mRCOMB4= A(this->mRCOMB4);
  const s16 mLAPF1 = A(this->mLAPF1);
  const s16 mLAPF2 = A(this->mLAPF2);
  const s16 mRAPF1 = A(this->mRAPF1);
  const s16 mRAPF2 = A(this->mRAPF2);
  const s16 dAPF1  = A(this->dAPF1);
  const s16 dAPF2  = A(this->dAPF2);
  const T   vAPF1  = V(this->vAPF1);
  const T   vAPF2  = V(this->vAPF2);

  const PcmSample maxVol = 0x8000;
  const bool masterEchoEnable = ctrl.r.mst_rev;
  const u32 begin = reverbBegin.r.address() & SPU_MEM_ECHO_MASK;
  const u32 len   = SPU_MEM_ECHO_MASK - begin;
  const s32 hwlen = len >> 1;
  s16 *base = (s16*)(mem + begin);

#define L(addr)     base[(echo_addr_offset + ((addr)>>1)) % hwlen]
#define R(addr)     L(addr + hwlen)
#define MUL(a, b)   ((T(a)*T(b)) / maxVol)

  for (u32 i = 0; i < (nframe<<1); i+=2) {
    if (masterEchoEnable) {
      //TODO: echo输入音量超过x引起爆音, 寻找x
      T Lin = T(vLIN.r.sl) * echo[i+0] *0.7;
      T Rin = T(vRIN.r.sl) * echo[i+1] *0.7;

      L(mLSAME) = MUL((Lin + MUL(L(dLSAME), vWALL) - L(mLSAME -2)), vIIR) + L(mLSAME -2);
      R(mRSAME) = MUL((Rin + MUL(R(dRSAME), vWALL) - R(mRSAME -2)), vIIR) + R(mRSAME -2);

      L(mLDIFF) = MUL((Lin + MUL(L(dRDIFF), vWALL) - L(mLDIFF -2)), vIIR) + L(mLDIFF -2);
      R(mRDIFF) = MUL((Rin + MUL(R(dLDIFF), vWALL) - R(mRDIFF -2)), vIIR) + R(mRDIFF -2);
    }

    T Lout = MUL(vCOMB1, L(mLCOMB1)) +MUL(vCOMB2, L(mLCOMB2)) +MUL(vCOMB3, L(mLCOMB3)) +MUL(vCOMB4, L(mLCOMB4));
    T Rout = MUL(vCOMB1, R(mRCOMB1)) +MUL(vCOMB2, R(mRCOMB2)) +MUL(vCOMB3, R(mRCOMB3)) +MUL(vCOMB4, R(mRCOMB4));

    T o1 = Lout - MUL(vAPF1, L(mLAPF1 - dAPF1));
    Lout = MUL(o1, vAPF1) + L(mLAPF1 - dAPF1);

    T o2 = Rout - MUL(vAPF1, R(mRAPF1 - dAPF1));
    Rout = MUL(o2, vAPF1) + R(mRAPF1 - dAPF1);

    T o3 = Lout - MUL(vAPF2, L(mLAPF2 - dAPF2));
    Lout = MUL(o3, vAPF2) + L(mLAPF2 - dAPF2);

    T o4 = Rout - MUL(vAPF2, R(mRAPF2 - dAPF2));
    Rout = MUL(o4, vAPF2) + R(mRAPF2 - dAPF2);

    if (masterEchoEnable) {
      L(mLAPF1) = o1;
      R(mRAPF1) = o2;
      L(mLAPF2) = o3;
      R(mRAPF2) = o4;
    }

    echo[i+0] = T(Lout) / maxVol;
    echo[i+1] = T(Rout) / maxVol;
    ++echo_addr_offset;
  }
#undef A
#undef V
#undef L
#undef R
#undef MUL
#undef T
}


void SoundProcessing::set_transfer_address(u32 a, u32) {
  mem_write_addr = a << 3; // * 8
  spudbg("set transfer address %x (%x << 3)\n", mem_write_addr, a);
}


void SoundProcessing::push_fifo(u32 a, u32) {
  //if (ps1e_t::ext_stop) printf("spu fifo %02x = %04x\n", fifo_point, a);
  fifo[fifo_point & SPU_FIFO_MASK] = u16(a);
  ++fifo_point;
}


void SoundProcessing::trigger_manual_write() {
  std::lock_guard<std::mutex> _lk(for_copy_data);
  status.r.busy = 1;

  if (ps1e_t::ext_stop) {
    spudbg("\rTrigger spu data trans, start at %x, MODE %x", 
            mem_write_addr, 0B111 & (ramTransferCtrl.r.v >> 1));
    print_fifo();
  }
      
  copy_fifo_to_mem();
  ///// END
  // 当 irq 控制位为 关闭/应答(0) 的时候, 复位 irq 状态位
  if (!ctrl.r.irq_enb) {
    status.r.irq = 0;
  }
  status.r.busy = 0;
  status.r.dma_trs = ctrl.r.dma_trs;
}


void SoundProcessing::copy_fifo_to_mem() {
  u16 *wbuf = (u16*)(&mem[mem_write_addr]);
  const u8 type = 0B111 & (ramTransferCtrl.r.v >> 1);
  u16 v;

  switch (type) {
    case 0: case 1: case 6: case 7:
      v = fifo[SPU_FIFO_INDEX(fifo_point + SPU_FIFO_MASK)];
      for (int i=0; i<SPU_FIFO_SIZE; ++i) {
        wbuf[i] = v;
      }
      break;

    case 2:
      for (int i=0; i<SPU_FIFO_SIZE; ++i) {
        wbuf[i] = fifo[SPU_FIFO_INDEX(fifo_point + i)];
      }
      break;

    case 3:
      for (int i=0; i<SPU_FIFO_SIZE; ++i) {
        wbuf[i] = fifo[SPU_FIFO_INDEX(fifo_point + (i & 0xFE))];
      }
      break;

    case 4:
      for (int i=0; i<SPU_FIFO_SIZE; ++i) {
        wbuf[i] = fifo[SPU_FIFO_INDEX(fifo_point + (i & 0xFC))];
      }
      break;

    case 5:
      for (int i=0; i<SPU_FIFO_SIZE; ++i) {
        wbuf[i] = fifo[SPU_FIFO_INDEX(fifo_point + 7 + (i & 0xF8))];
      }
      break;
  }

  check_irq(SPU_FIFO_INDEX(mem_write_addr), (SPU_FIFO_SIZE << 1));
  mem_write_addr += (SPU_FIFO_SIZE << 1);
}


//TODO: DMA 应用FIFO, 待测试
void SoundProcessing::dma_ram2dev_block(psmem addr, u32 bytesize, s32 inc) {
  std::lock_guard<std::mutex> _lk(for_copy_data);
  if (SpuDmaDir(ctrl.r.dma_trs) != SpuDmaDir::DMAwrite) return;
  status.r.busy = 1;

  u32 waddr = mem_write_addr;
  u16 *m16  = (u16*) mem;
  const int step = inc << 1;
  const int len = bytesize >> 1;

  for (int i=0; i < len; ++i) {
    m16[i + waddr] = bus.read16(addr);
    addr += step;
  }

  check_irq(waddr, bytesize);
  status.r.busy = 0;
  status.r.dma_w = 0;
  status.r.dma_rw = 0;
}


//TODO: DMA 应用FIFO, 待测试
void SoundProcessing::dma_dev2ram_block(psmem addr, u32 bytesize, s32 inc) {
  std::lock_guard<std::mutex> _lk(for_copy_data);
  if (SpuDmaDir(ctrl.r.dma_trs) != SpuDmaDir::DMAread) return;
  status.r.busy = 1;

  u32 waddr = mem_write_addr;
  u16 *m16  = (u16*) mem;
  const int step = inc << 1;
  const int len = bytesize >> 1;

  for (int i=0; i < len; ++i) {
    bus.write16(addr, m16[i + waddr]);
    addr += step;
  }

  check_irq(waddr, bytesize);
  status.r.busy = 0;
  status.r.dma_r = 0;
  status.r.dma_rw = 0;
}


void SoundProcessing::send_irq() {
  bus.send_irq(IrqDevMask::spu);
  status.r.irq = 1;
}


bool SoundProcessing::check_irq(u32 beginAddr, u32 offset) {
  if (ctrl.r.irq_enb) {
    const u32 irq_addr = ramIrqAdress.r.address();
    if (ADDR_IN_SCOPE(beginAddr, offset, irq_addr)) {
      send_irq();
      return true;
    }
  }
  return false;
}


AdpcmFlag SoundProcessing::readAdpcmBlock(PcmSample *buf, PcmHeader& h) {
  const u32 readAddr = h.addr & SPU_MEM_MASK & 0xFFFF'FFF0;
  check_irq(readAddr, SPU_MEM_SIZE);
  AdpcmBlock* af = (AdpcmBlock*) &mem[readAddr];

  u8 coef_index   = (af->filter >> 4) & 0xf;
  u8 shift_factor = (af->filter >> 0) & 0xf;
  u8 nbit = 0;
  s32 nibble;
  PcmSample sp;

  if (shift_factor > 12) shift_factor = 9; //?
  if (coef_index > 5) coef_index = 0; //?
  shift_factor = 20 - shift_factor;

  for (int i=0; i<SPU_PCM_BLK_SZ; ++i) {
    u8 niindex = af->data[nbit].v;
    if (i & 1) {
      nibble = nibble_dict[niindex >> 4];
      ++nbit;
    } else {
      nibble = nibble_dict[niindex & 0x0F];
    }

    sp = nibble << shift_factor;
    sp += ((adpcm_coefs_dict[coef_index][0]* h.hist1 
          + adpcm_coefs_dict[coef_index][1]* h.hist2) * 256.0f);
    sp = sp / 256.0f;

    // 一般很少出现
    if (sp > 32767) sp = 32767;
    else if (sp < -32768) sp = -32768;

    buf[i] = sp / 32768.0f;
    h.hist2 = h.hist1;
    h.hist1 = sp;
  }
  //spudbg("ADPCM %x %x\n", readAddr, af->flag);
  return af->flag;
}


// 每个通道生成的噪声不同
void SoundProcessing::readNoiseSampleBlocks(PcmSample* buf, u32 nframe) {
  for (u32 i=0; i<nframe; ++i) {
    noiseTimer -= (4 + ctrl.r.nos_stp);
    if (noiseTimer < 0) {
      noiseTimer += (0x2'0000 >> ctrl.r.nos_shf);
      s32 pb = ((nsLevel>>15) & 1) ^ ((nsLevel>>12) & 1) ^ 
               ((nsLevel>>11) & 1) ^ ((nsLevel>>10) & 1) ^ 1;
      nsLevel = (nsLevel << 1) | pb;
    }
    buf[i] = nsLevel / PcmSample(0x7fff);
  }
}


void SoundProcessing::setEndxFlag(u8 channelIndex) {
  endx.set(channelIndex);
}


bool SoundProcessing::isAttackOn(u8 channelIndex) {
  bool v = nKeyOn.get(channelIndex);
  nKeyOn.reset(channelIndex);
  return v;
}


bool SoundProcessing::isReleaseOn(u8 channelIndex) {
  bool v = nKeyOff.get(channelIndex);
  nKeyOff.reset(channelIndex);
  return v;
}


bool SoundProcessing::isNoise(u8 channelIndex) {
  return nNoise.get(channelIndex);
}


bool SoundProcessing::usePrevChannelFM(u8 channelIndex) {
  return nFM.get(channelIndex);
}


bool SoundProcessing::isEnableEcho(u8 channelIndex) {
  return ctrl.r.mst_rev && nReverb.get(channelIndex);
}


bool SoundProcessing::isEnableCDEcho() {
  return ctrl.r.mst_rev && ctrl.r.cda_rev;
}


bool SoundProcessing::isEnableCDVolume() {
  return ctrl.r.cda_enb;
}


bool SoundProcessing::isEnableExternalEcho() {
  return ctrl.r.mst_rev && ctrl.r.exa_rev;
}


void SoundProcessing::print_fifo() {
  PrintfBuf buf;
  buf.printf("FIFO point %x\n", fifo_point);

  for (u32 i=0; i<SPU_FIFO_SIZE; ++i) {
    u32 a = (fifo_point+i) & SPU_FIFO_MASK;
    buf.printf(" [%02x %04x]", a, fifo[a]);
    if ((i & 0b111) == 0b111) {
      buf.putchar('\n');
    }
  }
  buf.putchar('\n');
}


u8* SoundProcessing::get_spu_mem() {
  return mem;
}


u32 SoundProcessing::get_var(SpuChVarFlag f, int c) {
  switch (f) {
    case SpuChVarFlag::key_on:
      return c>=0 ? nKeyOn.get(c) : nKeyOn.read();

    case SpuChVarFlag::key_off:
      return c>=0 ? nKeyOff.get(c) : nKeyOff.read();

    case SpuChVarFlag::endx:
      return c>=0 ? endx.get(c) : endx.read();

    case SpuChVarFlag::fm:
      return c>=0 ? nFM.get(c) : nFM.read();

    case SpuChVarFlag::noise:
      return c>=0 ? nNoise.get(c) : nNoise.read();

    case SpuChVarFlag::echo:
      return c>=0 ? nReverb.get(c) : nReverb.read();

    case SpuChVarFlag::ctrl:
      return ctrl.r.v;
  }
  if (c >= 0 && c < 24) {
    return channel_stream[c]->getVar(f);
  }
  return 0;
}


void SoundProcessing::set_ctrl_req(u32, u32) {
  switch (SpuDmaDir(ctrl.r.dma_trs)) {
    case SpuDmaDir::ManualWrite:
      trigger_manual_write();
      break;

    case SpuDmaDir::DMAread:
      status.r.dma_trs = ctrl.r.dma_trs;
      status.r.dma_r = 1;
      status.r.dma_rw = 1;
      break;

    case SpuDmaDir::DMAwrite:
      status.r.dma_trs = ctrl.r.dma_trs;
      status.r.dma_w = 1;
      status.r.dma_rw = 1;
      break;
  }
}


void SoundProcessing::key_on_changed() {
  for (int i=0; i<SPU_CHANNEL_COUNT; ++i) {
    if (nKeyOn.f[i]) {
      endx.f[i] = 0;
      channel_stream[i]->copyStartToRepeat();
    }
  }
}


u32 SoundProcessing::getOutputRate() {
  return devSampleRate;
}


void SpuAdsr::reset(u8 mode, u8 dir, u8 shift, u8 _step) {
  this->isExponential = (mode == 1);
  this->initCyc = 1 << MaxT(0, s32(shift) - 11);
  this->isInc = dir == 0;
  s32 rstep = isInc ? (7 - s32(_step)) : (-8 + s32(_step));
  this->step = s32(rstep << MaxT(0, 11 - s32(shift)));
}


void SpuAdsr::next(s32& level, u32& cycles) {
  cycles = initCyc;
  if (isExponential) {
    if (!isInc) {
      PcmSample st = step * PcmSample(level) / PcmSample(0x8000);
      if (st < 0) {
        cycles = initCyc * (step / st);
      }
    } else if (level > 0x6000) {
      // 没有指数上升
      cycles = initCyc * 4;
    }
  }
  level += step;
}


void PcmHeader::set(u32 a, PcmSample h1, PcmSample h2, bool c) {
  addr = a;
  hist1 = h1;
  hist2 = h2;
  changed = c;
}


bool PcmHeader::sameAddr(PcmHeader& o) {
  return addr == o.addr;
}


static long resample_src_callback(void *cb_data, float **data) {
  PcmResample *p = static_cast<PcmResample*>(cb_data);
  return p->readSrc(data);
}


PcmResample::PcmResample(PcmStreamer *p) : stage(0), stream(p) {
  // SRC_SINC_MEDIUM_QUALITY SRC_SINC_FASTEST
  int error;
  stage = src_callback_new(resample_src_callback, SRC_SINC_MEDIUM_QUALITY, 1, &error, this);
  check_error(error);
  buf = new float[buf_size];
}


PcmResample::~PcmResample() {
  src_delete(stage);
  delete[] buf;
}


void PcmResample::check_error(int error, bool throwErr) {
  if (error) {
    std::string msg = "PCM resample error, ";
    msg += src_strerror(error);
    if (throwErr) {
      throw std::runtime_error(msg);
    } else {
      puts(msg.c_str());
    }
  }
}


bool PcmResample::read(float* out, long frames, double ratio) {
  //src_set_ratio(static_cast<SRC_STATE*>(stage), ratio);
  if (0 == src_callback_read(stage, ratio, frames, out)) {
    check_error(src_error(stage));
    return 0;
  }
  return 1;
}


long PcmResample::readSrc(float **out) {
  for (u32 i=0; i<buf_size; ++i) {
    buf[i] = stream->readPcmSample();
  }
  *out = buf;
  return buf_size;
}


class PcmLowpassInner {
public:
  static const int order = 1;
  static const int cutoff_frequency = 4;

  Iir::ChebyshevI::LowPass<order> f1;
  Iir::ChebyshevI::LowPass<order> f2;

  PcmLowpassInner(float samplingrate) {
    f1.setup(samplingrate, 6000, cutoff_frequency);
    f2.setup(samplingrate, 3000, cutoff_frequency);
  }
};


PcmLowpass::PcmLowpass(float samplingrate) : inner(0) {
  inner = new PcmLowpassInner(samplingrate);
}


PcmLowpass::~PcmLowpass() {
  delete inner;
}


void PcmLowpass::filter(PcmSample *data, u32 frame, double freq) {
  if (freq < 20000) {
    for (u32 i = 0; i < frame; ++i) {
      data[i] = inner->f2.filter(data[i]);
    }
  } else if (freq < 40000) {
    for (u32 i = 0; i < frame; ++i) {
      data[i] = inner->f1.filter(data[i]);
    }
  }
}


VolumeEnvelope* VolumeEnvelopeSet::get(bool left, VolData& vd, s32 clevel) {
  if (vd.mode == 0) {
    if (vd.vol == 0) {
      return left ? &lz : &rz;
    } else {
      FixVol* f = left ? &lf : &rf;
      f->reset(vd);
      return f;
    }
  }

  Sweep* s = left ? &ls : &rs;
  s->reset(vd, clevel);
  return s;
}


void VolumeEnvelopeSet::Sweep::apply(PcmSample& v) {
  if (cycles <= 0) {
    if (dir == 0 && level > 0x7fff) {
      dir = 1;
      level = 0x7fff;
      filter.reset(arg.mode, dir, arg.shift, arg.step);
    }
    else if (dir == 1 && level < 0) {
      dir = 0;
      level = 0;
      filter.reset(arg.mode, dir, arg.shift, arg.step);  
    }
    filter.next(level, cycles);
  }
  --cycles;
  //当前音量将增加到+ 7FFFh，或减少到0000h
  v *= (float(level) / float(0x8000)) * phase;
}


void VolumeEnvelopeSet::Sweep::reset(VolData& vd, s32 clevel) {
  if (vd.v != arg.v) {
    filter.reset(vd.mode, vd.dir, vd.shift, vd.step);
    arg    = vd;
    dir    = vd.dir;
    cycles = 0;
    level  = clevel;

    if (vd.phase == 0 || vd.mode == 0) {
      phase = 1;
    } else if (vd.phase == 1) {
      phase = -1;
    }
  }
}

s16 VolumeEnvelopeSet::Sweep::getVol() {
  return u16(level & 0xffff);
}


void VolumeEnvelopeSet::FixVol::apply(PcmSample& v) {
  v *= vol;
}


void VolumeEnvelopeSet::FixVol::reset(VolData& vd) {
  vol = SPU_F_VOLUME(vd.vol << 1);
}


s16 VolumeEnvelopeSet::FixVol::getVol() {
  return u16(vol);
}


void VolumeEnvelopeSet::DoZero::apply(PcmSample& v) {
  //Do nothing
}


s16 VolumeEnvelopeSet::DoZero::getVol() {
  return 0;
}

}