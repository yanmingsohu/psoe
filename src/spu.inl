#pragma once

#include "spu.h"

namespace ps1e {

template<DeviceIOMapper t_vol, DeviceIOMapper t_sr,
         DeviceIOMapper t_sa,  DeviceIOMapper t_adsr,
         DeviceIOMapper t_acv, DeviceIOMapper t_ra,
         DeviceIOMapper t_cv,  int Number>
SPUChannel<t_vol, t_sr, t_sa, t_adsr, t_acv, t_ra, t_cv, Number>::
SPUChannel(SoundProcessing& parent, Bus& b) :
  volume(*this, b), 
  pcmSampleRate(*this, b), 
  pcmStartAddr(*this, b),
  adsr(*this, b), 
  adsrVol(*this, b), 
  pcmRepeatAddr(*this, b), 
  currVolume(*this, b) 
{
}

}