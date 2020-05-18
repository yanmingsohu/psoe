#pragma once

#include "util.h"
#include "io.h"
#include "bus.h"

namespace ps1e {

class SoundProcessing;


class U16Reg {
protected:
  u16 reg;
public:
  inline void write(u32 value) {
    reg = 0xFFFF & value;
  }
  inline u32 read() {
    return reg;
  }
};


template<DeviceIOMapper t_vol, DeviceIOMapper t_sr,
         DeviceIOMapper t_sa,  DeviceIOMapper t_adsr,
         DeviceIOMapper t_acv, DeviceIOMapper t_ra,
         DeviceIOMapper t_cv,  int Number>
class SPUChannel {
private:
  InnerDeviceIO<SPUChannel, U32Reg> volume;
  InnerDeviceIO<SPUChannel, U16Reg> pcmSampleRate;
  InnerDeviceIO<SPUChannel, U16Reg> pcmStartAddr;
  InnerDeviceIO<SPUChannel, U32Reg> adsr;
  InnerDeviceIO<SPUChannel, U16Reg> adsrVol;
  InnerDeviceIO<SPUChannel, U16Reg> pcmRepeatAddr;
  InnerDeviceIO<SPUChannel, U32Reg> currVolume;

public:
  SPUChannel(SoundProcessing& parent, Bus& b) : 
    volume(*this), pcmSampleRate(*this), pcmStartAddr(*this),
    adsr(*this), adsrVol(*this), pcmRepeatAddr(*this), currVolume(*this)
  {
    b.bind_io(t_vol,  &volume);
    b.bind_io(t_sr,   &pcmSampleRate);
    b.bind_io(t_sa,   &pcmStartAddr);
    b.bind_io(t_adsr, &adsr);
    b.bind_io(t_acv,  &adsrVol);
    b.bind_io(t_ra,   &pcmRepeatAddr);
    b.bind_io(t_cv,   &currVolume);
  }
};


#define SPU_CHANNEL_TYPES(_, name, __, ___)  DeviceIOMapper::name,
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

  Reg32<DeviceIOMapper::spu_main_vol>       mainVol;
  Reg32<DeviceIOMapper::spu_reverb_vol>     reverbVol;
  Reg32<DeviceIOMapper::spu_cd_vol>         cdVol;
  Reg32<DeviceIOMapper::spu_ext_vol>        externVol;
  Reg32<DeviceIOMapper::spu_curr_main_vol>  mainCurrVol;

  Reg32<DeviceIOMapper::spu_n_key_on>       nKeyOn;
  Reg32<DeviceIOMapper::spu_n_key_off>      nKeyOff;
  Reg32<DeviceIOMapper::spu_n_fm>           nFM;
  Reg32<DeviceIOMapper::spu_n_noise>        nNoise;
  Reg32<DeviceIOMapper::spu_n_reverb>       nReverb;
  Reg32<DeviceIOMapper::spu_n_status>       nStatus;

  Reg16<DeviceIOMapper::spu_ram_rev_addr>   ramReverbStartAddress;
  Reg16<DeviceIOMapper::spu_ram_irq>        ramIrqAdress;
  Reg16<DeviceIOMapper::spu_ram_trans_addr> ramTransferAddress;
  Reg16<DeviceIOMapper::spu_ram_trans_fifo> ramTransferFifo;
  Reg16<DeviceIOMapper::spu_ram_trans_ctrl> ramTransferCtrl;
  Reg16<DeviceIOMapper::spu_ctrl>           ctrl;
  Reg16<DeviceIOMapper::spu_status>         status;

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
  
  SPU_DEF_ALL_CHANNELS(ch, SPU_DEF_VAL)

public:
  SoundProcessing(Bus&);
};

#undef SPU_DEF_VAL
#undef SPU_CHANNEL_TYPES
#undef SPU_CHANNEL_CLASS
}