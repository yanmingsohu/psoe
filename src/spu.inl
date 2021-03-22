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
SPU_CHANNEL_DEF(void)::read_sample(PcmSample *_in, PcmSample *_out, u32 blockcount) {
  repeatAddr.addr = pcmRepeatAddr.r.address();
  PcmSample* pcmbuf = _out;

  if (currentReadAddr.changed) {
    currentReadAddr.set(pcmStartAddr.r.address(), 0, 0);
  }

  for (u32 i=0; i<blockcount; ++i) {
    AdpcmFlag flag = spu.read_adpcm_block(pcmbuf, currentReadAddr);
    resample(pcmbuf, SPU_PCM_BLK_SZ);
    apply_adsr(pcmbuf, SPU_PCM_BLK_SZ);
    
    if (flag.loop_start) {
      repeatAddr = currentReadAddr;
      pcmRepeatAddr.r.saveAddr(repeatAddr.addr);
    }

    if (flag.loop_end) {
      spu.set_endx_flag(Number);

      if (flag.loop_repeat) {
        currentReadAddr = repeatAddr;
      } else {
        adsr_state = AdsrState::Release;
      }
    }
    else if (ADDR_IN_SCOPE(currentReadAddr.addr + 0x10, SPU_ADPCM_BLK_SZ, repeatAddr.addr)) {
      // 预判下一个音块数据, 不解析循环区段
      currentReadAddr = repeatAddr;
    } else {
      currentReadAddr.addr += SPU_ADPCM_BLK_SZ;
    }

    pcmbuf += SPU_PCM_BLK_SZ;
  }
}


SPU_CHANNEL_DEF(void)::apply_adsr(PcmSample* buf, u32 lsize) {
  s32 decay_out = (adsr.r.su_lv +1) *0x800;
  s32 level = adsrVol.r.v;
  //const u32 lsize = blockcount * SPU_PCM_BLK_SZ;

  for (u32 i=0; i<lsize;) {
    if (spu.is_release_on(Number)) {
      //TODO 不知道是否应该复位 nKeyOff
      adsr_state = AdsrState::Release;
      adsr_filter.reset(adsr.r.re_md, 1, adsr.r.re_sh, 0);
      adsr_cycles_remaining = 0;
    }
    else if (spu.is_attack_on(Number)) {
      adsr_state = AdsrState::Attack;
      adsrVol.r.v = 0;
      adsr_filter.reset(adsr.r.at_md, 0, adsr.r.at_sh, adsr.r.at_st);
      adsr_cycles_remaining = 0;
    }

    while (adsr_cycles_remaining > 0 && i <lsize) {
      if (level > 0) {
        buf[i] *= float(level) / float(0x8000);
      } else {
        buf[i] = 0;
      }
      --adsr_cycles_remaining;
      ++i;
    }

    switch (adsr_state) {
    case AdsrState::Release:
      adsr_filter.next(level, adsr_cycles_remaining);
      if (level <= 0) {
        level = 0;
        adsr_state = AdsrState::Wait;
      }
      //break; no break;

    case AdsrState::Wait:
      return;

    case AdsrState::Attack:
      adsr_filter.next(level, adsr_cycles_remaining);
      if (level > 0x7FFF) {
        level = 0x7fff;
        adsr_state = AdsrState::Decay;
        adsr_filter.reset(1, 1, adsr.r.de_sh, 0);
        decay_out = (adsr.r.su_lv +1) *0x800;
      }
      break;

    case AdsrState::Decay:
      adsr_filter.next(level, adsr_cycles_remaining);
      if (level <= decay_out) {
        level = decay_out;
        adsr_state = AdsrState::Sustain;
        adsr_filter.reset(adsr.r.su_md, adsr.r.su_di, adsr.r.su_sh, adsr.r.su_st);
      }
      break;

    case AdsrState::Sustain:
      adsr_filter.next(level, adsr_cycles_remaining);
      if (level < 0) level = 0;
      else if (level > 0x7fff) level = 0x7fff;
      break;
    }
    //printf("%d %d \n", adsr_state, level);
  }

  adsrVol.r.v = u32(level) & 0x07FFF;
}


SPU_CHANNEL_DEF(void)::resample(PcmSample* buf, u32 sample_count) {
}


SPU_CHANNEL_DEF(void)::copy_start_to_repeat() {
  pcmRepeatAddr.r.v = pcmStartAddr.r.v;
}


SPU_CHANNEL_DEF(void)::set_start_address(u32 v) {
  currentReadAddr.changed = true;
  spudbg("set channel %d start address %x (%x << 3)\n", Number, v<<3, v);
}


#undef SPU_CHANNEL_DEF
}