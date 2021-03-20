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
  pcmStartAddr(*this, b),
  adsr(*this, b), 
  adsrVol(*this, b), 
  pcmRepeatAddr(*this, b), 
  currVolume(*this, b) 
{
}


//TODO 双声道
SPU_CHANNEL_DEF(void)::read_sample(PcmSample* buf, u32 blockcount) 
{
  u32 current = pcmStartAddr.r.address();
  u32 repeat  = pcmRepeatAddr.r.address();

  for (u32 i=0; i<blockcount; ++i) {
    AdpcmFlag flag = spu.read_adpcm_block(buf, current, hist1, hist2);

    //TODO: 扫频
    //TODO: 噪声模式
    resample(buf, blockcount);
    apply_adsr(buf, blockcount);
    
    if (flag.loop_start) {
      repeat = current;
    } else if (flag.loop_end) {
      current = repeat;
      spu.set_endx_flag(Number);
      
      if (!flag.loop_repeat) {
        //Force Release, Env=0000h
        adsr_state = AdsrState::Release;
      }
    }

    if (ADDR_IN_SCOPE(current, SPU_ADPCM_BLK_SZ, repeat)) {
      current = repeat;
    }
  }

  pcmStartAddr.r.saveAddr(current);
  pcmRepeatAddr.r.saveAddr(repeat);
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


#undef SPU_CHANNEL_DEF
}