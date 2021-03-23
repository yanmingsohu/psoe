#pragma once

#include "spu.h"

namespace ps1e {

#define MinT(a,b)   (((a) < (b)) ? (a) : (b))
#define MaxT(a,b)   (((a) > (b)) ? (a) : (b))
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
  pcmSampleRate(*this, b, &SPUChannel::set_sample_rate), 
  pcmStartAddr(*this, b, &SPUChannel::set_start_address),
  adsr(*this, b), 
  adsrVol(*this, b), 
  pcmRepeatAddr(*this, b, &SPUChannel::set_repeat_addr), 
  currVolume(*this, b),
  resample(this),
  lowpass(/*parent.getOutputRate()*/)
{
}


SPU_CHANNEL_DEF(PcmSample)::read_pcm_sample() {
  if (pcm_buf_remaining <= 0) {
    // read next block
    if (currentReadAddr.changed) {
      currentReadAddr.set(pcmStartAddr.r.address(), 0, 0);
      pcm_buf_remaining = 0;
    }
    if (repeatAddr.changed) {
      repeatAddr.set(pcmRepeatAddr.r.address(), 0, 0);
    }

    PcmSample prevh1 = currentReadAddr.hist1;
    PcmSample prevh2 = currentReadAddr.hist2;
    AdpcmFlag flag = spu.read_adpcm_block(pcm_read_buf, currentReadAddr);
    pcm_buf_remaining = SPU_PCM_BLK_SZ;
    
    if (flag.loop_start) {
      repeatAddr.set(currentReadAddr.addr, prevh1, prevh2);
      pcmRepeatAddr.r.saveAddr(repeatAddr.addr);
    }

    if (flag.loop_end) {
      spu.set_endx_flag(Number);

      if (flag.loop_repeat) {
        currentReadAddr = repeatAddr;
      } else {
        adsr_state = AdsrState::Release;
        //TODO: Mute??
      }
    } else {
      currentReadAddr.addr += SPU_ADPCM_BLK_SZ;
    }
  }

  const s32 p = SPU_PCM_BLK_SZ - pcm_buf_remaining;
  --pcm_buf_remaining;
  return pcm_read_buf[p];
}


//TODO 双声道
SPU_CHANNEL_DEF(void)::read_sample_blocks(PcmSample *_in, PcmSample *out, u32 nframe) {
  const double rate = play_rate;
  bool direct = (rate == 0 || rate == 1);

  if (!direct) {
    direct = !resample.read(_in, nframe, rate);
  }

  if (direct) {
    for (u32 i=0; i<nframe; ++i) {
      _in[i] = read_pcm_sample();
    }
  } else if (spu.use_low_pass) {
    //TODO 低通滤波 
    lowpass.filter(_in, nframe, spu.getOutputRate() / rate);
  }
  apply_adsr(_in, out, nframe);
}


SPU_CHANNEL_DEF(void)::apply_adsr(PcmSample *_in, PcmSample *out, u32 lsize) {
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
        out[i] = _in[i] * (float(level) / float(0x8000));
      } else {
        out[i] = 0;
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


SPU_CHANNEL_DEF(void)::copy_start_to_repeat() {
  pcmRepeatAddr.r.v = pcmStartAddr.r.v;
  repeatAddr.changed = true;
}


SPU_CHANNEL_DEF(void)::set_start_address(u32 v) {
  currentReadAddr.changed = true;
  spudbg("set channel %d start address %x (%x << 3)\n", Number, v<<3, v);
}


SPU_CHANNEL_DEF(void)::set_repeat_addr(u32) {
  repeatAddr.changed = true;
}


SPU_CHANNEL_DEF(void)::set_sample_rate(u32 v) {
  double orate = spu.getOutputRate();
  double irate = double(MinT(0x4000, v)) * (double(SPU_WORK_FREQ) / double(0x1000));
  double r = orate/irate;
  if (r < (1.0 / 255.0) || r > 255) {
    play_rate = 0;
  } else {
    play_rate = r;
  }
}


#undef SPU_CHANNEL_DEF
}