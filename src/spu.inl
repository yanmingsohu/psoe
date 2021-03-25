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
  volume(*this, b, &SPUChannel::set_volume), 
  pcmSampleRate(*this, b, &SPUChannel::set_sample_rate), 
  pcmStartAddr(*this, b, &SPUChannel::set_start_address),
  adsr(*this, b), 
  adsrVol(*this, b), 
  pcmRepeatAddr(*this, b, &SPUChannel::set_repeat_addr), 
  currVolume(*this, b),
  resample(this),
  lowpass(/*parent.getOutputRate()*/)
{
  /*printf("InitCH %d vol.%x rate.%x addr.%x adsrvol.%x repeat.%x cval.%x\n", 
         Number, t_vol, t_sr, t_sa, t_adsr, t_acv, t_ra, t_cv);*/
}


SPU_CHANNEL_DEF(PcmSample)::readPcmSample() {
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
    AdpcmFlag flag = spu.readAdpcmBlock(pcm_read_buf, currentReadAddr);
    pcm_buf_remaining = SPU_PCM_BLK_SZ;
    
    if (flag.loop_start) {
      repeatAddr.set(currentReadAddr.addr, prevh1, prevh2);
      pcmRepeatAddr.r.saveAddr(repeatAddr.addr);
    }

    if (flag.loop_end) {
      spu.setEndxFlag(Number);

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


SPU_CHANNEL_DEF(bool)::readSampleBlocks(PcmSample *_in, PcmSample *out, u32 nframe) {
  const double rate = play_rate;
  if (rate == 0) return false; //TODO 停止工作, 噪音模式?
  if (adsr.r.v == 0) return false; //TODO: 待验证 ADSR 为0停止工作??
  
  if (spu.isNoise(Number)) {
    spu.readNoiseSampleBlocks(_in, nframe);
  }
  else if (resample.read(_in, nframe, rate)) {
    if (spu.use_low_pass) {
      lowpass.filter(_in, nframe, spu.getOutputRate() / rate);
    }
  }
  else {
    return false;
  }

  // 扫描模式是否启用adsr?
  applyADSR(_in, out, nframe);
  return true;
}


SPU_CHANNEL_DEF(void)::applyADSR(PcmSample *_in, PcmSample *out, u32 lsize) {
  s32 decay_out = (adsr.r.su_lv +1) *0x800;
  s32 level = adsrVol.r.v;
  
  for (u32 i=0; i<lsize;) {
    if (spu.isReleaseOn(Number)) {
      adsr_state = AdsrState::Release;
      adsr_filter.reset(adsr.r.re_md, 1, adsr.r.re_sh, 0);
      adsr_cycles_remaining = 0;
    }
    else if (spu.isAttackOn(Number)) {
      adsr_state = AdsrState::Attack;
      level = 0;
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
      break;

    case AdsrState::Wait:
      while (i < lsize) {
        out[i] = 0;
        ++i;
      }
      break;

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
  }
  //printf("\r%d %d", Number, level);
  adsrVol.r.v = u32(level) & 0x07FFF;
}


SPU_CHANNEL_DEF(void)::copyStartToRepeat() {
  pcmRepeatAddr.r.v = pcmStartAddr.r.v;
  repeatAddr.changed = true;
  spudbg("set %d start -> repeat, ken on, adsr %x vol %x\n", 
         Number, adsr.r.v, adsrVol.r.v);
}


SPU_CHANNEL_DEF(void)::set_start_address(u32 v, u32) {
  currentReadAddr.changed = true;
  spudbg("set channel %d start address %x (%x << 3) adsr %x vol %x\n", 
         Number, v<<3, adsr.r.v, adsrVol.r.v);
}


SPU_CHANNEL_DEF(void)::set_repeat_addr(u32, u32) {
  repeatAddr.changed = true;
}


SPU_CHANNEL_DEF(void)::set_sample_rate(u32 v, u32) {
  double orate = spu.getOutputRate();
  double irate = double(MinT(0x4000, v)) * (double(SPU_WORK_FREQ) / double(0x1000));
  double r = orate/irate;
  if (r < (1.0 / 255.0) || r > 255) {
    play_rate = 0;
  } else {
    play_rate = r;
  }
  spudbg("set channel %d sample rate %f\n", Number, r);
}


// 当音量变为扫描模式时, 需要同步 currVolume 寄存器
SPU_CHANNEL_DEF(void)::set_volume(u32 v, u32 old) {
  const u32 lmask = 1<<15;
  u32 curr = currVolume.r.v;
  if (((v & lmask) == 1) && ((old & lmask) == 0)) {
    curr = (curr & 0xffff'0000) | ((old & 0x0000'7fff) << 1);
  }
  const u32 rmask = 1<<31;
  if (((v & rmask) == 1) && ((old & rmask) == 0)) {
    curr = (curr & 0x0000'ffff) | ((old & 0x7fff'0000) << 1);
  }
  currVolume.r.v = curr;
}


SPU_CHANNEL_DEF(VolumeEnvelope*)::getVolumeEnvelope(bool left) {
  VolData vd = (left ? volume.r.left : volume.r.right);
  s32 clevel = (left ? s16(currVolume.r.v & 0xffff) : s16(currVolume.r.v >> 16));
  return sweet_ve.get(left, vd, clevel);
}


SPU_CHANNEL_DEF(void)::syncVol(VolumeEnvelope* l, VolumeEnvelope* r) {
  currVolume.r.v = l->getVol() | (r->getVol() << 16);
}


SPU_CHANNEL_DEF(u32)::getVar(SpuChVarFlag f) {
  switch (f) {
    case SpuChVarFlag::volume:
      return volume.r.v;
    case SpuChVarFlag::work_volume:
      return currVolume.r.v;
    case SpuChVarFlag::sample_rate:
      return pcmSampleRate.r.v;
    case SpuChVarFlag::start_address:
      return currentReadAddr.addr;
    case SpuChVarFlag::repeat_address:
      return repeatAddr.addr;
    case SpuChVarFlag::adsr:
      return adsr.r.v;
    case SpuChVarFlag::adsr_volume:
      return adsrVol.r.v;
    case SpuChVarFlag::adsr_state:
      return u32(adsr_state);
    default: return 0;
  }
}


#undef SPU_CHANNEL_DEF
}