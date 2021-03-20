#include <rtaudio/RtAudio.h>
#include "spu.h"
#include "spu.inl"

namespace ps1e {

#define SPU_INIT_CHANNEL(name, n)  name ## n(*this, b),
#define SPU_II(name) name(*this, b)


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
  

SoundProcessing::SoundProcessing(Bus& b) : 
  DMADev(b, DeviceIOMapper::dma_spu_base), bus(b), dac(0),
  SPU_II(mainVol),  SPU_II(cdVol),    SPU_II(reverbVol),
  SPU_II(externVol),SPU_II(nKeyOn),   SPU_II(mainCurrVol),
  SPU_II(nKeyOff),  SPU_II(nFM),      SPU_II(nNoise),
  SPU_II(nReverb),  SPU_II(endx),     SPU_II(ctrl),
  SPU_II(status),
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
  ramTransferAddress(*this, b, &SoundProcessing::set_transfer_address)
{
  memset(mem, 0, SPU_MEM_SIZE);
  memset(fifo, 0, SPU_FIFO_SIZE << 1);
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

    u32 bf = bufferFrames;
    dac->openStream(&parameters, NULL, RTAUDIO_FLOAT32,
                    sampleRate, &bf, &SpuRtAudioCallback, this);

    if (bf != bufferFrames) {
      error("Cannot use %d buffer frames, must be %d\n", bufferFrames, bf);
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
    dac = 0;
  }
}


void SoundProcessing::request_audio_data(PcmSample *buf, u32 nframe, double time) {
  //printf("\r\t\t\t\tReQ audio data %d %f", nframe, time);
  ch0.read_sample(buf, nframe / 28);
}


void SoundProcessing::set_transfer_address(u32 a) {
  trans_point = a << 3; // * 8
}


void SoundProcessing::push_fifo(u32 a) {
  fifo[fifo_point++] = u16(a);
}


void SoundProcessing::process() {
  status.r.busy = 1;
  
  if (ctrl.r.dma_trs == u8(SpuDmaDir::ManualWrite)) {
    printf("Trigger spu data trans\n");
    ctrl.r.dma_trs = u8(SpuDmaDir::Stop);
    manual_write();
  }
  //else if (ctrl.r.dma_trs == u8(SpuDmaDir::DMAwrite)) {
  //  //TODO wait??
  //}
  //else if (ctrl.r.dma_trs == u8(SpuDmaDir::DMAread)) {
  //  //TODO
  //}

  ///// END
  // 当 irq 控制位为 关闭/应答(0) 的时候, 复位 irq 状态位
  if (!ctrl.r.irq_enb) {
    status.r.irq = 0;
  }
  status.r.mode = ctrl.r.v & 0B11111;
  status.r.busy = 0;
}


void SoundProcessing::manual_write() {
  u16 *start = (u16*)(&mem[trans_point]);
  const u8 t = 0B111 & (ramTransferCtrl.r.v >> 1);
  u16 v;

  switch (t) {
    case 0: case 1: case 6: case 7:
      v = fifo[SPU_FIFO_INDEX(fifo_point + SPU_FIFO_MASK)];
      for (int i=0; i<SPU_FIFO_SIZE; ++i) {
        start[i] = v;
      }
      break;

    case 2:
      for (int i=0; i<SPU_FIFO_SIZE; ++i) {
        start[i] = fifo[SPU_FIFO_INDEX(fifo_point + i)];
      }
      break;

    case 3:
      for (int i=0; i<SPU_FIFO_SIZE; ++i) {
        start[i] = fifo[SPU_FIFO_INDEX(fifo_point + (i & 0xFE))];
      }
      break;

    case 4:
      for (int i=0; i<SPU_FIFO_SIZE; ++i) {
        start[i] = fifo[SPU_FIFO_INDEX(fifo_point + (i & 0xFC))];
      }
      break;

    case 5:
      for (int i=0; i<SPU_FIFO_SIZE; ++i) {
        start[i] = fifo[SPU_FIFO_INDEX(fifo_point + 7 + (i & 0xF8))];
      }
      break;
  }

  check_irq(SPU_FIFO_INDEX(trans_point), (SPU_FIFO_SIZE << 1));
  trans_point += (SPU_FIFO_SIZE << 1);
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


static PcmSample nibble_dict[16] = {0,1,2,3,4,5,6,7,-8,-7,-6,-5,-4,-3,-2,-1};
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


AdpcmFlag SoundProcessing::read_adpcm_block(PcmSample *buf, 
                                            u32 readAddr, 
                                            PcmSample& hist1, 
                                            PcmSample& hist2) 
{
  AdpcmBlock* af = (AdpcmBlock*) &mem[ SPU_MEM_MASK & readAddr ];
  check_irq(SPU_MEM_MASK & readAddr, SPU_MEM_SIZE);

  u8 coef_index   = (af->filter >> 4) & 0xf;
  u8 shift_factor = (af->filter >> 0) & 0xf;
  PcmSample sample;

  for (int i=0; i<28; ++i) {
    if (af->flag.unknow) {
      sample = 0;
    } 
    else {
      u8 nibble = af->data[i & 0xF0].v;
      if (i & 1) {
        sample = nibble_dict[nibble >> 4];
      } else {
        sample = nibble_dict[nibble & 0x0F];
      }
      sample += ((adpcm_coefs_dict[coef_index][0] * hist1 
                + adpcm_coefs_dict[coef_index][1] * hist2));
      //sample >>= 8; //???!!! s32 sample
      sample /= 8;
    }

    /*if (sample > 32767) {
      (*buf)[i] = 32767;
    } else if (sample < -32768) {
      (*buf)[i] = -32768;
    } else {
      (*buf)[i] = sample;
    }*/
    buf[i] = sample;
    hist2 = hist1;
    hist1 = sample;
  }
  return af->flag;
}


void SoundProcessing::set_endx_flag(u8 channelIndex) {
  endx.set(channelIndex);
}


bool SoundProcessing::is_attack_on(u8 channelIndex) {
  return nKeyOn.get(channelIndex);
}


bool SoundProcessing::is_release_on(u8 channelIndex) {
  return nKeyOff.get(channelIndex);
}


}