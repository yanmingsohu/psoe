#include <rtaudio/RtAudio.h>
#include "spu.h"
#include "spu.inl"

namespace ps1e {

#define SPU_INIT_CHANNEL(name, n)  name ## n(*this, b),
#define SPU_II(name) name(*this, b)

#ifdef SPU_DEBUG_INFO
  void _spudbg(const char* format, ...) {
    warp_printf(format, "\x1b[32m\x1b[44mSPU: ");
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


int SpuRtAudioCallback( void *outputBuffer, void *inputBuffer,
                        unsigned int nFrames,
                        double streamTime,
                        RtAudioStreamStatus status,
                        void *userData ) 
{
  auto p = static_cast<SoundProcessing*>(userData);
  p->request_audio_data(static_cast<PcmSample*>(outputBuffer), nFrames, streamTime);
  return 0;
}


// 混音算法 
// int32: C = A + B - (A * B >> 0x10)
// float: C = A + B - (A * B)
static inline PcmSample mixer(PcmSample a, PcmSample b) {
  return (a + b) - (a * b);
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
  SPU_II(vAPF2),    SPU_II(mSAMEr),   SPU_II(mCOMB1r),
                    SPU_II(mSAMEl),   SPU_II(mCOMB1l),
  SPU_II(mCOMB2r),  SPU_II(dSAMEr),   SPU_II(mDIFFr),
  SPU_II(mCOMB2l),  SPU_II(dSAMEl),   SPU_II(mDIFFl),
  SPU_II(mCOMB3r),  SPU_II(mCOMB4r),  SPU_II(dDIFFr),
  SPU_II(mCOMB3l),  SPU_II(mCOMB4l),  SPU_II(dDIFFl),
  SPU_II(mAPF1r),   SPU_II(mAPF2r),   SPU_II(vINr),
  SPU_II(mAPF1l),   SPU_II(mAPF2l),   SPU_II(vINl),
  SPU_DEF_ALL_CHANNELS(ch, SPU_INIT_CHANNEL) _un1(*this, b), _un2(*this, b),
  SPU_II(ramReverbStartAddress), SPU_II(ramIrqAdress), SPU_II(ramTransferCtrl),
  ramTransferFifo(*this, b, &SoundProcessing::push_fifo),
  ramTransferAddress(*this, b, &SoundProcessing::set_transfer_address),
  ctrl(*this, b, &SoundProcessing::set_ctrl_req),
  nKeyOn(*this, b, &SoundProcessing::key_on_changed)
{
  mem = new u8[SPU_MEM_SIZE];
  memset(mem, 0, SPU_MEM_SIZE);
  memset(fifo, 0, SPU_FIFO_SIZE << 1);
  init_dac();
}


void SoundProcessing::init_dac() {
  try {
    dac = new RtAudio();
    RtAudio::StreamParameters parameters;
    parameters.deviceId = dac->getDefaultOutputDevice();
    parameters.nChannels = 1;
    parameters.firstChannel = 0;

    RtAudio::DeviceInfo di = dac->getDeviceInfo(parameters.deviceId);
    info("Audio Device: %s - %dHz\n", di.name.c_str(), di.preferredSampleRate);

    u32 bf = bufferFrames;
    dac->openStream(&parameters, NULL, RTAUDIO_FLOAT32,
                    sampleRate, &bf, &SpuRtAudioCallback, this);

    if (bf != bufferFrames) {
      error("Cannot use %d buffer frames, set to %d\n", bufferFrames, bf);
    }
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


static void mix(PcmSample *dst, PcmSample *src, u32 nframe) {
  for (u32 i=0; i<nframe; ++i) {
    dst[i] = mixer(dst[i], src[i]);
  }
}


#define CALL_READ_SAMPLE(name, n)   name ## n.read_sample(buf, blockcount);

void SoundProcessing::request_audio_data(PcmSample *buf, u32 nframe, double time) {
  if (nframe != bufferFrames) {
    error("Cannot use %d buffer frames, set to %d\n", bufferFrames, nframe);
  }
  //spudbg("\r\t\t\t\tReQ audio data %d %f", nframe, time);
  u32 blockcount = nframe / SPU_PCM_BLK_SZ;
  setzero(buf, nframe);

  std::shared_ptr<PcmSample> p1(new PcmSample[nframe]);
  std::shared_ptr<PcmSample> p2(new PcmSample[nframe]);

  //SPU_DEF_ALL_CHANNELS(ch, CALL_READ_SAMPLE)
  ch0.read_sample(buf, p2.get(), blockcount);
  mix(buf, p2.get(), nframe);
  ch1.read_sample(p2.get(), p1.get(), blockcount);
  mix(buf, p1.get(), nframe);
  ch2.read_sample(p1.get(), p2.get(), blockcount);
  mix(buf, p2.get(), nframe);
  ch3.read_sample(p2.get(), p1.get(), blockcount);
  mix(buf, p1.get(), nframe);
}

#undef CALL_READ_SAMPLE


void SoundProcessing::set_transfer_address(u32 a) {
  mem_write_addr = a << 3; // * 8
  spudbg("set transfer address %x (%x << 3)\n", mem_write_addr, a);
}


void SoundProcessing::push_fifo(u32 a) {
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


void SoundProcessing::dma_ram2dev_block(psmem addr, u32 bytesize, s32 inc) {
  throw std::runtime_error("not implement DMA RAM to Device"); 
}


void SoundProcessing::dma_dev2ram_block(psmem addr, u32 bytesize, s32 inc) {
  throw std::runtime_error("not implement DMA Device to RAM");
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


AdpcmFlag SoundProcessing::read_adpcm_block(PcmSample *buf, PcmHeader& h) {
  u32 readAddr = h.addr & SPU_MEM_MASK & 0xFFFF'FFF0;
  check_irq(readAddr, SPU_MEM_SIZE);
  AdpcmBlock* af = (AdpcmBlock*) &mem[readAddr];
  //spudbg("ADPCM %x\n", readAddr);

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
  return af->flag;
}


void SoundProcessing::set_endx_flag(u8 channelIndex) {
  endx.set(channelIndex);
}


bool SoundProcessing::is_attack_on(u8 channelIndex) {
  bool v = nKeyOn.get(channelIndex);
  nKeyOn.reset(channelIndex);
  return v;
}


bool SoundProcessing::is_release_on(u8 channelIndex) {
  return nKeyOff.get(channelIndex);
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


void SoundProcessing::set_ctrl_req(u32) {
  if (ctrl.r.dma_trs == u8(SpuDmaDir::ManualWrite)) {
    trigger_manual_write();
  }
}


#define SPU_CASE_COPY(name, n)  case n: name ## n.copy_start_to_repeat();

void SoundProcessing::key_on_changed() {
  for (u8 i=0; i<SPU_CHANNEL_COUNT; ++i) {
    if (nKeyOn.f[i]) {
      endx.f[i] = 0;
      switch (i) {
        SPU_DEF_ALL_CHANNELS(ch, SPU_CASE_COPY)
      }
    }
  }
}


}