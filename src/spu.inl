#pragma once

#include "spu.h"

namespace ps1e {

#define CONSTRUCT
#define SPU_CHANNEL_DEF(RET) \
  template<DeviceIOMapper t_vol, DeviceIOMapper t_sr, \
         DeviceIOMapper t_sa,  DeviceIOMapper t_adsr, \
         DeviceIOMapper t_acv, DeviceIOMapper t_ra,   \
         DeviceIOMapper t_cv,  int Number>            \
  RET SPUChannel<t_vol, t_sr, t_sa, t_adsr, t_acv, t_ra, t_cv, Number>


SPU_CHANNEL_DEF(CONSTRUCT)::SPUChannel(SoundProcessing& parent, Bus& b) :
  spu(parent),
  volume(*this, b), 
  pcmSampleRate(*this, b), 
  pcmStartAddr(*this, b, &SPUChannel::set_start_address),
  adsr(*this, b), 
  adsrVol(*this, b), 
  pcmRepeatAddr(*this, b), 
  currVolume(*this, b) 
{
}


//TODO 双声道
SPU_CHANNEL_DEF(void)::read_sample(PcmSample* buf, u32 blockcount) 
{
  u32 repeat = pcmRepeatAddr.r.address();
  PcmSample* pcmbuf = buf;

  for (u32 i=0; i<blockcount; ++i) {
    AdpcmFlag flag = spu.read_adpcm_block(pcmbuf, currentReadAddr, hist1, hist2);

    resample(pcmbuf, blockcount);
    apply_adsr(pcmbuf, blockcount);
    
    if (flag.loop_start) {
      repeat = currentReadAddr;
      pcmRepeatAddr.r.saveAddr(repeat);
    } 
    else if (flag.loop_end) {
      currentReadAddr = repeat;
      spu.set_endx_flag(Number);
      
      if (!flag.loop_repeat) {
        //Force Release, Env=0000h
        adsr_state = AdsrState::Release;
      }
    }
    else if (ADDR_IN_SCOPE(currentReadAddr, SPU_ADPCM_BLK_SZ, repeat)) {
      currentReadAddr = repeat;
    }
    else {
      currentReadAddr += SPU_ADPCM_BLK_SZ;
    }
    pcmbuf += SPU_PCM_BLK_SZ;
  }
}


SPU_CHANNEL_DEF(void)::apply_adsr(PcmSample* buf, u32 blockcount) {
  // adsr and volume
  switch (adsr_state) {
    case AdsrState::Wait:
      if (spu.is_attack_on(Number)) {
        adsr_state = AdsrState::Attack;
      }
      break;

    case AdsrState::Attack:
      //TODO: Attack
      break;

    case AdsrState::Decay:
      //TODO: Decay
      break;

    case AdsrState::Sustain:
      if (spu.is_release_on(Number)) {
        adsr_state = AdsrState::Release;
      } else {
        //TODO Sustain
      }
      break;

    case AdsrState::Release:
      //TODO: Release
      break;
  }
}


SPU_CHANNEL_DEF(void)::resample(PcmSample* buf, u32 blockcount) {
}


SPU_CHANNEL_DEF(void)::set_start_address(u32 v) {
  currentReadAddr = v << 3;
  spudbg("set channel %d start address %x (%x << 3)\n", Number, currentReadAddr, v);
}


#undef SPU_CHANNEL_DEF
}